"""Fail-closed source gate for the Whiteout Station v0.5 release candidate.

This gate validates the committed/runtime version contract, tracked credentials,
Git LFS integrity, and the protected character asset Git objects.  It reports
the user's editable map when dirty but never modifies, stashes, resets, checks
out, or cleans any worktree file.
"""

from __future__ import annotations

import argparse
import io
import re
import sys
import zipfile
from pathlib import Path, PurePosixPath
from typing import Iterable

try:
    from .v05_gate_common import (
        AGENT_RUNTIME_REL,
        OBJECT_ID_PATTERN,
        PROJECT_CONFIG_REL,
        PROJECT_VERSION,
        PROTECTED_MANIFEST_REL,
        RULES_REL,
        USER_MAP_REL,
        GateError,
        GateReport,
        decode_process_output,
        git_object_type,
        git_status_entries,
        load_json_file,
        read_index_blob,
        resolve_commit,
        resolve_path_object,
        run_command,
        run_git,
        tracked_paths,
        validate_relative_path,
    )
except ImportError:
    from v05_gate_common import (
        AGENT_RUNTIME_REL,
        OBJECT_ID_PATTERN,
        PROJECT_CONFIG_REL,
        PROJECT_VERSION,
        PROTECTED_MANIFEST_REL,
        RULES_REL,
        USER_MAP_REL,
        GateError,
        GateReport,
        decode_process_output,
        git_object_type,
        git_status_entries,
        load_json_file,
        read_index_blob,
        resolve_commit,
        resolve_path_object,
        run_command,
        run_git,
        tracked_paths,
        validate_relative_path,
    )


REPO_ROOT = Path(__file__).resolve().parents[2]
RULES_BASENAME = Path(RULES_REL).name
AGENT_BASENAME = Path(AGENT_RUNTIME_REL).name
OLD_RULES_BASENAME = "WhiteoutStationRules.v0.1.json"
OLD_AGENT_BASENAME = "AgentRuntime.v0.1.json"

RUNTIME_REFERENCES = {
    "WhiteoutStation/Source/WhiteoutStation/Private/State/WindStationStateSubsystem.cpp": (
        RULES_BASENAME,
        OLD_RULES_BASENAME,
    ),
    "WhiteoutStation/Source/WhiteoutStation/Private/Tests/WhiteoutRulesTests.cpp": (
        RULES_BASENAME,
        OLD_RULES_BASENAME,
    ),
    "WhiteoutStation/Source/WhiteoutStation/Private/Agents/WSAgentGateway.cpp": (
        AGENT_BASENAME,
        OLD_AGENT_BASENAME,
    ),
}
LEGACY_LOAD_MARKERS = (
    "_path",
    "configpath",
    "default_",
    "fpaths::projectcontentdir",
    "loadconfig",
    "open(",
    "path(",
    "read_bytes",
    "read_text",
)

SCANNABLE_ENDINGS = (
    ".bat",
    ".cmd",
    ".config",
    ".cpp",
    ".cs",
    ".csv",
    ".docx",
    ".env",
    ".h",
    ".ini",
    ".ini.example",
    ".js",
    ".json",
    ".md",
    ".ps1",
    ".py",
    ".rels",
    ".sh",
    ".toml",
    ".ts",
    ".tsx",
    ".txt",
    ".uplugin",
    ".uproject",
    ".xml",
    ".xlsx",
    ".yaml",
    ".yml",
    ".pptx",
)
SCANNABLE_NAMES = {".gitattributes", ".gitignore", "dockerfile"}

SK_TOKEN_PATTERN = re.compile(
    rb"(?<![A-Za-z0-9_-])s" rb"k-[A-Za-z0-9_-]{12,}"
)
SAFE_PLACEHOLDER_PATTERN = re.compile(
    rb"(?i)s"
    rb"k-(?:test|example|placeholder|redacted|dummy)"
    rb"(?:-(?:test|example|placeholder|redacted|dummy|not|a|secret|x+|0+))*"
)
SECRET_PATTERNS = {
    "bearer_token": re.compile(
        rb"(?i)authorization\s*[:=]\s*[\"']?bearer\s+[A-Za-z0-9._~+/-]{12,}"
    ),
    "private_key": re.compile(
        rb"-----" rb"BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----"
    ),
}
CREDENTIAL_ASSIGNMENT_PATTERN = re.compile(
    rb"""(?ix)
    (?<![A-Za-z0-9_])
    [\"']?(?:api[_ -]?key|secret[_ -]?key|access[_ -]?token|password)[\"']?
    (?![A-Za-z0-9_])
    \s*[:=]\s*(?P<quote>[\"']?)(?P<value>[A-Za-z0-9._~+/-]{20,})(?P=quote)
    """
)
SAFE_PLACEHOLDER_ROOTS = (
    PurePosixPath("Tools/Release"),
)


