"""Validate one explicitly selected, fresh Whiteout Station v0.6 artifact.

The validator never searches for a package and therefore cannot silently fall
back to a historical v0.4 archive.  The artifact directory must be unique for
the run and contain ``Validation/gate_manifest.json`` with this shape:

{
  "schema": "whiteout.v0.6.release-manifest.v1",
  "version": "0.6.0",
  "run_id": "YYYYMMDDTHHMMSSZ-<commit8>-<nonce>",
  "artifact_root_name": "WhiteoutStation-v0.6-Win64-<run_id>",
  "source_commit": "<full git object id>",
  "source_tree": "<full git tree id>",
  "source_dirty": false,
  "build_timestamp_utc": "<timezone-aware ISO-8601>",
  "engine_version": "5.8.0",
  "python_version": "<version>",
  "checksums": {"relative/file": "<sha256>", "...": "..."}
}
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from datetime import datetime, timedelta, timezone
from pathlib import Path

try:
    from .v06_gate_common import (
        AGENT_RUNTIME_REL,
        MANIFEST_REL,
        MANIFEST_SCHEMA,
        OBJECT_ID_PATTERN,
        PROJECT_CONFIG_REL,
        PROJECT_VERSION,
        REQUIRED_PACKAGE_FILES,
        RULES_REL,
        RUN_ID_PATTERN,
        SHA256_PATTERN,
        USER_MAP_REL,
        GateError,
        GateReport,
        artifact_relative_path,
        file_change_time_utc,
        file_mtime_utc,
        git_commit_time,
        git_status_entries,
        load_json_file,
        load_json_bytes,
        parse_utc_timestamp,
        read_commit_blob,
        regular_artifact_files,
        resolve_commit,
        resolve_tree,
        sha256_file,
        utc_now,
        validate_relative_path,
    )
    from .validate_source_v06 import (
        AGENT_BASENAME,
        OLD_AGENT_BASENAME,
        OLD_RULES_BASENAME,
        RULES_BASENAME,
        RUNTIME_REFERENCES,
        _credential_keys,
        validate_protected_assets,
    )
except ImportError:
    from v06_gate_common import (
        AGENT_RUNTIME_REL,
        MANIFEST_REL,
        MANIFEST_SCHEMA,
        OBJECT_ID_PATTERN,
        PROJECT_CONFIG_REL,
        PROJECT_VERSION,
        REQUIRED_PACKAGE_FILES,
        RULES_REL,
        RUN_ID_PATTERN,
        SHA256_PATTERN,
        USER_MAP_REL,
        GateError,
        GateReport,
        artifact_relative_path,
        file_change_time_utc,
        file_mtime_utc,
        git_commit_time,
        git_status_entries,
        load_json_file,
        load_json_bytes,
        parse_utc_timestamp,
        read_commit_blob,
        regular_artifact_files,
        resolve_commit,
        resolve_tree,
        sha256_file,
        utc_now,
        validate_relative_path,
    )
    from validate_source_v06 import (
        AGENT_BASENAME,
        OLD_AGENT_BASENAME,
        OLD_RULES_BASENAME,
        RULES_BASENAME,
        RUNTIME_REFERENCES,
        _credential_keys,
        validate_protected_assets,
    )


REPO_ROOT = Path(__file__).resolve().parents[2]
ENGINE_VERSION = "5.8.0"
FRESHNESS_SKEW = timedelta(seconds=120)
FUTURE_SKEW = timedelta(minutes=5)

MINIMUM_PACKAGE_SIZES = {
    "Windows/WhiteoutStation.exe": 100_000,
    "Windows/WhiteoutStation/Binaries/Win64/WhiteoutStation-Win64-Shipping.exe": 1_000_000,
    "Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.pak": 100_000,
    "Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.ucas": 1_000_000,
    "Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.utoc": 10_000,
    "Windows/WhiteoutStation/Content/Rules/WhiteoutStationRules.v0.6.json": 100,
    "Windows/WhiteoutStation/Content/Agents/AgentRuntime.v0.6.json": 100,
    "README_v0.6.txt": 10,
    "ASSET_LICENSES.md": 10,
}


def _decode_text(payload: bytes, label: str) -> str:
    for encoding in ("utf-8-sig", "utf-8", "gb18030", "utf-16"):
        try:
            return payload.decode(encoding)
        except UnicodeDecodeError:
            continue
    raise GateError(f"Cannot decode {label}")


def validate_committed_version_contract(
    repo_root: Path,
    source_commit: str,
) -> GateReport:
    report = GateReport()
    try:
        config_text = _decode_text(
            read_commit_blob(repo_root, source_commit, PROJECT_CONFIG_REL),
            f"{source_commit}:{PROJECT_CONFIG_REL}",
        )
        versions = re.findall(
            r"(?m)^\s*ProjectVersion\s*=\s*([^\r\n]+?)\s*$",
            config_text,
        )
        if versions != [PROJECT_VERSION]:
            report.error(
                f"Source commit project version mismatch: expected "
                f"{PROJECT_VERSION}, found {versions!r}"
            )

        rules = load_json_bytes(
            read_commit_blob(repo_root, source_commit, RULES_REL),
            f"{source_commit}:{RULES_REL}",
        )
        if not isinstance(rules, dict) or rules.get("rules_version") != PROJECT_VERSION:
            report.error(
                f"Source commit rules_version must be {PROJECT_VERSION}"
            )

        agent = load_json_bytes(
            read_commit_blob(repo_root, source_commit, AGENT_RUNTIME_REL),
            f"{source_commit}:{AGENT_RUNTIME_REL}",
        )
        if not isinstance(agent, dict) or agent.get("runtime_version") != PROJECT_VERSION:
            report.error(
                f"Source commit Agent runtime_version must be {PROJECT_VERSION}"
            )
        elif agent.get("model") != "deepseek-v4-flash":
            report.error("Source commit Agent model must be deepseek-v4-flash")
    except GateError as exc:
        report.error(f"Cannot validate source version contract: {exc}")

    for relative_path, (expected_name, forbidden_name) in RUNTIME_REFERENCES.items():
        try:
            text = _decode_text(
                read_commit_blob(repo_root, source_commit, relative_path),
                f"{source_commit}:{relative_path}",
            )
        except GateError as exc:
            report.error(f"Cannot inspect runtime reference: {exc}")
            continue
        if expected_name not in text:
            report.error(
                f"Source commit runtime reference {relative_path} does not load "
                f"{expected_name}"
            )
        if forbidden_name in text:
            report.error(
                f"Source commit runtime reference {relative_path} still loads "
                f"{forbidden_name}"
            )
    return report


def _validate_manifest_identity(
    report: GateReport,
    manifest: dict[str, object],
    artifact_root: Path,
) -> str | None:
    if manifest.get("schema") != MANIFEST_SCHEMA:
        report.error(f"Manifest schema must be {MANIFEST_SCHEMA}")
    if manifest.get("version") != PROJECT_VERSION:
        report.error(f"Manifest version must be {PROJECT_VERSION}")

    run_id = manifest.get("run_id")
    if not isinstance(run_id, str) or not RUN_ID_PATTERN.fullmatch(run_id):
        report.error(
            "Manifest run_id must match "
            "YYYYMMDDTHHMMSSZ-<commit8>-<4..16 lowercase nonce>"
        )
        return None
    expected_root_name = f"WhiteoutStation-v0.6-Win64-{run_id}"
    if artifact_root.name != expected_root_name:
        report.error(
            f"Artifact directory must be the unique v0.6 run directory "
            f"{expected_root_name}; got {artifact_root.name}"
        )
    if manifest.get("artifact_root_name") != expected_root_name:
        report.error("Manifest artifact_root_name does not match its run_id")
    if "v0.4" in artifact_root.name.lower():
        report.error("Historical v0.4 artifact directories are forbidden")
    return run_id


def _validate_provenance(
    report: GateReport,
    manifest: dict[str, object],
    repo_root: Path,
    expected_source_ref: str,
) -> tuple[str | None, datetime | None]:
    source_commit = manifest.get("source_commit")
    source_tree = manifest.get("source_tree")
    if (
        not isinstance(source_commit, str)
        or not OBJECT_ID_PATTERN.fullmatch(source_commit)
    ):
        report.error("Manifest source_commit is not a full Git object id")
        return None, None
    if not isinstance(source_tree, str) or not OBJECT_ID_PATTERN.fullmatch(source_tree):
        report.error("Manifest source_tree is not a full Git object id")
        return None, None
    if manifest.get("source_dirty") is not False:
        report.error("Manifest source_dirty must be the boolean false")

    try:
        expected_commit = resolve_commit(repo_root, expected_source_ref)
        resolved_manifest_commit = resolve_commit(repo_root, source_commit)
        if resolved_manifest_commit != source_commit:
            report.error("Manifest source_commit must be the canonical full commit id")
        if source_commit != expected_commit:
            report.error(
                f"Artifact source_commit is stale/mismatched: "
                f"manifest={source_commit} expected={expected_commit}"
            )
        actual_tree = resolve_tree(repo_root, source_commit)
        if source_tree != actual_tree:
            report.error(
                f"Manifest source_tree mismatch: manifest={source_tree} git={actual_tree}"
            )
        commit_time = git_commit_time(repo_root, source_commit)
    except GateError as exc:
        report.error(f"Cannot verify manifest Git provenance: {exc}")
        return source_commit, None

    report.merge(validate_committed_version_contract(repo_root, source_commit))
    return source_commit, commit_time


def _validate_build_time(
    report: GateReport,
    manifest: dict[str, object],
    commit_time: datetime | None,
    *,
    now: datetime,
    max_age_hours: float,
) -> datetime | None:
    try:
        build_time = parse_utc_timestamp(
            manifest.get("build_timestamp_utc"),
            "Manifest build_timestamp_utc",
        )
    except GateError as exc:
        report.error(str(exc))
        return None
    if build_time > now + FUTURE_SKEW:
        report.error("Manifest build timestamp is unreasonably in the future")
    if now - build_time > timedelta(hours=max_age_hours):
        report.error(
            f"Artifact is stale: build is older than {max_age_hours:g} hours"
        )
    if commit_time is not None and build_time < commit_time - FRESHNESS_SKEW:
        report.error("Build timestamp predates its source commit")
    return build_time


def _validate_package_versions(
    report: GateReport,
    repo_root: Path,
    artifact_root: Path,
    source_commit: str,
) -> None:
    rules_rel = (
        "Windows/WhiteoutStation/Content/Rules/"
        "WhiteoutStationRules.v0.6.json"
    )
    agent_rel = (
        "Windows/WhiteoutStation/Content/Agents/"
        "AgentRuntime.v0.6.json"
    )
    try:
        rules = load_json_file(artifact_root / rules_rel)
        if not isinstance(rules, dict) or rules.get("rules_version") != PROJECT_VERSION:
            report.error(f"Packaged rules_version must be {PROJECT_VERSION}")
        committed_rules = load_json_bytes(
            read_commit_blob(repo_root, source_commit, RULES_REL),
            f"{source_commit}:{RULES_REL}",
        )
        if rules != committed_rules:
            report.error(
                "Packaged rules JSON does not match the declared source_commit"
            )
    except GateError as exc:
        report.error(str(exc))
    try:
        agent = load_json_file(artifact_root / agent_rel)
        if not isinstance(agent, dict):
            report.error("Packaged Agent runtime root must be an object")
        else:
            if agent.get("runtime_version") != PROJECT_VERSION:
                report.error(
                    f"Packaged Agent runtime_version must be {PROJECT_VERSION}"
                )
            if agent.get("model") != "deepseek-v4-flash":
                report.error("Packaged Agent model must be deepseek-v4-flash")
            if agent.get("endpoint") != "https://api.deepseek.com/chat/completions":
                report.error("Packaged Agent endpoint is not the official endpoint")
            if agent.get("llm_enabled") is not False:
                report.error("Packaged Agent must default llm_enabled=false")
            for key_path in _credential_keys(agent):
                report.error(
                    f"Packaged Agent runtime contains credential field {key_path}"
                )
        committed_agent = load_json_bytes(
            read_commit_blob(repo_root, source_commit, AGENT_RUNTIME_REL),
            f"{source_commit}:{AGENT_RUNTIME_REL}",
        )
        if agent != committed_agent:
            report.error(
                "Packaged Agent runtime JSON does not match the declared source_commit"
            )
    except GateError as exc:
        report.error(str(exc))
    try:
        first_line = (
            artifact_root.joinpath("README_v0.6.txt")
            .read_text(encoding="utf-8-sig")
            .splitlines()[0]
        )
        if "v0.6" not in first_line or "v0.4" in first_line:
            report.error("Packaged README first line must identify v0.6 only")
    except (OSError, UnicodeDecodeError, IndexError) as exc:
        report.error(f"Cannot validate packaged README: {exc}")


def _validate_artifact_files(
    report: GateReport,
    artifact_root: Path,
    manifest_path: Path,
    manifest: dict[str, object],
    build_time: datetime | None,
    *,
    now: datetime,
) -> None:
    try:
        files = regular_artifact_files(artifact_root)
    except GateError as exc:
        report.error(str(exc))
        return
    manifest_rel = artifact_relative_path(artifact_root, manifest_path)
    actual_paths = {
        artifact_relative_path(artifact_root, path)
        for path in files
        if artifact_relative_path(artifact_root, path) != manifest_rel
    }

    for relative_path in REQUIRED_PACKAGE_FILES:
        path = artifact_root / relative_path
        if relative_path not in actual_paths or not path.is_file():
            report.error(f"Missing required v0.6 artifact file: {relative_path}")
            continue
        minimum_size = MINIMUM_PACKAGE_SIZES[relative_path]
        if path.stat().st_size < minimum_size:
            report.error(
                f"Artifact file is too small: {relative_path} "
                f"({path.stat().st_size} < {minimum_size})"
            )

    for relative_path in actual_paths:
        lowered = relative_path.lower()
        if "v0.4" in lowered:
            report.error(
                f"Historical v0.4 file is forbidden in v0.6 artifact: {relative_path}"
            )
        if lowered.endswith(
            (
                f"/{OLD_RULES_BASENAME.lower()}",
                f"/{OLD_AGENT_BASENAME.lower()}",
            )
        ):
            report.error(
                f"Legacy runtime config is forbidden in v0.6 artifact: {relative_path}"
            )

    checksums = manifest.get("checksums")
    if not isinstance(checksums, dict):
        report.error("Manifest checksums must be an object")
        return
    normalized_checksums: dict[str, str] = {}
    for raw_path, raw_digest in checksums.items():
        try:
            relative_path = validate_relative_path(raw_path, "Manifest checksum path")
        except GateError as exc:
            report.error(str(exc))
            continue
        if relative_path == manifest_rel:
            report.error("Manifest must not checksum itself")
            continue
        if (
            not isinstance(raw_digest, str)
            or not SHA256_PATTERN.fullmatch(raw_digest)
        ):
            report.error(f"Invalid SHA-256 for artifact path {relative_path}")
            continue
        normalized_checksums[relative_path] = raw_digest

    checksum_paths = set(normalized_checksums)
    missing_checksums = sorted(actual_paths - checksum_paths)
    extra_checksums = sorted(checksum_paths - actual_paths)
    if missing_checksums:
        report.error(
            "Manifest is missing artifact checksums: " + ", ".join(missing_checksums)
        )
    if extra_checksums:
        report.error(
            "Manifest references absent artifact files: " + ", ".join(extra_checksums)
        )

    for relative_path in sorted(actual_paths & checksum_paths):
        path = artifact_root / relative_path
        actual_digest = sha256_file(path)
        if actual_digest != normalized_checksums[relative_path]:
            report.error(
                f"Artifact checksum mismatch: {relative_path}; "
                f"manifest={normalized_checksums[relative_path]} actual={actual_digest}"
            )
        if build_time is not None:
            mtime = file_mtime_utc(path)
            change_time = file_change_time_utc(path)
            if max(mtime, change_time) < build_time - FRESHNESS_SKEW:
                report.error(
                    f"Artifact file predates this build and may be stale: {relative_path}"
                )
            if max(mtime, change_time) > now + FUTURE_SKEW:
                report.error(
                    f"Artifact file timestamp is unreasonably in the future: "
                    f"{relative_path}"
                )

    for executable_rel in (
        "Windows/WhiteoutStation.exe",
        "Windows/WhiteoutStation/Binaries/Win64/WhiteoutStation-Win64-Shipping.exe",
    ):
        path = artifact_root / executable_rel
        if path.is_file() and path.read_bytes()[:2] != b"MZ":
            report.error(f"Packaged executable lacks PE signature: {executable_rel}")


def validate_release_artifact(
    repo_root: Path,
    artifact_root: Path,
    *,
    manifest_path: Path | None = None,
    expected_source_ref: str = "HEAD",
    max_age_hours: float = 72.0,
    now: datetime | None = None,
) -> GateReport:
    report = GateReport()
    now = (now or utc_now()).astimezone(timezone.utc)
    if max_age_hours <= 0:
        report.error("max_age_hours must be positive")
        return report

    try:
        repo_root = repo_root.resolve(strict=True)
        resolve_commit(repo_root, expected_source_ref)
    except (OSError, GateError) as exc:
        report.error(f"Invalid repository/source ref: {exc}")
        return report
    try:
        for status, relative_path in git_status_entries(repo_root, [USER_MAP_REL]):
            if relative_path == USER_MAP_REL:
                report.warn(
                    f"User map is dirty and was left untouched: "
                    f"{status} {USER_MAP_REL}"
                )
    except GateError as exc:
        report.error(f"Cannot report preserved user map status: {exc}")

    try:
        artifact_root = artifact_root.resolve(strict=True)
    except OSError as exc:
        report.error(f"Artifact root does not exist: {exc}")
        return report
    if not artifact_root.is_dir():
        report.error(f"Artifact root is not a directory: {artifact_root}")
        return report
    if not artifact_root.name.startswith("WhiteoutStation-v0.6-Win64-"):
        report.error(
            "Artifact root must be an explicitly selected unique v0.6 directory; "
            f"got {artifact_root.name}"
        )
    if "v0.4" in artifact_root.name.lower():
        report.error("Historical v0.4 artifact roots are never accepted")

    manifest_path = manifest_path or (artifact_root / MANIFEST_REL)
    try:
        manifest_path = manifest_path.resolve(strict=True)
        manifest_path.relative_to(artifact_root)
    except (OSError, ValueError) as exc:
        report.error(
            f"Manifest must exist inside the selected artifact root at "
            f"{MANIFEST_REL}: {exc}"
        )
        return report
    try:
        manifest = load_json_file(manifest_path)
    except GateError as exc:
        report.error(str(exc))
        return report
    if not isinstance(manifest, dict):
        report.error("Release manifest root must be an object")
        return report

    run_id = _validate_manifest_identity(report, manifest, artifact_root)
    source_commit, commit_time = _validate_provenance(
        report,
        manifest,
        repo_root,
        expected_source_ref,
    )
    if run_id and source_commit and not run_id.split("-")[1] == source_commit[:8]:
        report.error("Manifest run_id commit prefix does not match source_commit")

    build_time = _validate_build_time(
        report,
        manifest,
        commit_time,
        now=now,
        max_age_hours=max_age_hours,
    )
    if build_time is not None:
        root_freshness = max(
            file_mtime_utc(artifact_root),
            file_change_time_utc(artifact_root),
        )
        if root_freshness < build_time - FRESHNESS_SKEW:
            report.error(
                "Artifact output directory predates the declared build and is stale"
            )
    if manifest.get("engine_version") != ENGINE_VERSION:
        report.error(f"Manifest engine_version must be {ENGINE_VERSION}")
    python_version = manifest.get("python_version")
    if not isinstance(python_version, str) or not re.fullmatch(
        r"\d+\.\d+(?:\.\d+)?",
        python_version,
    ):
        report.error("Manifest python_version is missing or malformed")

    report.merge(validate_protected_assets(repo_root, require_tracked=True))
    _validate_artifact_files(
        report,
        artifact_root,
        manifest_path,
        manifest,
        build_time,
        now=now,
    )
    if source_commit:
        _validate_package_versions(report, repo_root, artifact_root, source_commit)

    if not report.errors:
        report.detail(
            f"Validated fresh v0.6 artifact {artifact_root.name} "
            f"from source {source_commit}"
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
        "RELEASE VALIDATION v0.6: PASS"
        if report.passed
        else f"RELEASE VALIDATION v0.6: FAIL ({len(report.errors)} error(s))"
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=REPO_ROOT)
    parser.add_argument(
        "--artifact-root",
        type=Path,
        required=True,
        help="Exact unique v0.6 artifact directory; no directory auto-discovery occurs",
    )
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--expected-source-ref", default="HEAD")
    parser.add_argument("--max-age-hours", type=float, default=72.0)
    args = parser.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")
    report = validate_release_artifact(
        args.repo_root,
        args.artifact_root,
        manifest_path=args.manifest,
        expected_source_ref=args.expected_source_ref,
        max_age_hours=args.max_age_hours,
    )
    print_report(report)
    return 0 if report.passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
