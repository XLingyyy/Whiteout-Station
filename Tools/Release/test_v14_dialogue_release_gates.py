from __future__ import annotations

import json
import subprocess
from pathlib import Path

import pytest

from Tools.Release import validate_source_v14 as source_gate


def _write_text(root: Path, relative_path: str, content: str) -> None:
    path = root / relative_path
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def _write_fixture(root: Path) -> None:
    runtime = {
        "runtime_version": "1.4.0",
        "schema_version": 7,
        "protocol_version": "bounded_roleplay_v4",
        "prompt_mode": "subjective_context_single_call",
        "max_sentences": 3,
        "max_line_chars": 120,
        "max_output_tokens": 320,
        "temperature": 0.5,
        "top_k_knowledge": 10,
        "max_session_turns": 3,
        "transport": "openai_chat_completions",
        "llm_enabled": False,
        "endpoint": "https://api.deepseek.com",
    }
    _write_text(
        root,
        source_gate.PROJECT_CONFIG_REL,
        "ProjectVersion=1.4.0\n"
        '+DirectoriesToAlwaysStageAsNonUFS=(Path="Dialogue")\n',
    )
    _write_text(
        root,
        source_gate.AGENT_RUNTIME_REL,
        json.dumps(runtime),
    )

    dialogue_assets: dict[str, dict[str, object]] = {
        "WorldKnowledge.json": {"schema_version": 1, "knowledge": []},
        "NPC_GuHeng.json": {
            "schema_version": 1,
            "profile": {},
            "knowledge": [],
        },
        "NPC_YeCheng.json": {
            "schema_version": 1,
            "profile": {},
            "knowledge": [],
        },
        "Relationship_GuHeng_YeCheng.json": {
            "schema_version": 1,
            "knowledge": [],
        },
        "DialoguePolicy.json": {
            "schema_version": 1,
            "top_k": 10,
            "max_sentences": 3,
            "max_characters": 120,
            "max_output_tokens": 320,
            "temperature": 0.5,
            "allowed_speech_functions": [],
            "allowed_proposal_types": [],
        },
        "SafeFallbacks.json": {"schema_version": 1, "fallbacks": []},
    }
    for name, payload in dialogue_assets.items():
        _write_text(
            root,
            f"{source_gate.DIALOGUE_DIR_REL}/{name}",
            json.dumps(payload),
        )

    asset_names = " ".join(source_gate.DIALOGUE_ASSET_NAMES)
    output_fields = " ".join(source_gate.OUTPUT_FIELDS)
    _write_text(
        root,
        source_gate.ROLEPLAY_TYPES_REL,
        """
EWSEpistemicStatus EWSRoleplayDisclosureLevel FWSRoleplayKnowledgeItem
FWSRoleplaySubjectiveState FWSRoleplayActionProposal FWSRoleplayMemoryEntry
FWSRoleplayRequest FWSRoleplayResponse
int32 TurnIndex = 1;
int32 RemainingTurns = 2;
""",
    )
    _write_text(
        root,
        source_gate.KNOWLEDGE_REPOSITORY_REL,
        f"""
void UWSRoleplayKnowledgeRepository::LoadDefault()
{{
    const char* Directory = "Dialogue/v1.4";
    const char* Assets = "{asset_names}";
}}
""",
    )
    _write_text(
        root,
        source_gate.CONTEXT_BUILDER_REL,
        """
void UWSNPCContextBuilder::BuildRequest()
{
    OutRequest.AvailableKnowledge;
    OutRequest.ForbiddenFactIds;
    OutRequest.ResponsePolicy.TopK;
}
""",
    )
    _write_text(
        root,
        source_gate.RESPONSE_VALIDATOR_REL,
        """
bool UWSRoleplayResponseValidator::Validate()
{
    return Request.AvailableKnowledge && Request.ForbiddenFactIds
        && Request.AllowedActionProposals;
}
bool UWSRoleplayResponseValidator::ValidateAndDeriveDisclosures()
{
    return true;
}
""",
    )
    _write_text(
        root,
        source_gate.STATE_TYPES_REL,
        """
int32 DialogueTurnIndex = 1;
int32 DialogueSessionMaxTurns = 3;
bool bDialogueSessionFollowUp = false;
bool bRoleplayV14 = false;
FWSRoleplayRequest RoleplayRequest;
""",
    )
    _write_text(
        root,
        source_gate.GATEWAY_REL,
        f"""
void UWSAgentGateway::LoadConfig()
{{
    const char* Runtime = "AgentRuntime.v1.4.json";
    const char* Protocol = "bounded_roleplay_v4";
    const char* Prompt = "subjective_context_single_call";
    bool Valid = SchemaVersion == 7.0;
}}

void UWSAgentGateway::RequestDialogueRealization()
{{
    BuildDialogueRealizationRequestJson();
    ValidateDialogueOutcomePayload();
    FRetryLimitCountSetting(0u);
    Request->ProcessRequest();
}}

FString UWSAgentGateway::BuildDialogueRealizationContextJson() const
{{
    if (Prepared.bRoleplayV14)
    {{
        return "role_profile subjective_state available_knowledge "
            "allowed_action_proposals long_term_memory recent_turns "
            "response_policy";
    }}
    return FString();
}}

FString UWSAgentGateway::BuildDialogueRealizationRequestJson() const
{{
    if (Prepared.bRoleplayV14)
    {{
        return "{output_fields}";
    }}
    return FString();
}}

bool UWSAgentGateway::ValidateDialogueOutcomePayload()
{{
    if (Prepared.bRoleplayV14)
    {{
        const char* Fields = "{output_fields}";
        return ValidateAndDeriveDisclosures();
    }}
    return false;
}}
""",
    )
    _write_text(
        root,
        source_gate.PLAYER_REL,
        """
void AWhiteoutCharacter::SubmitDialogueText()
{
    UWSNPCContextBuilder::BuildSemanticFrame();
    CommitDialogueChoice();
}
""",
    )
    _write_text(
        root,
        source_gate.STATE_SUBSYSTEM_REL,
        """
bool UWindStationStateSubsystem::NormalizeDialogueSessionRequest()
{
    InOutRequest.DialogueSessionMaxTurns = 3;
    InOutRequest.DialogueTurnIndex = Session->CommittedTurns + 1;
    InOutRequest.bDialogueSessionFollowUp = true;
    return DialogueSessionComplete;
}
FWSActionResult UWindStationStateSubsystem::PrepareDialogue()
{
    UWSNPCContextBuilder::BuildRequest();
    Prepared.bRoleplayV14 = true;
    return Prepared.RoleplayRequest;
}
void UWindStationStateSubsystem::RealizePreparedDialogue()
{
    AgentGateway->RequestDialogueRealization();
}
""",
    )
    _write_text(
        root,
        source_gate.BUILD_GUIDE_REL,
        """
AgentRuntime.v1.4.json bounded_roleplay_v4 validate_source_v14.py
validate_roleplay_content.py ws.DialogueDebug 1 WhiteoutStation_Autosave_v1_4
一轮模型请求，三轮会话，权威状态由本地规则提交。
""",
    )


