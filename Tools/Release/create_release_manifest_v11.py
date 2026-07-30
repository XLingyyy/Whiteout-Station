"""Create the checksum/provenance manifest for a completed v1.1 package."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Iterable

try:
    from .v11_gate_common import (
        AGENT_RUNTIME_REL,
        ARTIFACT_PREFIX,
        DISTRIBUTION_CLASS,
        MANIFEST_REL,
        MANIFEST_SCHEMA,
        PACKAGED_AGENT_RUNTIME_REL,
        PACKAGED_RULES_REL,
        PROJECT_VERSION,
        REQUIRED_PACKAGE_FILES,
        RULES_REL,
        RUN_ID_PATTERN,
        GateError,
        artifact_relative_path,
        ensure_unique_artifact_root,
        format_utc,
        is_scannable_path,
        load_json_file,
        parse_utc_timestamp,
        read_commit_blob,
        regular_artifact_files,
        require_clean_main,
        resolve_commit,
        resolve_tree,
        secret_finding_names,
        sha256_file,
    )
except ImportError:
    from v11_gate_common import (
        AGENT_RUNTIME_REL,
        ARTIFACT_PREFIX,
        DISTRIBUTION_CLASS,
        MANIFEST_REL,
        MANIFEST_SCHEMA,
        PACKAGED_AGENT_RUNTIME_REL,
        PACKAGED_RULES_REL,
        PROJECT_VERSION,
        REQUIRED_PACKAGE_FILES,
        RULES_REL,
        RUN_ID_PATTERN,
        GateError,
        artifact_relative_path,
        ensure_unique_artifact_root,
        format_utc,
        is_scannable_path,
        load_json_file,
        parse_utc_timestamp,
        read_commit_blob,
        regular_artifact_files,
        require_clean_main,
        resolve_commit,
        resolve_tree,
        secret_finding_names,
        sha256_file,
    )


REPO_ROOT = Path(__file__).resolve().parents[2]
ENGINE_VERSION_PATTERN = re.compile(r"^\d+\.\d+(?:\.\d+)?$")


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


def _validate_packaged_contract(artifact_root: Path) -> None:
    rules = load_json_file(artifact_root / PACKAGED_RULES_REL)
    if not isinstance(rules, dict):
        raise GateError("Packaged v1.1 rules root must be an object")
    if (
        rules.get("rules_version") != PROJECT_VERSION
        or rules.get("schema_version") != 4
    ):
        raise GateError(
            "Packaged rules must declare rules_version=1.1.0 and schema_version=4"
        )

    agent = load_json_file(artifact_root / PACKAGED_AGENT_RUNTIME_REL)
    if not isinstance(agent, dict):
        raise GateError("Packaged v1.1 Agent runtime root must be an object")
    if (
        agent.get("runtime_version") != PROJECT_VERSION
        or agent.get("schema_version") != 4
    ):
        raise GateError(
            "Packaged Agent runtime must declare runtime_version=1.1.0 "
            "and schema_version=4"
        )
    if agent.get("llm_enabled") is not False:
        raise GateError("Packaged Agent runtime must default llm_enabled=false")
    credential_fields = list(_credential_keys(agent))
    if credential_fields:
        raise GateError(
            "Packaged Agent runtime contains forbidden credential field(s): "
            + ", ".join(credential_fields)
        )


def _validate_artifact_secrets(relative_files: dict[str, Path]) -> None:
    findings: list[tuple[str, str]] = []
    for relative_path, path in relative_files.items():
        if not is_scannable_path(relative_path):
            continue
        try:
            payload = path.read_bytes()
        except OSError as exc:
            raise GateError(
                f"Cannot inspect packaged file {relative_path}: {exc}"
            ) from exc
        for pattern_name in sorted(secret_finding_names(payload)):
            findings.append((relative_path, pattern_name))
    if findings:
        summary = ", ".join(
            f"{pattern_name} in {relative_path}"
            for relative_path, pattern_name in findings
        )
        raise GateError(
            "Release artifact contains credential-like material; "
            f"values suppressed: {summary}"
        )


def create_manifest(
    repo_root: Path,
    artifact_root: Path,
    *,
    run_id: str,
    source_ref: str,
    build_timestamp_utc: str,
    engine_version: str = "5.8.1",
) -> Path:
    repo_root = repo_root.resolve(strict=True)
    if artifact_root.is_symlink():
        raise GateError(f"Artifact root must not be a symbolic link: {artifact_root}")
    artifact_root = artifact_root.resolve(strict=True)
    if not artifact_root.is_dir():
        raise GateError(f"Artifact root is not a directory: {artifact_root}")
    if not RUN_ID_PATTERN.fullmatch(run_id):
        raise GateError("run_id does not match the v1.1 unique-run format")
    expected_name = f"{ARTIFACT_PREFIX}{run_id}"
    if artifact_root.name != expected_name:
        raise GateError(
            f"Artifact root must be named {expected_name}; got {artifact_root.name}"
        )
    ensure_unique_artifact_root(artifact_root)

    head_commit = require_clean_main(repo_root)
    source_commit = resolve_commit(repo_root, source_ref)
    if source_commit != head_commit:
        raise GateError(
            "source_ref must resolve to the clean main HEAD used for packaging"
        )
    source_tree = resolve_tree(repo_root, source_commit)
    if run_id.split("-")[1] != source_commit[:8]:
        raise GateError("run_id commit prefix does not match source_ref")
    build_time = parse_utc_timestamp(
        build_timestamp_utc,
        "build_timestamp_utc",
    )
    if not ENGINE_VERSION_PATTERN.fullmatch(engine_version):
        raise GateError("engine_version must be a numeric Unreal Engine version")

    manifest_path = artifact_root / MANIFEST_REL
    if manifest_path.exists():
        raise GateError(
            f"Refusing to overwrite an existing release manifest: {manifest_path}"
        )

    files = regular_artifact_files(artifact_root)
    relative_files = {
        artifact_relative_path(artifact_root, path): path for path in files
    }
    missing = [
        relative_path
        for relative_path in REQUIRED_PACKAGE_FILES
        if relative_path not in relative_files
    ]
    if missing:
        raise GateError(
            "Cannot create manifest; required package files are missing: "
            + ", ".join(missing)
        )

    committed_payloads = {
        PACKAGED_RULES_REL: read_commit_blob(repo_root, source_commit, RULES_REL),
        PACKAGED_AGENT_RUNTIME_REL: read_commit_blob(
            repo_root,
            source_commit,
            AGENT_RUNTIME_REL,
        ),
    }
    for packaged_path, committed_payload in committed_payloads.items():
        try:
            packaged_payload = relative_files[packaged_path].read_bytes()
        except OSError as exc:
            raise GateError(
                f"Cannot read packaged file {packaged_path}: {exc}"
            ) from exc
        if packaged_payload != committed_payload:
            raise GateError(
                f"{packaged_path} does not match the declared source_commit"
            )

    _validate_packaged_contract(artifact_root)
    _validate_artifact_secrets(relative_files)

    checksums = {
        relative_path: sha256_file(path)
        for relative_path, path in sorted(relative_files.items())
    }
    manifest = {
        "schema": MANIFEST_SCHEMA,
        "version": PROJECT_VERSION,
        "distribution_class": DISTRIBUTION_CLASS,
        "run_id": run_id,
        "artifact_root_name": expected_name,
        "source_commit": source_commit,
        "source_tree": source_tree,
        "source_dirty": False,
        "source_branch": "main",
        "build_timestamp_utc": format_utc(build_time),
        "engine_version": engine_version,
        "python_version": ".".join(map(str, sys.version_info[:3])),
        "checksums": checksums,
    }
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    return manifest_path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=REPO_ROOT)
    parser.add_argument("--artifact-root", type=Path, required=True)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--source-ref", default="HEAD")
    parser.add_argument("--build-timestamp-utc", required=True)
    parser.add_argument("--engine-version", default="5.8.1")
    args = parser.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")
    try:
        manifest_path = create_manifest(
            args.repo_root,
            args.artifact_root,
            run_id=args.run_id,
            source_ref=args.source_ref,
            build_timestamp_utc=args.build_timestamp_utc,
            engine_version=args.engine_version,
        )
    except (GateError, OSError) as exc:
        print(f"MANIFEST v1.1: FAIL: {exc}")
        return 1
    print(f"MANIFEST v1.1: CREATED {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
