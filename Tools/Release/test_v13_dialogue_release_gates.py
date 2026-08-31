from __future__ import annotations

import json
import subprocess
from copy import deepcopy
from pathlib import Path

import pytest

from Tools.Agents import mock_server
from Tools.Release import run_v11_shipping_smoke as v11_smoke
from Tools.Release import run_v13_shipping_smoke as v13_smoke
from Tools.Release import validate_source_v13 as source_gate
from Tools.Release.run_v13_shipping_smoke import (
    DIALOGUE_AUDIT_FIELDS,
    GAMEPLAY_STATE_FIELDS,
    MOCK_MODES,
    V13_SCENARIOS,
    configure_v13_contract,
    validate_ai_ab_equivalence,
    validate_dialogue_audit_records,
)
from Tools.Release.validate_source_v13 import (
    validate_versions,
    validate_worktree,
)


def _write_fixture(root: Path) -> None:
    output_fields = (
        "npc_line realized_atom_ids disclosed_fact_ids emotion "
        "movement_intent reaction_action"
    )
    stable_atoms = (
        "PLAYER_ASSISTANCE_NEEDED GU_HENG_NEEDS_RECOVERY "
        "REPAIR_ROOM_SHOULD_BE_WARM BACKUP_POWER_DECLINING "
        "BLIZZARD_WINDOW_SHRINKING HEAT_PRIORITY_DECISION "
        "HAND_INJURY_AFFECTS_FINE_WORK HEAT_PACK_AVAILABLE"
    )
    files = {
        "WhiteoutStation/Config/DefaultGame.ini": "ProjectVersion=1.3.0\n",
        "WhiteoutStation/Content/Rules/WhiteoutStationRules.v1.1.json": json.dumps(
            {"rules_version": "1.1.0", "schema_version": 6}
        ),
        "WhiteoutStation/Content/Agents/AgentRuntime.v1.3.json": json.dumps(
            {
                "runtime_version": "1.3.0",
                "schema_version": 6,
                "protocol_version": "dialogue_epistemic_v3",
                "prompt_mode": "semantic_atoms_full_line",
                "max_sentences": 2,
                "max_line_chars": 96,
                "max_output_tokens": 256,
                "transport": "openai_chat_completions",
                "llm_enabled": False,
                "endpoint": "https://api.deepseek.com",
            }
        ),
        "README.md": "当前版本为 v1.3\ndocs/BUILD_AND_PLAY_v1.3.md\n",
        "docs/BUILD_AND_PLAY_v1.3.md": (
            "AgentRuntime.v1.3.json ws.DialogueDebug 1 "
            "WhiteoutStation_DialogueAudit.jsonl"
        ),
        "WhiteoutStation/Source/WhiteoutStation/Private/Agents/"
        "WSAgentGateway.cpp": f"""
void UWSAgentGateway::LoadConfig()
{{
    const char* Runtime = "AgentRuntime.v1.3.json";
    const char* Protocol = "dialogue_epistemic_v3";
    const char* PromptMode = "semantic_atoms_full_line";
}}

void UWSAgentGateway::RequestDialogueRealization()
{{
    BuildDialogueRealizationRequestJson();
    ValidateDialogueOutcomePayload();
}}

FString UWSAgentGateway::BuildDialogueRealizationContextJson() const
{{
    return "dialogue_epistemic_v3 semantic_atoms_full_line "
        "must_realize may_realize";
}}

FString UWSAgentGateway::BuildDialogueRealizationRequestJson() const
{{
    BuildDialogueRealizationContextJson();
    return "{output_fields}";
}}

bool UWSAgentGateway::ValidateDialogueOutcomePayload()
{{
    const char* ExactFields = "{output_fields}";
    return true;
}}
""",
        "WhiteoutStation/Source/WhiteoutStation/Private/Agents/"
        "WSNPCDecisionService.cpp": stable_atoms,
        "WhiteoutStation/Source/WhiteoutStation/Private/State/"
        "WindStationStateSubsystem.cpp": """
const char* SaveSlots = "WhiteoutStation_Autosave_v1_3 "
    "WhiteoutStation_Autosave_v1_2 WhiteoutStation_DialogueAudit.jsonl";
void UWindStationStateSubsystem::RealizePreparedDialogue()
{
    AgentGateway->RequestDialogueRealization();
}
""",
        "WhiteoutStation/Source/WhiteoutStation/Private/Flow/"
        "WhiteoutGameMode.cpp": """
void AWhiteoutGameMode::BeginPlay()
{
    IntentProbeGateway->RequestDialogueRealization();
}
void AWhiteoutGameMode::RunDialogueHistoryProbeStep(const int32 Step)
{
    IntentProbeGateway->RequestDialogueRealization();
}
""",
        "WhiteoutStation/Source/WhiteoutStation/Public/Save/"
        "WindStationSaveGame.h": 'TEXT("1.3.0")',
        "Tools/Agents/agent_contract.py": (
            f"dialogue_epistemic_v3 semantic_atoms_full_line {output_fields}"
        ),
        "Tools/Agents/mock_server.py": " ".join(MOCK_MODES),
        "Tools/Agents/probe_deepseek.py": (
            "PROBE_MUST_ATOMS build_request_payload( validate_completion("
        ),
    }
    for relative_path, content in files.items():
        path = root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")


