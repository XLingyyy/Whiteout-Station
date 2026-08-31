"""Fail-closed source gate for the Whiteout Station v1.3 dialogue release."""

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


REPO_ROOT = Path(__file__).resolve().parents[2]
PROJECT_VERSION = "1.3.0"
RULES_VERSION = "1.1.0"
RULES_SCHEMA = 6
AGENT_RUNTIME_VERSION = "1.3.0"
AGENT_SCHEMA = 6
AGENT_PROTOCOL = "dialogue_epistemic_v3"
AGENT_PROMPT_MODE = "semantic_atoms_full_line"
PROJECT_CONFIG_REL = "WhiteoutStation/Config/DefaultGame.ini"
RULES_REL = "WhiteoutStation/Content/Rules/WhiteoutStationRules.v1.1.json"
AGENT_RUNTIME_REL = "WhiteoutStation/Content/Agents/AgentRuntime.v1.3.json"
PROTECTED_MANIFEST_REL = "docs/PROTECTED_CHARACTER_ASSETS_v1.0.json"
SOURCE_BRIEF_RELS = frozenset(
    {
        "docs/风雪站_断电前夜_v1.2对话落地重构执行施工文档.docx",
        "docs/风雪站_断电前夜_v1.3知识边界与自然对话重构执行施工文档.docx",
    }
)

GATEWAY_REL = (
    "WhiteoutStation/Source/WhiteoutStation/Private/Agents/WSAgentGateway.cpp"
)
DECISION_SERVICE_REL = (
    "WhiteoutStation/Source/WhiteoutStation/Private/Agents/"
    "WSNPCDecisionService.cpp"
)
STATE_SUBSYSTEM_REL = (
    "WhiteoutStation/Source/WhiteoutStation/Private/State/"
    "WindStationStateSubsystem.cpp"
)
GAME_MODE_REL = (
    "WhiteoutStation/Source/WhiteoutStation/Private/Flow/"
    "WhiteoutGameMode.cpp"
)
SAVE_HEADER_REL = (
    "WhiteoutStation/Source/WhiteoutStation/Public/Save/WindStationSaveGame.h"
)

