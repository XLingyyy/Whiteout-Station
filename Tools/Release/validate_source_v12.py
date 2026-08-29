"""Fail-closed source gate for the Whiteout Station v1.2 dialogue release."""

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
PROJECT_VERSION = "1.2.0"
RULES_VERSION = "1.1.0"
RULES_SCHEMA = 4
AGENT_RUNTIME_VERSION = "1.2.0"
AGENT_SCHEMA = 5
PROJECT_CONFIG_REL = "WhiteoutStation/Config/DefaultGame.ini"
RULES_REL = "WhiteoutStation/Content/Rules/WhiteoutStationRules.v1.1.json"
AGENT_RUNTIME_REL = "WhiteoutStation/Content/Agents/AgentRuntime.v1.2.json"
PROTECTED_MANIFEST_REL = "docs/PROTECTED_CHARACTER_ASSETS_v1.0.json"
SOURCE_BRIEF_REL = "docs/风雪站_断电前夜_v1.2对话落地重构执行施工文档.docx"


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

    try:
        agent = load_json_file(repo_root / AGENT_RUNTIME_REL)
        if not isinstance(agent, dict):
            report.error(f"{AGENT_RUNTIME_REL} root must be an object")
        else:
            expected = {
                "runtime_version": AGENT_RUNTIME_VERSION,
                "schema_version": AGENT_SCHEMA,
                "protocol_version": "dialogue_grounding_v2",
                "prompt_mode": "semantic_spine_plus_persona_tail",
                "max_tail_chars": 48,
                "max_tail_tokens": 128,
                "transport": "openai_chat_completions",
                "llm_enabled": False,
            }
            for key, value in expected.items():
                if agent.get(key) != value:
                    report.error(f"{AGENT_RUNTIME_REL} {key} must be {value!r}")
            endpoint = agent.get("endpoint")
            if not isinstance(endpoint, str) or not endpoint.startswith("https://"):
                report.error(f"{AGENT_RUNTIME_REL} endpoint must default to HTTPS")
            for key_path in _credential_keys(agent):
                report.error(
                    f"{AGENT_RUNTIME_REL} contains forbidden credential field "
                    f"{key_path}"
                )
    except GateError as exc:
        report.error(str(exc))

    required_text = {
        "README.md": ("当前版本为 v1.2", "docs/BUILD_AND_PLAY_v1.2.md"),
        "docs/BUILD_AND_PLAY_v1.2.md": (
            "AgentRuntime.v1.2.json",
            "Whiteout.DialogueDebug 1",
            "WhiteoutStation_OfferAudit.jsonl",
        ),
        "WhiteoutStation/Source/WhiteoutStation/Private/Agents/WSAgentGateway.cpp": (
            "AgentRuntime.v1.2.json",
            "semantic_spine",
            "persona_tail",
        ),
        "WhiteoutStation/Source/WhiteoutStation/Private/State/"
        "WindStationStateSubsystem.cpp": (
            "WhiteoutStation_Autosave_v1_2",
            "WhiteoutStation_Autosave_v1_1",
            "WhiteoutStation_OfferAudit.jsonl",
        ),
        "WhiteoutStation/Source/WhiteoutStation/Public/Save/WindStationSaveGame.h": (
            'TEXT("1.2.0")',
        ),
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
            "Version contract is consistent: project 1.2.0, rules 1.1.0/schema 4, "
            "dialogue runtime 1.2.0/schema 5"
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
        report.error(f"v1.2 release source must be on main; found {branch}")
    external_brief = ("??", SOURCE_BRIEF_REL)
    remaining = [entry for entry in entries if entry != external_brief]
    if external_brief in entries:
        report.warn(
            "Ignoring the user-provided untracked v1.2 construction brief; it is "
            "an external input and is not part of the release source."
        )
    if remaining:
        report.error(
            "v1.2 release requires a clean main worktree and index: "
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
        "SOURCE GATE v1.2: PASS"
        if report.passed
        else f"SOURCE GATE v1.2: FAIL ({len(report.errors)} error(s))"
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
