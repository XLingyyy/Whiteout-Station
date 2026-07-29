from __future__ import annotations

import copy
import json
import uuid
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable


ROOT = Path(__file__).resolve().parents[2]
RULES_DIRECTORY = ROOT / "WhiteoutStation" / "Content" / "Rules"
DEFAULT_RULES_PATH = RULES_DIRECTORY / "WhiteoutStationRules.v1.0.json"

PROMISE_CONDITIONS = frozenset(
    {"reserve_medicine", "keep_records", "heat_repair_room"}
)

# Older route fixtures predate explicit dialogue intent parameters. Explicit JSON
# parameters take precedence when present.
ROUTE_DIALOGUE_DEFAULTS: dict[tuple[str, str], dict[str, Any]] = {
    ("medical_cooperation", "talk_gu_heng"): {
        "dialogue_act": "promise",
        "promise_condition": "heat_repair_room",
    },
    ("technical_replacement", "talk_gu_heng"): {
        "dialogue_act": "challenge",
    },
}


@dataclass(frozen=True)
class ActionResult:
    action_id: str
    committed: bool
    reason_code: str
    ap_before: int
    ap_after: int
    transaction_id: str
    changes: dict[str, Any]
    crisis_triggered: bool = False


class RuleError(ValueError):
    """Raised when the configuration cannot produce deterministic play."""


def load_rules(path: Path | str | None = None) -> dict[str, Any]:
    if path is None:
        path = DEFAULT_RULES_PATH
    return json.loads(Path(path).read_text(encoding="utf-8"))


