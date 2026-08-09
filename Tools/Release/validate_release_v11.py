"""Validate one explicitly selected Whiteout Station v1.1 release artifact."""

from __future__ import annotations

import argparse
import json
import os
import re
import stat
import struct
import sys
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Any, Iterable

try:
    from .v11_gate_common import (
        AGENT_RUNTIME_REL,
        ARTIFACT_PREFIX,
        DISTRIBUTION_BANNER,
        DISTRIBUTION_CLASS,
        MANIFEST_REL,
        MANIFEST_SCHEMA,
        OBJECT_ID_PATTERN,
        PACKAGED_AGENT_RUNTIME_REL,
        PACKAGED_RULES_REL,
        PROJECT_CONFIG_REL,
        PROJECT_VERSION,
        REQUIRED_PACKAGE_FILES as COMMON_REQUIRED_PACKAGE_FILES,
        RULES_REL,
        RUN_ID_PATTERN,
        SHA256_PATTERN,
        GateError,
        GateReport,
        artifact_relative_path,
        ensure_unique_artifact_root,
        is_scannable_path,
        load_json_file,
        parse_utc_timestamp,
        read_commit_blob,
        resolve_commit,
        resolve_tree,
        run_git,
        secret_finding_names,
        sha256_file,
        validate_relative_path,
    )
except ImportError:
    from v11_gate_common import (
        AGENT_RUNTIME_REL,
        ARTIFACT_PREFIX,
        DISTRIBUTION_BANNER,
        DISTRIBUTION_CLASS,
        MANIFEST_REL,
        MANIFEST_SCHEMA,
        OBJECT_ID_PATTERN,
        PACKAGED_AGENT_RUNTIME_REL,
        PACKAGED_RULES_REL,
        PROJECT_CONFIG_REL,
        PROJECT_VERSION,
        REQUIRED_PACKAGE_FILES as COMMON_REQUIRED_PACKAGE_FILES,
        RULES_REL,
        RUN_ID_PATTERN,
        SHA256_PATTERN,
        GateError,
        GateReport,
        artifact_relative_path,
        ensure_unique_artifact_root,
        is_scannable_path,
        load_json_file,
        parse_utc_timestamp,
        read_commit_blob,
        resolve_commit,
        resolve_tree,
        run_git,
        secret_finding_names,
        sha256_file,
        validate_relative_path,
    )

try:
    from .validate_source_v11 import validate_protected_assets
except ImportError:
    from validate_source_v11 import validate_protected_assets


REPO_ROOT = Path(__file__).resolve().parents[2]
ENGINE_VERSION = "5.8.1"
FRESHNESS_SKEW = timedelta(seconds=120)
FUTURE_SKEW = timedelta(minutes=5)
FILE_ATTRIBUTE_REPARSE_POINT = getattr(
    stat,
    "FILE_ATTRIBUTE_REPARSE_POINT",
    0x00000400,
)

INPUT_SUMMARY_REL = "Validation/InputSmokeV11/input_smoke_summary.json"
SHIPPING_SUMMARY_REL = "Validation/ShippingSmokeV11/shipping_smoke_summary.json"
README_REL = "README_v1.1.txt"
LICENSE_REL = "ASSET_LICENSES.md"
README_SOURCE_REL = "Distribution/README_v1.1.txt"
LICENSE_SOURCE_REL = "SourceAssets/ASSET_LICENSES.md"
EXECUTABLE_RELS = (
    "Windows/WhiteoutStation.exe",
    ("Windows/WhiteoutStation/Binaries/Win64/" "WhiteoutStation-Win64-Shipping.exe"),
)

REQUIRED_PACKAGE_FILES = tuple(
    relative_path
    for relative_path in COMMON_REQUIRED_PACKAGE_FILES
    if relative_path
    not in {
        "Validation/InputSmoke/input_smoke_summary.json",
        "Validation/ShippingSmoke/shipping_smoke_summary.json",
        INPUT_SUMMARY_REL,
        SHIPPING_SUMMARY_REL,
    }
) + (INPUT_SUMMARY_REL, SHIPPING_SUMMARY_REL)

MINIMUM_PACKAGE_SIZES = {
    "Windows/WhiteoutStation.exe": 100_000,
    (
        "Windows/WhiteoutStation/Binaries/Win64/" "WhiteoutStation-Win64-Shipping.exe"
    ): 1_000_000,
    "Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.pak": 100_000,
    "Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.ucas": 1_000_000,
    "Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.utoc": 10_000,
    PACKAGED_RULES_REL: 100,
    PACKAGED_AGENT_RUNTIME_REL: 100,
    README_REL: 100,
    LICENSE_REL: 100,
    INPUT_SUMMARY_REL: 100,
    SHIPPING_SUMMARY_REL: 100,
}