def _valid_dialogue_audit() -> dict[str, object]:
    return {
        "kind": "dialogue_expression",
        "transaction_id": "1fd143d9-2d66-4417-aec1-331202c24803",
        "speaker": "gu_heng",
        "query_type": "requirements",
        "target_action_id": "repair_generator",
        "planned_disclosure_fact_ids": ["FACT_RELAY_COMPATIBILITY"],
        "final_disclosed_fact_ids": ["FACT_RELAY_COMPATIBILITY"],
        "required_atom_ids": [
            "PLAYER_ASSISTANCE_NEEDED",
            "GU_HENG_NEEDS_RECOVERY",
        ],
        "realized_atom_ids": [
            "GU_HENG_NEEDS_RECOVERY",
            "PLAYER_ASSISTANCE_NEEDED",
        ],
        "answer_source": "online_full_line",
        "validation_outcome": "accepted",
        "prompt_tokens": 210,
        "completion_tokens": 54,
    }


def _character_state(*, trust: float, pressure: float, stamina: int) -> dict[str, object]:
    return {
        "health": 10.0,
        "temperature": 7.0,
        "hunger": 6.5,
        "fatigue": 6.5,
        "pressure": pressure,
        "trust": trust,
        "stamina": stamina,
        "injury_severity": "Normal",
        "injury_id": "none",
        "injury_worsening_marks": 0,
        "bandage_protection": 0,
        "temporary_support_uses": 0,
        "temporary_support_phase": "Complete",
        "location": "ControlRoom",
    }


def _gameplay_state() -> dict[str, object]:
    return {
        "remaining_ap": 2,
        "phase_action_points": 1,
        "phase": "Results",
        "day_phase": "Complete",
        "day_phase_started": True,
        "day_window_closed": True,
        "mid_crisis_triggered": True,
        "heating": {
            "current_zone": "RepairRoom",
            "locked": True,
            "history": [
                {"phase": "Morning", "zone": "MedicalRoom"},
                {"phase": "Afternoon", "zone": "RepairRoom"},
            ],
        },
        "ending": "TaskSuccess",
        "score": 75.94,
        "score_breakdown": {
            "task_quality": 28.0,
            "people": 16.0,
            "effective_reserves": 8.0,
            "social_stability": 12.0,
            "information_responsibility": 11.94,
            "total": 75.94,
            "rating": "B",
        },
        "player_knowledge": {
            "FACT_BURNT_RELAY": "Confirmed",
            "FACT_HAND_INJURY": "Suspected",
        },
        "disclosed_fact_ids": ["FACT_HAND_INJURY", "FACT_BURNT_RELAY"],
        "resources": {
            "fuel": 2,
            "food": 1,
            "medicine": 0,
            "heat_pack": 1,
            "replacement_relay": 0,
        },
        "related_flags": {
            "kitchen_heater_intact": True,
            "heat_pack_revealed": False,
            "repair_room_heated": True,
            "medical_room_heated": False,
            "gu_heng_diagnosed": False,
            "gu_heng_treated": False,
            "gu_heng_fed": True,
            "gu_heng_cooperative": True,
            "relay_compatibility_known": True,
            "relay_installed": True,
            "self_repair_used": False,
            "records_preserved": True,
            "player_fed": True,
            "ye_cheng_fed": True,
            "cabinet_inspected": True,
            "log_penalty_active": False,
            "forced_action_count": 0,
            "risky_repair_count": 0,
        },
        "tasks": {
            "generator_progress": 2,
            "antenna_calibration": 2,
            "signal_sent": True,
            "generator_stable": True,
        },
        "characters": {
            "Player": _character_state(trust=5.0, pressure=4.0, stamina=2),
            "GuHeng": _character_state(trust=7.0, pressure=5.0, stamina=1),
            "YeCheng": _character_state(trust=6.0, pressure=3.0, stamina=2),
        },
        "requirement_cards": [
            {
                "action_id": "repair_generator",
                "requirement_id": "repair_room_heated",
                "met": True,
                "player_facing_detail": "维修间已经暖起来了",
            },
            {
                "action_id": "repair_generator",
                "requirement_id": "replacement_relay_available",
                "met": True,
                "player_facing_detail": "替换件已确认",
            },
        ],
    }


