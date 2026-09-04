"""Fail-closed source gate for the Whiteout Station v1.4 dialogue release."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import Iterable

try:
    from .v11_gate_common import (
        GateError,
        GateReport,
        current_branch,
        git_status_entries,
        load_json_file,
        resolve_commit,
    )
    from .validate_source_v11 import (
        scan_tracked_secrets,
        validate_lfs,
        validate_protected_assets,
    )
    from .validate_source_v13 import _cpp_function, _strip_cpp_comments_and_literals
except ImportError:
    from v11_gate_common import (
        GateError,
        GateReport,
        current_branch,
        git_status_entries,
        load_json_file,
        resolve_commit,
    )
    from validate_source_v11 import (
        scan_tracked_secrets,
        validate_lfs,
        validate_protected_assets,
    )
    from validate_source_v13 import _cpp_function, _strip_cpp_comments_and_literals


REPO_ROOT = Path(__file__).resolve().parents[2]
PROJECT_VERSION = "1.4.0"
AGENT_RUNTIME_VERSION = "1.4.0"
AGENT_SCHEMA = 7
AGENT_PROTOCOL = "bounded_roleplay_v4"
AGENT_PROMPT_MODE = "subjective_context_single_call"

PROJECT_CONFIG_REL = "WhiteoutStation/Config/DefaultGame.ini"
AGENT_RUNTIME_REL = "WhiteoutStation/Content/Agents/AgentRuntime.v1.4.json"
DIALOGUE_DIR_REL = "WhiteoutStation/Content/Dialogue/v1.4"
PROTECTED_MANIFEST_REL = "docs/PROTECTED_CHARACTER_ASSETS_v1.0.json"
BUILD_GUIDE_REL = "docs/BUILD_AND_PLAY_v1.4.md"

DIALOGUE_ASSET_NAMES = frozenset(
    {
        "WorldKnowledge.json",
        "NPC_GuHeng.json",
        "NPC_YeCheng.json",
        "Relationship_GuHeng_YeCheng.json",
        "DialoguePolicy.json",
        "SafeFallbacks.json",
    }
)
SOURCE_BRIEF_RELS = frozenset(
    {
        "docs/风雪站_断电前夜_v1.2对话落地重构执行施工文档.docx",
        "docs/风雪站_断电前夜_v1.3知识边界与自然对话重构执行施工文档.docx",
        "docs/风雪站_断电前夜_v1.4认知约束型角色扮演对话重构方案.docx",
    }
)

GATEWAY_REL = (
    "WhiteoutStation/Source/WhiteoutStation/Private/Agents/WSAgentGateway.cpp"
)
PLAYER_REL = (
    "WhiteoutStation/Source/WhiteoutStation/Private/Player/WhiteoutCharacter.cpp"
)
STATE_SUBSYSTEM_REL = (
    "WhiteoutStation/Source/WhiteoutStation/Private/State/"
    "WindStationStateSubsystem.cpp"
)
ROLEPLAY_TYPES_REL = (
    "WhiteoutStation/Source/WhiteoutStation/Public/Agents/WSRoleplayTypes.h"
)
KNOWLEDGE_REPOSITORY_REL = (
    "WhiteoutStation/Source/WhiteoutStation/Private/Agents/"
    "WSRoleplayKnowledgeRepository.cpp"
)
CONTEXT_BUILDER_REL = (
    "WhiteoutStation/Source/WhiteoutStation/Private/Agents/"
    "WSNPCContextBuilder.cpp"
)
RESPONSE_VALIDATOR_REL = (
    "WhiteoutStation/Source/WhiteoutStation/Private/Agents/"
    "WSRoleplayResponseValidator.cpp"
)
STATE_TYPES_REL = (
    "WhiteoutStation/Source/WhiteoutStation/Public/State/WindStationTypes.h"
)

OUTPUT_FIELDS = (
    "npc_line",
    "speech_function",
    "referenced_knowledge_ids",
    "assertions",
    "proposed_action",
    "memory_summary",
    "emotion",
    "movement_intent",
    "reaction_action",
)


def _read_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8-sig")
    except (OSError, UnicodeDecodeError) as exc:
        raise GateError(f"Cannot read UTF-8 text {path}: {exc}") from exc


def _credential_keys(value: object, prefix: str = "") -> Iterable[str]:
    forbidden = {
        "api_key",
        "apikey",
        "authorization",
        "password",
        "secret",
        "secret_key",
        "token",
    }
    if isinstance(value, dict):
        for key, child in value.items():
            key_text = str(key)
            path = f"{prefix}.{key_text}" if prefix else key_text
            if key_text.casefold() in forbidden:
                yield path
            yield from _credential_keys(child, path)
    elif isinstance(value, list):
        for index, child in enumerate(value):
            yield from _credential_keys(child, f"{prefix}[{index}]")


def _cpp_call_count(text: str, function_name: str) -> int:
    lexical = _strip_cpp_comments_and_literals(text)
    return len(re.findall(rf"\b{re.escape(function_name)}\s*\(", lexical))


def validate_runtime_contract(repo_root: Path) -> GateReport:
    report = GateReport()
    try:
        runtime = load_json_file(repo_root / AGENT_RUNTIME_REL)
    except GateError as exc:
        report.error(str(exc))
        return report
    if not isinstance(runtime, dict):
        report.error(f"{AGENT_RUNTIME_REL} root must be an object")
        return report

    expected = {
        "runtime_version": AGENT_RUNTIME_VERSION,
        "schema_version": AGENT_SCHEMA,
        "protocol_version": AGENT_PROTOCOL,
        "prompt_mode": AGENT_PROMPT_MODE,
        "max_sentences": 3,
        "max_line_chars": 120,
        "max_output_tokens": 320,
        "temperature": 0.5,
        "top_k_knowledge": 10,
        "max_session_turns": 3,
        "transport": "openai_chat_completions",
        "llm_enabled": False,
    }
    for key, value in expected.items():
        if runtime.get(key) != value:
            report.error(f"{AGENT_RUNTIME_REL} {key} must be {value!r}")
    if runtime.get("endpoint") != "https://api.deepseek.com":
        report.error(
            f"{AGENT_RUNTIME_REL} endpoint must use the official DeepSeek BaseURL"
        )
    for key_path in _credential_keys(runtime):
        report.error(
            f"{AGENT_RUNTIME_REL} contains forbidden credential field {key_path}"
        )
    return report


def validate_project_config(repo_root: Path) -> GateReport:
    report = GateReport()
    try:
        config = _read_text(repo_root / PROJECT_CONFIG_REL)
    except GateError as exc:
        report.error(str(exc))
        return report

    versions = re.findall(
        r"(?m)^\s*ProjectVersion\s*=\s*([^\r\n]+?)\s*$",
        config,
    )
    if versions != [PROJECT_VERSION]:
        report.error(
            f"{PROJECT_CONFIG_REL} must contain exactly "
            f"ProjectVersion={PROJECT_VERSION}; found {versions!r}"
        )
    staging_entries = re.findall(
        r'(?m)^\s*\+DirectoriesToAlwaysStageAsNonUFS='
        r'\(Path="([^"]+)"\)\s*$',
        config,
    )
    if staging_entries.count("Dialogue") != 1:
        report.error(
            f'{PROJECT_CONFIG_REL} must stage Dialogue exactly once as NonUFS'
        )
    return report


def validate_dialogue_assets(repo_root: Path) -> GateReport:
    report = GateReport()
    asset_dir = repo_root / DIALOGUE_DIR_REL
    try:
        found = {path.name for path in asset_dir.iterdir() if path.suffix == ".json"}
    except OSError as exc:
        report.error(f"Cannot inspect {DIALOGUE_DIR_REL}: {exc}")
        return report
    if found != DIALOGUE_ASSET_NAMES:
        missing = sorted(DIALOGUE_ASSET_NAMES - found)
        unexpected = sorted(found - DIALOGUE_ASSET_NAMES)
        if missing:
            report.error(
                f"{DIALOGUE_DIR_REL} is missing required JSON: {', '.join(missing)}"
            )
        if unexpected:
            report.error(
                f"{DIALOGUE_DIR_REL} contains unexpected JSON: "
                + ", ".join(unexpected)
            )

    loaded: dict[str, dict[str, object]] = {}
    for name in sorted(DIALOGUE_ASSET_NAMES):
        try:
            payload = load_json_file(asset_dir / name)
        except GateError as exc:
            report.error(str(exc))
            continue
        if not isinstance(payload, dict):
            report.error(f"{DIALOGUE_DIR_REL}/{name} root must be an object")
            continue
        if payload.get("schema_version") != 1:
            report.error(f"{DIALOGUE_DIR_REL}/{name} schema_version must be 1")
        loaded[name] = payload
        for key_path in _credential_keys(payload):
            report.error(
                f"{DIALOGUE_DIR_REL}/{name} contains forbidden credential "
                f"field {key_path}"
            )

    required_roots = {
        "WorldKnowledge.json": ("knowledge",),
        "NPC_GuHeng.json": ("profile", "knowledge"),
        "NPC_YeCheng.json": ("profile", "knowledge"),
        "Relationship_GuHeng_YeCheng.json": ("knowledge",),
        "DialoguePolicy.json": ("allowed_speech_functions", "allowed_proposal_types"),
        "SafeFallbacks.json": ("fallbacks",),
    }
    for name, fields in required_roots.items():
        payload = loaded.get(name)
        if payload is None:
            continue
        for field in fields:
            if field not in payload:
                report.error(f"{DIALOGUE_DIR_REL}/{name} must contain {field}")

    policy = loaded.get("DialoguePolicy.json")
    if policy is not None:
        expected_policy = {
            "top_k": 10,
            "max_sentences": 3,
            "max_characters": 120,
            "max_output_tokens": 320,
            "temperature": 0.5,
        }
        for key, value in expected_policy.items():
            if policy.get(key) != value:
                report.error(
                    f"{DIALOGUE_DIR_REL}/DialoguePolicy.json {key} "
                    f"must be {value!r}"
                )
    return report


def _require_tokens(
    report: GateReport,
    relative_path: str,
    text: str,
    tokens: Iterable[str],
) -> None:
    for token in tokens:
        if token not in text:
            report.error(f"{relative_path} must reference {token}")


def validate_core_sources(repo_root: Path) -> GateReport:
    report = GateReport()
    sources: dict[str, str] = {}
    for relative_path in (
        GATEWAY_REL,
        PLAYER_REL,
        STATE_SUBSYSTEM_REL,
        ROLEPLAY_TYPES_REL,
        KNOWLEDGE_REPOSITORY_REL,
        CONTEXT_BUILDER_REL,
        RESPONSE_VALIDATOR_REL,
        STATE_TYPES_REL,
    ):
        try:
            sources[relative_path] = _read_text(repo_root / relative_path)
        except GateError as exc:
            report.error(str(exc))

    source_contracts = {
        ROLEPLAY_TYPES_REL: (
            "EWSEpistemicStatus",
            "EWSRoleplayDisclosureLevel",
            "FWSRoleplayKnowledgeItem",
            "FWSRoleplaySubjectiveState",
            "FWSRoleplayActionProposal",
            "FWSRoleplayMemoryEntry",
            "FWSRoleplayRequest",
            "FWSRoleplayResponse",
            "TurnIndex = 1",
            "RemainingTurns = 2",
        ),
        KNOWLEDGE_REPOSITORY_REL: (
            "UWSRoleplayKnowledgeRepository::LoadDefault",
            "Dialogue/v1.4",
            *DIALOGUE_ASSET_NAMES,
        ),
        CONTEXT_BUILDER_REL: (
            "UWSNPCContextBuilder::BuildRequest",
            "AvailableKnowledge",
            "ForbiddenFactIds",
            "ResponsePolicy.TopK",
        ),
        RESPONSE_VALIDATOR_REL: (
            "UWSRoleplayResponseValidator::Validate",
            "UWSRoleplayResponseValidator::ValidateAndDeriveDisclosures",
            "AvailableKnowledge",
            "ForbiddenFactIds",
            "AllowedActionProposals",
        ),
        STATE_TYPES_REL: (
            "DialogueTurnIndex = 1",
            "DialogueSessionMaxTurns = 3",
            "bDialogueSessionFollowUp = false",
            "bRoleplayV14 = false",
            "FWSRoleplayRequest RoleplayRequest",
        ),
    }
    for relative_path, tokens in source_contracts.items():
        text = sources.get(relative_path)
        if text is not None:
            _require_tokens(report, relative_path, text, tokens)

    gateway = sources.get(GATEWAY_REL)
    if gateway is not None:
        gateway_functions = {
            "UWSAgentGateway::LoadConfig": (
                "AgentRuntime.v1.4.json",
                AGENT_PROTOCOL,
                AGENT_PROMPT_MODE,
                "SchemaVersion == 7.0",
            ),
            "UWSAgentGateway::BuildDialogueRealizationContextJson": (
                "Prepared.bRoleplayV14",
                "role_profile",
                "subjective_state",
                "available_knowledge",
                "allowed_action_proposals",
                "long_term_memory",
                "recent_turns",
                "response_policy",
            ),
            "UWSAgentGateway::BuildDialogueRealizationRequestJson": (
                "Prepared.bRoleplayV14",
                *OUTPUT_FIELDS,
            ),
            "UWSAgentGateway::ValidateDialogueOutcomePayload": (
                "Prepared.bRoleplayV14",
                "ValidateAndDeriveDisclosures",
                *OUTPUT_FIELDS,
            ),
        }
        for function_name, tokens in gateway_functions.items():
            function = _cpp_function(gateway, function_name)
            if not function:
                report.error(f"{GATEWAY_REL} is missing {function_name}")
                continue
            _require_tokens(report, GATEWAY_REL, function, tokens)

        request_function = _cpp_function(
            gateway, "UWSAgentGateway::RequestDialogueRealization"
        )
        if not request_function:
            report.error(
                f"{GATEWAY_REL} is missing "
                "UWSAgentGateway::RequestDialogueRealization"
            )
        else:
            required_call_counts = {
                "BuildDialogueRealizationRequestJson": 1,
                "ValidateDialogueOutcomePayload": 1,
                "ProcessRequest": 1,
            }
            for method_name, expected_count in required_call_counts.items():
                count = _cpp_call_count(request_function, method_name)
                if count != expected_count:
                    report.error(
                        f"{GATEWAY_REL} RequestDialogueRealization must call "
                        f"{method_name} exactly once; found {count}"
                    )
            if "FRetryLimitCountSetting(0u)" not in request_function:
                report.error(
                    f"{GATEWAY_REL} RequestDialogueRealization must disable "
                    "transport retries for the one-call contract"
                )

    player = sources.get(PLAYER_REL)
    if player is not None:
        submit = _cpp_function(player, "AWhiteoutCharacter::SubmitDialogueText")
        if not submit:
            report.error(f"{PLAYER_REL} is missing AWhiteoutCharacter::SubmitDialogueText")
        else:
            _require_tokens(
                report,
                PLAYER_REL,
                submit,
                ("BuildSemanticFrame(", "CommitDialogueChoice("),
            )
            intent_calls = _cpp_call_count(submit, "RequestDialogueIntent")
            if intent_calls:
                report.error(
                    f"{PLAYER_REL} SubmitDialogueText contains {intent_calls} "
                    "legacy RequestDialogueIntent call(s)"
                )
            commit_calls = _cpp_call_count(submit, "CommitDialogueChoice")
            if commit_calls != 1:
                report.error(
                    f"{PLAYER_REL} SubmitDialogueText must dispatch one dialogue "
                    f"commit; found {commit_calls}"
                )

    state = sources.get(STATE_SUBSYSTEM_REL)
    if state is not None:
        state_functions = {
            "UWindStationStateSubsystem::NormalizeDialogueSessionRequest": (
                "DialogueSessionMaxTurns = 3",
                "DialogueTurnIndex = Session->CommittedTurns + 1",
                "bDialogueSessionFollowUp = true",
                "DialogueSessionComplete",
            ),
            "UWindStationStateSubsystem::PrepareDialogue": (
                "UWSNPCContextBuilder::BuildRequest",
                "Prepared.bRoleplayV14 = true",
                "Prepared.RoleplayRequest",
            ),
        }
        for function_name, tokens in state_functions.items():
            function = _cpp_function(state, function_name)
            if not function:
                report.error(f"{STATE_SUBSYSTEM_REL} is missing {function_name}")
                continue
            _require_tokens(report, STATE_SUBSYSTEM_REL, function, tokens)

        realize = _cpp_function(
            state, "UWindStationStateSubsystem::RealizePreparedDialogue"
        )
        if not realize:
            report.error(
                f"{STATE_SUBSYSTEM_REL} is missing "
                "UWindStationStateSubsystem::RealizePreparedDialogue"
            )
        else:
            calls = _cpp_call_count(realize, "RequestDialogueRealization")
            if calls != 1:
                report.error(
                    f"{STATE_SUBSYSTEM_REL} RealizePreparedDialogue must make "
                    f"one realization request; found {calls}"
                )
    return report


def validate_documentation(repo_root: Path) -> GateReport:
    report = GateReport()
    try:
        guide = _read_text(repo_root / BUILD_GUIDE_REL)
    except GateError as exc:
        report.error(str(exc))
        return report
    _require_tokens(
        report,
        BUILD_GUIDE_REL,
        guide,
        (
            "AgentRuntime.v1.4.json",
            "bounded_roleplay_v4",
            "validate_source_v14.py",
            "validate_roleplay_content.py",
            "ws.DialogueDebug 1",
            "WhiteoutStation_Autosave_v1_4",
            "一轮",
            "三轮",
            "本地规则",
        ),
    )
    return report


def validate_versions(repo_root: Path) -> GateReport:
    report = GateReport()
    report.merge(validate_project_config(repo_root))
    report.merge(validate_runtime_contract(repo_root))
    report.merge(validate_dialogue_assets(repo_root))
    report.merge(validate_core_sources(repo_root))
    report.merge(validate_documentation(repo_root))
    if not report.errors:
        report.detail(
            "v1.4 contract is consistent: project/runtime 1.4.0, schema 7, "
            "bounded_roleplay_v4, six staged dialogue assets, one model call, "
            "and three-turn sessions"
        )
    return report


def validate_worktree(repo_root: Path) -> GateReport:
    report = GateReport()
    try:
        branch = current_branch(repo_root)
        entries = git_status_entries(repo_root)
    except GateError as exc:
        report.error(str(exc))
        return report
    if branch != "main":
        report.error(f"v1.4 release source must be on main; found {branch}")

    ignored_briefs = {
        ("??", relative_path)
        for relative_path in SOURCE_BRIEF_RELS
        if ("??", relative_path) in entries
    }
    remaining = [entry for entry in entries if entry not in ignored_briefs]
    if ignored_briefs:
        report.warn(
            "Ignoring user-provided untracked construction brief(s); external "
            "inputs are not release source: "
            + ", ".join(sorted(path for _status, path in ignored_briefs))
        )
    if remaining:
        report.error(
            "v1.4 release requires a clean main worktree and index: "
            + ", ".join(f"{status} {path}" for status, path in remaining)
        )
    elif branch == "main":
        report.detail("Source is on main with no release-source changes pending")
    return report


def validate_source(
    repo_root: Path,
    manifest_path: Path | None = None,
    *,
    check_lfs: bool = True,
) -> GateReport:
    report = GateReport()
    try:
        repo_root = repo_root.resolve(strict=True)
        resolve_commit(repo_root, "HEAD")
    except (OSError, GateError) as exc:
        report.error(f"Invalid Git repository root: {exc}")
        return report
    report.merge(validate_worktree(repo_root))
    report.merge(validate_versions(repo_root))
    report.merge(scan_tracked_secrets(repo_root))
    if check_lfs:
        report.merge(validate_lfs(repo_root))
    report.merge(
        validate_protected_assets(
            repo_root,
            manifest_path,
            require_tracked=True,
        )
    )
    return report


def print_report(report: GateReport) -> None:
    for detail in report.details:
        print(f"DETAIL: {detail}")
    for warning in report.warnings:
        print(f"WARNING: {warning}")
    for error in report.errors:
        print(f"ERROR: {error}")
    print(
        "SOURCE GATE v1.4: PASS"
        if report.passed
        else f"SOURCE GATE v1.4: FAIL ({len(report.errors)} error(s))"
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=REPO_ROOT)
    parser.add_argument("--protected-manifest", type=Path)
    parser.add_argument("--final", action="store_true")
    parser.add_argument("--skip-lfs", action="store_true")
    parser.add_argument(
        "--contract-only",
        action="store_true",
        help="Validate the v1.4 contract without Git, LFS, secret, or asset gates.",
    )
    args = parser.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")
    manifest_path = args.protected_manifest
    if manifest_path is not None and not manifest_path.is_absolute():
        manifest_path = args.repo_root / manifest_path
    report = (
        validate_versions(args.repo_root.resolve())
        if args.contract_only
        else validate_source(
            args.repo_root,
            manifest_path,
            check_lfs=not args.skip_lfs,
        )
    )
    print_report(report)
    return 0 if report.passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