def _is_scannable(relative_path: str) -> bool:
    lowered = relative_path.lower()
    name = PurePosixPath(lowered).name
    return name in SCANNABLE_NAMES or lowered.endswith(SCANNABLE_ENDINGS)


def _is_test_placeholder(relative_path: str, token: bytes) -> bool:
    path = PurePosixPath(relative_path)
    if not any(path.is_relative_to(root) for root in SAFE_PLACEHOLDER_ROOTS):
        return False
    if not path.name.startswith("test_") or path.suffix != ".py":
        return False
    return SAFE_PLACEHOLDER_PATTERN.fullmatch(token) is not None


def _scan_bytes(
    payload: bytes,
    relative_path: str,
    source_label: str,
    findings: set[tuple[str, str, str]],
) -> None:
    for match in SK_TOKEN_PATTERN.finditer(payload):
        token = match.group(0)
        if not _is_test_placeholder(relative_path, token):
            findings.add((source_label, relative_path, "suspicious_sk_key"))
    for pattern_name, pattern in SECRET_PATTERNS.items():
        if pattern.search(payload):
            findings.add((source_label, relative_path, pattern_name))
    for match in CREDENTIAL_ASSIGNMENT_PATTERN.finditer(payload):
        value = match.group("value")
        quoted = bool(match.group("quote"))
        looks_like_unquoted_token = b"." not in value and any(
            byte in value for byte in b"0123456789_~+/-"
        )
        if quoted or looks_like_unquoted_token:
            findings.add((source_label, relative_path, "credential_assignment"))


def _scan_payload(
    payload: bytes,
    relative_path: str,
    source_label: str,
    findings: set[tuple[str, str, str]],
) -> None:
    _scan_bytes(payload, relative_path, source_label, findings)
    if not relative_path.lower().endswith((".docx", ".xlsx", ".pptx")):
        return
    try:
        with zipfile.ZipFile(io.BytesIO(payload)) as archive:
            for member in archive.namelist():
                if member.lower().endswith((".xml", ".rels", ".txt", ".json")):
                    _scan_bytes(
                        archive.read(member),
                        relative_path,
                        f"{source_label}!{member}",
                        findings,
                    )
    except zipfile.BadZipFile:
        findings.add((source_label, relative_path, "invalid_ooxml_container"))


def scan_tracked_secrets(repo_root: Path) -> GateReport:
    report = GateReport()
    findings: set[tuple[str, str, str]] = set()
    try:
        paths = tracked_paths(repo_root)
    except GateError as exc:
        report.error(f"Cannot enumerate tracked files: {exc}")
        return report

    scanned = 0
    for relative_path in paths:
        if not _is_scannable(relative_path):
            continue
        try:
            index_payload = read_index_blob(repo_root, relative_path)
        except GateError as exc:
            report.error(f"Cannot inspect tracked index blob {relative_path}: {exc}")
            continue
        _scan_payload(index_payload, relative_path, "index", findings)
        scanned += 1

        worktree_path = repo_root / Path(relative_path)
        if worktree_path.is_file():
            try:
                worktree_payload = worktree_path.read_bytes()
            except OSError as exc:
                report.error(f"Cannot inspect tracked worktree file {relative_path}: {exc}")
                continue
            if worktree_payload != index_payload:
                _scan_payload(worktree_payload, relative_path, "worktree", findings)

    for source_label, relative_path, pattern_name in sorted(findings):
        report.error(
            f"Tracked credential pattern {pattern_name} in "
            f"{source_label}:{relative_path}; value suppressed"
        )
    report.detail(f"Tracked secret scan inspected {scanned} textual/container files")
    return report


def _read_utf8_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8-sig")
    except (OSError, UnicodeDecodeError) as exc:
        raise GateError(f"Cannot read text {path}: {exc}") from exc


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
            if key_text.lower() in forbidden:
                yield path
            yield from _credential_keys(child, path)
    elif isinstance(value, list):
        for index, child in enumerate(value):
            yield from _credential_keys(child, f"{prefix}[{index}]")