def test_v13_version_contract_accepts_epistemic_runtime_over_v11_rules(
    tmp_path: Path,
) -> None:
    _write_fixture(tmp_path)
    assert validate_versions(tmp_path).passed


@pytest.mark.parametrize(
    ("field", "stale"),
    (
        ("runtime_version", "1.2.0"),
        ("schema_version", 5),
        ("protocol_version", "dialogue_grounding_v2"),
        ("prompt_mode", "semantic_spine_plus_persona_tail"),
        ("max_sentences", 3),
        ("max_line_chars", 97),
        ("max_output_tokens", 128),
        ("llm_enabled", True),
    ),
)
def test_v13_version_contract_rejects_runtime_drift(
    tmp_path: Path,
    field: str,
    stale: object,
) -> None:
    _write_fixture(tmp_path)
    runtime = tmp_path / "WhiteoutStation/Content/Agents/AgentRuntime.v1.3.json"
    payload = json.loads(runtime.read_text(encoding="utf-8"))
    payload[field] = stale
    runtime.write_text(json.dumps(payload), encoding="utf-8")
    assert not validate_versions(tmp_path).passed


def test_v13_version_contract_rejects_credential_fields(tmp_path: Path) -> None:
    _write_fixture(tmp_path)
    runtime = tmp_path / "WhiteoutStation/Content/Agents/AgentRuntime.v1.3.json"
    payload = json.loads(runtime.read_text(encoding="utf-8"))
    payload["api_key"] = "fixture"
    runtime.write_text(json.dumps(payload), encoding="utf-8")
    assert not validate_versions(tmp_path).passed


@pytest.mark.parametrize(
    "v2_token",
    (
        "AgentRuntime.v1.2.json",
        "dialogue_grounding_v2",
        "semantic_spine_plus_persona_tail",
    ),
)
def test_v13_shipping_entry_path_rejects_v2_tokens(
    tmp_path: Path,
    v2_token: str,
) -> None:
    _write_fixture(tmp_path)
    gateway = tmp_path / (
        "WhiteoutStation/Source/WhiteoutStation/Private/Agents/WSAgentGateway.cpp"
    )
    text = gateway.read_text(encoding="utf-8")
    function = source_gate._cpp_function(text, "UWSAgentGateway::LoadConfig")
    stale_function = function[:-1] + f'const char* Legacy = "{v2_token}";\n}}'
    gateway.write_text(
        text.replace(function, stale_function, 1),
        encoding="utf-8",
    )
    assert not validate_versions(tmp_path).passed


