"""Shared primitives for the Whiteout Station v0.9 release gates.

The helpers in this module are intentionally limited to read-only repository
inspection and deterministic artifact hashing.  They never modify or clean a
Git worktree.
"""

from __future__ import annotations

import hashlib
import json
import os
import re
import subprocess
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path, PurePosixPath
from typing import Any, Iterable


PROJECT_VERSION = "0.9.0"
VERSION_TOKEN = "v0.9"
DISTRIBUTION_CLASS = "local_review_only"
PROJECT_CONFIG_REL = "WhiteoutStation/Config/DefaultGame.ini"
RULES_REL = "WhiteoutStation/Content/Rules/WhiteoutStationRules.v0.9.json"
AGENT_RUNTIME_REL = "WhiteoutStation/Content/Agents/AgentRuntime.v0.9.json"
PROTECTED_MANIFEST_REL = "docs/PROTECTED_CHARACTER_ASSETS_v0.9.json"
USER_MAP_REL = "WhiteoutStation/Content/WindStation/World/MVP_StationMap.umap"

REQUIRED_PACKAGE_FILES = (
    "Windows/WhiteoutStation.exe",
    "Windows/WhiteoutStation/Binaries/Win64/WhiteoutStation-Win64-Shipping.exe",
    "Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.pak",
    "Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.ucas",
    "Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.utoc",
    "Windows/WhiteoutStation/Content/Rules/WhiteoutStationRules.v0.9.json",
    "Windows/WhiteoutStation/Content/Agents/AgentRuntime.v0.9.json",
    "README_v0.9.txt",
    "ASSET_LICENSES.md",
    "Validation/InputSmoke/input_smoke_summary.json",
    "Validation/ShippingSmoke/shipping_smoke_summary.json",
)

MANIFEST_REL = "Validation/gate_manifest.json"
MANIFEST_SCHEMA = "whiteout.v0.9.release-manifest.v1"
RUN_ID_PATTERN = re.compile(
    r"^\d{8}T\d{6}Z-[0-9a-f]{8}-[a-z0-9]{4,16}$"
)
OBJECT_ID_PATTERN = re.compile(r"^[0-9a-f]{40,64}$")
SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")


class GateError(RuntimeError):
    """Raised when a gate cannot safely inspect its requested input."""


@dataclass
class GateReport:
    errors: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)
    details: list[str] = field(default_factory=list)

    @property
    def passed(self) -> bool:
        return not self.errors

    def error(self, message: str) -> None:
        self.errors.append(message)

    def warn(self, message: str) -> None:
        self.warnings.append(message)

    def detail(self, message: str) -> None:
        self.details.append(message)

    def merge(self, other: "GateReport") -> None:
        self.errors.extend(other.errors)
        self.warnings.extend(other.warnings)
        self.details.extend(other.details)


def decode_process_output(payload: bytes) -> str:
    for encoding in ("utf-8-sig", "utf-8", "gb18030", "utf-16"):
        try:
            return payload.decode(encoding)
        except UnicodeDecodeError:
            continue
    return payload.decode("utf-8", errors="replace")


def run_command(
    arguments: list[str],
    *,
    cwd: Path,
    check: bool = True,
) -> subprocess.CompletedProcess[bytes]:
    completed = subprocess.run(
        arguments,
        cwd=cwd,
        check=False,
        capture_output=True,
    )
    if check and completed.returncode != 0:
        stderr = decode_process_output(completed.stderr).strip()
        stdout = decode_process_output(completed.stdout).strip()
        summary = stderr or stdout or f"exit code {completed.returncode}"
        raise GateError(f"{arguments[0]} failed: {summary}")
    return completed


def run_git(
    repo_root: Path,
    *arguments: str,
    check: bool = True,
    text: bool = True,
) -> str | bytes:
    completed = run_command(
        ["git", "-c", "core.quotepath=false", *arguments],
        cwd=repo_root,
        check=check,
    )
    if text:
        return decode_process_output(completed.stdout)
    return completed.stdout


def resolve_commit(repo_root: Path, ref: str) -> str:
    value = str(run_git(repo_root, "rev-parse", "--verify", f"{ref}^{{commit}}")).strip()
    if not OBJECT_ID_PATTERN.fullmatch(value):
        raise GateError(f"Git returned an invalid commit id for {ref!r}")
    return value


def resolve_tree(repo_root: Path, ref: str) -> str:
    value = str(run_git(repo_root, "rev-parse", "--verify", f"{ref}^{{tree}}")).strip()
    if not OBJECT_ID_PATTERN.fullmatch(value):
        raise GateError(f"Git returned an invalid tree id for {ref!r}")
    return value


def resolve_path_object(repo_root: Path, ref: str, relative_path: str) -> str:
    value = str(
        run_git(repo_root, "rev-parse", "--verify", f"{ref}:{relative_path}")
    ).strip()
    if not OBJECT_ID_PATTERN.fullmatch(value):
        raise GateError(f"Git returned an invalid object id for {ref}:{relative_path}")
    return value


