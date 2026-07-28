"""Fail closed when repository content resembles a committed credential.

The scanner never prints matching text. It understands OOXML containers so a
credential embedded in a DOCX is not hidden by ZIP compression.
"""

from __future__ import annotations

import argparse
import io
import re
import subprocess
import sys
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PATTERNS = {
    "openai_style_key": re.compile(rb"(?<![A-Za-z0-9_-])sk-[A-Za-z0-9_-]{12,}"),
    "bearer_token": re.compile(rb"(?i)authorization\s*[:=]\s*bearer\s+[A-Za-z0-9._~+/-]{12,}"),
    "private_key": re.compile(rb"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----"),
    "api_key_assignment": re.compile(
        rb"(?i)(?<![A-Za-z0-9_])(?:api[_ -]?key|secret[_ -]?key)"
        rb"(?![A-Za-z0-9_])[\"']?\s*[:=]\s*[\"']?[A-Za-z0-9_~+/-]{20,}"
    ),
}
ALLOWLIST_PATHS = {
    "Tools/Release/scan_secrets.py",
    "WhiteoutStation/LocalConfig/WhiteoutLLM.ini.example",
}
SCANNABLE_SUFFIXES = {
    ".bat", ".cmd", ".config", ".cpp", ".cs", ".csv", ".docx", ".env",
    ".h", ".ini", ".js", ".json", ".md", ".ps1", ".py", ".rels", ".sh",
    ".toml", ".ts", ".tsx", ".txt", ".uplugin", ".uproject", ".xml", ".xlsx",
    ".yaml", ".yml", ".pptx",
}
SCANNABLE_NAMES = {".gitignore", ".gitattributes", "Dockerfile"}


def should_scan_path(path: str) -> bool:
    candidate = Path(path)
    return candidate.suffix.lower() in SCANNABLE_SUFFIXES or candidate.name in SCANNABLE_NAMES


def git(*args: str, binary: bool = False) -> bytes | str:
    result = subprocess.run(
        ["git", *args],
        cwd=ROOT,
        check=True,
        capture_output=True,
        text=not binary,
        encoding=None if binary else "utf-8",
        errors=None if binary else "replace",
    )
    return result.stdout


def scan_bytes(payload: bytes, label: str, findings: list[tuple[str, str]]) -> None:
    for pattern_name, pattern in PATTERNS.items():
        if pattern.search(payload):
            findings.append((label, pattern_name))


def scan_container(payload: bytes, label: str, findings: list[tuple[str, str]]) -> None:
    scan_bytes(payload, label, findings)
    if not label.lower().endswith((".docx", ".xlsx", ".pptx")):
        return
    try:
        with zipfile.ZipFile(io.BytesIO(payload)) as archive:
            for name in archive.namelist():
                if name.lower().endswith((".xml", ".rels", ".txt", ".json")):
                    scan_bytes(archive.read(name), f"{label}!{name}", findings)
    except zipfile.BadZipFile:
        findings.append((label, "invalid_ooxml_container"))


def scan_index(findings: list[tuple[str, str]]) -> None:
    names = str(git("ls-files", "-z")).split("\0")
    for name in filter(None, names):
        normalized = name.replace("\\", "/")
        if normalized in ALLOWLIST_PATHS or not should_scan_path(normalized):
            continue
        path = ROOT / name
        if path.is_file():
            scan_container(path.read_bytes(), f"index:{normalized}", findings)


def scan_history(findings: list[tuple[str, str]]) -> None:
    # Codex maintains private, non-committish checkpoint refs for turn recovery.
    # Release history means the branches/remotes/tags that Git can push.
    objects = str(git("rev-list", "--objects", "--branches", "--remotes", "--tags")).splitlines()
    seen: set[str] = set()
    for line in objects:
        object_id, _, name = line.partition(" ")
        if not name or object_id in seen:
            continue
        seen.add(object_id)
        normalized = name.replace("\\", "/")
        if normalized in ALLOWLIST_PATHS or not should_scan_path(normalized):
            continue
        if str(git("cat-file", "-t", object_id)).strip() != "blob":
            continue
        payload = bytes(git("cat-file", "-p", object_id, binary=True))
        scan_container(payload, f"history:{object_id[:12]}:{normalized}", findings)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--history", action="store_true", help="scan every reachable Git blob")
    args = parser.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")
    findings: list[tuple[str, str]] = []
    scan_index(findings)
    if args.history:
        scan_history(findings)
    if findings:
        print(f"SECRET SCAN: FAIL ({len(findings)} finding(s); values suppressed)")
        for label, pattern_name in findings:
            print(f"- {label}: {pattern_name}")
        return 1
    scope = "index + history" if args.history else "index"
    print(f"SECRET SCAN: PASS ({scope})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