@pytest.mark.parametrize(
    ("relative_path", "function_name"),
    (
        (
            "WhiteoutStation/Source/WhiteoutStation/Private/Agents/WSAgentGateway.cpp",
            "UWSAgentGateway::LoadConfig",
        ),
        (
            "WhiteoutStation/Source/WhiteoutStation/Private/Agents/WSAgentGateway.cpp",
            "UWSAgentGateway::RequestDialogueRealization",
        ),
        (
            "WhiteoutStation/Source/WhiteoutStation/Private/Agents/WSAgentGateway.cpp",
            "UWSAgentGateway::BuildDialogueRealizationContextJson",
        ),
        (
            "WhiteoutStation/Source/WhiteoutStation/Private/Agents/WSAgentGateway.cpp",
            "UWSAgentGateway::BuildDialogueRealizationRequestJson",
        ),
        (
            "WhiteoutStation/Source/WhiteoutStation/Private/Agents/WSAgentGateway.cpp",
            "UWSAgentGateway::ValidateDialogueOutcomePayload",
        ),
        (
            "WhiteoutStation/Source/WhiteoutStation/Private/State/WindStationStateSubsystem.cpp",
            "UWindStationStateSubsystem::RealizePreparedDialogue",
        ),
        (
            "WhiteoutStation/Source/WhiteoutStation/Private/Flow/WhiteoutGameMode.cpp",
            "AWhiteoutGameMode::BeginPlay",
        ),
        (
            "WhiteoutStation/Source/WhiteoutStation/Private/Flow/WhiteoutGameMode.cpp",
            "AWhiteoutGameMode::RunDialogueHistoryProbeStep",
        ),
    ),
)
def test_v13_shipping_gate_rejects_each_missing_required_function(
    tmp_path: Path,
    relative_path: str,
    function_name: str,
) -> None:
    _write_fixture(tmp_path)
    source = tmp_path / relative_path
    text = source.read_text(encoding="utf-8")
    function = source_gate._cpp_function(text, function_name)
    assert function
    source.write_text(text.replace(function, "", 1), encoding="utf-8")
    assert not validate_versions(tmp_path).passed


@pytest.mark.parametrize(
    "relative_path",
    (
        "WhiteoutStation/Source/WhiteoutStation/Private/State/WindStationStateSubsystem.cpp",
        "WhiteoutStation/Source/WhiteoutStation/Private/Flow/WhiteoutGameMode.cpp",
    ),
)
def test_v13_shipping_gate_rejects_legacy_production_calls(
    tmp_path: Path,
    relative_path: str,
) -> None:
    _write_fixture(tmp_path)
    source = tmp_path / relative_path
    text = source.read_text(encoding="utf-8")
    source.write_text(
        text.replace("RequestDialogueRealization", "RequestExpression"),
        encoding="utf-8",
    )
    assert not validate_versions(tmp_path).passed


def test_v13_shipping_gate_does_not_accept_comments_or_unrelated_calls(
    tmp_path: Path,
) -> None:
    _write_fixture(tmp_path)
    state = tmp_path / source_gate.STATE_SUBSYSTEM_REL
    text = state.read_text(encoding="utf-8")
    function = source_gate._cpp_function(
        text,
        "UWindStationStateSubsystem::RealizePreparedDialogue",
    )
    misleading = """
// UWindStationStateSubsystem::RealizePreparedDialogue calls
// AgentGateway->RequestDialogueRealization();
void UWindStationStateSubsystem::UnrelatedHelper()
{
    AgentGateway->RequestDialogueRealization();
}
"""
    state.write_text(text.replace(function, misleading, 1), encoding="utf-8")
    assert not validate_versions(tmp_path).passed


def test_v13_shipping_gate_allows_retained_v2_compatibility_definition(
    tmp_path: Path,
) -> None:
    _write_fixture(tmp_path)
    gateway = tmp_path / source_gate.GATEWAY_REL
    gateway.write_text(
        gateway.read_text(encoding="utf-8")
        + """
void UWSAgentGateway::RequestExpression()
{
    const char* Protocol = "dialogue_grounding_v2";
    const char* Mode = "semantic_spine_plus_persona_tail";
}
""",
        encoding="utf-8",
    )
    assert validate_versions(tmp_path).passed