def test_repository_v14_contract_passes() -> None:
    assert source_gate.validate_versions(source_gate.REPO_ROOT).passed


def test_v14_contract_accepts_complete_fixture(tmp_path: Path) -> None:
    _write_fixture(tmp_path)
    assert source_gate.validate_versions(tmp_path).passed


@pytest.mark.parametrize(
    ("field", "stale"),
    (
        ("runtime_version", "1.3.0"),
        ("schema_version", 6),
        ("protocol_version", "dialogue_epistemic_v3"),
        ("prompt_mode", "semantic_atoms_full_line"),
        ("max_sentences", 2),
        ("max_line_chars", 96),
        ("max_output_tokens", 256),
        ("temperature", 0.2),
        ("top_k_knowledge", 7),
        ("max_session_turns", 4),
        ("llm_enabled", True),
    ),
)
def test_v14_contract_rejects_runtime_drift(
    tmp_path: Path,
    field: str,
    stale: object,
) -> None:
    _write_fixture(tmp_path)
    runtime_path = tmp_path / source_gate.AGENT_RUNTIME_REL
    payload = json.loads(runtime_path.read_text(encoding="utf-8"))
    payload[field] = stale
    runtime_path.write_text(json.dumps(payload), encoding="utf-8")
    assert not source_gate.validate_versions(tmp_path).passed


