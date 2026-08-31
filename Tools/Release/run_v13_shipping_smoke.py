"""Run v1.3 Shipping routes, epistemic mocks, audit, and AI A/B gates.

The mature v1.1 process runner remains the low-level harness. This module
reconfigures its versioned artifact contract and adds v1.3-only checks for the
semantic-atoms/full-line protocol. Credential values are never read or copied.
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import sys
import tempfile
from dataclasses import dataclass, replace
from pathlib import Path
from typing import Any, Iterable, Mapping
from uuid import UUID

try:
    from . import run_v11_shipping_smoke as smoke
except ImportError:
    import run_v11_shipping_smoke as smoke


MOCK_MODES = (
    "valid_natural",
    "missing_atom",
    "forbidden_fact",
    "added_condition",
    "system_jargon",
    "invalid_json",
    "timeout",
)
DIALOGUE_AUDIT_REL = Path("Saved/Logs/WhiteoutStation_DialogueAudit.jsonl")
DIALOGUE_AUDIT_FIELDS = frozenset(
    {
        "kind",
        "transaction_id",
        "speaker",
        "query_type",
        "target_action_id",
        "planned_disclosure_fact_ids",
        "final_disclosed_fact_ids",
        "required_atom_ids",
        "realized_atom_ids",
        "answer_source",
        "validation_outcome",
        "prompt_tokens",
        "completion_tokens",
    }
)
GAMEPLAY_STATE_FIELDS = (
    "remaining_ap",
    "phase_action_points",
    "phase",
    "day_phase",
    "day_phase_started",
    "day_window_closed",
    "mid_crisis_triggered",
    "heating",
    "ending",
    "score",
    "score_breakdown",
    "player_knowledge",
    "disclosed_fact_ids",
    "resources",
    "related_flags",
    "tasks",
    "characters",
    "requirement_cards",
)
RESOURCE_FIELDS = frozenset(
    {"fuel", "food", "medicine", "heat_pack", "replacement_relay"}
)
HEATING_FIELDS = frozenset({"current_zone", "locked", "history"})
SCORE_BREAKDOWN_FIELDS = frozenset(
    {
        "task_quality",
        "people",
        "effective_reserves",
        "social_stability",
        "information_responsibility",
        "total",
        "rating",
    }
)
TASK_FIELDS = frozenset(
    {"generator_progress", "antenna_calibration", "signal_sent", "generator_stable"}
)
RELATED_FLAG_FIELDS = frozenset(
    {
        "kitchen_heater_intact",
        "heat_pack_revealed",
        "repair_room_heated",
        "medical_room_heated",
        "gu_heng_diagnosed",
        "gu_heng_treated",
        "gu_heng_fed",
        "gu_heng_cooperative",
        "relay_compatibility_known",
        "relay_installed",
        "self_repair_used",
        "records_preserved",
        "player_fed",
        "ye_cheng_fed",
        "cabinet_inspected",
        "log_penalty_active",
        "forced_action_count",
        "risky_repair_count",
    }
)
CHARACTER_FIELDS = frozenset(
    {
        "health",
        "temperature",
        "hunger",
        "fatigue",
        "pressure",
        "trust",
        "stamina",
        "injury_severity",
        "injury_id",
        "injury_worsening_marks",
        "bandage_protection",
        "temporary_support_uses",
        "temporary_support_phase",
        "location",
    }
)
CHARACTER_IDS = frozenset({"player", "guheng", "yecheng"})
QUERY_TYPES = frozenset(
    {
        "Unknown",
        "Requirements",
        "Status",
        "Cause",
        "Alternative",
        "Evidence",
        "Consequence",
        "unknown",
        "requirements",
        "status",
        "cause",
        "alternative",
        "evidence",
        "consequence",
    }
)
ACTION_ID_PATTERN = re.compile(r"^[a-z][a-z0-9]*(?:_[a-z0-9]+)*$")
STABLE_LOWER_ID_PATTERN = re.compile(r"^[a-z][a-z0-9]*(?:_[a-z0-9]+)*$")
FACT_ID_PATTERN = re.compile(r"^FACT_[A-Z0-9]+(?:_[A-Z0-9]+)*$")
ATOM_ID_PATTERN = re.compile(r"^[A-Z][A-Z0-9]*(?:_[A-Z0-9]+)+$")
VALIDATION_OUTCOME_PATTERN = re.compile(
    r"^(?:accepted|fallback_[a-z0-9]+(?:_[a-z0-9]+)*)$"
)
CJK_PATTERN = re.compile(r"[\u3400-\u4dbf\u4e00-\u9fff]")
SENSITIVE_VALUE_PATTERNS = (
    re.compile(r"(?i)\bbearer\s+[a-z0-9._~+/=-]+"),
    re.compile(r"(?i)\bsk-[a-z0-9_-]{4,}"),
    re.compile(r"(?i)\bapi[\s_-]*key\b"),
)
SENSITIVE_TRACE_PATTERNS = (
    re.compile(r"(?i)(?:^|[\s{\"'])prompt\s*(?:[:=]|\b(?:body|content|text)\b)"),
    re.compile(r"(?i)(?:^|[\s{\"'])request\s*(?:[:=]|\b(?:body|content|payload)\b)"),
    re.compile(r"(?i)(?:^|[\s{\"'])response\s*(?:[:=]|\b(?:body|content|payload)\b)"),
    re.compile(r"(?i)(?:^|[\s{\"'])player[_\s-]*(?:said|input|text)\s*[:=]"),
    re.compile(r"(?i)(?:^|[\s{\"'])npc[_\s-]*(?:line|utterance)\s*[:=]"),
)
ONLINE_LLM_MODES = frozenset(
    {"loopback_mock", "timeout_endpoint", "unreachable_endpoint", "provider_rejected"}
)


@dataclass(frozen=True)
class V13Scenario:
    scenario_id: str
    route: str
    llm_mode: str
    expected_model_calls: int
    mock_mode: str = ""


V13_SCENARIOS = (
    V13Scenario("missing_key_medical", "medical", "default_missing_key", 0),
    V13Scenario("missing_key_technical", "technical", "default_missing_key", 0),
    V13Scenario("missing_key_quick", "quick", "default_missing_key", 0),
    V13Scenario("missing_key_wait", "wait", "default_missing_key", 0),
    V13Scenario("missing_key_collapse", "collapse", "default_missing_key", 0),
    V13Scenario("explicit_offline_medical", "medical", "explicit_offline", 0),
    V13Scenario(
        "loopback_online_technical",
        "technical",
        "loopback_mock",
        2,
        "valid_natural",
    ),
    V13Scenario(
        "mock_valid_natural_quick", "quick", "loopback_mock", 1, "valid_natural"
    ),
    V13Scenario(
        "mock_missing_atom_quick", "quick", "loopback_mock", 1, "missing_atom"
    ),
    V13Scenario(
        "mock_forbidden_fact_quick",
        "quick",
        "loopback_mock",
        1,
        "forbidden_fact",
    ),
    V13Scenario(
        "mock_added_condition_quick",
        "quick",
        "loopback_mock",
        1,
        "added_condition",
    ),
    V13Scenario(
        "mock_system_jargon_quick",
        "quick",
        "loopback_mock",
        1,
        "system_jargon",
    ),
    V13Scenario(
        "mock_invalid_json_quick", "quick", "loopback_mock", 1, "invalid_json"
    ),
    V13Scenario("mock_timeout_quick", "quick", "timeout_endpoint", 1, "timeout"),
    V13Scenario(
        "unreachable_endpoint_quick", "quick", "unreachable_endpoint", 1
    ),
    V13Scenario("invalid_credential_quick", "quick", "provider_rejected", 1),
)

_BASE_VALIDATE_EVENT_LOG = smoke.validate_event_log
_BASE_RUN_SCENARIO = smoke.run_scenario
_BASE_VALIDATE_AUDIT = smoke.validate_audit
_BASE_MOCK_ENDPOINT = smoke.MockLoopbackEndpoint
_ACTIVE_MOCK_MODE = "valid_natural"
_CONFIGURED = False


def _normalize_key(key: str) -> str:
    return "".join(character for character in key.casefold() if character.isalnum())


def _iter_string_values(
    value: object,
    path: str = "$",
) -> Iterable[tuple[str, str]]:
    if isinstance(value, str):
        yield path, value
    elif isinstance(value, Mapping):
        for key, child in value.items():
            yield from _iter_string_values(child, f"{path}.{key}")
    elif isinstance(value, (list, tuple)):
        for index, child in enumerate(value):
            yield from _iter_string_values(child, f"{path}[{index}]")


def _validate_audit_string_values(record: Mapping[str, object], index: int) -> None:
    for path, value in _iter_string_values(record):
        if CJK_PATTERN.search(value):
            raise smoke.SmokeError(
                f"dialogue audit record {index} contains possible raw dialogue "
                f"at {path}"
            )
        if any(pattern.search(value) for pattern in SENSITIVE_VALUE_PATTERNS):
            raise smoke.SmokeError(
                f"dialogue audit record {index} contains a credential trace at {path}"
            )
        if any(pattern.search(value) for pattern in SENSITIVE_TRACE_PATTERNS):
            raise smoke.SmokeError(
                f"dialogue audit record {index} contains a sensitive trace at {path}"
            )


def _validate_id_list(
    value: object,
    label: str,
    pattern: re.Pattern[str],
) -> tuple[str, ...]:
    if not isinstance(value, list) or any(
        not isinstance(item, str)
        or not pattern.fullmatch(item)
        for item in value
    ):
        raise smoke.SmokeError(
            f"dialogue audit {label} must be a stable identifier array"
        )
    if len(value) != len(set(value)):
        raise smoke.SmokeError(f"dialogue audit {label} contains duplicates")
    return tuple(value)


def validate_dialogue_audit_records(
    records: Iterable[Mapping[str, object]],
    *,
    expected_records: int | None = None,
) -> dict[str, Any]:
    """Validate the exact metadata-only whitelist from the v1.3 brief."""

    normalized_records = list(records)
    if expected_records is not None and len(normalized_records) != expected_records:
        raise smoke.SmokeError(
            "dialogue audit count mismatch "
            f"({len(normalized_records)} != {expected_records})"
        )
    answer_sources: list[str] = []
    outcomes: list[str] = []
    for index, record in enumerate(normalized_records):
        keys = set(record)
        if keys != DIALOGUE_AUDIT_FIELDS:
            missing = sorted(DIALOGUE_AUDIT_FIELDS - keys)
            extra = sorted(keys - DIALOGUE_AUDIT_FIELDS)
            raise smoke.SmokeError(
                f"dialogue audit record {index} violates whitelist; "
                f"missing={missing} extra={extra}"
            )
        _validate_audit_string_values(record, index)
        if record.get("kind") != "dialogue_expression":
            raise smoke.SmokeError(f"dialogue audit record {index} kind mismatch")
        transaction_id = record.get("transaction_id")
        try:
            parsed_transaction_id = UUID(str(transaction_id))
        except (ValueError, AttributeError):
            parsed_transaction_id = None
        if (
            not isinstance(transaction_id, str)
            or parsed_transaction_id is None
            or parsed_transaction_id.int == 0
            or str(parsed_transaction_id) != transaction_id
        ):
            raise smoke.SmokeError(
                f"dialogue audit record {index} transaction_id is invalid"
            )
        if record.get("speaker") not in {"gu_heng", "ye_cheng"}:
            raise smoke.SmokeError(f"dialogue audit record {index} speaker is invalid")
        if record.get("query_type") not in QUERY_TYPES:
            raise smoke.SmokeError(
                f"dialogue audit record {index} query_type is invalid"
            )
        target_action = record.get("target_action_id")
        if (
            not isinstance(target_action, str)
            or not ACTION_ID_PATTERN.fullmatch(target_action)
        ):
            raise smoke.SmokeError(
                f"dialogue audit record {index} target_action_id is invalid"
            )
        planned = _validate_id_list(
            record["planned_disclosure_fact_ids"],
            "planned_disclosure_fact_ids",
            FACT_ID_PATTERN,
        )
        disclosed = _validate_id_list(
            record["final_disclosed_fact_ids"],
            "final_disclosed_fact_ids",
            FACT_ID_PATTERN,
        )
        required = _validate_id_list(
            record["required_atom_ids"],
            "required_atom_ids",
            ATOM_ID_PATTERN,
        )
        realized = _validate_id_list(
            record["realized_atom_ids"],
            "realized_atom_ids",
            ATOM_ID_PATTERN,
        )
        if set(planned) != set(disclosed):
            raise smoke.SmokeError(
                f"dialogue audit record {index} changed planned disclosure facts"
            )
        if not set(required) <= set(realized):
            raise smoke.SmokeError(
                f"dialogue audit record {index} did not realize every required atom"
            )
        answer_source = record.get("answer_source")
        validation_outcome = record.get("validation_outcome")
        if answer_source not in {"online_full_line", "local_natural_fallback"}:
            raise smoke.SmokeError(
                f"dialogue audit record {index} answer_source is invalid"
            )
        if (
            not isinstance(validation_outcome, str)
            or not VALIDATION_OUTCOME_PATTERN.fullmatch(validation_outcome)
        ):
            raise smoke.SmokeError(
                f"dialogue audit record {index} validation_outcome is invalid"
            )
        for token_field in ("prompt_tokens", "completion_tokens"):
            token_count = record[token_field]
            if (
                isinstance(token_count, bool)
                or not isinstance(token_count, int)
                or token_count < -1
            ):
                raise smoke.SmokeError(
                    f"dialogue audit record {index} {token_field} is invalid"
                )
        answer_sources.append(answer_source)
        outcomes.append(validation_outcome)
    return {
        "records": len(normalized_records),
        "fields": sorted(DIALOGUE_AUDIT_FIELDS),
        "answer_sources": answer_sources,
        "validation_outcomes": outcomes,
    }


def load_and_validate_dialogue_audit(
    path: Path,
    *,
    expected_records: int,
) -> dict[str, Any]:
    if expected_records == 0 and not path.exists():
        return validate_dialogue_audit_records((), expected_records=0)
    records = smoke.load_json_lines(path, "v1.3 dialogue audit")
    return validate_dialogue_audit_records(
        records,
        expected_records=expected_records,
    )


def _canonical(value: object) -> object:
    if isinstance(value, dict):
        return {
            str(key): _canonical(child)
            for key, child in sorted(value.items(), key=lambda pair: str(pair[0]))
        }
    if isinstance(value, list):
        return [_canonical(child) for child in value]
    if isinstance(value, float):
        return round(value, 6)
    return value


def _sorted_canonical_list(value: list[object]) -> list[object]:
    children = [_canonical(child) for child in value]
    return sorted(
        children,
        key=lambda child: json.dumps(
            child,
            ensure_ascii=False,
            sort_keys=True,
            separators=(",", ":"),
        ),
    )


def _validate_exact_object(
    value: object,
    expected_fields: frozenset[str],
    label: str,
) -> Mapping[str, object]:
    if not isinstance(value, Mapping):
        raise smoke.SmokeError(f"{label} must be an object")
    actual_fields = {str(key) for key in value}
    if actual_fields != expected_fields:
        missing = sorted(expected_fields - actual_fields)
        extra = sorted(actual_fields - expected_fields)
        raise smoke.SmokeError(
            f"{label} fields are incomplete; missing={missing} extra={extra}"
        )
    return value


def _normalize_gameplay_state(
    state: Mapping[str, object],
    label: str,
) -> dict[str, object]:
    missing = [field for field in GAMEPLAY_STATE_FIELDS if field not in state]
    if missing:
        raise smoke.SmokeError(
            f"{label} is missing v1.3 AI A/B state fields: " + ", ".join(missing)
        )
    for integer_field in ("remaining_ap", "phase_action_points"):
        value = state[integer_field]
        if isinstance(value, bool) or not isinstance(value, int):
            raise smoke.SmokeError(f"{label} {integer_field} must be an integer")
    for string_field in ("phase", "day_phase", "ending"):
        if not isinstance(state[string_field], str) or not state[string_field]:
            raise smoke.SmokeError(f"{label} {string_field} must be a stable enum")
    for bool_field in (
        "day_phase_started",
        "day_window_closed",
        "mid_crisis_triggered",
    ):
        if not isinstance(state[bool_field], bool):
            raise smoke.SmokeError(f"{label} {bool_field} must be boolean")
    score = state["score"]
    if isinstance(score, bool) or not isinstance(score, (int, float)):
        raise smoke.SmokeError(f"{label} score must be numeric")
    score_breakdown = _validate_exact_object(
        state["score_breakdown"],
        SCORE_BREAKDOWN_FIELDS,
        f"{label} score_breakdown",
    )
    for score_field in SCORE_BREAKDOWN_FIELDS - {"rating"}:
        value = score_breakdown[score_field]
        if isinstance(value, bool) or not isinstance(value, (int, float)):
            raise smoke.SmokeError(
                f"{label} score_breakdown.{score_field} must be numeric"
            )
    if (
        not isinstance(score_breakdown["rating"], str)
        or not score_breakdown["rating"]
    ):
        raise smoke.SmokeError(f"{label} score_breakdown.rating must be a string")

    heating = _validate_exact_object(
        state["heating"],
        HEATING_FIELDS,
        f"{label} heating",
    )
    if not isinstance(heating["history"], list):
        raise smoke.SmokeError(f"{label} heating history must be an array")
    resources = _validate_exact_object(
        state["resources"],
        RESOURCE_FIELDS,
        f"{label} resources",
    )
    if any(
        isinstance(value, bool) or not isinstance(value, int)
        for value in resources.values()
    ):
        raise smoke.SmokeError(f"{label} resource values must be integers")
    tasks = _validate_exact_object(
        state["tasks"],
        TASK_FIELDS,
        f"{label} tasks",
    )
    related_flags = _validate_exact_object(
        state["related_flags"],
        RELATED_FLAG_FIELDS,
        f"{label} related_flags",
    )
    for field in RELATED_FLAG_FIELDS - {"forced_action_count", "risky_repair_count"}:
        if not isinstance(related_flags[field], bool):
            raise smoke.SmokeError(f"{label} related_flags.{field} must be boolean")
    for field in ("forced_action_count", "risky_repair_count"):
        value = related_flags[field]
        if isinstance(value, bool) or not isinstance(value, int):
            raise smoke.SmokeError(f"{label} related_flags.{field} must be integer")

    characters = state["characters"]
    if not isinstance(characters, Mapping):
        raise smoke.SmokeError(f"{label} characters must be an object")
    normalized_character_ids = {_normalize_key(str(key)) for key in characters}
    if normalized_character_ids != CHARACTER_IDS:
        raise smoke.SmokeError(
            f"{label} characters must contain Player, GuHeng, and YeCheng"
        )
    for character_id, character in characters.items():
        _validate_exact_object(
            character,
            CHARACTER_FIELDS,
            f"{label} characters.{character_id}",
        )

    knowledge = state["player_knowledge"]
    if not isinstance(knowledge, Mapping):
        raise smoke.SmokeError(f"{label} player_knowledge must be an object")
    knowledge_levels = {
        "Unknown",
        "Claimed",
        "Suspected",
        "Confirmed",
        "unknown",
        "claimed",
        "suspected",
        "confirmed",
    }
    if any(
        not isinstance(fact_id, str)
        or not FACT_ID_PATTERN.fullmatch(fact_id)
        or level not in knowledge_levels
        for fact_id, level in knowledge.items()
    ):
        raise smoke.SmokeError(f"{label} player_knowledge is malformed")
    disclosed = state["disclosed_fact_ids"]
    if (
        not isinstance(disclosed, list)
        or any(
            not isinstance(fact_id, str)
            or not FACT_ID_PATTERN.fullmatch(fact_id)
            for fact_id in disclosed
        )
        or len(disclosed) != len(set(disclosed))
    ):
        raise smoke.SmokeError(f"{label} disclosed_fact_ids is malformed")
    cards = state["requirement_cards"]
    if not isinstance(cards, list):
        raise smoke.SmokeError(f"{label} requirement_cards must be an array")
    card_identities: list[tuple[str, str]] = []
    for index, card in enumerate(cards):
        if not isinstance(card, Mapping):
            raise smoke.SmokeError(
                f"{label} requirement_cards[{index}] must be an object"
            )
        action_id = card.get("action_id")
        requirement_id = card.get("requirement_id")
        if (
            not isinstance(action_id, str)
            or not ACTION_ID_PATTERN.fullmatch(action_id)
            or not isinstance(requirement_id, str)
            or not STABLE_LOWER_ID_PATTERN.fullmatch(requirement_id)
        ):
            raise smoke.SmokeError(
                f"{label} requirement_cards[{index}] has invalid identity"
            )
        card_identities.append((action_id, requirement_id))
    if len(card_identities) != len(set(card_identities)):
        raise smoke.SmokeError(f"{label} requirement_cards contains duplicates")

    normalized = {
        field: _canonical(state[field]) for field in GAMEPLAY_STATE_FIELDS
    }
    normalized["disclosed_fact_ids"] = _sorted_canonical_list(disclosed)
    normalized["requirement_cards"] = _sorted_canonical_list(cards)
    return normalized


def extract_gameplay_state(event_log: Mapping[str, object]) -> dict[str, object]:
    return _normalize_gameplay_state(event_log, "event log")


def validate_ai_ab_equivalence(
    offline_state: Mapping[str, object],
    online_state: Mapping[str, object],
) -> dict[str, object]:
    offline = _normalize_gameplay_state(offline_state, "offline state")
    online = _normalize_gameplay_state(online_state, "online state")
    if offline != online:
        differing = [
            field
            for field in GAMEPLAY_STATE_FIELDS
            if offline.get(field) != online.get(field)
        ]
        raise smoke.SmokeError(
            "AI A/B changed authoritative gameplay state: " + ", ".join(differing)
        )
    return {
        "authoritative_results_equal": True,
        "compared_fields": list(GAMEPLAY_STATE_FIELDS),
    }


def _validate_event_log_v13(
    scenario: V13Scenario,
    event_log: dict[str, Any],
) -> dict[str, Any]:
    summary = _BASE_VALIDATE_EVENT_LOG(scenario, event_log)
    summary["gameplay_state"] = extract_gameplay_state(event_log)
    return summary


class _ConfiguredMockLoopbackEndpoint(_BASE_MOCK_ENDPOINT):
    def __init__(self, audit_path: Path, config: Any = None) -> None:
        if config is None:
            config = smoke.MockConfig(mode=_ACTIVE_MOCK_MODE)
        elif hasattr(config, "mode"):
            config = replace(config, mode=_ACTIVE_MOCK_MODE)
        super().__init__(audit_path, config)


def _validate_model_audit_v13(
    scenario: V13Scenario,
    audit_path: Path,
) -> dict[str, Any] | None:
    online_modes = {
        "loopback_mock",
        "timeout_endpoint",
        "unreachable_endpoint",
        "provider_rejected",
    }
    if scenario.llm_mode not in online_modes:
        if audit_path.exists() and audit_path.stat().st_size:
            raise smoke.SmokeError(
                f"{scenario.scenario_id}: unexpected live-provider audit"
            )
        return None
    records = smoke.load_json_lines(audit_path, scenario.scenario_id)
    if len(records) != scenario.expected_model_calls:
        raise smoke.SmokeError(f"{scenario.scenario_id}: model audit count mismatch")
    expected_actions = [
        action_id
        for action_id in smoke.EXPECTED_ROUTES[scenario.route]["actions"]
        if action_id in smoke.EXPRESSION_ACTION_IDS
    ]
    observed_actions = [str(record.get("action_id", "")) for record in records]
    if sorted(expected_actions) != sorted(observed_actions):
        raise smoke.SmokeError(
            f"{scenario.scenario_id}: model audit action identity mismatch"
        )
    for record in records:
        if record.get("kind") != "dialogue_expression_v3":
            raise smoke.SmokeError(
                f"{scenario.scenario_id}: model audit kind is not v1.3 dialogue"
            )
        outcome = record.get("outcome")
        if scenario.mock_mode == "valid_natural" and outcome != "accepted":
            raise smoke.SmokeError(
                f"{scenario.scenario_id}: valid natural output was not accepted"
            )
        if scenario.mock_mode in {
            "missing_atom",
            "forbidden_fact",
            "added_condition",
            "system_jargon",
            "invalid_json",
        } and outcome == "accepted":
            raise smoke.SmokeError(
                f"{scenario.scenario_id}: invalid model output was accepted"
            )
        if scenario.llm_mode in {"timeout_endpoint", "unreachable_endpoint"} and (
            not isinstance(outcome, str)
            or not any(token in outcome for token in ("transport", "timeout", "http"))
        ):
            raise smoke.SmokeError(
                f"{scenario.scenario_id}: transport failure was not audited"
            )
        if (
            scenario.llm_mode == "provider_rejected"
            and record.get("http_status") != 401
        ):
            raise smoke.SmokeError(
                f"{scenario.scenario_id}: provider rejection status mismatch"
            )
        if record.get("transport_attempt_limit") not in {1, 2}:
            raise smoke.SmokeError(
                f"{scenario.scenario_id}: transport attempt limit mismatch"
            )
    return {
        "records": len(records),
        "action_ids": observed_actions,
        "http_statuses": [record.get("http_status") for record in records],
        "outcomes": [record.get("outcome") for record in records],
        "mock_mode": scenario.mock_mode,
    }


def _run_scenario_v13(
    executable: Path,
    scenario: V13Scenario,
    staging_root: Path,
    timeout_seconds: float,
) -> dict[str, Any]:
    global _ACTIVE_MOCK_MODE
    _ACTIVE_MOCK_MODE = scenario.mock_mode or "valid_natural"
    try:
        summary = _BASE_RUN_SCENARIO(
            executable,
            scenario,
            staging_root,
            timeout_seconds,
        )
    finally:
        _ACTIVE_MOCK_MODE = "valid_natural"

    runtime_root = staging_root / "Runtime" / scenario.scenario_id
    expected_records = sum(
        1
        for action_id in smoke.EXPECTED_ROUTES[scenario.route]["actions"]
        if action_id in {"talk_gu_heng", "talk_ye_cheng"}
    )
    dialogue_audit_path = runtime_root / DIALOGUE_AUDIT_REL
    dialogue_audit = load_and_validate_dialogue_audit(
        dialogue_audit_path,
        expected_records=expected_records,
    )
    audit_name = f"{scenario.scenario_id}_DialogueAudit.jsonl"
    if dialogue_audit_path.is_file():
        shutil.copy2(dialogue_audit_path, staging_root / "Evidence" / audit_name)
        summary["dialogue_audit_file"] = audit_name
    summary["dialogue_audit"] = dialogue_audit
    if scenario.mock_mode:
        summary["mock_mode"] = scenario.mock_mode
    return summary


def configure_v13_contract() -> None:
    global _CONFIGURED
    smoke.ARTIFACT_PREFIX = "WhiteoutStation-v1.3-Win64-"
    smoke.RELEASE_LABEL = "v1.3"
    smoke.AGENT_RUNTIME_REL = Path(
        "Windows/WhiteoutStation/Content/Agents/AgentRuntime.v1.3.json"
    )
    smoke.OUTPUT_REL = Path("Validation/ShippingSmokeV13")
    smoke.AGENT_RUNTIME_VERSION = "1.3.0"
    smoke.AGENT_SCHEMA_VERSION = 6
    smoke.SMOKE_SCHEMA = "whiteout.v1.3.shipping-smoke.v1"
    smoke.EXPRESSION_ACTION_IDS = frozenset(
        {"talk_gu_heng", "talk_ye_cheng"}
    )
    smoke.PERFORMANCE_VALIDATION_REASON = "accepted"
    smoke.RUN_TIMEOUT_PROBE = False
    smoke.SCENARIOS = V13_SCENARIOS
    if not _CONFIGURED:
        smoke.validate_event_log = _validate_event_log_v13
        smoke.validate_audit = _validate_model_audit_v13
        smoke.run_scenario = _run_scenario_v13
        smoke.MockLoopbackEndpoint = _ConfiguredMockLoopbackEndpoint
        _CONFIGURED = True


def _validate_packaged_runtime_contract(artifact_root: Path) -> None:
    runtime = smoke.load_json(artifact_root / smoke.AGENT_RUNTIME_REL)
    if not isinstance(runtime, dict):
        raise smoke.SmokeError("Packaged v1.3 Agent runtime root must be an object")
    expected = {
        "runtime_version": "1.3.0",
        "schema_version": 6,
        "protocol_version": "dialogue_epistemic_v3",
        "prompt_mode": "semantic_atoms_full_line",
        "max_sentences": 2,
        "max_line_chars": 96,
        "max_output_tokens": 256,
        "llm_enabled": False,
    }
    for field, value in expected.items():
        if runtime.get(field) != value:
            raise smoke.SmokeError(
                f"Packaged v1.3 Agent runtime {field} must be {value!r}"
            )


def _validate_mock_coverage(report: Mapping[str, object]) -> None:
    scenarios = report.get("scenarios")
    if not isinstance(scenarios, list):
        raise smoke.SmokeError("v1.3 summary scenarios must be an array")
    by_id = {
        str(item.get("scenario_id")): item
        for item in scenarios
        if isinstance(item, dict)
    }
    observed_modes = {
        str(item.get("mock_mode"))
        for item in by_id.values()
        if item.get("mock_mode")
    }
    if observed_modes != set(MOCK_MODES):
        raise smoke.SmokeError(
            "v1.3 Shipping smoke did not cover every mock mode: "
            + ", ".join(sorted(set(MOCK_MODES) - observed_modes))
        )

    offline_by_route = {
        route: by_id[f"missing_key_{route}"]["gameplay_state"]
        for route in ("medical", "technical", "quick", "wait", "collapse")
    }
    for scenario in by_id.values():
        route = scenario.get("route")
        llm_mode = scenario.get("llm_mode")
        if llm_mode not in ONLINE_LLM_MODES or route not in offline_by_route:
            continue
        validate_ai_ab_equivalence(
            offline_by_route[str(route)],
            scenario["gameplay_state"],
        )


def run_shipping_smoke(
    artifact_root: Path,
    *,
    timeout_seconds: float = 90.0,
) -> Path:
    configure_v13_contract()
    if not 10.0 <= timeout_seconds <= 300.0:
        raise smoke.SmokeError("timeout_seconds must be between 10 and 300")
    root, executable = smoke.validate_artifact_root(artifact_root)
    _validate_packaged_runtime_contract(root)
    validation_root = root / "Validation"
    validation_root.mkdir(parents=True, exist_ok=True)
    output_root = root / smoke.OUTPUT_REL

    with tempfile.TemporaryDirectory(
        prefix=".shipping-smoke-v13-",
        dir=validation_root,
    ) as temporary:
        staging_root = Path(temporary)
        summaries = [
            _run_scenario_v13(executable, scenario, staging_root, timeout_seconds)
            for scenario in V13_SCENARIOS
        ]
        report: dict[str, Any] = {
            "schema": smoke.SMOKE_SCHEMA,
            "passed": True,
            "artifact_root_name": root.name,
            "credential_policy": {
                "api_key_value_read": False,
                "api_key_value_persisted": False,
                "child_api_key_forced_empty": False,
                "synthetic_invalid_credential_used": True,
                "local_credential_config_accepted": False,
            },
            "scenarios": summaries,
            "mock_modes": list(MOCK_MODES),
        }
        _validate_mock_coverage(report)
        offline = next(
            item
            for item in summaries
            if item.get("scenario_id") == "missing_key_technical"
        )
        online = next(
            item
            for item in summaries
            if item.get("scenario_id") == "loopback_online_technical"
        )
        report["ai_ab"] = {
            "route": "technical",
            **validate_ai_ab_equivalence(
                offline["gameplay_state"],
                online["gameplay_state"],
            ),
            "offline_model_calls": offline["model_calls"],
            "online_model_calls": online["model_calls"],
        }
        evidence_root = staging_root / "Evidence"
        evidence_root.mkdir(parents=True, exist_ok=True)
        summary_path = evidence_root / "shipping_smoke_summary.json"
        summary_path.write_text(
            json.dumps(report, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )
        evidence_root.replace(output_root)
    return output_root / "shipping_smoke_summary.json"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--artifact-root",
        type=Path,
        required=True,
        help="Exact unique WhiteoutStation-v1.3-Win64-<run_id> artifact root",
    )
    parser.add_argument("--timeout-seconds", type=float, default=90.0)
    args = parser.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")
    configure_v13_contract()
    smoke.force_empty_credential_inputs()
    try:
        summary_path = run_shipping_smoke(
            args.artifact_root,
            timeout_seconds=args.timeout_seconds,
        )
    except (smoke.SmokeError, OSError, StopIteration) as exc:
        print(f"SHIPPING SMOKE v1.3: FAIL: {exc}")
        return 1
    print(
        "SHIPPING SMOKE v1.3: PASS "
        f"({len(V13_SCENARIOS)} routes, {len(MOCK_MODES)} mock modes) "
        f"summary={summary_path}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