def test_v13_worktree_ignores_both_user_construction_briefs(tmp_path: Path) -> None:
    subprocess.run(
        ["git", "init", "--initial-branch=main"],
        cwd=tmp_path,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    for name in (
        "风雪站_断电前夜_v1.2对话落地重构执行施工文档.docx",
        "风雪站_断电前夜_v1.3知识边界与自然对话重构执行施工文档.docx",
    ):
        path = tmp_path / "docs" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"external fixture")
    report = validate_worktree(tmp_path)
    assert report.passed
    assert len(report.warnings) == 1


def test_v13_dialogue_audit_accepts_exact_metadata_whitelist() -> None:
    record = _valid_dialogue_audit()
    summary = validate_dialogue_audit_records([record], expected_records=1)
    assert set(summary["fields"]) == DIALOGUE_AUDIT_FIELDS
    assert "contains_raw_dialogue_or_credentials" not in summary


@pytest.mark.parametrize(
    "private_field",
    ("npc_line", "player_text", "prompt", "api_key"),
)
def test_v13_dialogue_audit_rejects_raw_or_credential_fields(
    private_field: str,
) -> None:
    record = _valid_dialogue_audit()
    record[private_field] = "sensitive fixture"
    with pytest.raises(v11_smoke.SmokeError, match="whitelist"):
        validate_dialogue_audit_records([record], expected_records=1)


def test_v13_dialogue_audit_rejects_disclosure_or_atom_mismatch() -> None:
    disclosure = _valid_dialogue_audit()
    disclosure["final_disclosed_fact_ids"] = []
    with pytest.raises(v11_smoke.SmokeError, match="planned disclosure"):
        validate_dialogue_audit_records([disclosure])

    atom = _valid_dialogue_audit()
    atom["realized_atom_ids"] = ["PLAYER_ASSISTANCE_NEEDED"]
    with pytest.raises(v11_smoke.SmokeError, match="required atom"):
        validate_dialogue_audit_records([atom])


def test_v13_dialogue_audit_rejects_raw_text_in_code_fields() -> None:
    record = _valid_dialogue_audit()
    record["validation_outcome"] = "玩家原话不应进入审计"
    with pytest.raises(v11_smoke.SmokeError, match="possible raw dialogue"):
        validate_dialogue_audit_records([record])


@pytest.mark.parametrize(
    ("field", "value"),
    (
        ("speaker", "gu_heng 玩家台词"),
        ("answer_source", "Bearer fixture-token"),
        ("answer_source", "sk-fixture123"),
        ("answer_source", "api_key=fixture"),
        ("validation_outcome", "prompt=fixture"),
        ("validation_outcome", "request payload: fixture"),
        ("validation_outcome", "response: fixture"),
        ("validation_outcome", "player_said=fixture"),
    ),
)
def test_v13_dialogue_audit_recursively_rejects_sensitive_string_values(
    field: str,
    value: str,
) -> None:
    record = _valid_dialogue_audit()
    record[field] = value
    with pytest.raises(
        v11_smoke.SmokeError,
        match="raw dialogue|credential trace|sensitive trace",
    ):
        validate_dialogue_audit_records([record])


@pytest.mark.parametrize(
    ("field", "value", "error"),
    (
        ("kind", "dialogue", "kind"),
        ("transaction_id", "00000000-0000-0000-0000-000000000000", "transaction_id"),
        ("speaker", "unknown_npc", "speaker"),
        ("query_type", "private_state", "query_type"),
        ("target_action_id", "RepairGenerator", "target_action_id"),
        ("answer_source", "model_raw", "answer_source"),
        ("validation_outcome", "arbitrary", "validation_outcome"),
    ),
)
def test_v13_dialogue_audit_rejects_invalid_catalog_values(
    field: str,
    value: str,
    error: str,
) -> None:
    record = _valid_dialogue_audit()
    record[field] = value
    with pytest.raises(v11_smoke.SmokeError, match=error):
        validate_dialogue_audit_records([record])


