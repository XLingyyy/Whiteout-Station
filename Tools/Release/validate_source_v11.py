"""Fail-closed source gate for the Whiteout Station v1.1 release candidate."""

from __future__ import annotations

import argparse
import io
import re
import sys
import zipfile
from pathlib import Path, PurePosixPath
from typing import Iterable

try:
    from .v11_gate_common import (
        AGENT_RUNTIME_REL,
        OBJECT_ID_PATTERN,
        PROJECT_CONFIG_REL,
        PROJECT_VERSION,
        PROTECTED_BASELINE_VERSION,
        PROTECTED_MANIFEST_REL,
        REQUIRED_BRANCH,
        RULES_REL,
        SAFE_PLACEHOLDER_PATTERN,
        SHA256_PATTERN,
        SK_TOKEN_PATTERN,
        GateError,
        GateReport,
        current_branch,
        decode_process_output,
        git_object_type,
        git_status_entries,
        is_scannable_path,
        load_json_file,
        read_index_blob,
        resolve_commit,
        resolve_path_object,
        run_command,
        run_git,
        secret_finding_names,
        sha256_file,
        tracked_paths,
        validate_relative_path,
    )
except ImportError:
    from v11_gate_common import (
        AGENT_RUNTIME_REL,
        OBJECT_ID_PATTERN,
        PROJECT_CONFIG_REL,
        PROJECT_VERSION,
        PROTECTED_BASELINE_VERSION,
        PROTECTED_MANIFEST_REL,
        REQUIRED_BRANCH,
        RULES_REL,
        SAFE_PLACEHOLDER_PATTERN,
        SHA256_PATTERN,
        SK_TOKEN_PATTERN,
        GateError,
        GateReport,
        current_branch,
        decode_process_output,
        git_object_type,
        git_status_entries,
        is_scannable_path,
        load_json_file,
        read_index_blob,
        resolve_commit,
        resolve_path_object,
        run_command,
        run_git,
        secret_finding_names,
        sha256_file,
        tracked_paths,
        validate_relative_path,
    )


REPO_ROOT = Path(__file__).resolve().parents[2]
RULES_BASENAME = Path(RULES_REL).name
AGENT_BASENAME = Path(AGENT_RUNTIME_REL).name
RUNTIME_REFERENCES = {
    "WhiteoutStation/Source/WhiteoutStation/Private/State/"
    "WindStationStateSubsystem.cpp": RULES_BASENAME,
    "WhiteoutStation/Source/WhiteoutStation/Private/Agents/"
    "WSAgentGateway.cpp": AGENT_BASENAME,
    "Tools/Rules/whiteout_rules_v11.py": RULES_BASENAME,
}
SAFE_PLACEHOLDER_ROOT = PurePosixPath("Tools/Release")
OOXML_ENDINGS = (".docx", ".xlsx", ".pptx")


def _read_utf8_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8-sig")
    except (OSError, UnicodeDecodeError) as exc:
        raise GateError(f"Cannot read text {path}: {exc}") from exc


def _is_test_placeholder(relative_path: str, token: bytes) -> bool:
    path = PurePosixPath(relative_path)
    return (
        path.is_relative_to(SAFE_PLACEHOLDER_ROOT)
        and path.name.startswith("test_")
        and path.suffix == ".py"
        and SAFE_PLACEHOLDER_PATTERN.fullmatch(token) is not None
    )


def _scan_bytes(
    payload: bytes,
    relative_path: str,
    source_label: str,
    findings: set[tuple[str, str, str]],
) -> None:
    names = secret_finding_names(payload)
    if "suspicious_sk_key" in names:
        tokens = SK_TOKEN_PATTERN.findall(payload)
        if tokens and all(
            _is_test_placeholder(relative_path, token) for token in tokens
        ):
            names.remove("suspicious_sk_key")
    for pattern_name in names:
        findings.add((source_label, relative_path, pattern_name))


