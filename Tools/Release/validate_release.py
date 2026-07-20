from __future__ import annotations

import json
import os
import struct
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
PACKAGE_ROOT = REPO_ROOT / "Builds" / "WhiteoutStation-v0.1-Win64"
WINDOWS_ROOT = PACKAGE_ROOT / "Windows"
GAME_ROOT = WINDOWS_ROOT / "WhiteoutStation"
SAVED_ROOT = Path(os.environ["LOCALAPPDATA"]) / "WhiteoutStation" / "Saved"


def require_file(path: Path, minimum_size: int = 1) -> None:
    if not path.is_file():
        raise SystemExit(f"MISSING: {path}")
    if path.stat().st_size < minimum_size:
        raise SystemExit(f"TOO SMALL: {path}")


def load_json(path: Path) -> dict:
    require_file(path)
    return json.loads(path.read_text(encoding="utf-8-sig"))


def png_size(path: Path) -> tuple[int, int]:
    require_file(path, 24)
    header = path.read_bytes()[:24]
    if header[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit(f"NOT PNG: {path}")
    return struct.unpack(">II", header[16:24])


def main() -> None:
    required_files = {
        "bootstrap": WINDOWS_ROOT / "WhiteoutStation.exe",
        "shipping executable": GAME_ROOT / "Binaries" / "Win64" / "WhiteoutStation-Win64-Shipping.exe",
        "pak": GAME_ROOT / "Content" / "Paks" / "WhiteoutStation-Windows.pak",
        "ucas": GAME_ROOT / "Content" / "Paks" / "WhiteoutStation-Windows.ucas",
        "utoc": GAME_ROOT / "Content" / "Paks" / "WhiteoutStation-Windows.utoc",
        "player readme": PACKAGE_ROOT / "README_v0.1.txt",
        "asset licenses": PACKAGE_ROOT / "ASSET_LICENSES.md",
    }
    for path in required_files.values():
        require_file(path)

    rules_path = GAME_ROOT / "Content" / "Rules" / "WhiteoutStationRules.v0.1.json"
    agent_path = GAME_ROOT / "Content" / "Agents" / "AgentRuntime.v0.1.json"
    rules = load_json(rules_path)
    agent = load_json(agent_path)
    if rules.get("rules_version") != "0.1.0":
        raise SystemExit("RULE VERSION MISMATCH")
    if agent.get("endpoint"):
        raise SystemExit("DEFAULT AGENT ENDPOINT MUST BE EMPTY")

    event_log = load_json(SAVED_ROOT / "Logs" / "WhiteoutStation_EventLog.json")
    events = event_log.get("events", [])
    if len(events) != 8:
        raise SystemExit(f"EXPECTED 8 EVENTS, GOT {len(events)}")
    if sum(bool(event.get("crisis_triggered")) for event in events) != 1:
        raise SystemExit("CRISIS MUST TRIGGER EXACTLY ONCE")
    if event_log.get("ending") != "TaskSuccess":
        raise SystemExit("PACKAGED ROUTE DID NOT REACH TASK SUCCESS")
    if event_log.get("remaining_ap") != 0 or not event_log.get("signal_sent"):
        raise SystemExit("0 AP SIGNAL WINDOW REGRESSION")
    promises = event_log.get("promises", [])
    if not any(promise.get("settled") and promise.get("fulfilled") for promise in promises):
        raise SystemExit("FULFILLED PROMISE RESULT MISSING")

    width, height = png_size(SAVED_ROOT / "WhiteoutRuntimeSmoke.png")
    if (width, height) != (1280, 720):
        raise SystemExit(f"SCREENSHOT SIZE MISMATCH: {width}x{height}")

    package_files = [path for path in PACKAGE_ROOT.rglob("*") if path.is_file()]
    package_bytes = sum(path.stat().st_size for path in package_files)
    print("RELEASE VALIDATION: PASS")
    print(f"package_files={len(package_files)} package_bytes={package_bytes}")
    print(
        f"events={len(events)} crisis=1 ending={event_log['ending']} "
        f"score={event_log['score']:.2f} rating={event_log['rating']}"
    )
    print(f"screenshot={width}x{height} rules={rules['rules_version']} agent=offline")


if __name__ == "__main__":
    main()