EXPECTED_SHIPPING_SCENARIOS = {
    "missing_key_medical",
    "missing_key_technical",
    "missing_key_quick",
    "missing_key_wait",
    "missing_key_collapse",
    "explicit_offline_medical",
    "loopback_online_quick",
    "loopback_online_technical",
    "unreachable_endpoint_quick",
}
EXPECTED_LOOPBACK_SHIPPING_SCENARIOS = {
    "loopback_online_quick",
    "loopback_online_technical",
}


def _decode_text(payload: bytes, label: str) -> str:
    encodings: list[str]
    if payload.startswith(b"\xef\xbb\xbf"):
        encodings = ["utf-8-sig", "utf-8", "gb18030", "utf-16"]
    elif payload.startswith((b"\xff\xfe", b"\xfe\xff")):
        encodings = ["utf-16", "utf-8-sig", "utf-8", "gb18030"]
    else:
        encodings = ["utf-8-sig", "utf-8", "gb18030", "utf-16"]
    for encoding in encodings:
        try:
            text = payload.decode(encoding)
        except UnicodeDecodeError:
            continue
        if "\ufffd" not in text:
            return text
    raise GateError(f"Cannot decode {label}")


def _validate_readme_contract(report: GateReport, readme_text: str) -> None:
    lines = readme_text.splitlines()
    if not lines or "v1.1" not in lines[0]:
        report.error("Packaged README first line must identify v1.1")
    if DISTRIBUTION_BANNER not in readme_text:
        report.error("Packaged README must identify the onsite competition demo")


def _load_json_bytes(payload: bytes, label: str) -> Any:
    try:
        return json.loads(_decode_text(payload, label))
    except json.JSONDecodeError as exc:
        raise GateError(f"Cannot parse JSON {label}: {exc}") from exc


def _credential_keys(
    value: object,
    prefix: str = "",
) -> Iterable[str]:
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


def _has_reparse_attribute(path: Path) -> bool:
    try:
        info = path.lstat()
    except OSError as exc:
        raise GateError(f"Cannot inspect artifact entry {path}: {exc}") from exc
    attributes = int(getattr(info, "st_file_attributes", 0))
    return path.is_symlink() or bool(attributes & FILE_ATTRIBUTE_REPARSE_POINT)


def _regular_artifact_files(artifact_root: Path) -> list[Path]:
    if _has_reparse_attribute(artifact_root):
        raise GateError(
            f"Artifact root is a symbolic link or reparse point: {artifact_root}"
        )
    files: list[Path] = []
    pending = [artifact_root]
    while pending:
        directory = pending.pop()
        try:
            entries = list(os.scandir(directory))
        except OSError as exc:
            raise GateError(
                f"Cannot enumerate artifact directory {directory}: {exc}"
            ) from exc
        for entry in entries:
            path = Path(entry.path)
            try:
                info = entry.stat(follow_symlinks=False)
            except OSError as exc:
                raise GateError(f"Cannot inspect artifact entry {path}: {exc}") from exc
            attributes = int(getattr(info, "st_file_attributes", 0))
            if entry.is_symlink() or (attributes & FILE_ATTRIBUTE_REPARSE_POINT):
                raise GateError(
                    "Artifact contains a symbolic link or reparse point: " f"{path}"
                )
            if stat.S_ISDIR(info.st_mode):
                pending.append(path)
            elif stat.S_ISREG(info.st_mode):
                files.append(path)
            else:
                raise GateError(f"Artifact contains a non-regular entry: {path}")
    return sorted(
        files,
        key=lambda path: path.relative_to(artifact_root).as_posix(),
    )


def _file_mtime_utc(path: Path) -> datetime:
    return datetime.fromtimestamp(
        path.stat().st_mtime,
        tz=timezone.utc,
    )


def _file_change_time_utc(path: Path) -> datetime:
    return datetime.fromtimestamp(
        path.stat().st_ctime,
        tz=timezone.utc,
    )


def _git_commit_time(repo_root: Path, source_commit: str) -> datetime:
    value = str(
        run_git(
            repo_root,
            "show",
            "-s",
            "--format=%cI",
            source_commit,
        )
    ).strip()
    return parse_utc_timestamp(value, "Git commit timestamp")