@pytest.mark.parametrize(
    ("field", "value"),
    (
        ("planned_disclosure_fact_ids", ["RELAY_COMPATIBILITY"]),
        ("final_disclosed_fact_ids", ["fact_relay_compatibility"]),
        ("required_atom_ids", ["player_assistance_needed"]),
        ("realized_atom_ids", ["PLAYERASSISTANCE"]),
    ),
)
def test_v13_dialogue_audit_rejects_malformed_fact_and_atom_ids(
    field: str,
    value: list[str],
) -> None:
    record = _valid_dialogue_audit()
    record[field] = value
    with pytest.raises(v11_smoke.SmokeError, match="stable identifier array"):
        validate_dialogue_audit_records([record])


def test_v13_dialogue_audit_allows_unknown_token_counts_only_as_minus_one() -> None:
    record = _valid_dialogue_audit()
    record["prompt_tokens"] = -1
    record["completion_tokens"] = -1
    assert validate_dialogue_audit_records([record])["records"] == 1

    record["completion_tokens"] = -2
    with pytest.raises(v11_smoke.SmokeError, match="completion_tokens"):
        validate_dialogue_audit_records([record])


@pytest.mark.parametrize(
    "outcome",
    ("fallback_request_not_started", "fallback_empty_response"),
)
def test_v13_dialogue_audit_allows_safe_fallback_reason_identifiers(
    outcome: str,
) -> None:
    record = _valid_dialogue_audit()
    record["validation_outcome"] = outcome
    assert validate_dialogue_audit_records([record])["records"] == 1


def test_v13_ai_ab_accepts_reordered_maps_facts_and_cards() -> None:
    offline = _gameplay_state()
    online = deepcopy(offline)
    online["player_knowledge"] = dict(
        reversed(list(offline["player_knowledge"].items()))
    )
    for field in (
        "score_breakdown",
        "resources",
        "related_flags",
        "tasks",
        "characters",
    ):
        online[field] = dict(reversed(list(offline[field].items())))
    online["disclosed_fact_ids"] = list(reversed(offline["disclosed_fact_ids"]))
    online["requirement_cards"] = list(reversed(offline["requirement_cards"]))
    summary = validate_ai_ab_equivalence(offline, online)
    assert summary["authoritative_results_equal"] is True
    assert tuple(summary["compared_fields"]) == GAMEPLAY_STATE_FIELDS


@pytest.mark.parametrize("field", GAMEPLAY_STATE_FIELDS)
def test_v13_ai_ab_rejects_every_authoritative_state_difference(field: str) -> None:
    offline = _gameplay_state()
    online = deepcopy(offline)
    if field == "remaining_ap":
        online[field] = 1
    elif field == "phase_action_points":
        online[field] = 0
    elif field == "phase":
        online[field] = "Ending"
    elif field == "day_phase":
        online[field] = "Dusk"
    elif field == "day_phase_started":
        online[field] = False
    elif field == "day_window_closed":
        online[field] = False
    elif field == "mid_crisis_triggered":
        online[field] = False
    elif field == "heating":
        online[field]["current_zone"] = "MedicalRoom"
    elif field == "ending":
        online[field] = "CostUncontrolled"
    elif field == "score":
        online[field] = 75.93
    elif field == "score_breakdown":
        online[field]["information_responsibility"] = 11.93
    elif field == "player_knowledge":
        online[field] = {}
    elif field == "disclosed_fact_ids":
        online[field] = []
    elif field == "resources":
        online[field]["fuel"] = 1
    elif field == "related_flags":
        online[field]["gu_heng_diagnosed"] = True
    elif field == "tasks":
        online[field]["generator_progress"] = 1
    elif field == "characters":
        online[field]["GuHeng"]["trust"] = 6.0
    elif field == "requirement_cards":
        online[field][0]["met"] = False
    with pytest.raises(v11_smoke.SmokeError, match="authoritative gameplay state"):
        validate_ai_ab_equivalence(offline, online)


@pytest.mark.parametrize(
    ("container", "field"),
    (
        ("resources", "fuel"),
        ("related_flags", "records_preserved"),
        ("tasks", "signal_sent"),
        ("score_breakdown", "rating"),
        ("characters.GuHeng", "temperature"),
    ),
)
def test_v13_ai_ab_rejects_incomplete_authoritative_subobjects(
    container: str,
    field: str,
) -> None:
    offline = _gameplay_state()
    online = deepcopy(offline)
    if container.startswith("characters."):
        character_id = container.split(".", 1)[1]
        del online["characters"][character_id][field]
    else:
        del online[container][field]
    with pytest.raises(v11_smoke.SmokeError, match="incomplete"):
        validate_ai_ab_equivalence(offline, online)