def test_v14_contract_rejects_credential_fields(tmp_path: Path) -> None:
    _write_fixture(tmp_path)
    runtime_path = tmp_path / source_gate.AGENT_RUNTIME_REL
    payload = json.loads(runtime_path.read_text(encoding="utf-8"))
    payload["api_key"] = "fixture"
    runtime_path.write_text(json.dumps(payload), encoding="utf-8")
    assert not source_gate.validate_versions(tmp_path).passed


def test_v14_contract_requires_dialogue_non_ufs_staging(tmp_path: Path) -> None:
    _write_fixture(tmp_path)
    config_path = tmp_path / source_gate.PROJECT_CONFIG_REL
    config_path.write_text("ProjectVersion=1.4.0\n", encoding="utf-8")
    assert not source_gate.validate_versions(tmp_path).passed


def test_v14_contract_requires_exact_six_dialogue_json_files(
    tmp_path: Path,
) -> None:
    _write_fixture(tmp_path)
    missing = tmp_path / source_gate.DIALOGUE_DIR_REL / "SafeFallbacks.json"
    missing.unlink()
    assert not source_gate.validate_versions(tmp_path).passed

    _write_text(
        tmp_path,
        f"{source_gate.DIALOGUE_DIR_REL}/Unexpected.json",
        '{"schema_version": 1}',
    )
    assert not source_gate.validate_versions(tmp_path).passed


def test_v14_contract_rejects_dialogue_policy_drift(tmp_path: Path) -> None:
    _write_fixture(tmp_path)
    policy_path = (
        tmp_path / source_gate.DIALOGUE_DIR_REL / "DialoguePolicy.json"
    )
    payload = json.loads(policy_path.read_text(encoding="utf-8"))
    payload["top_k"] = 13
    policy_path.write_text(json.dumps(payload), encoding="utf-8")
    assert not source_gate.validate_versions(tmp_path).passed


def test_v14_submit_path_rejects_legacy_intent_call(tmp_path: Path) -> None:
    _write_fixture(tmp_path)
    player_path = tmp_path / source_gate.PLAYER_REL
    text = player_path.read_text(encoding="utf-8")
    player_path.write_text(
        text.replace(
            "CommitDialogueChoice();",
            "StateSubsystem->RequestDialogueIntent();\n    CommitDialogueChoice();",
        ),
        encoding="utf-8",
    )
    assert not source_gate.validate_versions(tmp_path).passed


def test_v14_gate_allows_retained_intent_outside_submit_path(
    tmp_path: Path,
) -> None:
    _write_fixture(tmp_path)
    player_path = tmp_path / source_gate.PLAYER_REL
    player_path.write_text(
        player_path.read_text(encoding="utf-8")
        + "\nvoid AWhiteoutCharacter::LegacyProbe()\n"
        "{ StateSubsystem->RequestDialogueIntent(); }\n",
        encoding="utf-8",
    )
    assert source_gate.validate_versions(tmp_path).passed


def test_v14_gate_rejects_second_realization_request(tmp_path: Path) -> None:
    _write_fixture(tmp_path)
    state_path = tmp_path / source_gate.STATE_SUBSYSTEM_REL
    text = state_path.read_text(encoding="utf-8")
    state_path.write_text(
        text.replace(
            "AgentGateway->RequestDialogueRealization();",
            "AgentGateway->RequestDialogueRealization();\n"
            "    AgentGateway->RequestDialogueRealization();",
        ),
        encoding="utf-8",
    )
    assert not source_gate.validate_versions(tmp_path).passed


def test_v14_gate_rejects_session_turn_limit_drift(tmp_path: Path) -> None:
    _write_fixture(tmp_path)
    state_path = tmp_path / source_gate.STATE_SUBSYSTEM_REL
    text = state_path.read_text(encoding="utf-8")
    state_path.write_text(
        text.replace("DialogueSessionMaxTurns = 3", "DialogueSessionMaxTurns = 4"),
        encoding="utf-8",
    )
    assert not source_gate.validate_versions(tmp_path).passed


def test_v14_worktree_ignores_user_construction_briefs(tmp_path: Path) -> None:
    subprocess.run(
        ["git", "init", "--initial-branch=main"],
        cwd=tmp_path,
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    for relative_path in source_gate.SOURCE_BRIEF_RELS:
        path = tmp_path / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"external fixture")
    report = source_gate.validate_worktree(tmp_path)
    assert report.passed
    assert len(report.warnings) == 1