def _validate_committed_version_contract(
    repo_root: Path,
    source_commit: str,
) -> GateReport:
    report = GateReport()
    try:
        config_text = _decode_text(
            read_commit_blob(
                repo_root,
                source_commit,
                PROJECT_CONFIG_REL,
            ),
            f"{source_commit}:{PROJECT_CONFIG_REL}",
        )
        versions = re.findall(
            r"(?m)^\s*ProjectVersion\s*=\s*([^\r\n]+?)\s*$",
            config_text,
        )
        if versions != [PROJECT_VERSION]:
            report.error(
                "Source commit ProjectVersion mismatch: "
                f"expected {PROJECT_VERSION}, found {versions!r}"
            )

        rules = _load_json_bytes(
            read_commit_blob(repo_root, source_commit, RULES_REL),
            f"{source_commit}:{RULES_REL}",
        )
        if (
            not isinstance(rules, dict)
            or rules.get("rules_version") != PROJECT_VERSION
            or rules.get("schema_version") != 4
        ):
            report.error(
                "Source commit rules must declare "
                "rules_version=1.1.0 and schema_version=4"
            )

        agent = _load_json_bytes(
            read_commit_blob(
                repo_root,
                source_commit,
                AGENT_RUNTIME_REL,
            ),
            f"{source_commit}:{AGENT_RUNTIME_REL}",
        )
        if not isinstance(agent, dict):
            report.error("Source commit Agent runtime root must be an object")
        else:
            if (
                agent.get("runtime_version") != PROJECT_VERSION
                or agent.get("schema_version") != 4
            ):
                report.error(
                    "Source commit Agent runtime must declare "
                    "runtime_version=1.1.0 and schema_version=4"
                )
            if agent.get("llm_enabled") is not False:
                report.error(
                    "Source commit Agent runtime must default llm_enabled=false"
                )
            for key_path in _credential_keys(agent):
                report.error(
                    "Source commit Agent runtime contains forbidden "
                    f"credential field {key_path}"
                )
    except GateError as exc:
        report.error(f"Cannot validate committed v1.1 version contract: {exc}")
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
    if manifest.get("distribution_class") != DISTRIBUTION_CLASS:
        report.error("Manifest distribution_class must be " f"{DISTRIBUTION_CLASS}")
    if manifest.get("source_branch") != "main":
        report.error("Manifest source_branch must be main")

    run_id = manifest.get("run_id")
    if not isinstance(run_id, str) or not RUN_ID_PATTERN.fullmatch(run_id):
        report.error(
            "Manifest run_id must match "
            "YYYYMMDDTHHMMSSZ-<commit8>-<4..16 lowercase nonce>"
        )
        return None
    expected_root_name = f"{ARTIFACT_PREFIX}{run_id}"
    if artifact_root.name != expected_root_name:
        report.error(
            "Artifact directory must be the unique v1.1 run directory "
            f"{expected_root_name}; got {artifact_root.name}"
        )
    if manifest.get("artifact_root_name") != expected_root_name:
        report.error("Manifest artifact_root_name does not match its run_id")
    return run_id


def _validate_provenance(
    report: GateReport,
    manifest: dict[str, object],
    repo_root: Path,
    expected_source_ref: str,
) -> tuple[str | None, datetime | None]:
    source_commit = manifest.get("source_commit")
    source_tree = manifest.get("source_tree")
    if not isinstance(source_commit, str) or not OBJECT_ID_PATTERN.fullmatch(
        source_commit
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
        canonical_commit = resolve_commit(repo_root, source_commit)
        if canonical_commit != source_commit:
            report.error("Manifest source_commit must be the canonical full commit id")
        if source_commit != expected_commit:
            report.error(
                "Artifact source_commit is stale or mismatched: "
                f"manifest={source_commit} expected={expected_commit}"
            )
        actual_tree = resolve_tree(repo_root, source_commit)
        if source_tree != actual_tree:
            report.error(
                "Manifest source_tree mismatch: "
                f"manifest={source_tree} git={actual_tree}"
            )
        commit_time = _git_commit_time(repo_root, source_commit)
    except GateError as exc:
        report.error(f"Cannot verify manifest Git provenance: {exc}")
        return source_commit, None

    report.merge(
        _validate_committed_version_contract(
            repo_root,
            source_commit,
        )
    )
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
            "Artifact is stale: build is older than " f"{max_age_hours:g} hours"
        )
    if commit_time is not None and build_time < commit_time - FRESHNESS_SKEW:
        report.error("Build timestamp predates its source commit")
    return build_time


def _validate_pe64(path: Path, relative_path: str) -> str | None:
    try:
        file_size = path.stat().st_size
        with path.open("rb") as handle:
            dos_header = handle.read(64)
            if len(dos_header) != 64 or dos_header[:2] != b"MZ":
                return f"Packaged executable lacks DOS MZ header: {relative_path}"
            pe_offset = struct.unpack_from("<I", dos_header, 0x3C)[0]
            if pe_offset < 64 or pe_offset > file_size - 26:
                return f"Packaged executable has invalid PE offset: {relative_path}"
            handle.seek(pe_offset)
            pe_header = handle.read(26)
    except (OSError, struct.error) as exc:
        return f"Cannot inspect packaged executable {relative_path}: {exc}"

    if pe_header[:4] != b"PE\0\0":
        return f"Packaged executable lacks PE signature: {relative_path}"
    machine = struct.unpack_from("<H", pe_header, 4)[0]
    optional_size = struct.unpack_from("<H", pe_header, 20)[0]
    characteristics = struct.unpack_from("<H", pe_header, 22)[0]
    optional_magic = struct.unpack_from("<H", pe_header, 24)[0]
    if (
        machine != 0x8664
        or optional_size < 2
        or optional_magic != 0x020B
        or not characteristics & 0x0002
    ):
        return (
            "Packaged executable is not a Windows x64 PE32+ image: " f"{relative_path}"
        )
    return None