def git_object_type(repo_root: Path, object_id: str) -> str:
    return str(run_git(repo_root, "cat-file", "-t", object_id)).strip()


def git_commit_time(repo_root: Path, ref: str) -> datetime:
    raw = str(run_git(repo_root, "show", "-s", "--format=%cI", ref)).strip()
    return parse_utc_timestamp(raw, "Git commit timestamp")


def git_status_entries(
    repo_root: Path,
    pathspecs: Iterable[str] = (),
) -> list[tuple[str, str]]:
    arguments = ["status", "--porcelain=v1", "-z", "--untracked-files=all"]
    pathspec_list = list(pathspecs)
    if pathspec_list:
        arguments.extend(["--", *pathspec_list])
    payload = bytes(run_git(repo_root, *arguments, text=False))
    records = payload.split(b"\0")
    entries: list[tuple[str, str]] = []
    index = 0
    while index < len(records):
        record = records[index]
        index += 1
        if not record:
            continue
        if len(record) < 4:
            raise GateError("Git returned malformed porcelain status output")
        status = record[:2].decode("ascii", errors="replace")
        path = decode_process_output(record[3:]).replace("\\", "/")
        entries.append((status, path))
        if ("R" in status or "C" in status) and index < len(records):
            index += 1
    return entries


def tracked_paths(repo_root: Path) -> list[str]:
    payload = bytes(run_git(repo_root, "ls-files", "-z", text=False))
    return [
        decode_process_output(item).replace("\\", "/")
        for item in payload.split(b"\0")
        if item
    ]


def read_index_blob(repo_root: Path, relative_path: str) -> bytes:
    return bytes(run_git(repo_root, "show", f":{relative_path}", text=False))


def read_commit_blob(repo_root: Path, ref: str, relative_path: str) -> bytes:
    return bytes(run_git(repo_root, "show", f"{ref}:{relative_path}", text=False))


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_json_file(path: Path) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise GateError(f"Cannot read JSON {path}: {exc}") from exc


def load_json_bytes(payload: bytes, label: str) -> Any:
    for encoding in ("utf-8-sig", "utf-8", "gb18030", "utf-16"):
        try:
            text = payload.decode(encoding)
            break
        except UnicodeDecodeError:
            continue
    else:
        raise GateError(f"Cannot decode JSON {label}")
    try:
        return json.loads(text)
    except json.JSONDecodeError as exc:
        raise GateError(f"Cannot parse JSON {label}: {exc}") from exc


def parse_utc_timestamp(value: object, label: str) -> datetime:
    if not isinstance(value, str) or not value.strip():
        raise GateError(f"{label} must be a non-empty ISO-8601 timestamp")
    normalized = value.strip()
    if normalized.endswith("Z"):
        normalized = normalized[:-1] + "+00:00"
    try:
        parsed = datetime.fromisoformat(normalized)
    except ValueError as exc:
        raise GateError(f"{label} is not valid ISO-8601: {value!r}") from exc
    if parsed.tzinfo is None:
        raise GateError(f"{label} must include a UTC offset")
    return parsed.astimezone(timezone.utc)


def utc_now() -> datetime:
    return datetime.now(timezone.utc)


def format_utc(value: datetime) -> str:
    return value.astimezone(timezone.utc).isoformat().replace("+00:00", "Z")


def validate_relative_path(value: object, label: str) -> str:
    if not isinstance(value, str) or not value:
        raise GateError(f"{label} must be a non-empty relative path")
    normalized = value.replace("\\", "/")
    candidate = PurePosixPath(normalized)
    if candidate.is_absolute() or ".." in candidate.parts or "." in candidate.parts:
        raise GateError(f"{label} escapes the artifact/repository root: {value!r}")
    if normalized.startswith("/") or re.match(r"^[A-Za-z]:", normalized):
        raise GateError(f"{label} must not be absolute: {value!r}")
    return candidate.as_posix()


def regular_artifact_files(artifact_root: Path) -> list[Path]:
    files: list[Path] = []
    for path in artifact_root.rglob("*"):
        if path.is_symlink():
            raise GateError(f"Artifact contains a symbolic link: {path}")
        if path.is_file():
            files.append(path)
    return sorted(files, key=lambda item: item.relative_to(artifact_root).as_posix())


def artifact_relative_path(artifact_root: Path, path: Path) -> str:
    try:
        return path.resolve().relative_to(artifact_root.resolve()).as_posix()
    except ValueError as exc:
        raise GateError(f"Artifact path escapes root: {path}") from exc


def is_within(path: Path, root: Path) -> bool:
    try:
        path.resolve().relative_to(root.resolve())
        return True
    except ValueError:
        return False


def file_mtime_utc(path: Path) -> datetime:
    return datetime.fromtimestamp(path.stat().st_mtime, tz=timezone.utc)


def file_change_time_utc(path: Path) -> datetime:
    return datetime.fromtimestamp(path.stat().st_ctime, tz=timezone.utc)


def process_environment_summary() -> dict[str, str]:
    return {
        "python": os.sys.version.split()[0],
        "platform": os.name,
    }
