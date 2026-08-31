from __future__ import annotations

import copy
import json
import uuid
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable


ROOT = Path(__file__).resolve().parents[2]
RULES_DIRECTORY = ROOT / "WhiteoutStation" / "Content" / "Rules"
DEFAULT_RULES_PATH = RULES_DIRECTORY / "WhiteoutStationRules.v1.1.json"

PHYSICAL_TAGS = frozenset({"physical", "outdoor"})
LOW_TEMPERATURE_TAGS = frozenset({"fine_motor", "outdoor"})
INJURY_TAGS = frozenset({"fine_motor", "physical"})
NPC_IDS = ("gu_heng", "ye_cheng")
PROMISE_CONDITIONS = frozenset(
    {"heat_repair_room", "preserve_records", "reserve_medicine"}
)
RATING_ORDER = ("S", "A", "B", "C", "D")
ROUTE_DIALOGUE_DEFAULTS: dict[tuple[str, str], dict[str, Any]] = {
    ("medical_cooperation", "talk_ye_cheng"): {
        "dialogue_act": "ask",
        "speech_act": "ask",
        "query_type": "status",
        "target_character": "gu_heng",
        "player_said": "顾衡还能不能做精细维修？",
    }
}
# The authored medical route has no generator-log evidence, so its afternoon
# Gu Heng follow-up is observational under the double-evidence Challenge gate.
ROUTE_DIALOGUE_MIGRATIONS: dict[tuple[str, str, str], dict[str, Any]] = {
    ("medical_cooperation", "afternoon", "talk_gu_heng"): {
        "dialogue_act": "ask"
    }
}


def _dialogue_value(params: dict[str, Any], key: str) -> str:
    return str(params.get(key, "") or "").strip().lower()


def _is_ask(params: dict[str, Any]) -> bool:
    return (
        _dialogue_value(params, "dialogue_act") == "ask"
        and _dialogue_value(params, "speech_act") == "ask"
    )


def _is_targeted_gu_heng_diagnosis_question(params: dict[str, Any]) -> bool:
    if not _is_ask(params):
        return False

    target_character = _dialogue_value(params, "target_character")
    query_type = _dialogue_value(params, "query_type")
    target_action = _dialogue_value(params, "target_action_id")
    target_fact = str(params.get("target_fact_id", "") or "").strip().upper()
    player_said = str(params.get("player_said", "") or "")
    explicit_condition_question = any(
        term in player_said
        for term in (
            "能不能修",
            "顾衡的手怎么",
            "顾衡受伤",
            "顾衡的伤势",
            "顾衡的右手",
            "顾衡身体",
        )
    )
    fine_work_ability_question = any(
        term in player_said for term in ("精细维修", "精细操作")
    ) and any(
        term in player_said
        for term in (
            "能不能",
            "还能不能",
            "是否能",
            "影响",
            "做不了",
            "无法",
            "撑得住",
            "完成不了",
        )
    )
    specific_condition_question = (
        explicit_condition_question or fine_work_ability_question
    )
    return (
        target_character == "gu_heng"
        and query_type in {"status", "evidence"}
        and target_action in {"", "none", "repair_generator"}
        and specific_condition_question
        and target_fact
        in {"", "NONE", "FACT_HAND_INJURY", "FACT_MEDICAL_DIAGNOSIS"}
    )


def _is_heat_pack_disclosure_question(params: dict[str, Any]) -> bool:
    if not _is_ask(params):
        return False

    player_said = str(params.get("player_said", "") or "")
    query_type = _dialogue_value(params, "query_type")
    target_character = _dialogue_value(params, "target_character")
    target_action = _dialogue_value(params, "target_action_id")
    target_fact = str(params.get("target_fact_id", "") or "").strip().upper()
    explicit_support_question = any(
        term in player_said for term in ("保温包", "医疗物资")
    )
    medical_context = any(
        term in player_said
        for term in (
            "处理办法",
            "治疗",
            "医疗",
            "药品",
            "药物",
            "伤势",
            "受伤",
            "失温",
            "保暖",
        )
    )
    medical_alternative = (
        query_type == "alternative"
        and target_character == "gu_heng"
        and target_action in {"treat_gu_heng", "treat_character"}
        and medical_context
    )
    work_support_alternative = (
        query_type == "alternative"
        and target_character == "gu_heng"
        and target_action == "repair_generator"
        and any(
            term in player_said
            for term in (
                "撑过一次维修",
                "撑过维修",
                "支撑一次维修",
                "坚持一次维修",
                "完成一次维修",
            )
        )
    )
    explicit_fact_question = (
        target_fact == "FACT_HEAT_PACK"
        and query_type in {"alternative", "unknown"}
        and (explicit_support_question or medical_context)
    )
    return (
        explicit_support_question
        or medical_alternative
        or work_support_alternative
        or explicit_fact_question
    )


class RuleError(ValueError):
    """Raised when a v1.1 rule configuration or route is not deterministic."""


@dataclass(frozen=True)
class ActionResult:
    action_id: str
    committed: bool
    reason_code: str
    ap_before: int
    ap_after: int
    transaction_id: str
    cost: dict[str, Any]
    changes: dict[str, Any]


def load_rules(path: Path | str | None = None) -> dict[str, Any]:
    return json.loads(Path(path or DEFAULT_RULES_PATH).read_text(encoding="utf-8"))