def _normalize_manifest_file_map(
    report: GateReport,
    value: object,
    *,
    manifest_rel: str,
) -> dict[str, str]:
    if not isinstance(value, dict):
        report.error("Manifest checksums must be an object")
        return {}
    normalized: dict[str, str] = {}
    casefold_paths: dict[str, str] = {}
    for raw_path, raw_digest in value.items():
        try:
            relative_path = validate_relative_path(
                raw_path,
                "Manifest checksum path",
            )
        except GateError as exc:
            report.error(str(exc))
            continue
        if relative_path == manifest_rel:
            report.error("Manifest must not checksum itself")
            continue
        folded = relative_path.casefold()
        if folded in casefold_paths:
            report.error(
                "Manifest has duplicate case-insensitive checksum paths: "
                f"{casefold_paths[folded]}, {relative_path}"
            )
            continue
        casefold_paths[folded] = relative_path
        if not isinstance(raw_digest, str) or not SHA256_PATTERN.fullmatch(raw_digest):
            report.error(f"Invalid SHA-256 for artifact path {relative_path}")
            continue
        normalized[relative_path] = raw_digest
    return normalized


def _validate_artifact_secrets(
    report: GateReport,
    artifact_root: Path,
    files: list[Path],
) -> None:
    findings: list[tuple[str, str]] = []
    for path in files:
        relative_path = artifact_relative_path(artifact_root, path)
        if not is_scannable_path(relative_path):
            continue
        try:
            payload = path.read_bytes()
        except OSError as exc:
            report.error(f"Cannot inspect packaged text file {relative_path}: {exc}")
            continue
        for finding_name in sorted(secret_finding_names(payload)):
            findings.append((relative_path, finding_name))
    for relative_path, finding_name in findings:
        report.error(
            "Release artifact contains credential-like material "
            f"({finding_name}) in {relative_path}; value suppressed"
        )


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
        files = _regular_artifact_files(artifact_root)
    except GateError as exc:
        report.error(str(exc))
        return
    try:
        manifest_rel = artifact_relative_path(
            artifact_root,
            manifest_path,
        )
    except GateError as exc:
        report.error(str(exc))
        return
    actual_files = {
        artifact_relative_path(artifact_root, path): path
        for path in files
        if artifact_relative_path(artifact_root, path) != manifest_rel
    }
    actual_paths = set(actual_files)

    for relative_path in REQUIRED_PACKAGE_FILES:
        path = actual_files.get(relative_path)
        if path is None:
            report.error(f"Missing required v1.1 artifact file: {relative_path}")
            continue
        minimum_size = MINIMUM_PACKAGE_SIZES.get(relative_path, 1)
        try:
            actual_size = path.stat().st_size
        except OSError as exc:
            report.error(f"Cannot inspect artifact size {relative_path}: {exc}")
            continue
        if actual_size < minimum_size:
            report.error(
                f"Artifact file is too small: {relative_path} "
                f"({actual_size} < {minimum_size})"
            )

    for relative_path, path in actual_files.items():
        try:
            file_size = path.stat().st_size
        except OSError as exc:
            report.error(f"Cannot inspect artifact size {relative_path}: {exc}")
            continue
        if file_size <= 0:
            report.error(f"Artifact contains an empty file: {relative_path}")
        file_name = Path(relative_path).name
        if (
            relative_path.startswith("Windows/WhiteoutStation/Binaries/Win64/")
            and file_name.startswith("WhiteoutStation-Win64-")
            and file_name.endswith(".exe")
            and relative_path != EXECUTABLE_RELS[1]
        ):
            report.error(
                "Non-Shipping game executable is forbidden: " f"{relative_path}"
            )

    checksums = _normalize_manifest_file_map(
        report,
        manifest.get("checksums"),
        manifest_rel=manifest_rel,
    )
    checksum_paths = set(checksums)
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
        path = actual_files[relative_path]
        try:
            actual_digest = sha256_file(path)
        except OSError as exc:
            report.error(f"Cannot hash artifact file {relative_path}: {exc}")
            continue
        if actual_digest != checksums[relative_path]:
            report.error(
                f"Artifact checksum mismatch: {relative_path}; "
                f"manifest={checksums[relative_path]} "
                f"actual={actual_digest}"
            )
        if build_time is not None:
            try:
                freshness = max(
                    _file_mtime_utc(path),
                    _file_change_time_utc(path),
                )
            except OSError as exc:
                report.error(
                    f"Cannot inspect artifact timestamp {relative_path}: {exc}"
                )
                continue
            if freshness < build_time - FRESHNESS_SKEW:
                report.error(
                    "Artifact file predates this build and may be stale: "
                    f"{relative_path}"
                )
            if freshness > now + FUTURE_SKEW:
                report.error(
                    "Artifact file timestamp is unreasonably in the future: "
                    f"{relative_path}"
                )

    for executable_rel in EXECUTABLE_RELS:
        path = actual_files.get(executable_rel)
        if path is None:
            continue
        pe_error = _validate_pe64(path, executable_rel)
        if pe_error:
            report.error(pe_error)

    _validate_artifact_secrets(report, artifact_root, files)