def validate_versions(repo_root: Path) -> GateReport:
    report = GateReport()
    config_path = repo_root / PROJECT_CONFIG_REL
    rules_path = repo_root / RULES_REL
    agent_path = repo_root / AGENT_RUNTIME_REL

    try:
        config_text = _read_utf8_text(config_path)
        project_versions = re.findall(
            r"(?m)^\s*ProjectVersion\s*=\s*([^\r\n]+?)\s*$",
            config_text,
        )
        if project_versions != [PROJECT_VERSION]:
            report.error(
                f"{PROJECT_CONFIG_REL} must contain exactly "
                f"ProjectVersion={PROJECT_VERSION}; found {project_versions!r}"
            )
    except GateError as exc:
        report.error(str(exc))

    if not rules_path.is_file():
        report.error(f"Missing v0.5 rules file: {RULES_REL}")
    else:
        try:
            rules = load_json_file(rules_path)
            if not isinstance(rules, dict):
                report.error(f"{RULES_REL} root must be an object")
            else:
                if rules.get("rules_version") != PROJECT_VERSION:
                    report.error(
                        f"{RULES_REL} rules_version must be {PROJECT_VERSION}"
                    )
                schema_version = rules.get("schema_version")
                if (
                    isinstance(schema_version, bool)
                    or not isinstance(schema_version, int)
                    or schema_version < 1
                ):
                    report.error(f"{RULES_REL} schema_version must be a positive integer")
        except GateError as exc:
            report.error(str(exc))

    if not agent_path.is_file():
        report.error(f"Missing v0.5 Agent runtime file: {AGENT_RUNTIME_REL}")
    else:
        try:
            agent = load_json_file(agent_path)
            if not isinstance(agent, dict):
                report.error(f"{AGENT_RUNTIME_REL} root must be an object")
            else:
                if agent.get("runtime_version") != PROJECT_VERSION:
                    report.error(
                        f"{AGENT_RUNTIME_REL} runtime_version must be {PROJECT_VERSION}"
                    )
                schema_version = agent.get("schema_version")
                if (
                    isinstance(schema_version, bool)
                    or not isinstance(schema_version, int)
                    or schema_version < 1
                ):
                    report.error(
                        f"{AGENT_RUNTIME_REL} schema_version must be a positive integer"
                    )
                if agent.get("model") != "deepseek-v4-flash":
                    report.error(
                        f"{AGENT_RUNTIME_REL} model must be deepseek-v4-flash"
                    )
                if agent.get("endpoint") != "https://api.deepseek.com/chat/completions":
                    report.error(
                        f"{AGENT_RUNTIME_REL} endpoint must be the official DeepSeek "
                        "chat completions endpoint"
                    )
                if agent.get("llm_enabled") is not False:
                    report.error(f"{AGENT_RUNTIME_REL} must default llm_enabled=false")
                for key_path in _credential_keys(agent):
                    report.error(
                        f"{AGENT_RUNTIME_REL} contains forbidden credential field "
                        f"{key_path}"
                    )
        except GateError as exc:
            report.error(str(exc))

    for content_dir, expected_name in (
        (rules_path.parent, RULES_BASENAME),
        (agent_path.parent, AGENT_BASENAME),
    ):
        if not content_dir.is_dir():
            continue
        versioned_json = sorted(path.name for path in content_dir.glob("*.v*.json"))
        unexpected = [name for name in versioned_json if name != expected_name]
        if unexpected:
            report.error(
                f"Legacy/runtime version files remain in {content_dir.relative_to(repo_root)}: "
                + ", ".join(unexpected)
            )

    for relative_path, (expected_name, forbidden_name) in RUNTIME_REFERENCES.items():
        path = repo_root / relative_path
        try:
            text = _read_utf8_text(path)
        except GateError as exc:
            report.error(str(exc))
            continue
        if expected_name not in text:
            report.error(
                f"Current runtime/tool reference {relative_path} does not load {expected_name}"
            )
        if forbidden_name in text:
            report.error(
                f"Current runtime/tool reference {relative_path} still loads {forbidden_name}"
            )

    legacy_scan_files = [
        *repo_root.joinpath("WhiteoutStation/Source").rglob("*.cpp"),
        *repo_root.joinpath("WhiteoutStation/Source").rglob("*.h"),
        *repo_root.joinpath("Tools/Agents").glob("*.py"),
        *(
            path
            for path in repo_root.joinpath("Tools/Release").glob("*v05*.py")
            if not path.name.startswith("test_")
        ),
        *(
            path
            for path in repo_root.joinpath("Tools/Release").glob("v05_*.py")
            if not path.name.startswith("test_")
        ),
    ]
    for path in sorted(set(legacy_scan_files)):
        try:
            lines = _read_utf8_text(path).splitlines()
        except GateError as exc:
            report.error(str(exc))
            continue
        for line_number, line in enumerate(lines, 1):
            lowered = line.lower()
            if (
                OLD_RULES_BASENAME.lower() not in lowered
                and OLD_AGENT_BASENAME.lower() not in lowered
            ):
                continue
            if any(marker in lowered for marker in LEGACY_LOAD_MARKERS):
                report.error(
                    f"Current runtime/v0.5 tool loads a legacy runtime file at "
                    f"{path.relative_to(repo_root).as_posix()}:{line_number}"
                )

    if not report.errors:
        report.detail(
            f"Version contract is consistent: project/rules/Agent={PROJECT_VERSION}"
        )
    return report