OUTPUT_FIELDS = (
    "npc_line",
    "realized_atom_ids",
    "disclosed_fact_ids",
    "emotion",
    "movement_intent",
    "reaction_action",
)
MOCK_MODES = (
    "valid_natural",
    "missing_atom",
    "forbidden_fact",
    "added_condition",
    "system_jargon",
    "invalid_json",
    "timeout",
)
V2_PRODUCTION_TOKENS = (
    "AgentRuntime.v1.2.json",
    "dialogue_grounding_v2",
    "semantic_spine_plus_persona_tail",
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


def _cpp_function(text: str, qualified_name: str) -> str:
    """Return one C++ function definition, including its signature and body."""

    lexical = _strip_cpp_comments_and_literals(text)
    definition = re.search(
        rf"(?m)^[^\r\n{{}};]*\b{re.escape(qualified_name)}\s*\(",
        lexical,
    )
    if definition is None:
        return ""
    position = definition.start()
    body_start = lexical.find("{", definition.end())
    if body_start < 0:
        return ""
    depth = 0
    for index in range(body_start, len(lexical)):
        if lexical[index] == "{":
            depth += 1
        elif lexical[index] == "}":
            depth -= 1
            if depth == 0:
                return text[position : index + 1]
    return ""


def _strip_cpp_comments_and_literals(text: str) -> str:
    """Blank comments and literals while preserving offsets and newlines."""

    output = list(text)
    index = 0
    state = "code"
    quote = ""
    while index < len(text):
        current = text[index]
        following = text[index + 1] if index + 1 < len(text) else ""
        if state == "code":
            if current == "/" and following == "/":
                output[index] = output[index + 1] = " "
                state = "line_comment"
                index += 2
                continue
            if current == "/" and following == "*":
                output[index] = output[index + 1] = " "
                state = "block_comment"
                index += 2
                continue
            if current in {'"', "'"}:
                quote = current
                output[index] = " "
                state = "literal"
                index += 1
                continue
        elif state == "line_comment":
            if current in "\r\n":
                state = "code"
            else:
                output[index] = " "
        elif state == "block_comment":
            if current == "*" and following == "/":
                output[index] = output[index + 1] = " "
                state = "code"
                index += 2
                continue
            if current not in "\r\n":
                output[index] = " "
        else:
            if current == "\\" and following:
                output[index] = " "
                if following not in "\r\n":
                    output[index + 1] = " "
                index += 2
                continue
            if current == quote:
                output[index] = " "
                state = "code"
            elif current not in "\r\n":
                output[index] = " "
        index += 1
    return "".join(output)


def _cpp_method_call_count(text: str, method_name: str) -> int:
    lexical = _strip_cpp_comments_and_literals(text)
    return len(
        re.findall(
            rf"(?:->|\.)\s*{re.escape(method_name)}\s*\(",
            lexical,
        )
    )


def validate_runtime_contract(repo_root: Path) -> GateReport:
    report = GateReport()
    try:
        agent = load_json_file(repo_root / AGENT_RUNTIME_REL)
    except GateError as exc:
        report.error(str(exc))
        return report
    if not isinstance(agent, dict):
        report.error(f"{AGENT_RUNTIME_REL} root must be an object")
        return report

    expected = {
        "runtime_version": AGENT_RUNTIME_VERSION,
        "schema_version": AGENT_SCHEMA,
        "protocol_version": AGENT_PROTOCOL,
        "prompt_mode": AGENT_PROMPT_MODE,
        "max_sentences": 2,
        "max_line_chars": 96,
        "max_output_tokens": 256,
        "transport": "openai_chat_completions",
        "llm_enabled": False,
    }
    for key, value in expected.items():
        if agent.get(key) != value:
            report.error(f"{AGENT_RUNTIME_REL} {key} must be {value!r}")
    if agent.get("endpoint") != "https://api.deepseek.com":
        report.error(
            f"{AGENT_RUNTIME_REL} endpoint must use the official DeepSeek BaseURL"
        )
    for key_path in _credential_keys(agent):
        report.error(
            f"{AGENT_RUNTIME_REL} contains forbidden credential field {key_path}"
        )
    return report


def validate_shipping_dialogue_path(repo_root: Path) -> GateReport:
    """Prove the packaged entry path selects v3, not the retained v1.2 protocol."""

    report = GateReport()
    try:
        gateway = _read_text(repo_root / GATEWAY_REL)
    except GateError as exc:
        report.error(str(exc))
        return report

    function_contracts = {
        "UWSAgentGateway::LoadConfig": (
            "AgentRuntime.v1.3.json",
            AGENT_PROTOCOL,
            AGENT_PROMPT_MODE,
        ),
        "UWSAgentGateway::RequestDialogueRealization": (
            "BuildDialogueRealizationRequestJson(",
            "ValidateDialogueOutcomePayload(",
        ),
        "UWSAgentGateway::BuildDialogueRealizationContextJson": (
            AGENT_PROTOCOL,
            AGENT_PROMPT_MODE,
            "must_realize",
            "may_realize",
        ),
        "UWSAgentGateway::BuildDialogueRealizationRequestJson": (
            "BuildDialogueRealizationContextJson(",
            *OUTPUT_FIELDS,
        ),
        "UWSAgentGateway::ValidateDialogueOutcomePayload": OUTPUT_FIELDS,
    }
    extracted: dict[str, str] = {}
    for function_name, required_tokens in function_contracts.items():
        function = _cpp_function(gateway, function_name)
        if not function:
            report.error(
                f"{GATEWAY_REL} is missing required Shipping function "
                f"{function_name}"
            )
            continue
        extracted[function_name] = function
        for token in required_tokens:
            if token not in function:
                report.error(
                    f"{GATEWAY_REL} {function_name} must reference {token}"
                )

    shipping_surface = "\n".join(extracted.values())
    for token in V2_PRODUCTION_TOKENS:
        if token in shipping_surface:
            report.error(
                f"{GATEWAY_REL} Shipping dialogue path still references v1.2 token "
                f"{token}"
            )

    production_contracts = (
        (
            STATE_SUBSYSTEM_REL,
            "UWindStationStateSubsystem::RealizePreparedDialogue",
            1,
        ),
        (GAME_MODE_REL, "AWhiteoutGameMode::BeginPlay", 1),
        (
            GAME_MODE_REL,
            "AWhiteoutGameMode::RunDialogueHistoryProbeStep",
            1,
        ),
    )
    loaded_sources: dict[str, str] = {}
    for relative_path, function_name, minimum_v3_calls in production_contracts:
        if relative_path not in loaded_sources:
            try:
                loaded_sources[relative_path] = _read_text(
                    repo_root / relative_path
                )
            except GateError as exc:
                report.error(str(exc))
                continue
        function = _cpp_function(loaded_sources[relative_path], function_name)
        if not function:
            report.error(
                f"{relative_path} is missing required Shipping function "
                f"{function_name}"
            )
            continue
        v3_calls = _cpp_method_call_count(function, "RequestDialogueRealization")
        if v3_calls < minimum_v3_calls:
            report.error(
                f"{relative_path} {function_name} must call "
                "RequestDialogueRealization"
            )

    for relative_path in (STATE_SUBSYSTEM_REL, GAME_MODE_REL):
        source = loaded_sources.get(relative_path)
        if source is None:
            continue
        legacy_calls = _cpp_method_call_count(source, "RequestExpression")
        if legacy_calls:
            report.error(
                f"{relative_path} contains {legacy_calls} Shipping-reachable "
                "RequestExpression call(s); use RequestDialogueRealization"
            )
    game_mode = loaded_sources.get(GAME_MODE_REL)
    if game_mode is not None:
        v3_probe_calls = _cpp_method_call_count(
            game_mode,
            "RequestDialogueRealization",
        )
        if v3_probe_calls < 2:
            report.error(
                f"{GAME_MODE_REL} must contain both v3 Shipping probe calls; "
                f"found {v3_probe_calls}"
            )

    tool_contracts = {
        "Tools/Agents/agent_contract.py": (
            AGENT_PROTOCOL,
            AGENT_PROMPT_MODE,
            *OUTPUT_FIELDS,
        ),
        "Tools/Agents/mock_server.py": MOCK_MODES,
        "Tools/Agents/probe_deepseek.py": (
            "PROBE_MUST_ATOMS",
            "build_request_payload(",
            "validate_completion(",
        ),
    }
    for relative_path, tokens in tool_contracts.items():
        try:
            text = _read_text(repo_root / relative_path)
        except GateError as exc:
            report.error(str(exc))
            continue
        for token in tokens:
            if token not in text:
                report.error(f"{relative_path} must reference {token}")
    return report


def validate_versions(repo_root: Path) -> GateReport:
    report = GateReport()
    try:
        config = _read_text(repo_root / PROJECT_CONFIG_REL)
        versions = re.findall(
            r"(?m)^\s*ProjectVersion\s*=\s*([^\r\n]+?)\s*$",
            config,
        )
        if versions != [PROJECT_VERSION]:
            report.error(
                f"{PROJECT_CONFIG_REL} must contain exactly "
                f"ProjectVersion={PROJECT_VERSION}; found {versions!r}"
            )
    except GateError as exc:
        report.error(str(exc))

    try:
        rules = load_json_file(repo_root / RULES_REL)
        if not isinstance(rules, dict):
            report.error(f"{RULES_REL} root must be an object")
        else:
            if rules.get("rules_version") != RULES_VERSION:
                report.error(f"{RULES_REL} rules_version must remain {RULES_VERSION}")
            if rules.get("schema_version") != RULES_SCHEMA:
                report.error(f"{RULES_REL} schema_version must remain {RULES_SCHEMA}")
    except GateError as exc:
        report.error(str(exc))

    report.merge(validate_runtime_contract(repo_root))
    report.merge(validate_shipping_dialogue_path(repo_root))

    required_text = {
        "README.md": ("当前版本为 v1.3", "docs/BUILD_AND_PLAY_v1.3.md"),
        "docs/BUILD_AND_PLAY_v1.3.md": (
            "AgentRuntime.v1.3.json",
            "ws.DialogueDebug 1",
            "WhiteoutStation_DialogueAudit.jsonl",
        ),
        DECISION_SERVICE_REL: (
            "PLAYER_ASSISTANCE_NEEDED",
            "GU_HENG_NEEDS_RECOVERY",
            "REPAIR_ROOM_SHOULD_BE_WARM",
            "BACKUP_POWER_DECLINING",
            "BLIZZARD_WINDOW_SHRINKING",
            "HEAT_PRIORITY_DECISION",
            "HAND_INJURY_AFFECTS_FINE_WORK",
            "HEAT_PACK_AVAILABLE",
        ),
        STATE_SUBSYSTEM_REL: (
            "WhiteoutStation_Autosave_v1_3",
            "WhiteoutStation_Autosave_v1_2",
            "WhiteoutStation_DialogueAudit.jsonl",
        ),
        SAVE_HEADER_REL: ('TEXT("1.3.0")',),
    }
    for relative_path, tokens in required_text.items():
        try:
            text = _read_text(repo_root / relative_path)
        except GateError as exc:
            report.error(str(exc))
            continue
        for token in tokens:
            if token not in text:
                report.error(f"{relative_path} must reference {token}")

    if not report.errors:
        report.detail(
            "Version contract is consistent: project/runtime 1.3.0, runtime "
            "schema 6, dialogue_epistemic_v3, rules 1.1.0/schema 6"
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
        report.error(f"v1.3 release source must be on main; found {branch}")

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
            "v1.3 release requires a clean main worktree and index: "
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
        "SOURCE GATE v1.3: PASS"
        if report.passed
        else f"SOURCE GATE v1.3: FAIL ({len(report.errors)} error(s))"
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=REPO_ROOT)
    parser.add_argument("--protected-manifest", type=Path)
    parser.add_argument("--final", action="store_true")
    parser.add_argument("--skip-lfs", action="store_true")
    args = parser.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")
    manifest_path = args.protected_manifest
    if manifest_path is not None and not manifest_path.is_absolute():
        manifest_path = args.repo_root / manifest_path
    report = validate_source(
        args.repo_root,
        manifest_path,
        check_lfs=not args.skip_lfs,
    )
    print_report(report)
    return 0 if report.passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
