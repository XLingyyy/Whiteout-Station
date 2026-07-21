from __future__ import annotations

import json
import struct
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
PACKAGE_ROOT = REPO_ROOT / "Builds" / "WhiteoutStation-v0.2-Win64"
WINDOWS_ROOT = PACKAGE_ROOT / "Windows"
GAME_ROOT = WINDOWS_ROOT / "WhiteoutStation"
VALIDATION_ROOT = PACKAGE_ROOT / "Validation"
PACKAGE_BUDGET_BYTES = 2_500_000_000

ROUTES = {
    "medical": {"events": 8, "remaining_ap": 0, "score": 76.64},
    "technical": {"events": 8, "remaining_ap": 0, "score": 71.90},
    "quick": {"events": 6, "remaining_ap": 2, "score": 72.06},
}


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


def validate_route(route: str, expected: dict) -> str:
    event_log = load_json(VALIDATION_ROOT / f"{route}_EventLog.json")
    events = event_log.get("events", [])
    if len(events) != expected["events"]:
        raise SystemExit(f"{route}: EXPECTED {expected['events']} EVENTS, GOT {len(events)}")
    if sum(bool(event.get("crisis_triggered")) for event in events) != 1:
        raise SystemExit(f"{route}: CRISIS MUST TRIGGER EXACTLY ONCE")
    if event_log.get("ending") != "TaskSuccess" or not event_log.get("signal_sent"):
        raise SystemExit(f"{route}: ROUTE DID NOT REACH TASK SUCCESS")
    if event_log.get("remaining_ap") != expected["remaining_ap"]:
        raise SystemExit(f"{route}: AP RESULT MISMATCH")
    if abs(float(event_log.get("score", -1.0)) - expected["score"]) > 0.02:
        raise SystemExit(f"{route}: SCORE MISMATCH")
    if route == "medical":
        promises = event_log.get("promises", [])
        if not any(promise.get("settled") and promise.get("fulfilled") for promise in promises):
            raise SystemExit("medical: FULFILLED PROMISE RESULT MISSING")
    width, height = png_size(VALIDATION_ROOT / f"{route}_RuntimeSmoke.png")
    if (width, height) != (1280, 720):
        raise SystemExit(f"{route}: SCREENSHOT SIZE MISMATCH: {width}x{height}")
    return f"{route}=PASS events={len(events)} ap={event_log['remaining_ap']} score={event_log['score']:.2f}"


def main() -> None:
    required_files = {
        "bootstrap": WINDOWS_ROOT / "WhiteoutStation.exe",
        "shipping executable": GAME_ROOT / "Binaries" / "Win64" / "WhiteoutStation-Win64-Shipping.exe",
        "pak": GAME_ROOT / "Content" / "Paks" / "WhiteoutStation-Windows.pak",
        "ucas": GAME_ROOT / "Content" / "Paks" / "WhiteoutStation-Windows.ucas",
        "utoc": GAME_ROOT / "Content" / "Paks" / "WhiteoutStation-Windows.utoc",
        "player readme": PACKAGE_ROOT / "README_v0.2.txt",
        "asset licenses": PACKAGE_ROOT / "ASSET_LICENSES.md",
    }
    for path in required_files.values():
        require_file(path)

    rules = load_json(GAME_ROOT / "Content" / "Rules" / "WhiteoutStationRules.v0.1.json")
    agent = load_json(GAME_ROOT / "Content" / "Agents" / "AgentRuntime.v0.1.json")
    if rules.get("rules_version") != "0.1.0":
        raise SystemExit("RULE VERSION MISMATCH")
    if agent.get("endpoint"):
        raise SystemExit("DEFAULT AGENT ENDPOINT MUST BE EMPTY")

    route_summaries = [validate_route(route, expected) for route, expected in ROUTES.items()]
    package_files = [path for path in PACKAGE_ROOT.rglob("*") if path.is_file()]
    package_bytes = sum(path.stat().st_size for path in package_files)
    if package_bytes > PACKAGE_BUDGET_BYTES:
        raise SystemExit(f"PACKAGE BUDGET EXCEEDED: {package_bytes} > {PACKAGE_BUDGET_BYTES}")

    print("RELEASE VALIDATION v0.2: PASS")
    print(f"package_files={len(package_files)} package_bytes={package_bytes}")
    for summary in route_summaries:
        print(summary)
    print(f"rules={rules['rules_version']} agent=offline budget_bytes={PACKAGE_BUDGET_BYTES}")


if __name__ == "__main__":
    main()