def validate_rules(rules: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    actions = rules.get("actions", [])
    facts = rules.get("facts", [])
    action_ids = [item.get("id") for item in actions]
    fact_ids = [item.get("id") for item in facts]

    if len(actions) != 13:
        errors.append(f"Expected 13 core actions, found {len(actions)}")
    if len(action_ids) != len(set(action_ids)):
        errors.append("Action IDs must be unique")
    if len(fact_ids) != len(set(fact_ids)):
        errors.append("Fact IDs must be unique")
    if any(not item or not isinstance(item, str) for item in action_ids):
        errors.append("Every action requires a stable string ID")
    if any(action.get("ap_cost", -1) < 0 for action in actions):
        errors.append("Action AP costs cannot be negative")
    if sum(rules.get("score", {}).get("weights", {}).values()) != 100:
        errors.append("Score weights must total 100")
    if rules.get("gameplay", {}).get("starting_action_points") != 12:
        errors.append("v1.0 requires exactly 12 starting AP")
    if rules.get("gameplay", {}).get("mid_crisis_threshold") != 6:
        errors.append("v1.0 mid-crisis threshold must be 6 AP")
    for character_id, character in (
        rules.get("initial_state", {}).get("characters", {}).items()
    ):
        for stat in (
            "health",
            "temperature",
            "hunger",
            "fatigue",
            "pressure",
            "trust",
        ):
            value = character.get(stat)
            if not isinstance(value, (int, float)) or not 0 <= value <= 10:
                errors.append(
                    f"Character {character_id} {stat} must be within 0..10"
                )

    action_set = set(action_ids)
    for route_id, route in rules.get("routes", {}).items():
        spent = 0
        for step in route.get("steps", []):
            action_id = step.get("action")
            if action_id not in action_set:
                errors.append(f"Route {route_id} references unknown action {action_id}")
                continue
            spent += next(a["ap_cost"] for a in actions if a["id"] == action_id)
        if spent > rules["gameplay"]["starting_action_points"]:
            errors.append(f"Route {route_id} costs {spent} AP")
        if route.get("expected_ap_spent") != spent:
            errors.append(
                f"Route {route_id} expected_ap_spent={route.get('expected_ap_spent')} "
                f"but steps cost {spent}"
            )
    return errors


def classify_rating(total: float, ratings: dict[str, list[float]]) -> str:
    ordered = sorted(
        ratings.items(),
        key=lambda item: float(item[1][0]),
        reverse=True,
    )
    if not ordered:
        raise RuleError("Score ratings cannot be empty")
    for rating, (minimum, _maximum) in ordered:
        if total >= float(minimum):
            return rating
    return ordered[-1][0]


class WhiteoutSimulator:
    """Deterministic, transaction-safe implementation of the v1.0 rules."""

    KNOWLEDGE_ORDER = {"unknown": 0, "claimed": 1, "suspected": 2, "confirmed": 3}

    def __init__(self, rules: dict[str, Any] | None = None):
        self.rules = copy.deepcopy(rules or load_rules())
        errors = validate_rules(self.rules)
        if errors:
            raise RuleError("; ".join(errors))
        self.actions = {item["id"]: item for item in self.rules["actions"]}
        self.facts = {item["id"]: item for item in self.rules["facts"]}
        self.state = self._make_initial_state()

    def _make_initial_state(self) -> dict[str, Any]:
        state = copy.deepcopy(self.rules["initial_state"])
        state.update(
            {
                "ap": self.rules["gameplay"]["starting_action_points"],
                "phase": "action_phase",
                "mid_crisis_triggered": False,
                "player_knowledge": {},
                "evidence": [],
                "public_facts": [],
                "action_counts": {},
                "committed_transactions": [],
                "promises": [],
                "event_log": [],
                "model_calls": 0,
                "ending": None,
                "score": None,
            }
        )
        return state

    def new_game(self) -> None:
        self.state = self._make_initial_state()

    def _action_count(self, action_id: str) -> int:
        return int(self.state["action_counts"].get(action_id, 0))

    @staticmethod
    def _promise_condition(params: dict[str, Any]) -> Any:
        return params.get("promise_condition", params.get("condition"))

    def _knows(self, fact_id: str, at_least: str = "suspected") -> bool:
        current = self.state["player_knowledge"].get(fact_id, "unknown")
        return self.KNOWLEDGE_ORDER[current] >= self.KNOWLEDGE_ORDER[at_least]

    def _discover_fact(self, fact_id: str, level: str) -> None:
        current = self.state["player_knowledge"].get(fact_id, "unknown")
        if self.KNOWLEDGE_ORDER[level] > self.KNOWLEDGE_ORDER[current]:
            self.state["player_knowledge"][fact_id] = level

    def _add_evidence(self, *evidence_ids: str) -> None:
        known = set(self.state["evidence"])
        known.update(evidence_ids)
        self.state["evidence"] = sorted(known)

    @staticmethod
    def _clamp(value: float, low: float, high: float) -> float:
        return max(low, min(high, value))

    def _change_character(self, character_id: str, **changes: float) -> None:
        character = self.state["characters"][character_id]
        for stat, delta in changes.items():
            character[stat] = self._clamp(character[stat] + delta, 0, 10)

    def _can_execute(self, action_id: str, params: dict[str, Any]) -> str:
        if action_id not in self.actions:
            return "unknown_action"
        action = self.actions[action_id]
        if self.state["phase"] not in {"action_phase", "post_action_window"}:
            return "phase_locked"
        if action["ap_cost"] > self.state["ap"]:
            return "insufficient_ap"
        count = self._action_count(action_id)
        if not action.get("repeatable", False) and count > 0:
            return "already_completed"
        if count >= action.get("max_uses", 10**9):
            return "use_limit_reached"

        flags = self.state["flags"]
        resources = self.state["resources"]
        tasks = self.state["tasks"]
        gu = self.state["characters"]["gu_heng"]
        dialogue_act = str(params.get("dialogue_act", "ask")).strip().lower()

        if action_id in {"talk_gu_heng", "talk_ye_cheng"}:
            if dialogue_act not in {"ask", "challenge", "reassure", "promise"}:
                return "dialogue_act_unavailable"
            condition = self._promise_condition(params)
            has_condition = condition is not None and str(condition) != ""
            if dialogue_act != "promise" and has_condition:
                return "invalid_promise_condition"
            if dialogue_act == "challenge":
                challenge_available = (
                    self._knows("FACT_FORCED_RESTART_SUSPICION")
                    or self._knows("FACT_BURNT_RELAY")
                    if action_id == "talk_gu_heng"
                    else flags["heat_pack_revealed"]
                )
                if not challenge_available:
                    return "dialogue_act_unavailable"
            elif dialogue_act == "reassure":
                character_id = (
                    "gu_heng" if action_id == "talk_gu_heng" else "ye_cheng"
                )
                threshold = 6.5 if character_id == "gu_heng" else 6.0
                reassure_available = (
                    self.state["mid_crisis_triggered"]
                    or self.state["characters"][character_id]["pressure"] >= threshold
                    or (
                        character_id == "gu_heng"
                        and flags["gu_heng_diagnosed"]
                    )
                )
                if not reassure_available:
                    return "dialogue_act_unavailable"
            if dialogue_act == "promise":
                if condition not in PROMISE_CONDITIONS:
                    return "invalid_promise_condition"
                if action_id != "talk_gu_heng":
                    return "dialogue_act_unavailable"
                context_available = (
                    condition == "reserve_medicine"
                    and flags["gu_heng_diagnosed"]
                    and resources["medicine"] > 0
                ) or (
                    condition == "keep_records"
                    and (
                        self._knows("FACT_FORCED_RESTART_SUSPICION")
                        or self._knows("FACT_FORCED_RESTART_CONFIRMED")
                    )
                ) or (
                    condition == "heat_repair_room"
                    and (
                        flags["gu_heng_diagnosed"]
                        or self._knows("FACT_HAND_INJURY")
                    )
                    and not flags["repair_room_heated"]
                )
                if not context_available:
                    return "dialogue_act_unavailable"
                promise_id = f"player_to_gu_heng:{condition}"
                if any(
                    promise.get("id") == promise_id
                    for promise in self.state["promises"]
                ):
                    return "duplicate_promise"

        if action_id == "heat_repair_room":
            if flags["repair_room_heated"]:
                return "already_heated"
            if resources["fuel"] < 1:
                return "needs_fuel"
        elif action_id == "heat_medical_room":
            if flags["medical_room_heated"]:
                return "already_heated"
            if resources["fuel"] < 1:
                return "needs_fuel"
        elif action_id == "distribute_food":
            allocations = [int(params.get(key, 0)) for key in ("player", "gu_heng", "ye_cheng")]
            if any(value not in (0, 1) for value in allocations):
                return "invalid_food_allocation"
            if sum(allocations) == 0:
                return "empty_food_allocation"
            if sum(allocations) > resources["food"]:
                return "insufficient_food"
        elif action_id == "treat_gu_heng":
            if not flags["medical_room_heated"]:
                return "needs_medical_heat"
            if not flags["gu_heng_diagnosed"]:
                return "needs_diagnosis"
            resource = params.get("resource", "medicine")
            if resource == "heat_pack" and not flags["heat_pack_revealed"]:
                return "heat_pack_hidden"
            if resource not in {"medicine", "heat_pack"}:
                return "invalid_treatment_resource"
            if resources[resource] < 1:
                return f"needs_{resource}"
        elif action_id == "dismantle_kitchen_heater":
            if not self._knows("FACT_BURNT_RELAY"):
                return "needs_relay_evidence"
            if not flags["kitchen_heater_intact"]:
                return "heater_already_dismantled"
        elif action_id == "repair_generator":
            if tasks["generator_progress"] >= self.rules["gameplay"]["generator_required"]:
                return "generator_already_repaired"
            if gu["health"] <= self.rules["balance"]["thresholds"]["critical_health"]:
                return "gu_heng_critical"
            if not (
                flags["gu_heng_cooperative"]
                or flags["gu_heng_treated"]
                or (flags["repair_room_heated"] and flags["gu_heng_fed"])
            ):
                return "needs_cooperation"
        elif action_id == "forced_self_repair":
            if not self._knows("FACT_FORCED_RESTART_SUSPICION"):
                return "needs_generator_records"
            if flags["self_repair_used"]:
                return "self_repair_already_used"
            if tasks["generator_progress"] >= self.rules["gameplay"]["generator_required"]:
                return "generator_already_repaired"
        elif action_id == "calibrate_antenna":
            if tasks["generator_progress"] < self.rules["gameplay"]["generator_required"]:
                return "needs_generator"
            if tasks["antenna_calibration"] >= self.rules["gameplay"]["antenna_required"]:
                return "antenna_already_calibrated"
            if (
                self.state["characters"]["player"]["temperature"]
                < self.rules["balance"]["thresholds"]["safe_antenna_temperature"]
            ):
                return "player_too_cold"
        elif action_id == "send_signal":
            if tasks["generator_progress"] < self.rules["gameplay"]["generator_required"]:
                return "needs_generator"
            if tasks["antenna_calibration"] < self.rules["gameplay"]["antenna_required"]:
                return "needs_antenna"
        return "ok"

    def preview_action(self, action_id: str, params: dict[str, Any] | None = None) -> dict[str, Any]:
        params = params or {}
        reason = self._can_execute(action_id, params)
        action = self.actions.get(action_id, {})
        return {
            "action_id": action_id,
            "can_execute": reason == "ok",
            "reason_code": reason,
            "ap_cost": action.get("ap_cost", 0),
            "preview": action.get("preview", ""),
            "risk": action.get("risk", ""),
        }

    def apply_action(
        self,
        action_id: str,
        params: dict[str, Any] | None = None,
        transaction_id: str | None = None,
    ) -> ActionResult:
        params = copy.deepcopy(params or {})
        transaction_id = transaction_id or str(uuid.uuid4())
        ap_before = int(self.state["ap"])
        if transaction_id in self.state["committed_transactions"]:
            return ActionResult(
                action_id,
                False,
                "duplicate_transaction",
                ap_before,
                ap_before,
                transaction_id,
                {},
            )

        reason = self._can_execute(action_id, params)
        if reason != "ok":
            return ActionResult(action_id, False, reason, ap_before, ap_before, transaction_id, {})

        before = self._snapshot_for_diff()
        self._apply_effect(action_id, params)
        ap_cost = int(self.actions[action_id]["ap_cost"])
        self._apply_environment(ap_cost, outdoors=action_id == "calibrate_antenna")
        self.state["ap"] = max(0, ap_before - ap_cost)
        self.state["action_counts"][action_id] = self._action_count(action_id) + 1
        self.state["committed_transactions"].append(transaction_id)

        crisis_triggered = False
        threshold = self.rules["gameplay"]["mid_crisis_threshold"]
        if (
            not self.state["mid_crisis_triggered"]
            and ap_before > threshold
            and self.state["ap"] <= threshold
        ):
            self._trigger_mid_crisis()
            crisis_triggered = True

        if action_id == "send_signal":
            self.state["phase"] = "ending_choice"
        elif self._signal_available():
            self.state["phase"] = "post_action_window"
        elif self.state["ap"] == 0:
            self.state["phase"] = "ending"
        else:
            self.state["phase"] = "action_phase"

        changes = self._diff(before, self._snapshot_for_diff())
        self.state["event_log"].append(
            {
                "index": len(self.state["event_log"]) + 1,
                "action_id": action_id,
                "transaction_id": transaction_id,
                "ap_before": ap_before,
                "ap_after": self.state["ap"],
                "reason_code": "committed",
                "changes": changes,
                "crisis_triggered": crisis_triggered,
            }
        )
        return ActionResult(
            action_id,
            True,
            "committed",
            ap_before,
            int(self.state["ap"]),
            transaction_id,
            changes,
            crisis_triggered,
        )

    def _apply_effect(self, action_id: str, params: dict[str, Any]) -> None:
        flags = self.state["flags"]
        resources = self.state["resources"]
        tasks = self.state["tasks"]
        balance = self.rules["balance"]["actions"]

        if action_id == "investigate_generator_log":
            self._add_evidence("EVIDENCE_DEEP_GENERATOR_LOG")
            self._discover_fact("FACT_GENERATOR_PROTECTION_STOP", "confirmed")
            self._discover_fact("FACT_FORCED_RESTART_SUSPICION", "suspected")
            flags["records_preserved"] = True
        elif action_id == "inspect_control_cabinet":
            self._add_evidence(
                "EVIDENCE_BURNT_RELAY", "EVIDENCE_ARC_MARKS", "EVIDENCE_HAND_OBSERVATION"
            )
            self._discover_fact("FACT_BURNT_RELAY", "confirmed")
            self._discover_fact("FACT_HAND_INJURY", "suspected")
        elif action_id == "talk_ye_cheng":
            self._change_character(
                "ye_cheng",
                trust=balance["talk_ye_cheng"]["ye_trust"],
                pressure=balance["talk_ye_cheng"]["ye_pressure"],
            )
            flags["gu_heng_diagnosed"] = True
            self._add_evidence("EVIDENCE_MEDICAL_DIAGNOSIS")
            self._discover_fact("FACT_HAND_INJURY", "confirmed")
            self._discover_fact("FACT_MEDICAL_DIAGNOSIS", "confirmed")
            if (
                self.state["characters"]["ye_cheng"]["trust"]
                >= self.rules["balance"]["thresholds"]["ye_heat_pack_disclosure_trust"]
            ):
                flags["heat_pack_revealed"] = True
                self._add_evidence("EVIDENCE_HEAT_PACK")
                self._discover_fact("FACT_HEAT_PACK", "confirmed")
            self._apply_dialogue_modifier(action_id, params)
        elif action_id == "talk_gu_heng":
            if flags["gu_heng_treated"]:
                delta = balance["talk_gu_heng_treated"]
                self._change_character(
                    "gu_heng", trust=delta["gu_trust"], pressure=delta["gu_pressure"]
                )
                flags["gu_heng_cooperative"] = True
            elif self._knows("FACT_FORCED_RESTART_SUSPICION") and self._knows(
                "FACT_BURNT_RELAY"
            ):
                delta = balance["talk_gu_heng_evidence"]
                self._change_character(
                    "gu_heng", trust=delta["gu_trust"], pressure=delta["gu_pressure"]
                )
                flags["gu_heng_cooperative"] = True
                flags["relay_compatibility_known"] = True
                self._add_evidence("EVIDENCE_HEATER_SERVICE_LABEL")
                self._discover_fact("FACT_RELAY_COMPATIBILITY", "confirmed")
                self._discover_fact("FACT_FORCED_RESTART_CONFIRMED", "confirmed")
            else:
                delta = balance["talk_gu_heng_unprepared"]
                self._change_character(
                    "gu_heng", trust=delta["gu_trust"], pressure=delta["gu_pressure"]
                )
            self._apply_dialogue_modifier(action_id, params)
            if str(params.get("dialogue_act", "ask")).strip().lower() == "promise":
                self._recognize_promise(params)
        elif action_id == "heat_repair_room":
            resources["fuel"] -= 1
            flags["repair_room_heated"] = True
            delta = balance["heat_repair_room"]
            self._change_character(
                "gu_heng", temperature=delta["gu_temperature"], pressure=delta["gu_pressure"]
            )
        elif action_id == "heat_medical_room":
            resources["fuel"] -= 1
            flags["medical_room_heated"] = True
            delta = balance["heat_medical_room"]
            self._change_character(
                "gu_heng", temperature=delta["gu_temperature"]
            )
            self._change_character(
                "ye_cheng",
                temperature=delta["ye_temperature"],
                pressure=delta["ye_pressure"],
            )
        elif action_id == "distribute_food":
            allocations = {key: int(params.get(key, 0)) for key in ("player", "gu_heng", "ye_cheng")}
            resources["food"] -= sum(allocations.values())
            for character_id, units in allocations.items():
                if units:
                    self._change_character(
                        character_id, hunger=balance["food_unit"]["hunger"] * units
                    )
            flags["gu_heng_fed"] = allocations["gu_heng"] > 0
            self._change_character(
                "gu_heng",
                trust=(
                    balance["food_fair_gu_trust"]
                    if allocations["gu_heng"]
                    else balance["food_unfair_gu_trust"]
                ),
            )
            self._change_character(
                "ye_cheng",
                trust=(
                    balance["food_fair_ye_trust"]
                    if allocations["ye_cheng"]
                    else balance["food_unfair_ye_trust"]
                ),
            )
        elif action_id == "treat_gu_heng":
            resource = params.get("resource", "medicine")
            resources[resource] -= 1
            delta = balance["treat_with_medicine" if resource == "medicine" else "treat_with_heat_pack"]
            self._change_character(
                "gu_heng",
                health=delta["gu_health"],
                temperature=delta["gu_temperature"],
                fatigue=delta["gu_fatigue"],
                pressure=delta["gu_pressure"],
                trust=delta["gu_trust"],
            )
            flags["gu_heng_treated"] = True
            flags["gu_heng_cooperative"] = True
        elif action_id == "dismantle_kitchen_heater":
            cooperative = flags["gu_heng_cooperative"] and flags["relay_compatibility_known"]
            delta = balance["dismantle_cooperative" if cooperative else "dismantle_solo"]
            if cooperative:
                self._change_character(
                    "gu_heng", health=delta["gu_health"], pressure=delta["gu_pressure"]
                )
            else:
                self._change_character(
                    "player", health=delta["player_health"], fatigue=delta["player_fatigue"]
                )
                self._change_character("gu_heng", trust=delta["gu_trust"])
            resources["replacement_relay"] = 1
            flags["kitchen_heater_intact"] = False
        elif action_id == "repair_generator":
            stable = flags["gu_heng_treated"] or (
                resources["replacement_relay"] > 0 and flags["repair_room_heated"]
            )
            progress = 2 if stable else 1
            if resources["replacement_relay"] > 0:
                resources["replacement_relay"] = 0
                flags["relay_installed"] = True
            tasks["generator_progress"] = min(
                self.rules["gameplay"]["generator_required"],
                tasks["generator_progress"] + progress,
            )
            delta = balance["stable_repair" if stable else "injured_repair"]
            self._change_character(
                "gu_heng",
                **{
                    key.removeprefix("gu_"): value
                    for key, value in delta.items()
                    if key.startswith("gu_")
                },
            )
        elif action_id == "forced_self_repair":
            flags["self_repair_used"] = True
            tasks["generator_progress"] = min(
                self.rules["gameplay"]["generator_required"],
                tasks["generator_progress"] + 1,
            )
            delta = balance["forced_self_repair"]
            self._change_character(
                "player",
                **{
                    key.removeprefix("player_"): value
                    for key, value in delta.items()
                    if key.startswith("player_")
                },
            )
        elif action_id == "calibrate_antenna":
            tasks["antenna_calibration"] = self.rules["gameplay"]["antenna_required"]
        elif action_id == "send_signal":
            tasks["signal_sent"] = True

    def _apply_environment(self, ap_cost: int, outdoors: bool) -> None:
        per_ap = self.rules["balance"]["per_ap"]
        for _ in range(ap_cost):
            for character_id in self.state["characters"]:
                self._change_character(character_id, **per_ap)
            if outdoors:
                outdoor = self.rules["balance"]["outdoor_per_ap"]
                self._change_character(
                    "player",
                    temperature=outdoor["player_temperature"],
                    fatigue=outdoor["player_fatigue"],
                )

    def _trigger_mid_crisis(self) -> None:
        self.state["mid_crisis_triggered"] = True
        balance = self.rules["balance"]["actions"]
        if self.state["tasks"]["generator_progress"] == 0:
            self._change_character(
                "gu_heng", pressure=balance["mid_crisis_generator_idle_pressure"]
            )
            self._change_character(
                "ye_cheng", pressure=balance["mid_crisis_generator_idle_pressure"]
            )
        if not self.state["flags"]["gu_heng_treated"] and self._action_count(
            "repair_generator"
        ):
            self._change_character(
                "gu_heng",
                health=balance["mid_crisis_injured_worker_health"],
                pressure=balance["mid_crisis_injured_worker_pressure"],
            )
        if (
            self.state["characters"]["ye_cheng"]["trust"]
            >= self.rules["balance"]["thresholds"]["ye_heat_pack_disclosure_trust"]
        ):
            self.state["flags"]["heat_pack_revealed"] = True
            self._discover_fact("FACT_HEAT_PACK", "confirmed")

    def _apply_dialogue_modifier(
        self, action_id: str, params: dict[str, Any]
    ) -> None:
        dialogue_act = str(params.get("dialogue_act", "ask")).strip().lower()
        character_id = "gu_heng" if action_id == "talk_gu_heng" else "ye_cheng"
        if dialogue_act == "challenge":
            if (
                action_id == "talk_gu_heng"
                and self._knows("FACT_FORCED_RESTART_SUSPICION")
                and self._knows("FACT_BURNT_RELAY")
            ):
                self._change_character(character_id, pressure=0.2, trust=0.2)
            else:
                self._change_character(character_id, pressure=0.3, trust=-0.3)
        elif dialogue_act == "reassure":
            self._change_character(character_id, pressure=-0.4, trust=0.3)
        elif dialogue_act == "promise" and action_id == "talk_gu_heng":
            self._change_character(character_id, pressure=-0.2, trust=0.2)

    def _recognize_promise(self, params: dict[str, Any]) -> None:
        condition = self._promise_condition(params)
        if condition not in PROMISE_CONDITIONS:
            return
        promise_id = f"player_to_gu_heng:{condition}"
        if any(promise["id"] == promise_id for promise in self.state["promises"]):
            return
        self.state["promises"].append(
            {
                "id": promise_id,
                "promiser": "player",
                "recipient": "gu_heng",
                "condition": condition,
                "recognized": True,
                "settled": False,
                "fulfilled": None,
            }
        )

    def _settle_promises(self) -> None:
        for promise in self.state["promises"]:
            if promise["settled"]:
                continue
            condition = promise["condition"]
            fulfilled = {
                "reserve_medicine": self.state["resources"]["medicine"] > 0,
                "keep_records": self.state["flags"]["records_preserved"],
                "heat_repair_room": self.state["flags"]["repair_room_heated"],
            }[condition]
            promise["settled"] = True
            promise["fulfilled"] = fulfilled
            self._change_character(
                "gu_heng",
                trust=0.6 if fulfilled else -1.2,
            )

    def _signal_available(self) -> bool:
        tasks = self.state["tasks"]
        return (
            not tasks["signal_sent"]
            and tasks["generator_progress"] >= self.rules["gameplay"]["generator_required"]
            and tasks["antenna_calibration"] >= self.rules["gameplay"]["antenna_required"]
        )

    def end_game(self) -> dict[str, Any]:
        self._settle_promises()
        ending = self.classify_ending()
        score = self.calculate_score()
        self.state["phase"] = "results"
        self.state["ending"] = ending
        self.state["score"] = score
        return {"ending": ending, "score": score, "timeline": self.causal_timeline()}

    def classify_ending(self) -> str:
        characters = self.state["characters"].values()
        thresholds = self.rules["balance"]["thresholds"]
        critical = any(
            c["health"] <= thresholds["critical_health"]
            or c["temperature"] <= thresholds["critical_temperature"]
            or c["fatigue"] <= thresholds["critical_fatigue"]
            or c["pressure"] >= thresholds["critical_pressure"]
            for c in characters
        )
        tasks = self.state["tasks"]
        if tasks["signal_sent"] and not critical:
            return "task_success"
        if tasks["signal_sent"] and critical:
            return "cost_uncontrolled"
        safe_to_wait = not critical and self.state["resources"]["fuel"] >= 2
        if safe_to_wait:
            return "survival_wait"
        if tasks["generator_progress"] > 0 or not critical:
            return "cost_uncontrolled"
        return "total_collapse"

    def calculate_score(self) -> dict[str, Any]:
        weights = self.rules["score"]["weights"]
        tasks = self.state["tasks"]
        gameplay = self.rules["gameplay"]

        task_raw = 0.0
        task_raw += 10.0 * min(1.0, tasks["generator_progress"] / gameplay["generator_required"])
        task_raw += 8.0 * min(1.0, tasks["antenna_calibration"] / gameplay["antenna_required"])
        task_raw += 7.0 if tasks["signal_sent"] else 0.0
        task_raw += min(5.0, self.state["ap"] * 2.5) if tasks["signal_sent"] else 0.0

        people_raw = 0.0
        for character in self.state["characters"].values():
            normalized = (
                0.40 * character["health"]
                + 0.25 * character["temperature"]
                + 0.20 * character["fatigue"]
                + 0.15 * character["hunger"]
            ) / 10.0
            people_raw += 10.0 * self._clamp(normalized, 0.0, 1.0)

        resources = self.state["resources"]
        fuel_raw = 6.0 * min(1.0, resources["fuel"] / 2.0)
        food_raw = 4.0 * min(1.0, resources["food"] / 2.0)
        medical_raw = 5.0 if resources["medicine"] + resources["heat_pack"] > 0 else 0.0
        heater_raw = 0.0
        if self.state["flags"]["kitchen_heater_intact"]:
            heater_raw = 5.0 if resources["fuel"] >= 2 else 1.25
        reserves_raw = fuel_raw + food_raw + medical_raw + heater_raw
        gu = self.state["characters"]["gu_heng"]
        thresholds = self.rules["balance"]["thresholds"]
        if (
            gu["health"] <= thresholds["critical_health"]
            or gu["temperature"] <= thresholds["critical_temperature"]
            or gu["fatigue"] <= thresholds["critical_fatigue"]
            or gu["pressure"] >= thresholds["critical_pressure"]
        ) and resources["medicine"] > 0:
            medical_raw = min(medical_raw, 1.25)
            reserves_raw = fuel_raw + food_raw + medical_raw + heater_raw

        trust_scores = [
            self._clamp(self.state["characters"][cid]["trust"] / 10.0, 0.0, 1.0)
            for cid in ("gu_heng", "ye_cheng")
        ]
        social_raw = weights["social_stability"] * sum(trust_scores) / len(trust_scores)
        for promise in self.state["promises"]:
            if promise.get("settled") and not promise.get("fulfilled"):
                social_raw = max(0.0, social_raw - 2.0)

        confirmed = sum(
            1 for level in self.state["player_knowledge"].values() if level == "confirmed"
        )
        info_raw = weights["information_responsibility"] * min(1.0, confirmed / 8.0)
        if self.state["flags"]["records_preserved"]:
            info_raw = min(weights["information_responsibility"], info_raw + 1.0)

        breakdown = {
            "task_quality": round(min(weights["task_quality"], task_raw), 2),
            "people": round(min(weights["people"], people_raw), 2),
            "effective_reserves": round(min(weights["effective_reserves"], reserves_raw), 2),
            "social_stability": round(min(weights["social_stability"], social_raw), 2),
            "information_responsibility": round(
                min(weights["information_responsibility"], info_raw), 2
            ),
        }
        total = round(sum(breakdown.values()), 2)
        rating = classify_rating(total, self.rules["score"]["ratings"])
        return {"total": total, "rating": rating, "breakdown": breakdown}

    def causal_timeline(self, limit: int = 5) -> list[dict[str, Any]]:
        important = [
            event
            for event in self.state["event_log"]
            if event["crisis_triggered"]
            or event["action_id"]
            in {
                "treat_gu_heng",
                "dismantle_kitchen_heater",
                "repair_generator",
                "calibrate_antenna",
                "send_signal",
            }
        ]
        return copy.deepcopy((important or self.state["event_log"])[-limit:])

    def build_agent_context(self, npc_id: str) -> dict[str, Any]:
        allowed = {
            fact["id"]
            for fact in self.rules["facts"]
            if npc_id in fact.get("initially_known_by", [])
        }
        allowed.update(self.state["public_facts"])
        character = copy.deepcopy(self.state["characters"][npc_id])
        return {
            "npc_id": npc_id,
            "state": character,
            "allowed_fact_ids": sorted(allowed),
            "memory": [
                promise
                for promise in self.state["promises"]
                if promise["recipient"] == npc_id
            ],
        }

    @staticmethod
    def validate_agent_response(
        response: dict[str, Any], allowed_fact_ids: Iterable[str]
    ) -> tuple[bool, str]:
        if not isinstance(response.get("utterance"), str) or not response["utterance"].strip():
            return False, "missing_utterance"
        if len(response["utterance"]) > 240:
            return False, "utterance_too_long"
        forbidden = {"resource_changes", "ap_delta", "allowed_action", "task_progress"}
        if forbidden.intersection(response):
            return False, "model_attempted_rule_change"
        referenced = set(response.get("referenced_fact_ids", []))
        if not referenced.issubset(set(allowed_fact_ids)):
            return False, "fact_permission_violation"
        return True, "ok"

    def _snapshot_for_diff(self) -> dict[str, Any]:
        return copy.deepcopy(
            {
                "ap": self.state["ap"],
                "phase": self.state["phase"],
                "resources": self.state["resources"],
                "flags": self.state["flags"],
                "tasks": self.state["tasks"],
                "characters": self.state["characters"],
                "player_knowledge": self.state["player_knowledge"],
                "evidence": self.state["evidence"],
                "mid_crisis_triggered": self.state["mid_crisis_triggered"],
            }
        )

    @staticmethod
    def _diff(before: dict[str, Any], after: dict[str, Any]) -> dict[str, Any]:
        changes: dict[str, Any] = {}

        def walk(prefix: str, left: Any, right: Any) -> None:
            if isinstance(left, dict) and isinstance(right, dict):
                for key in sorted(set(left) | set(right)):
                    walk(f"{prefix}.{key}" if prefix else key, left.get(key), right.get(key))
            elif left != right:
                changes[prefix] = {"before": left, "after": right}

        walk("", before, after)
        return changes


def run_route(simulator: WhiteoutSimulator, route_id: str) -> dict[str, Any]:
    route = simulator.rules["routes"][route_id]
    steps: list[dict[str, Any]] = []
    for index, step in enumerate(route["steps"], start=1):
        params = copy.deepcopy(step.get("params", {}))
        if not params:
            params = copy.deepcopy(
                ROUTE_DIALOGUE_DEFAULTS.get((route_id, step["action"]), {})
            )
        result = simulator.apply_action(
            step["action"],
            params,
            transaction_id=f"{route_id}:{index}:{step['action']}",
        )
        steps.append(
            {
                "action": step["action"],
                "params": params,
                "committed": result.committed,
                "reason": result.reason_code,
                "ap": result.ap_after,
            }
        )
        if not result.committed:
            raise RuleError(f"Route {route_id} failed at {step['action']}: {result.reason_code}")
    final = simulator.end_game()
    return {"route": route_id, "steps": steps, **final}