def _validate_package_contract(
    report: GateReport,
    repo_root: Path,
    artifact_root: Path,
    source_commit: str,
) -> None:
    source_mappings = {
        PACKAGED_RULES_REL: RULES_REL,
        PACKAGED_AGENT_RUNTIME_REL: AGENT_RUNTIME_REL,
        README_REL: README_SOURCE_REL,
        LICENSE_REL: LICENSE_SOURCE_REL,
    }
    for packaged_path, source_path in source_mappings.items():
        try:
            committed = read_commit_blob(
                repo_root,
                source_commit,
                source_path,
            )
            packaged = (artifact_root / packaged_path).read_bytes()
        except (GateError, OSError) as exc:
            report.error(f"Cannot compare packaged file {packaged_path}: {exc}")
            continue
        if packaged != committed:
            report.error(f"{packaged_path} does not match declared source_commit")

    try:
        rules = load_json_file(artifact_root / PACKAGED_RULES_REL)
        if (
            not isinstance(rules, dict)
            or rules.get("rules_version") != PROJECT_VERSION
            or rules.get("schema_version") != 4
        ):
            report.error(
                "Packaged rules must declare "
                "rules_version=1.1.0 and schema_version=4"
            )
    except GateError as exc:
        report.error(str(exc))

    try:
        agent = load_json_file(artifact_root / PACKAGED_AGENT_RUNTIME_REL)
        if not isinstance(agent, dict):
            report.error("Packaged Agent runtime root must be an object")
        else:
            if (
                agent.get("runtime_version") != PROJECT_VERSION
                or agent.get("schema_version") != 4
            ):
                report.error(
                    "Packaged Agent runtime must declare "
                    "runtime_version=1.1.0 and schema_version=4"
                )
            if agent.get("llm_enabled") is not False:
                report.error("Packaged Agent runtime must default llm_enabled=false")
            if agent.get("transport") != "openai_chat_completions":
                report.error(
                    "Packaged Agent transport must be " "openai_chat_completions"
                )
            for field in ("provider_name", "model", "endpoint"):
                value = agent.get(field)
                if not isinstance(value, str) or not value.strip():
                    report.error(f"Packaged Agent {field} must be non-empty")
            endpoint = agent.get("endpoint")
            if isinstance(endpoint, str) and not endpoint.startswith("https://"):
                report.error("Packaged Agent default endpoint must use HTTPS")
            for key_path in _credential_keys(agent):
                report.error(
                    "Packaged Agent runtime contains forbidden "
                    f"credential field {key_path}"
                )
    except GateError as exc:
        report.error(str(exc))

    try:
        readme_text = _decode_text(
            (artifact_root / README_REL).read_bytes(),
            README_REL,
        )
        _validate_readme_contract(report, readme_text)
    except (GateError, OSError) as exc:
        report.error(f"Cannot validate packaged README: {exc}")

    try:
        license_text = _decode_text(
            (artifact_root / LICENSE_REL).read_bytes(),
            LICENSE_REL,
        )
        if (
            "asset" not in license_text.casefold()
            or "license" not in license_text.casefold()
        ):
            report.error("Packaged ASSET_LICENSES.md lacks asset-license content")
    except (GateError, OSError) as exc:
        report.error(f"Cannot validate packaged ASSET_LICENSES.md: {exc}")


