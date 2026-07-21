"""Fail when v0.2 presentation work changes the frozen v0.1 rules baseline."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
MANIFEST = REPO_ROOT / "docs" / "RULE_FREEZE_v0.2.json"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> None:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8-sig"))
    mismatches: list[str] = []
    for relative, expected in manifest["files"].items():
        path = REPO_ROOT / relative
        if not path.is_file():
            mismatches.append(f"MISSING {relative}")
            continue
        actual = sha256(path)
        if actual != expected:
            mismatches.append(f"CHANGED {relative}: {actual} != {expected}")
    if mismatches:
        raise SystemExit("RULE FREEZE FAILED\n" + "\n".join(mismatches))
    print(f"RULE FREEZE: PASS ({len(manifest['files'])} files)")


if __name__ == "__main__":
    main()