def _scan_payload(
    payload: bytes,
    relative_path: str,
    source_label: str,
    findings: set[tuple[str, str, str]],
) -> None:
    _scan_bytes(payload, relative_path, source_label, findings)
    if not relative_path.lower().endswith(OOXML_ENDINGS):
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
        if not is_scannable_path(relative_path) and not relative_path.lower().endswith(
            OOXML_ENDINGS
        ):
            continue
        try:
            index_payload = read_index_blob(repo_root, relative_path)
        except GateError as exc:
            report.error(f"Cannot inspect tracked index blob {relative_path}: {exc}")
            continue
        _scan_payload(index_payload, relative_path, "index", findings)
        scanned += 1

        worktree_path = repo_root / Path(relative_path)
        if not worktree_path.is_file():
            continue
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


def _validate_rules_contract(rules: object, report: GateReport) -> None:
    if not isinstance(rules, dict):
        report.error(f"{RULES_REL} root must be an object")
        return
    if rules.get("rules_version") != PROJECT_VERSION:
        report.error(f"{RULES_REL} rules_version must be {PROJECT_VERSION}")
    if rules.get("schema_version") != 4:
        report.error(f"{RULES_REL} schema_version must be 4")

    gameplay = rules.get("gameplay")
    if not isinstance(gameplay, dict):
        report.error(f"{RULES_REL} gameplay must be an object")
    else:
        if gameplay.get("phases") != ["morning", "afternoon", "dusk"]:
            report.error(f"{RULES_REL} phases must be morning, afternoon, dusk")
        if gameplay.get("action_points_per_phase") != 4:
            report.error(f"{RULES_REL} action_points_per_phase must be 4")

    routes = rules.get("routes")
    if not isinstance(routes, dict):
        report.error(f"{RULES_REL} routes must be an object")
    else:
        successful = sum(
            1
            for route in routes.values()
            if isinstance(route, dict) and route.get("expected_success") is True
        )
        failures = sum(
            1
            for route in routes.values()
            if isinstance(route, dict) and route.get("expected_success") is False
        )
        if successful < 3 or failures < 2:
            report.error(
                f"{RULES_REL} must retain at least three success and two failure routes"
            )


def _validate_agent_contract(agent: object, report: GateReport) -> None:
    if not isinstance(agent, dict):
        report.error(f"{AGENT_RUNTIME_REL} root must be an object")
        return
    if agent.get("runtime_version") != PROJECT_VERSION:
        report.error(f"{AGENT_RUNTIME_REL} runtime_version must be {PROJECT_VERSION}")
    if agent.get("schema_version") != 4:
        report.error(f"{AGENT_RUNTIME_REL} schema_version must be 4")
    if agent.get("llm_enabled") is not False:
        report.error(f"{AGENT_RUNTIME_REL} must default llm_enabled=false")
    if agent.get("transport") != "openai_chat_completions":
        report.error(f"{AGENT_RUNTIME_REL} transport must be openai_chat_completions")
    for key in ("provider_name", "model", "endpoint"):
        value = agent.get(key)
        if not isinstance(value, str) or not value.strip():
            report.error(f"{AGENT_RUNTIME_REL} {key} must be non-empty")
    endpoint = agent.get("endpoint")
    if isinstance(endpoint, str) and not endpoint.startswith("https://"):
        report.error(f"{AGENT_RUNTIME_REL} endpoint must default to HTTPS")
    for key_path in _credential_keys(agent):
        report.error(
            f"{AGENT_RUNTIME_REL} contains forbidden credential field {key_path}"
        )


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
        report.error(f"Missing v1.1 rules file: {RULES_REL}")
    else:
        try:
            _validate_rules_contract(load_json_file(rules_path), report)
        except GateError as exc:
            report.error(str(exc))

    if not agent_path.is_file():
        report.error(f"Missing v1.1 Agent runtime file: {AGENT_RUNTIME_REL}")
    else:
        try:
            _validate_agent_contract(load_json_file(agent_path), report)
        except GateError as exc:
            report.error(str(exc))

    for relative_path, expected_name in RUNTIME_REFERENCES.items():
        path = repo_root / relative_path
        try:
            text = _read_utf8_text(path)
        except GateError as exc:
            report.error(str(exc))
            continue
        if expected_name not in text:
            report.error(
                f"Current runtime/tool reference {relative_path} "
                f"does not load {expected_name}"
            )

    if not report.errors:
        report.detail(
            f"Version contract is consistent: project/rules/Agent={PROJECT_VERSION}, "
            "schema=4, LLM defaults off"
        )
    return report


