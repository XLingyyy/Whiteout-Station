"""Validate the v0.2 player-facing Chinese StringTable source."""

from __future__ import annotations

import csv
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CSV_PATH = ROOT / "SourceAssets" / "UI" / "WhiteoutStation_zh.csv"
SOURCES = (
    ROOT / "WhiteoutStation" / "Source" / "WhiteoutStation" / "Private" / "HUD" / "WhiteoutHUDWidget.cpp",
    ROOT / "WhiteoutStation" / "Source" / "WhiteoutStation" / "Private" / "Presentation" / "WSPresentationText.cpp",
)
DYNAMIC_KEYS = {
    "ui_sent",
    "ui_not_sent",
    "ui_failed",
    "ui_discovered",
    "ui_unknown",
    "ui_intact",
    "ui_dismantled",
    "ui_promise_pending",
    "ui_promise_fulfilled",
    "ui_promise_broken",
}
FORBIDDEN_PLAYER_MARKERS = ("EWS", "FACT_", "ReasonCode", "ActionId", "<MISSING STRING TABLE ENTRY>")


def main() -> None:
    with CSV_PATH.open("r", encoding="utf-8-sig", newline="") as handle:
        rows = list(csv.DictReader(handle))
    keys = [row["Key"] for row in rows]
    if len(keys) != len(set(keys)):
        duplicates = sorted({key for key in keys if keys.count(key) > 1})
        raise SystemExit(f"duplicate StringTable keys: {duplicates}")
    if any(not row["SourceString"].strip() for row in rows):
        raise SystemExit("empty StringTable source string")

    required = set(DYNAMIC_KEYS)
    pattern = re.compile(r'(?:TableText|UI)\(TEXT\("([^"]+)"\)')
    for source in SOURCES:
        required.update(pattern.findall(source.read_text(encoding="utf-8")))
    missing = sorted(required - set(keys))
    if missing:
        raise SystemExit(f"missing StringTable keys: {missing}")

    violations: list[str] = []
    for row in rows:
        for marker in FORBIDDEN_PLAYER_MARKERS:
            if marker in row["SourceString"]:
                violations.append(f'{row["Key"]}: {marker}')
    if violations:
        raise SystemExit(f"player-facing internal marker leak: {violations}")
    print(f"STRING TABLE: PASS ({len(rows)} entries, {len(required)} referenced keys)")


if __name__ == "__main__":
    main()
