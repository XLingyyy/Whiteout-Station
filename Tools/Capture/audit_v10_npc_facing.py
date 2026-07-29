#!/usr/bin/env python3
"""Fail closed unless all v1.0 runtime-facing captures passed."""

from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
EVIDENCE = ROOT / "docs" / "evidence_v1.0"
CAPTURES = ("lookat_near", "lookat_side", "lookat_far")


def main() -> int:
    failures: list[str] = []
    for capture in CAPTURES:
        path = EVIDENCE / f"npc_facing_{capture}.json"
        if not path.is_file():
            failures.append(f"{capture}: evidence missing")
            continue
        record = json.loads(path.read_text(encoding="utf-8"))
        print(
            f"{capture}: passed={record['passed']} "
            f"distance={record['distance_cm']:.1f}cm "
            f"body_error={record['body_yaw_error_degrees']:.2f}deg "
            f"head_yaw={record['head_yaw_degrees']:.2f}deg "
            f"head_pitch={record['head_pitch_degrees']:.2f}deg"
        )
        if record.get("schema") != "whiteout.v1.0.npc-facing-audit.v1":
            failures.append(f"{capture}: unexpected schema")
        if not record.get("passed"):
            failures.append(f"{capture}: runtime facing audit failed")
    if failures:
        for failure in failures:
            print(f"NPC_FACING_AUDIT_ERROR {failure}")
        print(f"NPC FACING AUDIT v1.0: FAIL ({len(failures)} error(s))")
        return 1
    print("NPC FACING AUDIT v1.0: PASS (3/3 distances)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