def validate_protected_assets(
    repo_root: Path,
    manifest_path: Path | None = None,
    *,
    require_tracked: bool = False,
) -> GateReport:
    report = GateReport()
    path = manifest_path or (repo_root / PROTECTED_MANIFEST_REL)
    try:
        resolved_manifest = path.resolve()
        resolved_manifest.relative_to(repo_root.resolve())
    except ValueError:
        report.error(
            f"Protected asset manifest must remain under the repository root: {path}"
        )
        return report
    if not path.is_file():
        report.error(
            f"Missing protected character asset manifest: {PROTECTED_MANIFEST_REL}"
        )
        return report

    try:
        manifest = load_json_file(path)
    except GateError as exc:
        report.error(str(exc))
        return report
    if not isinstance(manifest, dict):
        report.error("Protected asset manifest root must be an object")
        return report
    if str(manifest.get("version", "")).lower().lstrip("v") not in {"0.5", "0.5.0"}:
        report.error("Protected asset manifest version must identify v0.5")

    baseline_ref = manifest.get("baseline_commit")
    if not isinstance(baseline_ref, str) or not baseline_ref:
        report.error("Protected asset manifest baseline_commit is missing")
        return report
    try:
        baseline_commit = resolve_commit(repo_root, baseline_ref)
        head_commit = resolve_commit(repo_root, "HEAD")
    except GateError as exc:
        report.error(f"Cannot resolve protected asset baseline: {exc}")
        return report

    ancestor = run_command(
        ["git", "merge-base", "--is-ancestor", baseline_commit, head_commit],
        cwd=repo_root,
        check=False,
    )
    if ancestor.returncode != 0:
        report.error(
            "Protected asset baseline_commit must be an ancestor of current HEAD"
        )

    entries = manifest.get("protected_git_objects")
    if not isinstance(entries, list) or not entries:
        report.error(
            "Protected asset manifest protected_git_objects must be a non-empty array"
        )
        return report

    seen_paths: set[str] = set()
    for index, entry in enumerate(entries):
        label = f"protected_git_objects[{index}]"
        if not isinstance(entry, dict):
            report.error(f"{label} must be an object")
            continue
        try:
            relative_path = validate_relative_path(entry.get("path"), f"{label}.path")
        except GateError as exc:
            report.error(str(exc))
            continue
        if relative_path in seen_paths:
            report.error(f"Duplicate protected asset path: {relative_path}")
            continue
        seen_paths.add(relative_path)

        expected_type = entry.get("type")
        if expected_type not in {"tree", "blob"}:
            report.error(f"{label}.type must be tree or blob")
            continue
        expected_object = entry.get("object_id")
        if (
            not isinstance(expected_object, str)
            or not OBJECT_ID_PATTERN.fullmatch(expected_object)
        ):
            report.error(f"{label}.object_id is not a valid Git object id")
            continue

        try:
            actual_type = git_object_type(repo_root, expected_object)
            if actual_type != expected_type:
                report.error(
                    f"Protected object type mismatch for {relative_path}: "
                    f"manifest={expected_type} git={actual_type}"
                )
            baseline_object = resolve_path_object(
                repo_root,
                baseline_commit,
                relative_path,
            )
            if baseline_object != expected_object:
                report.error(
                    f"Protected manifest object mismatch at baseline for {relative_path}: "
                    f"manifest={expected_object} baseline={baseline_object}"
                )
            current_object = resolve_path_object(repo_root, head_commit, relative_path)
            if current_object != expected_object:
                report.error(
                    f"Protected path changed in current HEAD: {relative_path}; "
                    f"expected={expected_object} current={current_object}"
                )
        except GateError as exc:
            report.error(f"Cannot validate protected path {relative_path}: {exc}")

        try:
            dirty = git_status_entries(repo_root, [relative_path])
        except GateError as exc:
            report.error(f"Cannot inspect protected path {relative_path}: {exc}")
            dirty = []
        if dirty:
            states = ", ".join(f"{status} {dirty_path}" for status, dirty_path in dirty)
            report.error(
                f"Protected path has worktree/index changes: {relative_path} ({states})"
            )

    try:
        manifest_relative = path.resolve().relative_to(repo_root.resolve()).as_posix()
        manifest_is_tracked = manifest_relative in tracked_paths(repo_root)
        if not manifest_is_tracked:
            message = (
                f"Protected asset manifest is not tracked by Git: {manifest_relative}"
            )
            if require_tracked:
                report.error(message)
            else:
                report.warn(message)
        manifest_changes = (
            git_status_entries(repo_root, [manifest_relative])
            if manifest_is_tracked
            else []
        )
        if manifest_changes:
            states = ", ".join(
                f"{status} {dirty_path}"
                for status, dirty_path in manifest_changes
            )
            message = (
                f"Protected asset manifest has worktree/index changes: {states}"
            )
            if require_tracked:
                report.error(message)
            else:
                report.warn(message)
    except (GateError, ValueError) as exc:
        report.error(f"Cannot verify protected manifest tracking: {exc}")

    if not report.errors:
        report.detail(
            f"Protected character assets match {len(entries)} baseline Git object(s)"
        )
    return report