def _manifest_relative_path(repo_root: Path, path: Path) -> str:
    try:
        return path.resolve().relative_to(repo_root.resolve()).as_posix()
    except ValueError as exc:
        raise GateError(
            f"Protected asset manifest must remain under the repository root: {path}"
        ) from exc


def _validated_path_list(
    raw_value: object,
    label: str,
    report: GateReport,
) -> list[str]:
    if not isinstance(raw_value, list) or not raw_value:
        report.error(f"Protected asset manifest {label} must be a non-empty array")
        return []
    paths: list[str] = []
    for index, raw_path in enumerate(raw_value):
        try:
            relative_path = validate_relative_path(raw_path, f"{label}[{index}]")
        except GateError as exc:
            report.error(str(exc))
            continue
        if relative_path in paths:
            report.error(f"Duplicate {label} path: {relative_path}")
            continue
        paths.append(relative_path)
    return paths


def _git_diff_paths(
    repo_root: Path,
    baseline_commit: str,
    head_commit: str,
    diff_filter: str,
    protected_roots: list[str],
) -> list[str]:
    payload = bytes(
        run_git(
            repo_root,
            "diff",
            "--name-only",
            "-z",
            f"--diff-filter={diff_filter}",
            baseline_commit,
            head_commit,
            "--",
            *protected_roots,
            text=False,
        )
    )
    return [
        decode_process_output(item).replace("\\", "/")
        for item in payload.split(b"\0")
        if item
    ]


