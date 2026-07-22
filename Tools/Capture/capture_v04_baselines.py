#!/usr/bin/env python3
"""Capture the complete v0.4 presentation baseline at both target resolutions."""

from __future__ import annotations

import argparse
import subprocess
import sys
import time
from pathlib import Path

import psutil


ROOT = Path(__file__).resolve().parents[2]
EDITOR = Path(r"G:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe")
PROJECT = ROOT / "WhiteoutStation" / "WhiteoutStation.uproject"
MAP = "/Game/WindStation/World/MVP_StationMap"
MODES = ("g1suite", "g2suite", "g3suite", "g4systems", "v04dialogue", "v04characters", "v04evidence", "v04focus")
RESOLUTIONS = ((1280, 720), (1920, 1080))


def matching_pids(mode: str) -> set[int]:
    result: set[int] = set()
    for process in psutil.process_iter(("pid", "name", "cmdline")):
        try:
            if (process.info["name"] or "").lower() != "unrealeditor.exe":
                continue
            arguments = process.info["cmdline"] or []
            if any(f"WhiteoutPresentationCapture={mode}" in argument for argument in arguments):
                result.add(int(process.info["pid"]))
        except (psutil.AccessDenied, psutil.NoSuchProcess):
            continue
    return result


def run_capture(mode: str, width: int, height: int, timeout_seconds: float) -> None:
    before = matching_pids(mode)
    arguments = [
        str(EDITOR),
        str(PROJECT),
        MAP,
        "-game",
        "-windowed",
        f"-ResX={width}",
        f"-ResY={height}",
        "-ForceRes",
        f"-WhiteoutPresentationCapture={mode}",
        "-WhiteoutV04Capture",
        "-NoSound",
        "-unattended",
        "-NoSplash",
    ]
    print(f"START {mode} {width}x{height}", flush=True)
    subprocess.Popen(arguments, cwd=ROOT, creationflags=subprocess.CREATE_NO_WINDOW)

    deadline = time.monotonic() + timeout_seconds
    active: set[int] = set()
    while time.monotonic() < deadline:
        active = matching_pids(mode) - before
        if active:
            break
        time.sleep(0.25)
    if not active:
        raise RuntimeError(f"Unreal Editor did not start for {mode} {width}x{height}")

    while time.monotonic() < deadline and any(psutil.pid_exists(pid) for pid in active):
        time.sleep(1.0)
    if any(psutil.pid_exists(pid) for pid in active):
        raise TimeoutError(f"Capture timed out for {mode} {width}x{height}: {sorted(active)}")
    print(f"PASS  {mode} {width}x{height}", flush=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", action="append", choices=MODES, help="Run only selected mode(s).")
    parser.add_argument("--resolution", action="append", choices=("720p", "1080p"), help="Run only selected resolution(s).")
    parser.add_argument("--timeout", type=float, default=120.0)
    args = parser.parse_args()

    modes = tuple(args.mode) if args.mode else MODES
    resolution_names = tuple(args.resolution) if args.resolution else ("720p", "1080p")
    resolutions = tuple(RESOLUTIONS[0] if name == "720p" else RESOLUTIONS[1] for name in resolution_names)

    if not EDITOR.is_file():
        raise FileNotFoundError(EDITOR)
    if not PROJECT.is_file():
        raise FileNotFoundError(PROJECT)

    for width, height in resolutions:
        for mode in modes:
            run_capture(mode, width, height, args.timeout)
    print("v0.4 baseline capture complete", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"FAIL {exc}", file=sys.stderr, flush=True)
        raise