def validate_lfs(repo_root: Path) -> GateReport:
    report = GateReport()
    version = run_command(["git", "lfs", "version"], cwd=repo_root, check=False)
    if version.returncode != 0:
        report.error("Git LFS is unavailable")
        return report
    fsck = run_command(["git", "lfs", "fsck"], cwd=repo_root, check=False)
    if fsck.returncode != 0:
        summary = (
            decode_process_output(fsck.stderr).strip()
            or decode_process_output(fsck.stdout).strip()
            or f"exit code {fsck.returncode}"
        )
        report.error(f"Git LFS integrity check failed: {summary}")
    else:
        report.detail("Git LFS integrity check passed")
    return report


def validate_worktree_state(repo_root: Path, *, final: bool = False) -> GateReport:
    report = GateReport()
    try:
        entries = git_status_entries(repo_root)
    except GateError as exc:
        report.error(f"Cannot inspect worktree state: {exc}")
        return report
    disallowed: list[str] = []
    for status, relative_path in entries:
        if relative_path == USER_MAP_REL:
            report.warn(
                f"User map is dirty and preserved without modification: "
                f"{status} {USER_MAP_REL}"
            )
        elif relative_path == PROTECTED_MANIFEST_REL and not final:
            report.warn(
                f"Protected asset manifest is uncommitted in development mode: "
                f"{status} {PROTECTED_MANIFEST_REL}"
            )
        else:
            disallowed.append(f"{status} {relative_path}")
    if disallowed:
        report.error(
            "Source worktree has changes outside the preserved user map: "
            + ", ".join(disallowed)
        )
    elif not entries:
        report.detail("Source worktree is clean")
    return report


def validate_source(
    repo_root: Path,
    manifest_path: Path | None = None,
    *,
    final: bool = False,
) -> GateReport:
    report = GateReport()
    try:
        repo_root = repo_root.resolve(strict=True)
        resolve_commit(repo_root, "HEAD")
    except (OSError, GateError) as exc:
        report.error(f"Invalid Git repository root: {exc}")
        return report

    report.merge(validate_worktree_state(repo_root, final=final))
    report.merge(validate_versions(repo_root))
    report.merge(scan_tracked_secrets(repo_root))
    report.merge(validate_lfs(repo_root))
    report.merge(
        validate_protected_assets(
            repo_root,
            manifest_path,
            require_tracked=final,
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
        "SOURCE GATE v0.5: PASS"
        if report.passed
        else f"SOURCE GATE v0.5: FAIL ({len(report.errors)} error(s))"
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=REPO_ROOT)
    parser.add_argument(
        "--protected-manifest",
        type=Path,
        help=(
            "Protected asset manifest under the repository root; defaults to "
            f"{PROTECTED_MANIFEST_REL}"
        ),
    )
    parser.add_argument(
        "--final",
        action="store_true",
        help="Require a clean final source tree and a tracked protection manifest",
    )
    args = parser.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")
    manifest_path = args.protected_manifest
    if manifest_path is not None and not manifest_path.is_absolute():
        manifest_path = args.repo_root / manifest_path
    report = validate_source(args.repo_root, manifest_path, final=args.final)
    print_report(report)
    return 0 if report.passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