def validate_protected_assets(
    repo_root: Path,
    manifest_path: Path | None = None,
    *,
    require_tracked: bool = False,
) -> GateReport:
    report = GateReport()
    path = manifest_path or (repo_root / PROTECTED_MANIFEST_REL)
    try:
        manifest_relative = _manifest_relative_path(repo_root, path)
    except GateError as exc:
        report.error(str(exc))
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
    if manifest.get("schema") != "whiteout.protected-assets.v3":
        report.error(
            "Protected asset manifest schema must be whiteout.protected-assets.v3"
        )
    if str(manifest.get("version", "")).lower().lstrip("v") != (
        PROTECTED_BASELINE_VERSION
    ):
        report.error(
            "Protected asset manifest must identify the approved v1.0 baseline"
        )

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

    protected_roots = _validated_path_list(
        manifest.get("protected_roots"),
        "protected_roots",
        report,
    )
    raw_additions = manifest.get("allowed_additions", [])
    if not isinstance(raw_additions, list):
        report.error("Protected asset manifest allowed_additions must be an array")
        allowed_additions: list[str] = []
    else:
        allowed_additions = []
        for index, raw_addition in enumerate(raw_additions):
            try:
                addition = validate_relative_path(
                    raw_addition,
                    f"allowed_additions[{index}]",
                )
            except GateError as exc:
                report.error(str(exc))
                continue
            if not any(addition.startswith(f"{root}/") for root in protected_roots):
                report.error(f"Allowed addition is outside protected roots: {addition}")
                continue
            if addition in allowed_additions:
                report.error(f"Duplicate allowed addition: {addition}")
                continue
            allowed_additions.append(addition)

    if protected_roots:
        try:
            changed_paths = _git_diff_paths(
                repo_root,
                baseline_commit,
                head_commit,
                "CDMRTUXB",
                protected_roots,
            )
            for changed_path in changed_paths:
                report.error(
                    f"Protected baseline file changed or was removed: {changed_path}"
                )

            added_paths = _git_diff_paths(
                repo_root,
                baseline_commit,
                head_commit,
                "A",
                protected_roots,
            )
            for added_path in added_paths:
                if not any(
                    added_path == allowed or added_path.startswith(f"{allowed}/")
                    for allowed in allowed_additions
                ):
                    report.error(
                        f"Unexpected addition under protected roots: {added_path}"
                    )

            dirty_roots = git_status_entries(repo_root, protected_roots)
            if dirty_roots:
                states = ", ".join(
                    f"{status} {dirty_path}" for status, dirty_path in dirty_roots
                )
                report.error("Protected roots have worktree/index changes: " + states)
        except GateError as exc:
            report.error(f"Cannot compare protected roots to baseline: {exc}")

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
        expected_object = entry.get("object_id")
        if expected_type not in {"tree", "blob"}:
            report.error(f"{label}.type must be tree or blob")
            continue
        if not isinstance(expected_object, str) or not OBJECT_ID_PATTERN.fullmatch(
            expected_object
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
                    f"Protected manifest object mismatch at baseline for "
                    f"{relative_path}"
                )
            current_object = resolve_path_object(
                repo_root,
                head_commit,
                relative_path,
            )
            contains_allowlisted_additions = any(
                allowed.startswith(f"{relative_path}/") for allowed in allowed_additions
            )
            if current_object != expected_object and not contains_allowlisted_additions:
                report.error(f"Protected path changed in current HEAD: {relative_path}")
        except GateError as exc:
            report.error(f"Cannot validate protected path {relative_path}: {exc}")

    if set(protected_roots) != seen_paths:
        missing_objects = sorted(set(protected_roots) - seen_paths)
        unexpected_objects = sorted(seen_paths - set(protected_roots))
        if missing_objects:
            report.error(
                "Protected roots missing Git object records: "
                + ", ".join(missing_objects)
            )
        if unexpected_objects:
            report.error(
                "Protected Git object records outside protected_roots: "
                + ", ".join(unexpected_objects)
            )

    map_baseline = manifest.get("map_baseline")
    if not isinstance(map_baseline, dict):
        report.error("Protected asset manifest map_baseline must be an object")
    else:
        try:
            map_path = validate_relative_path(
                map_baseline.get("path"),
                "map_baseline.path",
            )
        except GateError as exc:
            report.error(str(exc))
        else:
            expected_sha = map_baseline.get("sha256")
            if not isinstance(expected_sha, str) or not SHA256_PATTERN.fullmatch(
                expected_sha
            ):
                report.error("map_baseline.sha256 must be a lowercase SHA-256")
            elif map_path not in protected_roots:
                report.error("map_baseline.path must be listed in protected_roots")
            elif not (repo_root / map_path).is_file():
                report.error(f"Protected map is missing: {map_path}")
            elif sha256_file(repo_root / map_path) != expected_sha:
                report.error(f"Protected map SHA-256 mismatch: {map_path}")

    try:
        tracked = tracked_paths(repo_root)
        if manifest_relative not in tracked:
            message = (
                f"Protected asset manifest is not tracked by Git: {manifest_relative}"
            )
            if require_tracked:
                report.error(message)
            else:
                report.warn(message)
        elif git_status_entries(repo_root, [manifest_relative]):
            report.error(
                "Protected asset manifest has worktree/index changes: "
                f"{manifest_relative}"
            )
    except GateError as exc:
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
    del final
    report = GateReport()
    try:
        branch = current_branch(repo_root)
    except GateError as exc:
        report.error(str(exc))
        branch = ""
    if branch and branch != REQUIRED_BRANCH:
        report.error(
            f"Release source must be on branch {REQUIRED_BRANCH}; found {branch}"
        )
    try:
        entries = git_status_entries(repo_root)
    except GateError as exc:
        report.error(f"Cannot inspect worktree state: {exc}")
        return report
    if entries:
        report.error(
            "v1.1 release requires a clean main worktree and index: "
            + ", ".join(f"{status} {path}" for status, path in entries)
        )
    elif branch == REQUIRED_BRANCH:
        report.detail("Source is on clean main")
    return report


def validate_source(
    repo_root: Path,
    manifest_path: Path | None = None,
    *,
    final: bool = False,
    check_lfs: bool = True,
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
        "SOURCE GATE v1.1: PASS"
        if report.passed
        else f"SOURCE GATE v1.1: FAIL ({len(report.errors)} error(s))"
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
        help="Accepted for v1.0 CLI compatibility; v1.1 always enforces clean main",
    )
    args = parser.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")
    manifest_path = args.protected_manifest
    if manifest_path is not None and not manifest_path.is_absolute():
        manifest_path = args.repo_root / manifest_path
    report = validate_source(
        args.repo_root,
        manifest_path,
        final=args.final,
    )
    print_report(report)
    return 0 if report.passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