def _scenario_map(
    report: GateReport,
    scenarios: object,
    *,
    expected_ids: set[str],
    label: str,
) -> dict[str, dict[str, Any]]:
    if not isinstance(scenarios, list):
        report.error(f"{label} scenarios must be an array")
        return {}
    result: dict[str, dict[str, Any]] = {}
    for scenario in scenarios:
        if not isinstance(scenario, dict):
            report.error(f"{label} scenario must be an object")
            continue
        scenario_id = scenario.get("scenario_id")
        if not isinstance(scenario_id, str) or not scenario_id:
            report.error(f"{label} scenario_id is missing")
            continue
        if scenario_id in result:
            report.error(f"{label} has duplicate scenario_id {scenario_id}")
            continue
        result[scenario_id] = scenario
        if scenario.get("passed") is not True:
            report.error(f"{label} scenario did not pass: {scenario_id}")
    actual_ids = set(result)
    if actual_ids != expected_ids:
        report.error(
            f"{label} scenario matrix mismatch: "
            f"expected={sorted(expected_ids)!r} "
            f"actual={sorted(actual_ids)!r}"
        )
    return result


def _capture_names(scenario: dict[str, Any]) -> set[str]:
    captures = scenario.get("captures")
    if not isinstance(captures, list):
        return set()
    return {
        str(capture.get("name"))
        for capture in captures
        if isinstance(capture, dict) and isinstance(capture.get("name"), str)
    }


def _validate_input_smoke(
    report: GateReport,
    artifact_root: Path,
) -> None:
    try:
        summary = load_json_file(artifact_root / INPUT_SUMMARY_REL)
    except GateError as exc:
        report.error(str(exc))
        return
    if not isinstance(summary, dict):
        report.error("v1.1 real-input smoke summary root must be an object")
        return
    if (
        summary.get("schema") != "whiteout.v1.1.real-input-smoke.v1"
        or summary.get("passed") is not True
    ):
        report.error("v1.1 real-input smoke summary is not a passing report")
    if summary.get("artifact_root_name") != artifact_root.name:
        report.error("v1.1 real-input smoke artifact identity mismatch")
    credential_policy = summary.get("credential_policy")
    if (
        not isinstance(credential_policy, dict)
        or credential_policy.get("child_api_key_forced_empty") is not True
        or credential_policy.get("api_key_value_persisted") is not False
        or credential_policy.get("loopback_authorization_present") is not False
    ):
        report.error("v1.1 real-input smoke credential policy is incomplete")

    scenarios = _scenario_map(
        report,
        summary.get("scenarios"),
        expected_ids={"antenna_calibration", "dialogue_free_text"},
        label="v1.1 real-input smoke",
    )
    antenna = scenarios.get("antenna_calibration")
    if antenna is not None:
        real_inputs = antenna.get("real_inputs")
        calibration = antenna.get("calibration_event")
        game_log = antenna.get("game_log")
        if (
            not isinstance(real_inputs, list)
            or "F_preview" not in real_inputs
            or "F_commit" not in real_inputs
        ):
            report.error("v1.1 antenna input smoke lacks real F preview/commit")
        if (
            not isinstance(calibration, dict)
            or calibration.get("action_id") != "calibrate_antenna"
            or calibration.get("reason_code") != "Committed"
            or not isinstance(
                calibration.get("ap_before"),
                (int, float),
            )
            or not isinstance(
                calibration.get("ap_after"),
                (int, float),
            )
            or isinstance(calibration.get("ap_before"), bool)
            or isinstance(calibration.get("ap_after"), bool)
            or float(calibration.get("ap_after", 0))
            >= float(calibration.get("ap_before", 0))
        ):
            report.error("v1.1 real-input smoke does not prove antenna calibration")
        if (
            not isinstance(game_log, dict)
            or game_log.get("antenna_prep_ready") is not True
            or game_log.get("input_target_ready") is not True
            or game_log.get("readiness_source")
            not in {
                "shipping_log",
                "prepared_autosave_and_calibration_event",
            }
        ):
            report.error("v1.1 antenna smoke lacks ready/final-target game log proof")
        antenna_capture_names = _capture_names(antenna)
        for suffix in (
            "_final_proxy_focus",
            "_preview",
            "_committed",
        ):
            if not any(name.endswith(suffix) for name in antenna_capture_names):
                report.error("v1.1 antenna smoke lacks capture ending " f"{suffix}")

    dialogue = scenarios.get("dialogue_free_text")
    if dialogue is not None:
        real_inputs = dialogue.get("real_inputs")
        character_count = dialogue.get("player_text_characters")
        mock_audit = dialogue.get("mock_audit")
        game_audit = dialogue.get("game_model_audit")
        phase_prerequisite = dialogue.get("phase_prerequisite")
        if (
            not isinstance(real_inputs, list)
            or "unicode_chinese_free_text" not in real_inputs
            or "Enter_submit" not in real_inputs
            or isinstance(character_count, bool)
            or not isinstance(character_count, int)
            or character_count <= 0
            or not any(
                name.endswith("_chinese_typed") for name in _capture_names(dialogue)
            )
        ):
            report.error("v1.1 dialogue smoke lacks Chinese real-input evidence")
        if (
            not isinstance(phase_prerequisite, dict)
            or phase_prerequisite.get("autosave_changed") is not True
            or phase_prerequisite.get("heating_action") != "heat_control_room"
        ):
            report.error("v1.1 dialogue smoke lacks a legal phase prerequisite")
        if (
            not isinstance(mock_audit, dict)
            or mock_audit.get("has_player_text") is not True
            or mock_audit.get("response_format_json_object") is not True
            or mock_audit.get("authorization_present") is not False
            or mock_audit.get("status_code") != 200
        ):
            report.error(
                "v1.1 dialogue loopback lacks player text, JSON response "
                "format, HTTP 200, or no-Authorization proof"
            )
        if (
            not isinstance(game_audit, dict)
            or game_audit.get("kind") != "expression"
            or game_audit.get("action_id") != "talk_gu_heng"
            or game_audit.get("outcome") != "accepted"
            or game_audit.get("http_status") != 200
        ):
            report.error("v1.1 dialogue game ModelAudit is not accepted/HTTP 200")