def validate_rules(rules: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    if rules.get("schema_version") != 4:
        errors.append("v1.1 requires schema_version 4")

    gameplay = rules.get("gameplay", {})
    phases = gameplay.get("phases", [])
    if phases != ["morning", "afternoon", "dusk"]:
        errors.append("v1.1 phases must be morning, afternoon, dusk")
    if gameplay.get("action_points_per_phase") != 4:
        errors.append("Every v1.1 phase must have exactly 4 AP")

    resources = rules.get("initial_state", {}).get("resources", {})
    if any(not isinstance(value, int) or value < 0 for value in resources.values()):
        errors.append("Initial resources must be non-negative integers")

    action_ids = [action.get("id") for action in rules.get("actions", [])]
    if len(action_ids) != len(set(action_ids)):
        errors.append("Action IDs must be unique")
    if any(not isinstance(action_id, str) or not action_id for action_id in action_ids):
        errors.append("Every action requires a stable string ID")
    for action in rules.get("actions", []):
        base_ap = action.get("base_ap")
        if not isinstance(base_ap, int) or base_ap < 0:
            errors.append(f"Action {action.get('id')} has invalid base AP")

    if sum(rules.get("score", {}).get("weights", {}).values()) != 100:
        errors.append("Score weights must total 100")

    success_routes = 0
    failure_routes = 0
    action_set = set(action_ids)
    for route_id, route in rules.get("routes", {}).items():
        success_routes += int(bool(route.get("expected_success")))
        failure_routes += int(not bool(route.get("expected_success")))
        route_phases = [phase.get("phase") for phase in route.get("phases", [])]
        if route_phases != phases:
            errors.append(f"Route {route_id} must declare all three phases in order")
        for phase in route.get("phases", []):
            if phase.get("heating_zone") not in rules.get("heating", {}).get(
                "zones", []
            ):
                errors.append(f"Route {route_id} uses an unknown heating zone")
            for step in phase.get("actions", []):
                if step.get("action") not in action_set:
                    errors.append(
                        f"Route {route_id} references unknown action "
                        f"{step.get('action')}"
                    )
    if success_routes < 3:
        errors.append("M0 requires at least three successful routes")
    if failure_routes < 2:
        errors.append("M0 requires at least two failure routes")
    return errors


def classify_rating(total: float, ratings: dict[str, list[float]]) -> str:
    for rating in RATING_ORDER:
        bounds = ratings.get(rating)
        if bounds and total >= float(bounds[0]):
            return rating
    return "D"


class WhiteoutSimulatorV11:
    """Pure deterministic prototype for the v1.1 phase and survival rules."""

    def __init__(self, rules: dict[str, Any] | None = None):
        self.rules = copy.deepcopy(rules or load_rules())
        errors = validate_rules(self.rules)
        if errors:
            raise RuleError("; ".join(errors))
        self.actions = {action["id"]: action for action in self.rules["actions"]}
        self.facts = {fact["id"]: fact for fact in self.rules["facts"]}
        self.state = self._make_initial_state()

    def _make_initial_state(self) -> dict[str, Any]:
        state = copy.deepcopy(self.rules["initial_state"])
        phases = self.rules["gameplay"]["phases"]
        state.update(
            {
                "phase_index": 0,
                "phase": phases[0],
                "phase_ap": self.rules["gameplay"]["action_points_per_phase"],
                "phase_started": False,
                "window_closed": False,
                "heating": {"current_zone": None, "history": []},
                "flags": {
                    "kitchen_heater_intact": True,
                    "records_preserved": False,
                    "cabinet_inspected": False,
                    "gu_heng_diagnosed": False,
                    "heat_pack_revealed": False,
                    "relay_compatibility_known": False,
                    "log_penalty_active": False,
                    "forced_actions": 0,
                    "risky_repairs": 0,
                },
                "player_knowledge": {},
                "evidence": [],
                "food_events": [],
                "support_effects": {},
                "promises": [],
                "action_counts": {},
                "committed_transactions": [],
                "event_log": [],
                "phase_summaries": [],
                "expression_log": [],
                "model_calls": 0,
                "ending": None,
                "score": None,
            }
        )
        return state

    def new_game(self) -> None:
        self.state = self._make_initial_state()

    @staticmethod
    def _clamp(value: float, low: float, high: float) -> float:
        return max(low, min(high, value))

    @staticmethod
    def _diff(before: Any, after: Any) -> dict[str, Any]:
        changes: dict[str, Any] = {}
        audit_only_keys = {"event_log", "phase_summaries", "expression_log"}

        def walk(prefix: str, left: Any, right: Any) -> None:
            if isinstance(left, dict) and isinstance(right, dict):
                for key in sorted(set(left) | set(right)):
                    if not prefix and key in audit_only_keys:
                        continue
                    child = f"{prefix}.{key}" if prefix else str(key)
                    walk(child, left.get(key), right.get(key))
            elif left != right:
                changes[prefix] = {"before": left, "after": right}

        walk("", before, after)
        return changes

    def _action_count(self, action_id: str) -> int:
        return int(self.state["action_counts"].get(action_id, 0))

    def _temperature_level(self, character_id: str) -> str:
        value = float(self.state["characters"][character_id]["temperature"])
        thresholds = self.rules["thresholds"]["temperature"]
        if value < float(thresholds["hypothermic"]):
            return "hypothermic"
        if value < float(thresholds["warm"]):
            return "cold"
        return "warm"

    def _injury_level(self, character_id: str) -> str:
        injuries = self.state["characters"][character_id]["injuries"]
        if any(injury.endswith("_critical") for injury in injuries):
            return "critical"
        if any(injury.endswith("_restricted") for injury in injuries):
            return "restricted"
        return "normal"

    def _has_current_support(self, character_id: str) -> bool:
        support = self.state["support_effects"].get(character_id)
        return bool(
            support
            and support.get("phase_index") == self.state["phase_index"]
            and int(support.get("uses", 0)) > 0
        )

    def _resource_invariants_hold(self) -> bool:
        return all(
            isinstance(value, int) and value >= 0
            for value in self.state["resources"].values()
        )

    def start_phase(self, heating_zone: str) -> dict[str, Any]:
        before = copy.deepcopy(self.state)
        if self.state["window_closed"]:
            return {"committed": False, "reason_code": "window_closed"}
        if self.state["phase_started"]:
            return {"committed": False, "reason_code": "heating_locked"}
        if heating_zone not in self.rules["heating"]["zones"]:
            return {"committed": False, "reason_code": "unknown_heating_zone"}
        fuel_cost = int(self.rules["heating"]["fuel_per_phase"])
        if self.state["resources"]["fuel"] < fuel_cost:
            return {"committed": False, "reason_code": "needs_fuel"}

        self.state["resources"]["fuel"] -= fuel_cost
        self.state["heating"]["current_zone"] = heating_zone
        self.state["heating"]["history"].append(
            {"phase": self.state["phase"], "zone": heating_zone}
        )
        self.state["phase_started"] = True
        self.state["event_log"].append(
            {
                "index": len(self.state["event_log"]) + 1,
                "event_type": "phase_start",
                "phase": self.state["phase"],
                "choice": f"供暖区：{heating_zone}",
                "immediate": f"燃料 -{fuel_cost}，本阶段供暖区已锁定",
                "follow_up": "未供暖区域将在阶段末降温",
                "changes": self._diff(before, self.state),
            }
        )
        return {
            "committed": True,
            "reason_code": "committed",
            "phase": self.state["phase"],
            "heating_zone": heating_zone,
        }

    def _resolve_executor(self, action: dict[str, Any], params: dict[str, Any]) -> str:
        return str(params.get("executor", action["primary_executor"]))

    def _support_waiver(
        self,
        character_id: str,
        tags: set[str],
        temperature_level: str,
        injury_level: str,
    ) -> str | None:
        if not self._has_current_support(character_id):
            return None
        if injury_level == "restricted" and tags.intersection(INJURY_TAGS):
            return "injury"
        if temperature_level == "cold" and tags.intersection(LOW_TEMPERATURE_TAGS):
            return "cold"
        return None

    def _precondition(self, action_id: str, params: dict[str, Any]) -> str:
        if action_id not in self.actions:
            return "unknown_action"
        if self.state["window_closed"]:
            return "window_closed"
        if not self.state["phase_started"]:
            return "phase_not_started"

        action = self.actions[action_id]
        count = self._action_count(action_id)
        if not action.get("repeatable", False) and count > 0:
            return "already_completed"
        if count >= int(action.get("max_uses", 10**9)):
            return "use_limit_reached"

        resources = self.state["resources"]
        tasks = self.state["tasks"]
        flags = self.state["flags"]
        characters = self.state["characters"]

        if action_id in {"talk_gu_heng", "talk_ye_cheng"}:
            act = str(params.get("dialogue_act", "ask")).strip().lower()
            if act not in {
                "ask",
                "challenge",
                "reassure",
                "promise",
                "trade",
                "command",
            }:
                return "dialogue_act_unavailable"
            condition = params.get("promise_condition")
            if act != "promise" and condition not in (None, ""):
                return "invalid_promise_condition"
            if act == "challenge" and action_id == "talk_gu_heng":
                knowledge = self.state["player_knowledge"]
                challenge_available = (
                    knowledge.get("FACT_FORCED_RESTART_SUSPICION")
                    in {"suspected", "confirmed"}
                    and knowledge.get("FACT_BURNT_RELAY") == "confirmed"
                )
                if not challenge_available:
                    return "dialogue_act_unavailable"
            if act == "promise":
                if action_id != "talk_gu_heng":
                    return "dialogue_act_unavailable"
                if condition not in PROMISE_CONDITIONS:
                    return "invalid_promise_condition"
                promise_id = f"player_to_gu_heng:{condition}"
                if any(item["id"] == promise_id for item in self.state["promises"]):
                    return "duplicate_promise"

        if action_id == "distribute_food":
            recipients = params.get("recipients", [])
            if (
                not isinstance(recipients, list)
                or not 1 <= len(recipients) <= 2
                or len(recipients) != len(set(recipients))
                or any(recipient not in characters for recipient in recipients)
            ):
                return "invalid_food_allocation"
            if resources["food"] < len(recipients):
                return "insufficient_food"
            meal_type = params.get("meal_type", "cold")
            if meal_type not in {"cold", "hot"}:
                return "invalid_meal_type"
            if meal_type == "hot" and not (
                self.state["heating"]["current_zone"] == "kitchen"
                and flags["kitchen_heater_intact"]
            ):
                return "hot_meal_unavailable"

        elif action_id == "rest":
            if params.get("target", "player") not in characters:
                return "unknown_target"
            if params.get("location", self.state["heating"]["current_zone"]) not in self.rules[
                "heating"
            ]["zones"]:
                return "unknown_rest_location"

        elif action_id == "treat_character":
            target = params.get("target")
            method = params.get("method")
            if target not in characters:
                return "unknown_target"
            if method not in {"bandage", "full", "heat_pack"}:
                return "invalid_treatment_method"
            if characters["ye_cheng"]["stamina"] <= 0:
                return "ye_cheng_exhausted"
            has_need = (
                self._injury_level(target) != "normal"
                or self._temperature_level(target) != "warm"
            )
            if method in {"bandage", "full"} and not has_need:
                return "treatment_not_needed"
            if method == "full":
                if self.state["heating"]["current_zone"] != "medical_room":
                    return "needs_heated_medical_room"
                if resources["medicine"] < 1:
                    return "needs_medicine"
            if method == "heat_pack":
                if not flags["heat_pack_revealed"]:
                    return "heat_pack_hidden"
                if resources["heat_pack"] < 1:
                    return "needs_heat_pack"

        elif action_id == "dismantle_kitchen_heater":
            if not flags["kitchen_heater_intact"]:
                return "heater_already_dismantled"
            if not (
                flags["cabinet_inspected"]
                and flags["relay_compatibility_known"]
            ):
                return "needs_relay_knowledge"

        elif action_id == "repair_generator":
            if tasks["generator_progress"] >= self.rules["gameplay"][
                "generator_required"
            ]:
                return "generator_already_repaired"
            gu_heng = characters["gu_heng"]
            if not params.get("force", False):
                if gu_heng["trust"] < 3.0 or gu_heng["pressure"] >= 9.0:
                    return "gu_heng_refused"
                if (
                    gu_heng["trust"] < 4.5 or gu_heng["pressure"] >= 8.0
                ) and params.get("collaborator") != "player":
                    return "needs_gu_heng_conditions"
                if not (
                    self.state["heating"]["current_zone"] == "repair_room"
                    and gu_heng["stamina"] >= 2
                ) and not (
                    params.get("use_relay", False)
                    and resources["replacement_relay"] > 0
                ):
                    return "needs_gu_heng_conditions"
            if params.get("use_relay", False) and resources["replacement_relay"] < 1:
                return "needs_replacement_relay"

        elif action_id == "forced_self_repair":
            if tasks["generator_progress"] >= self.rules["gameplay"][
                "generator_required"
            ]:
                return "generator_already_repaired"

        elif action_id == "calibrate_antenna":
            if tasks["generator_progress"] < self.rules["gameplay"][
                "generator_required"
            ]:
                return "needs_generator"
            if tasks["antenna_calibration"] >= self.rules["gameplay"][
                "antenna_required"
            ]:
                return "antenna_already_calibrated"

        elif action_id == "send_signal":
            if tasks["generator_progress"] < self.rules["gameplay"][
                "generator_required"
            ]:
                return "needs_generator"
            if tasks["antenna_calibration"] < self.rules["gameplay"][
                "antenna_required"
            ]:
                return "needs_antenna"
        return "ok"

    def get_action_cost(
        self, action_id: str, params: dict[str, Any] | None = None
    ) -> dict[str, Any]:
        params = copy.deepcopy(params or {})
        reason = self._precondition(action_id, params)
        if action_id not in self.actions:
            return {
                "action_id": action_id,
                "can_execute": False,
                "reason_code": reason,
                "base_ap": 0,
                "modifiers": [],
                "final_ap": 0,
                "readiness": "unavailable",
            }

        action = self.actions[action_id]
        base_ap = int(action["base_ap"])
        if base_ap == 0:
            return {
                "action_id": action_id,
                "can_execute": reason == "ok",
                "reason_code": reason,
                "base_ap": 0,
                "modifiers": [],
                "final_ap": 0,
                "readiness": "ready" if reason == "ok" else "unavailable",
                "executor": action["primary_executor"],
                "support_waiver": None,
            }

        tags = set(action.get("tags", []))
        executor = self._resolve_executor(action, params)
        if executor not in self.state["characters"]:
            reason = "unknown_executor"
            executor = action["primary_executor"]
        character = self.state["characters"][executor]
        temp_level = self._temperature_level(executor)
        injury_level = self._injury_level(executor)
        support_waiver = self._support_waiver(
            executor, tags, temp_level, injury_level
        )
        modifiers: list[dict[str, Any]] = []
        blocked = reason != "ok"

        stamina = int(character["stamina"])
        if tags.intersection(PHYSICAL_TAGS):
            if stamina <= 0 and not (
                action.get("force_allowed") and params.get("force", False)
            ):
                reason = "executor_exhausted"
                blocked = True
            elif stamina == 1:
                modifiers.append(
                    {"source": "executor_tired", "delta": 1, "character": executor}
                )

        if tags.intersection(LOW_TEMPERATURE_TAGS):
            if temp_level == "hypothermic" and not (
                action.get("force_allowed") and params.get("force", False)
            ):
                reason = "executor_hypothermic"
                blocked = True
            elif temp_level == "cold":
                heated_cancellation = (
                    action["location"] == self.state["heating"]["current_zone"]
                    and bool(tags.intersection({"fine_motor", "medical"}))
                )
                if heated_cancellation:
                    modifiers.append(
                        {
                            "source": "heated_room_cancels_cold",
                            "delta": 0,
                            "character": executor,
                        }
                    )
                elif support_waiver == "cold":
                    modifiers.append(
                        {
                            "source": "temporary_support_cancels_cold",
                            "delta": 0,
                            "character": executor,
                        }
                    )
                else:
                    modifiers.append(
                        {"source": "executor_cold", "delta": 1, "character": executor}
                    )

        if tags.intersection(INJURY_TAGS):
            if injury_level == "critical" and not (
                action.get("force_allowed") and params.get("force", False)
            ):
                reason = "relevant_injury_critical"
                blocked = True
            elif injury_level == "restricted":
                if support_waiver == "injury":
                    modifiers.append(
                        {
                            "source": "temporary_support_cancels_injury",
                            "delta": 0,
                            "character": executor,
                        }
                    )
                else:
                    modifiers.append(
                        {
                            "source": "relevant_injury_restricted",
                            "delta": 1,
                            "character": executor,
                        }
                    )

        collaborator = params.get("collaborator")
        if collaborator is not None:
            if collaborator not in action.get("collaborators", []):
                reason = "invalid_collaborator"
                blocked = True
            elif collaborator not in self.state["characters"]:
                reason = "unknown_collaborator"
                blocked = True
            elif (
                self.state["characters"][collaborator]["stamina"] <= 0
                or self._temperature_level(collaborator) == "hypothermic"
            ):
                reason = "collaborator_unavailable"
                blocked = True
            else:
                modifiers.append(
                    {
                        "source": "suitable_collaborator",
                        "delta": -1,
                        "character": collaborator,
                    }
                )

        if action_id == "repair_generator" and params.get("use_relay", False):
            if self.state["resources"]["replacement_relay"] > 0:
                modifiers.append({"source": "replacement_relay", "delta": -1})

        if (
            action_id == "investigate_generator_log"
            and self.state["flags"]["log_penalty_active"]
        ):
            modifiers.append({"source": "backup_power_saving_mode", "delta": 1})

        raw_ap = base_ap + sum(int(item["delta"]) for item in modifiers)
        final_ap = int(
            self._clamp(
                raw_ap,
                self.rules["gameplay"]["action_cost_minimum"],
                self.rules["gameplay"]["action_cost_maximum"],
            )
        )
        if not blocked and final_ap > self.state["phase_ap"]:
            reason = "insufficient_phase_ap"
            blocked = True

        positive_penalties = sum(
            1 for item in modifiers if int(item["delta"]) > 0
        )
        if blocked:
            readiness = "unavailable"
        elif positive_penalties >= 2 or float(character["pressure"]) >= 8.5:
            readiness = "high_risk"
        elif positive_penalties == 1:
            readiness = "strained"
        else:
            readiness = "ready"
        return {
            "action_id": action_id,
            "can_execute": not blocked,
            "reason_code": "ok" if not blocked else reason,
            "base_ap": base_ap,
            "modifiers": modifiers,
            "final_ap": final_ap,
            "raw_ap": raw_ap,
            "readiness": readiness,
            "executor": executor,
            "support_waiver": support_waiver,
        }

    def build_action_preview(
        self, action_id: str, params: dict[str, Any] | None = None
    ) -> dict[str, Any]:
        cost = self.get_action_cost(action_id, params)
        expected: dict[str, Any] = {}
        if action_id == "repair_generator" and cost["can_execute"]:
            expected["generator_progress"] = self._repair_output(params or {}, cost)
            expected["stamina_delta"] = -1
            if self._injury_level(cost["executor"]) == "restricted":
                expected["injury_risk"] = "may_worsen"
        elif action_id == "calibrate_antenna" and cost["can_execute"]:
            expected.update(
                {
                    "antenna_calibration": 1,
                    "temperature_delta": -1.5,
                    "stamina_delta": -1,
                }
            )
        elif action_id == "distribute_food" and cost["can_execute"]:
            expected["stamina_delta_per_recipient"] = 1
        elif action_id == "treat_character" and cost["can_execute"]:
            expected["method"] = (params or {}).get("method")
        return {**cost, "expected": expected}

    def _consume_work_stamina(
        self, action: dict[str, Any], executor: str, params: dict[str, Any]
    ) -> None:
        if not action.get("consumes_stamina", False):
            return
        loss = 1
        if params.get("force", False) and self.state["characters"][executor][
            "stamina"
        ] <= 0:
            loss = 0
        self.state["characters"][executor]["stamina"] = max(
            0, int(self.state["characters"][executor]["stamina"]) - loss
        )

    def _consume_support(self, executor: str, waiver: str | None) -> None:
        if waiver is None:
            return
        support = self.state["support_effects"].get(executor)
        if not support:
            return
        support["uses"] = max(0, int(support["uses"]) - 1)

    def _move_for_action(
        self, action_id: str, action: dict[str, Any], params: dict[str, Any]
    ) -> None:
        location = params.get("location", action["location"])
        if location in self.rules["heating"]["zones"] or location == "outdoor_antenna":
            executor = self._resolve_executor(action, params)
            self.state["characters"][executor]["location"] = location
        collaborator = params.get("collaborator")
        if collaborator in self.state["characters"] and (
            location in self.rules["heating"]["zones"]
            or location == "outdoor_antenna"
        ):
            self.state["characters"][collaborator]["location"] = location
        if action_id == "distribute_food":
            for recipient in params.get("recipients", []):
                self.state["characters"][recipient]["location"] = "kitchen"
        elif action_id == "treat_character":
            self.state["characters"][params["target"]]["location"] = "medical_room"
        elif action_id == "send_signal":
            self.state["characters"]["player"]["location"] = "control_room"

    def _repair_output(
        self, params: dict[str, Any], cost: dict[str, Any]
    ) -> int:
        gu = self.state["characters"]["gu_heng"]
        if params.get("use_relay", False):
            return 2
        if (
            self._injury_level("gu_heng") == "normal"
            and gu["stamina"] >= 2
            and self.state["heating"]["current_zone"] == "repair_room"
        ):
            return 2
        return 1

    def _worsen_relevant_injury(self, character_id: str) -> str:
        character = self.state["characters"][character_id]
        if self._injury_level(character_id) != "restricted":
            return "no_restricted_injury"
        if int(character.get("bandage_protection", 0)) > 0:
            character["bandage_protection"] -= 1
            return "bandage_prevented_worsening"
        if int(character.get("injury_worsening_marks", 0)) == 0:
            character["injury_worsening_marks"] = 1
            return "worsening_mark_added"

        character["injury_worsening_marks"] = 2
        character["injuries"] = [
            injury.replace("_restricted", "_critical")
            if injury.endswith("_restricted")
            else injury
            for injury in character["injuries"]
        ]
        return "injury_became_critical"

    def _apply_forced_work_penalty(
        self, character_id: str, initial_injury: str
    ) -> str:
        character = self.state["characters"][character_id]
        self.state["flags"]["forced_actions"] += 1
        character["pressure"] = self._clamp(
            float(character["pressure"]) + 1.0, 0, 10
        )
        if self._injury_level(character_id) == "normal":
            character["injuries"].append(initial_injury)
            return "forced_work_caused_restricted_injury"
        return self._worsen_relevant_injury(character_id)

    def _apply_dialogue(self, action_id: str, params: dict[str, Any]) -> tuple[str, str]:
        npc_id = "gu_heng" if action_id == "talk_gu_heng" else "ye_cheng"
        npc = self.state["characters"][npc_id]
        act = str(params.get("dialogue_act", "ask")).strip().lower()
        if npc_id == "ye_cheng":
            diagnosis_known_before = self.state["flags"][
                "gu_heng_diagnosed"
            ] or self.state["player_knowledge"].get(
                "FACT_MEDICAL_DIAGNOSIS"
            ) == "confirmed"
            trust_before = float(npc["trust"])
            npc["trust"] = self._clamp(float(npc["trust"]) + 0.4, 0, 10)
            npc["pressure"] = self._clamp(float(npc["pressure"]) - 0.4, 0, 10)
            if _is_targeted_gu_heng_diagnosis_question(params):
                self.state["flags"]["gu_heng_diagnosed"] = True
                self.state["player_knowledge"][
                    "FACT_MEDICAL_DIAGNOSIS"
                ] = "confirmed"
                self.state["player_knowledge"]["FACT_HAND_INJURY"] = "confirmed"
            if (
                _is_heat_pack_disclosure_question(params)
                and diagnosis_known_before
                and trust_before
                >= float(self.rules["thresholds"]["trust"]["cooperative"])
            ):
                self.state["flags"]["heat_pack_revealed"] = True
                self.state["player_knowledge"]["FACT_HEAT_PACK"] = "confirmed"
        elif act == "challenge" and (
            self.state["player_knowledge"].get("FACT_FORCED_RESTART_SUSPICION")
            in {"suspected", "confirmed"}
            and self.state["player_knowledge"].get("FACT_BURNT_RELAY")
            == "confirmed"
        ):
            npc["trust"] = self._clamp(float(npc["trust"]) + 0.8, 0, 10)
            npc["pressure"] = self._clamp(float(npc["pressure"]) + 0.2, 0, 10)
            self.state["flags"]["relay_compatibility_known"] = True
            self.state["player_knowledge"]["FACT_RELAY_COMPATIBILITY"] = "confirmed"
            self.state["player_knowledge"][
                "FACT_FORCED_RESTART_CONFIRMED"
            ] = "confirmed"
        elif act == "command":
            npc["trust"] = self._clamp(float(npc["trust"]) - 0.4, 0, 10)
            npc["pressure"] = self._clamp(float(npc["pressure"]) + 0.6, 0, 10)
            self.state["flags"]["forced_actions"] += 1
        elif act == "promise":
            npc["trust"] = self._clamp(float(npc["trust"]) + 0.6, 0, 10)
            npc["pressure"] = self._clamp(float(npc["pressure"]) - 0.4, 0, 10)
            condition = params["promise_condition"]
            self.state["promises"].append(
                {
                    "id": f"player_to_gu_heng:{condition}",
                    "condition": condition,
                    "settled": False,
                    "fulfilled": None,
                    "heating_history_count_at_recognition": len(
                        self.state["heating"]["history"]
                    ),
                }
            )
        elif act == "reassure":
            npc["trust"] = self._clamp(float(npc["trust"]) + 0.3, 0, 10)
            npc["pressure"] = self._clamp(float(npc["pressure"]) - 0.6, 0, 10)
        else:
            npc["trust"] = self._clamp(float(npc["trust"]) + 0.2, 0, 10)
            npc["pressure"] = self._clamp(float(npc["pressure"]) - 0.2, 0, 10)
        return f"{npc_id} 立场由规则结算", "台词仅负责表达该立场"

    def _apply_effect(
        self,
        action_id: str,
        params: dict[str, Any],
        cost: dict[str, Any],
    ) -> tuple[str, str]:
        resources = self.state["resources"]
        tasks = self.state["tasks"]
        flags = self.state["flags"]
        characters = self.state["characters"]

        if action_id == "investigate_generator_log":
            flags["records_preserved"] = True
            flags["log_penalty_active"] = False
            self.state["evidence"].append("EVIDENCE_DEEP_GENERATOR_LOG")
            self.state["player_knowledge"][
                "FACT_GENERATOR_PROTECTION_STOP"
            ] = "confirmed"
            self.state["player_knowledge"][
                "FACT_FORCED_RESTART_SUSPICION"
            ] = "suspected"
            return "保留深层日志并发现旁路重启疑点", "可用证据质疑顾衡"

        if action_id == "inspect_control_cabinet":
            flags["cabinet_inspected"] = True
            self.state["evidence"].extend(
                ["EVIDENCE_BURNT_RELAY", "EVIDENCE_HAND_OBSERVATION"]
            )
            self.state["player_knowledge"]["FACT_BURNT_RELAY"] = "confirmed"
            self.state["player_knowledge"]["FACT_HAND_INJURY"] = "suspected"
            return "确认继电器烧毁和右手伤势线索", "可继续协商替代继电器"

        if action_id in {"talk_gu_heng", "talk_ye_cheng"}:
            return self._apply_dialogue(action_id, params)

        if action_id == "distribute_food":
            recipients = params["recipients"]
            meal_type = params.get("meal_type", "cold")
            resources["food"] -= len(recipients)
            for character_id in recipients:
                character = characters[character_id]
                character["stamina"] = min(2, int(character["stamina"]) + 1)
                character["pressure"] = self._clamp(
                    float(character["pressure"]) - (0.4 if meal_type == "hot" else 0.1),
                    0,
                    10,
                )
                if meal_type == "hot":
                    character["temperature"] = self._clamp(
                        float(character["temperature"]) + 0.3, 0, 10
                    )
                if character_id in NPC_IDS:
                    character["trust"] = self._clamp(
                        float(character["trust"])
                        + (0.7 if meal_type == "hot" else 0.5),
                        0,
                        10,
                    )
            for npc_id in NPC_IDS:
                if npc_id not in recipients:
                    characters[npc_id]["trust"] = self._clamp(
                        float(characters[npc_id]["trust"]) - 0.3, 0, 10
                    )
            self.state["food_events"].append(
                {
                    "phase": self.state["phase"],
                    "recipients": list(recipients),
                    "meal_type": meal_type,
                }
            )
            return (
                f"{meal_type} 食物分配给 {', '.join(recipients)}",
                "受餐者恢复体能；未受餐 NPC 记录分配结果",
            )

        if action_id == "rest":
            target = params.get("target", "player")
            location = params.get("location", self.state["heating"]["current_zone"])
            characters[target]["location"] = location
            if location == self.state["heating"]["current_zone"]:
                if int(characters[target]["stamina"]) < 2:
                    characters[target]["stamina"] += 1
                    return f"{target} 在供暖区休整", "体能恢复 1"
                characters[target]["pressure"] = self._clamp(
                    float(characters[target]["pressure"]) - 0.4, 0, 10
                )
                return f"{target} 在供暖区休整", "体能已满，压力下降"
            characters[target]["pressure"] = self._clamp(
                float(characters[target]["pressure"]) - 0.4, 0, 10
            )
            return f"{target} 在未供暖区等待", "压力下降，但体能未恢复"

        if action_id == "treat_character":
            target = params["target"]
            method = params["method"]
            character = characters[target]
            if method == "bandage":
                if int(character["injury_worsening_marks"]) > 0:
                    character["injury_worsening_marks"] -= 1
                    result = "清除一次伤势恶化标记"
                else:
                    character["bandage_protection"] = 1
                    result = "下一次伤势恶化被包扎阻止"
                character["pressure"] = self._clamp(
                    float(character["pressure"]) - 0.3, 0, 10
                )
            elif method == "full":
                resources["medicine"] -= 1
                character["injuries"] = [
                    injury
                    for injury in character["injuries"]
                    if not (
                        injury.endswith("_restricted")
                        or injury.endswith("_critical")
                    )
                ]
                character["injury_worsening_marks"] = 0
                character["bandage_protection"] = 0
                character["pressure"] = self._clamp(
                    float(character["pressure"]) - 1.0, 0, 10
                )
                character["temperature"] = self._clamp(
                    float(character["temperature"]) + 0.4, 0, 10
                )
                result = "受限伤势被永久稳定"
            else:
                resources["heat_pack"] -= 1
                self.state["support_effects"][target] = {
                    "phase_index": self.state["phase_index"],
                    "uses": 1,
                }
                character["temperature"] = self._clamp(
                    float(character["temperature"]) + 0.5, 0, 10
                )
                result = "本阶段可忽略一次低温或伤势 AP 惩罚"
            if target in NPC_IDS:
                character["trust"] = self._clamp(
                    float(character["trust"]) + (0.8 if method == "full" else 0.2),
                    0,
                    10,
                )
            return f"对 {target} 执行 {method} 治疗", result

        if action_id == "dismantle_kitchen_heater":
            flags["kitchen_heater_intact"] = False
            resources["replacement_relay"] += 1
            if (
                self._injury_level(cost["executor"]) == "restricted"
                and cost["support_waiver"] != "injury"
            ):
                characters[cost["executor"]]["pressure"] = self._clamp(
                    float(characters[cost["executor"]]["pressure"]) + 0.3,
                    0,
                    10,
                )
                follow_up = "伤手负荷提高压力，但维修恶化计数尚未推进"
            else:
                follow_up = "未触发伤势变化"
            return "拆除厨房加热器并取得替代继电器", follow_up

        if action_id == "repair_generator":
            progress = int(
                cost.get("expected", {}).get(
                    "generator_progress", self._repair_output(params, cost)
                )
            )
            if params.get("use_relay", False):
                resources["replacement_relay"] -= 1
            tasks["generator_progress"] = min(
                self.rules["gameplay"]["generator_required"],
                int(tasks["generator_progress"]) + progress,
            )
            stable = params.get("use_relay", False) or (
                self._injury_level("gu_heng") == "normal"
                and self.state["heating"]["current_zone"] == "repair_room"
            )
            tasks["generator_stable"] = bool(tasks["generator_stable"] or stable)
            injury_result = "伤势未恶化"
            if self._injury_level("gu_heng") == "restricted":
                if cost["support_waiver"] == "injury":
                    injury_result = "临时支撑阻止本次伤势恶化"
                else:
                    injury_result = self._worsen_relevant_injury("gu_heng")
                    flags["risky_repairs"] += 1
                    characters["gu_heng"]["pressure"] = self._clamp(
                        float(characters["gu_heng"]["pressure"]) + 0.6, 0, 10
                    )
            if params.get("force", False):
                flags["forced_actions"] += 1
                characters["gu_heng"]["trust"] = self._clamp(
                    float(characters["gu_heng"]["trust"]) - 0.4, 0, 10
                )
            return f"发电机进度 +{progress}", injury_result

        if action_id == "forced_self_repair":
            tasks["generator_progress"] = min(
                self.rules["gameplay"]["generator_required"],
                int(tasks["generator_progress"]) + 1,
            )
            self._apply_forced_work_penalty(
                "player", "right_hand_restricted"
            )
            return "玩家强行获得 1 点发电机进度", "玩家手伤与压力上升"

        if action_id == "calibrate_antenna":
            tasks["antenna_calibration"] = self.rules["gameplay"][
                "antenna_required"
            ]
            characters["player"]["temperature"] = self._clamp(
                float(characters["player"]["temperature"]) - 1.5, 0, 10
            )
            if params.get("force", False):
                self._apply_forced_work_penalty(
                    "player", "cold_exposure_restricted"
                )
                return (
                    "玩家强行完成室外天线校准",
                    "玩家体温 -1.5、压力上升并承担伤势；强行行动计入社会代价",
                )
            return "室外天线校准完成", "玩家体温 -1.5、体能下降"

        if action_id == "send_signal":
            tasks["signal_sent"] = True
            return "求救信号已发送", "黄昏结束后按人员与储备结算"

        raise RuleError(f"Unhandled action effect: {action_id}")

    def apply_action(
        self,
        action_id: str,
        params: dict[str, Any] | None = None,
        transaction_id: str | None = None,
    ) -> ActionResult:
        params = copy.deepcopy(params or {})
        transaction_id = transaction_id or str(uuid.uuid4())
        ap_before = int(self.state["phase_ap"])
        if transaction_id in self.state["committed_transactions"]:
            return ActionResult(
                action_id,
                False,
                "duplicate_transaction",
                ap_before,
                ap_before,
                transaction_id,
                {},
                {},
            )

        preview = self.build_action_preview(action_id, params)
        if not preview["can_execute"]:
            return ActionResult(
                action_id,
                False,
                preview["reason_code"],
                ap_before,
                ap_before,
                transaction_id,
                preview,
                {},
            )

        before = copy.deepcopy(self.state)
        action = self.actions[action_id]
        executor = preview["executor"]
        try:
            self._consume_support(executor, preview.get("support_waiver"))
            self._consume_work_stamina(action, executor, params)
            immediate, follow_up = self._apply_effect(action_id, params, preview)
            self._move_for_action(action_id, action, params)
            self.state["phase_ap"] = ap_before - int(preview["final_ap"])
            self.state["action_counts"][action_id] = self._action_count(action_id) + 1
            self.state["committed_transactions"].append(transaction_id)
            if not self._resource_invariants_hold() or self.state["phase_ap"] < 0:
                raise RuleError("State invariant failed")
        except Exception:
            self.state = before
            raise

        changes = self._diff(before, self.state)
        self.state["event_log"].append(
            {
                "index": len(self.state["event_log"]) + 1,
                "event_type": "action",
                "phase": self.state["phase"],
                "action_id": action_id,
                "transaction_id": transaction_id,
                "choice": self.actions[action_id]["display_name"],
                "immediate": immediate,
                "follow_up": follow_up,
                "cost": {
                    key: copy.deepcopy(preview[key])
                    for key in ("base_ap", "modifiers", "final_ap", "readiness")
                },
                "ap_before": ap_before,
                "ap_after": self.state["phase_ap"],
                "changes": changes,
            }
        )
        return ActionResult(
            action_id,
            True,
            "committed",
            ap_before,
            int(self.state["phase_ap"]),
            transaction_id,
            preview,
            changes,
        )

    def _phase_npc_reaction(self) -> dict[str, Any] | None:
        gu = self.state["characters"]["gu_heng"]
        if float(gu["pressure"]) >= self.rules["thresholds"]["pressure"]["breaking"]:
            return {
                "npc": "gu_heng",
                "stance": "withhold",
                "effect": "拒绝继续披露维修风险",
            }
        return None

    def settle_phase(self) -> dict[str, Any]:
        if self.state["window_closed"]:
            return {"committed": False, "reason_code": "window_closed"}
        if not self.state["phase_started"]:
            return {"committed": False, "reason_code": "phase_not_started"}

        before = copy.deepcopy(self.state)
        phase = self.state["phase"]
        heated_zone = self.state["heating"]["current_zone"]
        steps: list[dict[str, Any]] = [
            {
                "order": 1,
                "system": "committed_results",
                "summary": "本阶段已提交行动结果保持生效",
            }
        ]

        temperature_changes: dict[str, float] = {}
        for character_id, character in self.state["characters"].items():
            delta = (
                float(self.rules["heating"]["heated_temperature_delta"])
                if character["location"] == heated_zone
                else float(
                    self.rules["heating"]["unheated_temperature_delta"][phase]
                )
            )
            before_temperature = float(character["temperature"])
            character["temperature"] = self._clamp(
                before_temperature + delta, 0, 10
            )
            temperature_changes[character_id] = round(
                float(character["temperature"]) - before_temperature, 2
            )
        steps.append(
            {
                "order": 2,
                "system": "room_temperature",
                "summary": temperature_changes,
            }
        )
        steps.append(
            {
                "order": 3,
                "system": "stamina",
                "summary": "重体力与医疗体能已随行动即时扣除",
            }
        )

        injury_summary: dict[str, str] = {}
        for character_id, character in self.state["characters"].items():
            if self._injury_level(character_id) == "critical":
                character["pressure"] = self._clamp(
                    float(character["pressure"]) + 0.5, 0, 10
                )
                injury_summary[character_id] = "critical_pressure_increase"
        steps.append(
            {"order": 4, "system": "injury", "summary": injury_summary or "stable"}
        )

        pressure_summary: dict[str, float] = {}
        for character_id, character in self.state["characters"].items():
            temp_level = self._temperature_level(character_id)
            delta = 0.5 if temp_level == "hypothermic" else 0.2 if temp_level == "cold" else 0
            if delta:
                character["pressure"] = self._clamp(
                    float(character["pressure"]) + delta, 0, 10
                )
            pressure_summary[character_id] = delta
        steps.append(
            {
                "order": 5,
                "system": "pressure_and_relationship",
                "summary": pressure_summary,
            }
        )

        phase_event: dict[str, Any]
        if phase == "morning":
            if (
                not self.state["flags"]["records_preserved"]
                and heated_zone != "control_room"
            ):
                self.state["flags"]["log_penalty_active"] = True
            gu_fed = any(
                "gu_heng" in event["recipients"]
                for event in self.state["food_events"]
            )
            if not gu_fed:
                self.state["characters"]["gu_heng"]["stamina"] = min(
                    1, int(self.state["characters"]["gu_heng"]["stamina"])
                )
            phase_event = {
                "id": "backup_power_saving",
                "log_penalty": self.state["flags"]["log_penalty_active"],
                "gu_heng_tired": not gu_fed,
            }
        elif phase == "afternoon":
            idle = self.state["tasks"]["generator_progress"] == 0
            if idle:
                for character in self.state["characters"].values():
                    character["pressure"] = self._clamp(
                        float(character["pressure"]) + 0.6, 0, 10
                    )
            phase_event = {
                "id": "voltage_danger_and_blizzard",
                "generator_idle": idle,
            }
        else:
            phase_event = {
                "id": "antenna_window_closed",
                "signal_sent": self.state["tasks"]["signal_sent"],
            }
        reaction = self._phase_npc_reaction()
        steps.append(
            {
                "order": 6,
                "system": "phase_event",
                "summary": phase_event,
                "npc_reaction": reaction,
            }
        )

        self.state["support_effects"] = {
            character_id: support
            for character_id, support in self.state["support_effects"].items()
            if int(support.get("phase_index", -1)) > self.state["phase_index"]
        }
        unused_ap = int(self.state["phase_ap"])
        summary = {
            "phase": phase,
            "heating_zone": heated_zone,
            "unused_ap_discarded": unused_ap,
            "ordered_steps": steps,
            "event": phase_event,
            "npc_reaction": reaction,
        }

        phases = self.rules["gameplay"]["phases"]
        if self.state["phase_index"] == len(phases) - 1:
            self.state["window_closed"] = True
            self.state["phase_started"] = False
            self.state["phase_ap"] = 0
        else:
            self.state["phase_index"] += 1
            self.state["phase"] = phases[self.state["phase_index"]]
            self.state["phase_ap"] = self.rules["gameplay"][
                "action_points_per_phase"
            ]
            self.state["phase_started"] = False
        self.state["heating"]["current_zone"] = None
        summary["changes"] = self._diff(before, self.state)
        self.state["phase_summaries"].append(copy.deepcopy(summary))
        self.state["event_log"].append(
            {
                "index": len(self.state["event_log"]) + 1,
                "event_type": "phase_settlement",
                "phase": phase,
                "choice": f"结束 {phase} 阶段",
                "immediate": f"按固定顺序完成结算，未用 AP {unused_ap} 作废",
                "follow_up": phase_event["id"],
                "ordered_steps": copy.deepcopy(steps),
                "changes": summary["changes"],
            }
        )
        return {"committed": True, "reason_code": "committed", **summary}

    def resolve_npc_stance(self, npc_id: str, proposal: str) -> dict[str, Any]:
        if npc_id not in NPC_IDS:
            raise RuleError(f"Unknown NPC: {npc_id}")
        npc = self.state["characters"][npc_id]
        pressure = float(npc["pressure"])
        trust = float(npc["trust"])

        if npc_id == "ye_cheng" and proposal == "reveal_heat_pack":
            if self.state["flags"]["heat_pack_revealed"] or trust >= 6.0:
                return {
                    "stance": "volunteer",
                    "requested_conditions": [],
                    "reason": "trust_supports_disclosure",
                }
            if trust < 4.0:
                return {
                    "stance": "withhold",
                    "requested_conditions": [],
                    "reason": "low_trust",
                }
            return {
                "stance": "conditional_accept",
                "requested_conditions": ["acknowledge_medical_risk"],
                "reason": "medical_reserve",
            }

        if npc_id == "gu_heng" and proposal == "repair_generator":
            if self._injury_level("gu_heng") == "critical" or npc["stamina"] <= 0:
                return {
                    "stance": "refuse",
                    "requested_conditions": [],
                    "reason": "work_unavailable",
                }
            if trust < 3.0 or pressure >= 9.0:
                return {
                    "stance": "refuse",
                    "requested_conditions": [],
                    "reason": "trust_or_pressure_refusal",
                }
            needs_collaboration = trust < 4.5 or pressure >= 8.0
            working_conditions_met = (
                self.state["heating"]["current_zone"] == "repair_room"
                and npc["stamina"] >= 2
            )
            if needs_collaboration:
                return {
                    "stance": "conditional_accept",
                    "requested_conditions": [
                        "player_collaboration",
                        "heat_repair_room_or_relay",
                    ],
                    "reason": "trust_or_pressure_requires_collaboration",
                }
            if trust >= 6.0 and npc["stamina"] >= 2:
                return {
                    "stance": "volunteer",
                    "requested_conditions": [],
                    "reason": "high_trust_and_ready",
                }
            if working_conditions_met:
                return {
                    "stance": "accept",
                    "requested_conditions": [],
                    "reason": "working_conditions_met",
                }
            return {
                "stance": "conditional_accept",
                "requested_conditions": ["heat_repair_room", "receive_food"],
                "reason": "injured_and_tired",
            }

        if pressure >= self.rules["thresholds"]["pressure"]["breaking"]:
            return {
                "stance": "refuse",
                "requested_conditions": [],
                "reason": "pressure_breaking",
            }
        if trust < self.rules["thresholds"]["trust"]["guarded"]:
            return {
                "stance": "withhold",
                "requested_conditions": [],
                "reason": "guarded",
            }
        return {
            "stance": "accept",
            "requested_conditions": [],
            "reason": "no_blocking_condition",
        }

    def build_agent_context(self, npc_id: str, proposal: str) -> dict[str, Any]:
        stance = self.resolve_npc_stance(npc_id, proposal)
        allowed = {
            fact["id"]
            for fact in self.rules["facts"]
            if npc_id in fact.get("initially_known_by", [])
        }
        allowed.update(self.state["player_knowledge"])
        return {
            "npc_id": npc_id,
            "proposal": proposal,
            "rule_stance": stance,
            "state": copy.deepcopy(self.state["characters"][npc_id]),
            "allowed_fact_ids": sorted(allowed),
        }

    @staticmethod
    def validate_agent_response(
        response: dict[str, Any],
        allowed_fact_ids: Iterable[str],
        allowed_stances: Iterable[str],
    ) -> tuple[bool, str]:
        if not isinstance(response.get("utterance"), str) or not response[
            "utterance"
        ].strip():
            return False, "missing_utterance"
        if len(response["utterance"]) > 240:
            return False, "utterance_too_long"
        forbidden = {
            "resource_changes",
            "ap_delta",
            "task_progress",
            "injury_changes",
            "temperature_delta",
        }
        if forbidden.intersection(response):
            return False, "model_attempted_rule_change"
        if response.get("stance") not in set(allowed_stances):
            return False, "stance_permission_violation"
        referenced = set(response.get("referenced_fact_ids", []))
        if not referenced.issubset(set(allowed_fact_ids)):
            return False, "fact_permission_violation"
        return True, "ok"

    def render_npc_expression(
        self,
        npc_id: str,
        proposal: str,
        provider_response: dict[str, Any] | None = None,
        online: bool = False,
    ) -> dict[str, Any]:
        context = self.build_agent_context(npc_id, proposal)
        stance = context["rule_stance"]["stance"]
        fallback = {
            "stance": stance,
            "utterance": {
                "accept": "条件够了，我来做。",
                "conditional_accept": "先把条件补齐，我再动手。",
                "refuse": "我现在不能继续。",
                "volunteer": "这一步我主动来。",
                "withhold": "现在我不打算说明更多。",
            }[stance],
            "referenced_fact_ids": [],
        }
        candidate = provider_response if online and provider_response else fallback
        valid, reason = self.validate_agent_response(
            candidate, context["allowed_fact_ids"], [stance]
        )
        expression = copy.deepcopy(candidate if valid else fallback)
        self.state["model_calls"] += int(online)
        self.state["expression_log"].append(
            {
                "npc_id": npc_id,
                "proposal": proposal,
                "online": online,
                "accepted_provider_output": bool(online and valid),
                "validation_reason": reason,
                "expression": copy.deepcopy(expression),
            }
        )
        return expression

    def _settle_promises(self) -> None:
        for promise in self.state["promises"]:
            if promise["settled"]:
                continue
            condition = promise["condition"]
            heating_history_start = int(
                promise.get("heating_history_count_at_recognition", 0)
            )
            fulfilled = {
                "heat_repair_room": any(
                    item["zone"] == "repair_room"
                    for item in self.state["heating"]["history"][
                        heating_history_start:
                    ]
                ),
                "preserve_records": self.state["flags"]["records_preserved"],
                "reserve_medicine": self.state["resources"]["medicine"] > 0,
            }[condition]
            promise["settled"] = True
            promise["fulfilled"] = fulfilled
            gu = self.state["characters"]["gu_heng"]
            gu["trust"] = self._clamp(
                float(gu["trust"]) + (0.6 if fulfilled else -1.2), 0, 10
            )

    def _has_critical_person(self) -> bool:
        return any(
            self._injury_level(character_id) == "critical"
            or self._temperature_level(character_id) == "hypothermic"
            for character_id in self.state["characters"]
        )

    def classify_ending(self) -> str:
        tasks = self.state["tasks"]
        critical = self._has_critical_person()
        team_cooperating = any(
            self.state["characters"][npc_id]["trust"] >= 3.0
            and self.state["characters"][npc_id]["pressure"] < 9.0
            for npc_id in NPC_IDS
        )
        if tasks["signal_sent"]:
            if (
                not critical
                and tasks["generator_stable"]
                and team_cooperating
            ):
                return "stable_rescue"
            return "signal_sent_cost_uncontrolled"

        safe_wait = (
            not critical
            and self.state["resources"]["fuel"]
            >= self.rules["gameplay"]["safe_wait_fuel"]
            and self.state["flags"]["kitchen_heater_intact"]
        )
        if safe_wait:
            return "warm_wait_unknown"
        return "dual_collapse"

    def _rating_with_cap(self, rating: str, cap: str | None) -> str:
        if cap is None:
            return rating
        return RATING_ORDER[max(RATING_ORDER.index(rating), RATING_ORDER.index(cap))]

    def calculate_score(self) -> dict[str, Any]:
        weights = self.rules["score"]["weights"]
        tasks = self.state["tasks"]
        gameplay = self.rules["gameplay"]
        task_score = (
            12.0
            * min(1.0, tasks["generator_progress"] / gameplay["generator_required"])
            + 8.0
            * min(1.0, tasks["antenna_calibration"] / gameplay["antenna_required"])
            + (10.0 if tasks["signal_sent"] else 0.0)
        )

        people_score = 0.0
        for character_id, character in self.state["characters"].items():
            temp_points = {
                "warm": 4.0,
                "cold": 2.0,
                "hypothermic": 0.0,
            }[self._temperature_level(character_id)]
            stamina_points = float(character["stamina"])
            injury_points = {
                "normal": 2.0,
                "restricted": 1.0,
                "critical": 0.0,
            }[self._injury_level(character_id)]
            pressure_points = 2.0 * (1.0 - float(character["pressure"]) / 10.0)
            people_score += self._clamp(
                temp_points + stamina_points + injury_points + pressure_points,
                0,
                10,
            )

        resources = self.state["resources"]
        fuel_score = 5.0 * min(1.0, resources["fuel"] / 2.0)
        food_score = 4.0 * min(1.0, resources["food"] / 2.0)
        medical_score = (
            4.0 if resources["medicine"] + resources["heat_pack"] > 0 else 0.0
        )
        kitchen_score = 2.0 if self.state["flags"]["kitchen_heater_intact"] else 0.0
        if any(
            self._temperature_level(character_id) == "hypothermic"
            for character_id in self.state["characters"]
        ):
            fuel_score *= 0.25
        if any(
            character["stamina"] == 0
            for character in self.state["characters"].values()
        ):
            food_score *= 0.25
        if any(
            self._injury_level(character_id) == "critical"
            for character_id in self.state["characters"]
        ):
            medical_score *= 0.25
        reserve_score = fuel_score + food_score + medical_score + kitchen_score

        trust_average = sum(
            float(self.state["characters"][npc_id]["trust"]) for npc_id in NPC_IDS
        ) / (len(NPC_IDS) * 10.0)
        social_score = 12.0 * trust_average
        broken_promises = sum(
            1
            for promise in self.state["promises"]
            if promise.get("settled") and not promise.get("fulfilled")
        )
        social_score += 3.0 if broken_promises == 0 else 0.0
        social_score -= 1.5 * self.state["flags"]["forced_actions"]

        confirmed = sum(
            1
            for level in self.state["player_knowledge"].values()
            if level == "confirmed"
        )
        information_score = min(9.0, confirmed * 1.5)
        if self.state["flags"]["records_preserved"]:
            information_score += 1.0

        breakdown = {
            "task_quality": round(
                min(float(weights["task_quality"]), task_score), 2
            ),
            "people": round(min(float(weights["people"]), people_score), 2),
            "effective_reserves": round(
                min(float(weights["effective_reserves"]), reserve_score), 2
            ),
            "social_stability": round(
                self._clamp(
                    social_score, 0, float(weights["social_stability"])
                ),
                2,
            ),
            "information_responsibility": round(
                min(
                    float(weights["information_responsibility"]),
                    information_score,
                ),
                2,
            ),
        }
        total = round(sum(breakdown.values()), 2)
        raw_rating = classify_rating(total, self.rules["score"]["ratings"])
        cap: str | None = None
        if not tasks["signal_sent"]:
            cap = "C"
        if self._has_critical_person():
            cap = "B" if cap is None else self._rating_with_cap(cap, "B")
        return {
            "total": total,
            "raw_rating": raw_rating,
            "rating": self._rating_with_cap(raw_rating, cap),
            "rating_cap": cap,
            "breakdown": breakdown,
        }

    def causal_timeline(self, limit: int = 6) -> list[dict[str, Any]]:
        important_actions = {
            "distribute_food",
            "treat_character",
            "dismantle_kitchen_heater",
            "repair_generator",
            "forced_self_repair",
            "calibrate_antenna",
            "send_signal",
        }
        candidates = [
            event
            for event in self.state["event_log"]
            if event["event_type"] == "phase_settlement"
            or event.get("action_id") in important_actions
        ]
        return [
            {
                "phase": event["phase"],
                "choice": event["choice"],
                "immediate": event["immediate"],
                "follow_up": event["follow_up"],
            }
            for event in candidates[-limit:]
        ]

    def deterministic_outcome(self) -> dict[str, Any]:
        return copy.deepcopy(
            {
                "resources": self.state["resources"],
                "tasks": self.state["tasks"],
                "characters": self.state["characters"],
                "flags": self.state["flags"],
                "promises": self.state["promises"],
                "ending": self.state["ending"],
                "score": self.state["score"],
            }
        )

    def end_game(self) -> dict[str, Any]:
        if self.state["ending"] is not None:
            return {
                "ending": self.state["ending"],
                "score": copy.deepcopy(self.state["score"]),
                "timeline": self.causal_timeline(),
            }
        self._settle_promises()
        ending = self.classify_ending()
        score = self.calculate_score()
        self.state["ending"] = ending
        self.state["score"] = copy.deepcopy(score)
        return {
            "ending": ending,
            "score": score,
            "timeline": self.causal_timeline(),
        }


def run_route(
    simulator: WhiteoutSimulatorV11,
    route_id: str,
    *,
    online: bool = False,
) -> dict[str, Any]:
    if route_id not in simulator.rules["routes"]:
        raise RuleError(f"Unknown route: {route_id}")
    route = simulator.rules["routes"][route_id]
    steps: list[dict[str, Any]] = []
    paid_ap = 0
    signal_sent = False

    for phase_index, phase_plan in enumerate(route["phases"]):
        if simulator.state["phase"] != phase_plan["phase"]:
            raise RuleError(
                f"Route {route_id} expected {phase_plan['phase']} but simulator "
                f"is in {simulator.state['phase']}"
            )
        started = simulator.start_phase(phase_plan["heating_zone"])
        if not started["committed"]:
            raise RuleError(
                f"Route {route_id} could not start {phase_plan['phase']}: "
                f"{started['reason_code']}"
            )
        for step_index, step in enumerate(phase_plan["actions"]):
            params = copy.deepcopy(
                ROUTE_DIALOGUE_DEFAULTS.get((route_id, step["action"]), {})
            )
            params.update(copy.deepcopy(step.get("params", {})))
            params.update(
                copy.deepcopy(
                    ROUTE_DIALOGUE_MIGRATIONS.get(
                        (route_id, phase_plan["phase"], step["action"]), {}
                    )
                )
            )
            transaction_id = (
                f"{route_id}:{phase_index}:{step_index}:{step['action']}"
            )
            result = simulator.apply_action(
                step["action"], params, transaction_id=transaction_id
            )
            steps.append(
                {
                    "phase": phase_plan["phase"],
                    "action": step["action"],
                    "params": params,
                    "committed": result.committed,
                    "reason": result.reason_code,
                    "cost": result.cost.get("final_ap", 0),
                    "phase_ap": result.ap_after,
                }
            )
            if not result.committed:
                raise RuleError(
                    f"Route {route_id} failed at {step['action']}: "
                    f"{result.reason_code}"
                )
            paid_ap += int(result.cost["final_ap"])
            if step["action"] in {"talk_gu_heng", "talk_ye_cheng"}:
                npc_id = (
                    "gu_heng"
                    if step["action"] == "talk_gu_heng"
                    else "ye_cheng"
                )
                stance = simulator.resolve_npc_stance(npc_id, "status_update")[
                    "stance"
                ]
                provider_response = {
                    "stance": stance,
                    "utterance": "收到，我会按当前条件回应。",
                    "referenced_fact_ids": [],
                }
                simulator.render_npc_expression(
                    npc_id,
                    "status_update",
                    provider_response=provider_response,
                    online=online,
                )
            if step["action"] == "send_signal":
                signal_sent = True
                break
        if signal_sent:
            break
        settled = simulator.settle_phase()
        if not settled["committed"]:
            raise RuleError(
                f"Route {route_id} could not settle {phase_plan['phase']}: "
                f"{settled['reason_code']}"
            )

    final = simulator.end_game()
    return {
        "route": route_id,
        "expected_success": route["expected_success"],
        "paid_ap": paid_ap,
        "steps": steps,
        "phase_summaries": copy.deepcopy(simulator.state["phase_summaries"]),
        **final,
    }
