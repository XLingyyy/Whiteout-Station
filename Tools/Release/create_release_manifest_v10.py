"""Create the checksum/provenance manifest for a completed v1.0 package.

Run this only from the clean detached worktree used for the build.  The script
fails on every dirty path, including the user's editable map, and never tries
to clean or alter the worktree.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

try:
    from .v10_gate_common import (
        DISTRIBUTION_CLASS,
        MANIFEST_REL,
        MANIFEST_SCHEMA,
        PROJECT_VERSION,
        REQUIRED_PACKAGE_FILES,
        RUN_ID_PATTERN,
        GateError,
        artifact_relative_path,
        format_utc,
        git_status_entries,
        parse_utc_timestamp,
        regular_artifact_files,
        resolve_commit,
        resolve_tree,
        sha256_file,
    )
except ImportError:
    from v10_gate_common import (
        DISTRIBUTION_CLASS,
        MANIFEST_REL,
        MANIFEST_SCHEMA,
        PROJECT_VERSION,
        REQUIRED_PACKAGE_FILES,
        RUN_ID_PATTERN,
        GateError,
        artifact_relative_path,
        format_utc,
        git_status_entries,
        parse_utc_timestamp,
        regular_artifact_files,
        resolve_commit,
        resolve_tree,
        sha256_file,
    )


REPO_ROOT = Path(__file__).resolve().parents[2]


def create_manifest(
    repo_root: Path,
    artifact_root: Path,
    *,
    run_id: str,
    source_ref: str,
    build_timestamp_utc: str,
    engine_version: str = "5.8.0",
) -> Path:
    repo_root = repo_root.resolve(strict=True)
    artifact_root = artifact_root.resolve(strict=True)
    if not artifact_root.is_dir():
        raise GateError(f"Artifact root is not a directory: {artifact_root}")
    if not RUN_ID_PATTERN.fullmatch(run_id):
        raise GateError("run_id does not match the v1.0 unique-run format")
    expected_name = f"WhiteoutStation-v1.0-Win64-{run_id}"
    if artifact_root.name != expected_name:
        raise GateError(
            f"Artifact root must be named {expected_name}; got {artifact_root.name}"
        )

    statuses = git_status_entries(repo_root)
    if statuses:
        summary = ", ".join(f"{status} {path}" for status, path in statuses)
        raise GateError(
            "Manifest creation requires a clean detached build worktree. "
            f"Dirty paths were reported and left untouched: {summary}"
        )

    source_commit = resolve_commit(repo_root, source_ref)
    source_tree = resolve_tree(repo_root, source_commit)
    if run_id.split("-")[1] != source_commit[:8]:
        raise GateError("run_id commit prefix does not match source_ref")
    build_time = parse_utc_timestamp(
        build_timestamp_utc,
        "build_timestamp_utc",
    )

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
    parser.add_argument("--engine-version", default="5.8.0")
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
        print(f"MANIFEST v1.0: FAIL: {exc}")
        return 1
    print(f"MANIFEST v1.0: CREATED {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
