"""Shared primitives for the Whiteout Station v1.1 release gates.

These helpers only inspect repositories and release artifacts. They never
clean, reset, stash, or otherwise modify a Git worktree.
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


PROJECT_VERSION = "1.1.0"
VERSION_TOKEN = "v1.1"
DISTRIBUTION_CLASS = "onsite_competition_demo"
DISTRIBUTION_BANNER = "ONSITE COMPETITION DEMO"
REQUIRED_BRANCH = "main"

PROJECT_CONFIG_REL = "WhiteoutStation/Config/DefaultGame.ini"
RULES_REL = "WhiteoutStation/Content/Rules/WhiteoutStationRules.v1.1.json"
AGENT_RUNTIME_REL = "WhiteoutStation/Content/Agents/AgentRuntime.v1.1.json"
PROTECTED_MANIFEST_REL = "docs/PROTECTED_CHARACTER_ASSETS_v1.0.json"
PROTECTED_BASELINE_VERSION = "1.0.0"
USER_MAP_REL = "WhiteoutStation/Content/WindStation/World/MVP_StationMap.umap"

PACKAGED_RULES_REL = (
    "Windows/WhiteoutStation/Content/Rules/WhiteoutStationRules.v1.1.json"
)
PACKAGED_AGENT_RUNTIME_REL = (
    "Windows/WhiteoutStation/Content/Agents/AgentRuntime.v1.1.json"
)
REQUIRED_PACKAGE_FILES = (
    "Windows/WhiteoutStation.exe",
    "Windows/WhiteoutStation/Binaries/Win64/WhiteoutStation-Win64-Shipping.exe",
    "Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.pak",
    "Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.ucas",
    "Windows/WhiteoutStation/Content/Paks/WhiteoutStation-Windows.utoc",
    PACKAGED_RULES_REL,
    PACKAGED_AGENT_RUNTIME_REL,
    "README_v1.1.txt",
    "ASSET_LICENSES.md",
    "Validation/InputSmokeV11/input_smoke_summary.json",
    "Validation/ShippingSmokeV11/shipping_smoke_summary.json",
)

ARTIFACT_PREFIX = "WhiteoutStation-v1.1-Win64-"
MANIFEST_REL = "Validation/gate_manifest.json"
MANIFEST_SCHEMA = "whiteout.v1.1.release-manifest.v1"
RUN_ID_PATTERN = re.compile(r"^\d{8}T\d{6}Z-[0-9a-f]{8}-[a-z0-9]{4,16}$")
OBJECT_ID_PATTERN = re.compile(r"^[0-9a-f]{40,64}$")
SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")

SCANNABLE_ENDINGS = (
    ".bat",
    ".cmd",
    ".config",
    ".cpp",
    ".cs",
    ".csv",
    ".env",
    ".h",
    ".ini",
    ".ini.example",
    ".js",
    ".json",
    ".jsonl",
    ".log",
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
    ".yaml",
    ".yml",
)
SCANNABLE_NAMES = {".gitattributes", ".gitignore", "dockerfile"}
SK_TOKEN_PATTERN = re.compile(rb"(?<![A-Za-z0-9_-])s" rb"k-[A-Za-z0-9_-]{12,}")
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


class GateError(RuntimeError):
    """Raised when a release gate cannot safely inspect its input."""


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
    value = str(
        run_git(repo_root, "rev-parse", "--verify", f"{ref}^{{commit}}")
    ).strip()
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


def current_branch(repo_root: Path) -> str:
    completed = run_command(
        ["git", "symbolic-ref", "--quiet", "--short", "HEAD"],
        cwd=repo_root,
        check=False,
    )
    branch = decode_process_output(completed.stdout).strip()
    if completed.returncode != 0 or not branch:
        raise GateError(
            "Release source must be checked out on branch main, not detached"
        )
    return branch


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


def require_clean_main(repo_root: Path) -> str:
    branch = current_branch(repo_root)
    if branch != REQUIRED_BRANCH:
        raise GateError(
            f"Release source must be on branch {REQUIRED_BRANCH}; found {branch}"
        )
    statuses = git_status_entries(repo_root)
    if statuses:
        summary = ", ".join(f"{status} {path}" for status, path in statuses)
        raise GateError(
            "Release source must have a clean main worktree and index; "
            f"dirty paths were left untouched: {summary}"
        )
    return resolve_commit(repo_root, "HEAD")


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


def ensure_unique_artifact_root(artifact_root: Path) -> None:
    candidates = sorted(
        (
            child
            for child in artifact_root.parent.iterdir()
            if child.name.startswith(ARTIFACT_PREFIX)
        ),
        key=lambda child: child.name,
    )
    if len(candidates) != 1 or candidates[0].resolve() != artifact_root.resolve():
        names = ", ".join(path.name for path in candidates) or "<none>"
        raise GateError(
            f"Exactly one {ARTIFACT_PREFIX}* artifact is allowed in "
            f"{artifact_root.parent}; found: {names}"
        )


def is_scannable_path(relative_path: str) -> bool:
    lowered = relative_path.lower()
    name = PurePosixPath(lowered).name
    return name in SCANNABLE_NAMES or lowered.endswith(SCANNABLE_ENDINGS)


def secret_finding_names(payload: bytes) -> set[str]:
    findings: set[str] = set()
    if SK_TOKEN_PATTERN.search(payload):
        findings.add("suspicious_sk_key")
    for pattern_name, pattern in SECRET_PATTERNS.items():
        if pattern.search(payload):
            findings.add(pattern_name)
    for match in CREDENTIAL_ASSIGNMENT_PATTERN.finditer(payload):
        value = match.group("value")
        quoted = bool(match.group("quote"))
        looks_like_unquoted_token = b"." not in value and any(
            byte in value for byte in b"0123456789_~+/-"
        )
        if quoted or looks_like_unquoted_token:
            findings.add("credential_assignment")
    return findings


def process_environment_summary() -> dict[str, str]:
    return {
        "python": os.sys.version.split()[0],
        "platform": os.name,
    }
