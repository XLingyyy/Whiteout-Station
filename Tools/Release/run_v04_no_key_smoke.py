from __future__ import annotations

import argparse
import json
import os
import shutil
import struct
import subprocess
import time
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
PACKAGE_ROOT = REPO_ROOT / "Builds" / "WhiteoutStation-v0.4-Win64"
EXECUTABLE = PACKAGE_ROOT / "Windows" / "WhiteoutStation.exe"
VALIDATION_ROOT = PACKAGE_ROOT / "Validation"
SAVED_ROOT = Path(os.environ["LOCALAPPDATA"]) / "WhiteoutStation" / "Saved"
EVENT_LOG = SAVED_ROOT / "Logs" / "WhiteoutStation_EventLog.json"
SCREENSHOT = SAVED_ROOT / "WhiteoutRuntimeSmoke.png"
RUNTIME_LOG = SAVED_ROOT / "Logs" / "WhiteoutStation.log"

EXPECTED = {
    "medical": {"events": 8, "remaining_ap": 0, "score": 76.64},
    "technical": {"events": 8, "remaining_ap": 0, "score": 71.90},
    "quick": {"events": 6, "remaining_ap": 2, "score": 72.06},
}


def png_size(path: Path) -> tuple[int, int]:
    data = path.read_bytes()[:24]
    if len(data) != 24 or data[:8] != b"\x89PNG\r\n\x1a\n":
        raise RuntimeError(f"Not a PNG: {path}")
    return struct.unpack(">II", data[16:24])


def load_event_log() -> dict:
    return json.loads(EVENT_LOG.read_text(encoding="utf-8-sig"))


def validate_route(route: str, result: dict) -> None:
    expected = EXPECTED[route]
    events = result.get("events", [])
    if len(events) != expected["events"]:
        raise RuntimeError(f"{route}: event count mismatch")
    if result.get("ending") != "TaskSuccess" or not result.get("signal_sent"):
        raise RuntimeError(f"{route}: task success missing")
    if result.get("remaining_ap") != expected["remaining_ap"]:
        raise RuntimeError(f"{route}: AP mismatch")
    if abs(float(result.get("score", -1.0)) - expected["score"]) > 0.02:
        raise RuntimeError(f"{route}: score mismatch")
    if result.get("model_calls") != 0:
        raise RuntimeError(f"{route}: model_calls must be zero")
    if png_size(SCREENSHOT) != (1280, 720):
        raise RuntimeError(f"{route}: screenshot must be 1280x720")


def run_route(route: str) -> dict:
    for output in (EVENT_LOG, SCREENSHOT):
        if output.exists():
            output.unlink()

    child_environment = os.environ.copy()
    child_environment.pop("WHITEOUT_LLM_API_KEY", None)
    child_environment.pop("WHITEOUT_LLM_ENABLED", None)

    started = time.time()
    command = [
        str(EXECUTABLE),
        "-WhiteoutLLMEnabled=false",
        f"-WhiteoutAutoRoute={route}",
        "-WhiteoutAutoCapture",
        "-ResX=1280",
        "-ResY=720",
        "-ForceRes",
        "-WINDOWED",
        "-RenderOffscreen",
        "-nosplash",
        "-unattended",
        "-NoSound",
    ]
    completed = subprocess.run(
        command,
        cwd=EXECUTABLE.parent,
        env=child_environment,
        check=False,
    )
    if completed.returncode != 0:
        raise RuntimeError(f"{route}: executable returned {completed.returncode}")

    for output in (EVENT_LOG, SCREENSHOT):
        if not output.is_file():
            raise RuntimeError(f"{route}: missing fresh output {output.name}")
        if output.stat().st_mtime < started - 1.0:
            raise RuntimeError(f"{route}: stale output {output.name}")

    result = load_event_log()
    validate_route(route, result)

    VALIDATION_ROOT.mkdir(parents=True, exist_ok=True)
    shutil.copy2(EVENT_LOG, VALIDATION_ROOT / f"{route}_EventLog.json")
    shutil.copy2(SCREENSHOT, VALIDATION_ROOT / f"{route}_RuntimeSmoke.png")
    if RUNTIME_LOG.is_file():
        shutil.copy2(RUNTIME_LOG, VALIDATION_ROOT / f"{route}_Runtime.log")

    summary = {
        "route": route,
        "exit_code": completed.returncode,
        "ending": result["ending"],
        "events": len(result["events"]),
        "remaining_ap": result["remaining_ap"],
        "score": result["score"],
        "model_calls": result["model_calls"],
        "screenshot": "1280x720",
        "api_key_present_in_child_environment": False,
        "llm_enabled_present_in_child_environment": False,
        "llm_forced_off_by_command_line": True,
    }
    print(
        f"{route}=PASS ending={summary['ending']} events={summary['events']} "
        f"ap={summary['remaining_ap']} score={summary['score']:.2f} model_calls=0"
    )
    return summary


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--route", choices=[*EXPECTED, "all"], default="all")
    args = parser.parse_args()

    if not EXECUTABLE.is_file():
        raise SystemExit(f"Missing packaged executable: {EXECUTABLE}")

    routes = list(EXPECTED) if args.route == "all" else [args.route]
    summaries = [run_route(route) for route in routes]
    if args.route == "all":
        report = {
            "schema": "whiteout.v0.4.no_key_smoke.v1",
            "passed": True,
            "routes": summaries,
        }
        (VALIDATION_ROOT / "no_key_smoke_summary.json").write_text(
            json.dumps(report, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )
        print("NO-KEY SHIPPING SMOKE v0.4: PASS (3/3)")


if __name__ == "__main__":
    main()