def test_v13_ai_ab_requires_lowercase_rule_requirement_ids() -> None:
    offline = _gameplay_state()
    online = deepcopy(offline)
    online["requirement_cards"][0]["requirement_id"] = "REQ_WARM_ROOM"
    with pytest.raises(v11_smoke.SmokeError, match="invalid identity"):
        validate_ai_ab_equivalence(offline, online)


ONLINE_SCENARIO_IDS = tuple(
    scenario.scenario_id
    for scenario in V13_SCENARIOS
    if scenario.llm_mode in v13_smoke.ONLINE_LLM_MODES
)


@pytest.mark.parametrize("scenario_id", ONLINE_SCENARIO_IDS)
def test_v13_mock_coverage_runs_full_ai_ab_for_every_online_scenario(
    scenario_id: str,
) -> None:
    scenarios: list[dict[str, object]] = []
    for scenario in V13_SCENARIOS:
        state = _gameplay_state()
        if scenario.scenario_id == scenario_id:
            state["remaining_ap"] = 1
        item: dict[str, object] = {
            "scenario_id": scenario.scenario_id,
            "route": scenario.route,
            "llm_mode": scenario.llm_mode,
            "gameplay_state": state,
        }
        if scenario.mock_mode:
            item["mock_mode"] = scenario.mock_mode
        scenarios.append(item)
    with pytest.raises(v11_smoke.SmokeError, match="authoritative gameplay state"):
        v13_smoke._validate_mock_coverage({"scenarios": scenarios})


def test_v13_shipping_contract_covers_every_required_mock_mode() -> None:
    configure_v13_contract()
    observed = {scenario.mock_mode for scenario in V13_SCENARIOS if scenario.mock_mode}
    assert observed == set(MOCK_MODES)
    assert set(mock_server.MOCK_MODES) == set(MOCK_MODES)
    assert v11_smoke.AGENT_RUNTIME_VERSION == "1.3.0"
    assert v11_smoke.AGENT_SCHEMA_VERSION == 6
    assert v11_smoke.AGENT_RUNTIME_REL.as_posix().endswith("AgentRuntime.v1.3.json")


def test_v13_mock_valid_and_failure_payloads_are_distinct() -> None:
    context = {
        "protocol_version": "dialogue_epistemic_v3",
        "prompt_mode": "semantic_atoms_full_line",
        "action_id": "talk_gu_heng",
        "must_realize": [
            {
                "atom_id": "PLAYER_ASSISTANCE_NEEDED",
                "natural_fallback": "我需要你搭把手",
            },
            {
                "atom_id": "GU_HENG_NEEDS_RECOVERY",
                "natural_fallback": "我得先缓一缓",
            },
        ],
        "may_realize": [],
        "planned_disclosure_fact_ids": [],
        "allowed_emotions": ["focused"],
        "allowed_movement_intents": ["stay"],
        "allowed_reaction_actions": ["consider"],
    }
    request = {
        "messages": [
            {"role": "system", "content": "Realize one NPC line."},
            {"role": "user", "content": json.dumps(context, ensure_ascii=False)},
        ]
    }
    _kind, valid = mock_server.build_mock_content(request, "valid_natural")
    assert valid["realized_atom_ids"] == [
        "PLAYER_ASSISTANCE_NEEDED",
        "GU_HENG_NEEDS_RECOVERY",
    ]
    _kind, missing = mock_server.build_mock_content(request, "missing_atom")
    assert missing["realized_atom_ids"] != valid["realized_atom_ids"]
    _kind, forbidden = mock_server.build_mock_content(request, "forbidden_fact")
    _kind, condition = mock_server.build_mock_content(request, "added_condition")
    _kind, jargon = mock_server.build_mock_content(request, "system_jargon")
    assert "手伤" in forbidden["npc_line"]
    assert "天线" in condition["npc_line"]
    assert "AP" in jargon["npc_line"]