def _validate_shipping_smoke(
    report: GateReport,
    artifact_root: Path,
) -> None:
    try:
        summary = load_json_file(artifact_root / SHIPPING_SUMMARY_REL)
    except GateError as exc:
        report.error(str(exc))
        return
    if not isinstance(summary, dict):
        report.error("v1.1 Shipping smoke summary root must be an object")
        return
    if (
        summary.get("schema") != "whiteout.v1.1.shipping-smoke.v1"
        or summary.get("passed") is not True
    ):
        report.error("v1.1 Shipping smoke summary is not a passing report")
    if summary.get("artifact_root_name") != artifact_root.name:
        report.error("v1.1 Shipping smoke artifact identity mismatch")
    credential_policy = summary.get("credential_policy")
    if (
        not isinstance(credential_policy, dict)
        or credential_policy.get("api_key_value_read") is not False
        or credential_policy.get("api_key_value_persisted") is not False
        or credential_policy.get("child_api_key_forced_empty") is not True
        or credential_policy.get("local_credential_config_accepted") is not False
    ):
        report.error("v1.1 Shipping smoke credential policy is incomplete")

    scenarios = _scenario_map(
        report,
        summary.get("scenarios"),
        expected_ids=EXPECTED_SHIPPING_SCENARIOS,
        label="v1.1 Shipping smoke",
    )
    for scenario_id, scenario in scenarios.items():
        if (
            scenario.get("exit_code") != 0
            or scenario.get("api_key_supplied") is not False
        ):
            report.error(
                "v1.1 Shipping scenario lacks clean exit/no-key proof: "
                f"{scenario_id}"
            )
    for scenario_id in EXPECTED_LOOPBACK_SHIPPING_SCENARIOS:
        scenario = scenarios.get(scenario_id)
        if scenario is None:
            continue
        model_audit = scenario.get("model_audit")
        loopback = scenario.get("loopback_contract")
        if (
            not isinstance(model_audit, dict)
            or not isinstance(model_audit.get("outcomes"), list)
            or not model_audit["outcomes"]
            or any(outcome != "accepted" for outcome in model_audit["outcomes"])
            or not isinstance(
                model_audit.get("http_statuses"),
                list,
            )
            or any(status != 200 for status in model_audit["http_statuses"])
        ):
            report.error(
                "v1.1 Shipping loopback ModelAudit is not fully "
                f"accepted/HTTP 200: {scenario_id}"
            )
        if (
            not isinstance(loopback, dict)
            or loopback.get("authorization_present") is not False
        ):
            report.error("v1.1 Shipping loopback sent Authorization: " f"{scenario_id}")

    history = summary.get("dialogue_history_probe")
    if (
        not isinstance(history, dict)
        or history.get("passed") is not True
        or history.get("requests") != 2
        or history.get("history_turns") != [0, 1]
        or history.get("authorization_present") is not False
    ):
        report.error("v1.1 dialogue-history Shipping probe is incomplete")
    performances = summary.get("performance_probes")
    if not isinstance(performances, list) or len(performances) != 2:
        report.error("v1.1 Shipping smoke must contain two NPC performances")
    else:
        actions = {
            performance.get("action_id")
            for performance in performances
            if isinstance(performance, dict)
            and performance.get("passed") is True
            and performance.get("movement_intent") == "step_closer"
            and performance.get("reaction_action") == "acknowledge"
        }
        if actions != {"talk_gu_heng", "talk_ye_cheng"}:
            report.error("v1.1 Shipping smoke lacks both NPC performance probes")
    ai_ab = summary.get("ai_ab")
    if (
        not isinstance(ai_ab, dict)
        or ai_ab.get("route") != "technical"
        or ai_ab.get("authoritative_results_equal") is not True
    ):
        report.error("v1.1 Shipping AI A/B authority check is incomplete")


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
    now = (now or datetime.now(timezone.utc)).astimezone(timezone.utc)
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
        if _has_reparse_attribute(artifact_root):
            report.error("Artifact root must not be a symbolic link or reparse point")
            return report
        artifact_root = artifact_root.resolve(strict=True)
    except (OSError, GateError) as exc:
        report.error(f"Artifact root does not exist: {exc}")
        return report
    if not artifact_root.is_dir():
        report.error(f"Artifact root is not a directory: {artifact_root}")
        return report
    if not artifact_root.name.startswith(ARTIFACT_PREFIX):
        report.error(
            "Artifact root must be an explicitly selected unique v1.1 "
            f"directory; got {artifact_root.name}"
        )
    run_id_from_root = artifact_root.name[len(ARTIFACT_PREFIX) :]
    if not RUN_ID_PATTERN.fullmatch(run_id_from_root):
        report.error("Artifact root has an invalid v1.1 unique-run id")
    try:
        ensure_unique_artifact_root(artifact_root)
    except GateError as exc:
        report.error(str(exc))

    candidate_manifest = manifest_path or (artifact_root / MANIFEST_REL)
    try:
        if _has_reparse_attribute(candidate_manifest):
            raise GateError(
                "Release manifest must not be a symbolic link or " "reparse point"
            )
        resolved_manifest = candidate_manifest.resolve(strict=True)
        manifest_relative = resolved_manifest.relative_to(artifact_root).as_posix()
        if manifest_relative != MANIFEST_REL:
            raise GateError(f"Release manifest must be exactly {MANIFEST_REL}")
    except (OSError, ValueError, GateError) as exc:
        report.error(
            "Manifest must exist at the fixed path inside the selected "
            f"artifact root: {exc}"
        )
        return report
    try:
        manifest = load_json_file(resolved_manifest)
    except GateError as exc:
        report.error(str(exc))
        return report
    if not isinstance(manifest, dict):
        report.error("Release manifest root must be an object")
        return report

    run_id = _validate_manifest_identity(
        report,
        manifest,
        artifact_root,
    )
    source_commit, commit_time = _validate_provenance(
        report,
        manifest,
        repo_root,
        expected_source_ref,
    )
    if run_id and source_commit and run_id.split("-")[1] != source_commit[:8]:
        report.error("Manifest run_id commit prefix does not match source_commit")

    build_time = _validate_build_time(
        report,
        manifest,
        commit_time,
        now=now,
        max_age_hours=max_age_hours,
    )
    if build_time is not None:
        try:
            root_freshness = max(
                _file_mtime_utc(artifact_root),
                _file_change_time_utc(artifact_root),
            )
        except OSError as exc:
            report.error(f"Cannot inspect artifact root timestamp: {exc}")
        else:
            if root_freshness < build_time - FRESHNESS_SKEW:
                report.error(
                    "Artifact output directory predates the declared "
                    "build and is stale"
                )
    if manifest.get("engine_version") != ENGINE_VERSION:
        report.error(f"Manifest engine_version must be {ENGINE_VERSION}")
    python_version = manifest.get("python_version")
    if not isinstance(python_version, str) or not re.fullmatch(
        r"\d+\.\d+(?:\.\d+)?", python_version
    ):
        report.error("Manifest python_version is missing or malformed")

    report.merge(validate_protected_assets(repo_root, require_tracked=True))
    _validate_artifact_files(
        report,
        artifact_root,
        resolved_manifest,
        manifest,
        build_time,
        now=now,
    )
    _validate_input_smoke(report, artifact_root)
    _validate_shipping_smoke(report, artifact_root)
    if source_commit:
        _validate_package_contract(
            report,
            repo_root,
            artifact_root,
            source_commit,
        )

    if not report.errors:
        report.detail(
            f"Validated fresh v1.1 artifact {artifact_root.name} "
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
        "RELEASE VALIDATION v1.1: PASS"
        if report.passed
        else ("RELEASE VALIDATION v1.1: FAIL " f"({len(report.errors)} error(s))")
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=REPO_ROOT,
    )
    parser.add_argument(
        "--artifact-root",
        type=Path,
        required=True,
        help=(
            "Exact unique WhiteoutStation-v1.1-Win64-* artifact "
            "directory; no auto-discovery"
        ),
    )
    parser.add_argument("--manifest", type=Path)
    parser.add_argument(
        "--expected-source-ref",
        default="HEAD",
    )
    parser.add_argument(
        "--max-age-hours",
        type=float,
        default=72.0,
    )
    args = parser.parse_args()
    if hasattr(sys.stdout, "reconfigure"):
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
