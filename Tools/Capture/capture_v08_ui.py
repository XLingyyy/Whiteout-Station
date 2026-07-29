#!/usr/bin/env python3
"""Capture the v0.8 opening, HUD, guide, focus, and dialogue baselines."""

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
MODE = "v08ux"
RESOLUTIONS = {"720p": (1280, 720), "1080p": (1920, 1080)}


def matching_pids() -> set[int]:
    result: set[int] = set()
    for process in psutil.process_iter(("pid", "name", "cmdline")):
        try:
            if (process.info["name"] or "").lower() != "unrealeditor.exe":
                continue
            arguments = process.info["cmdline"] or []
            if any(
                f"WhiteoutPresentationCapture={MODE}" in argument
                for argument in arguments
            ):
                result.add(int(process.info["pid"]))
        except (psutil.AccessDenied, psutil.NoSuchProcess):
            continue
    return result


def run_capture(width: int, height: int, timeout_seconds: float) -> None:
    before = matching_pids()
    arguments = [
        str(EDITOR),
        str(PROJECT),
        MAP,
        "-game",
        "-windowed",
        f"-ResX={width}",
        f"-ResY={height}",
        "-ForceRes",
        f"-WhiteoutPresentationCapture={MODE}",
        "-WhiteoutV08Capture",
        "-NoSound",
        "-unattended",
        "-NoSplash",
    ]
    print(f"START {MODE} {width}x{height}", flush=True)
    subprocess.Popen(
        arguments,
        cwd=ROOT,
        creationflags=subprocess.CREATE_NO_WINDOW,
    )

    deadline = time.monotonic() + timeout_seconds
    active: set[int] = set()
    while time.monotonic() < deadline:
        active = matching_pids() - before
        if active:
            break
        time.sleep(0.25)
    if not active:
        raise RuntimeError(f"Unreal Editor did not start for {width}x{height}")

    while time.monotonic() < deadline and any(
        psutil.pid_exists(pid) for pid in active
    ):
        time.sleep(1.0)
    if any(psutil.pid_exists(pid) for pid in active):
        raise TimeoutError(
            f"Capture timed out for {width}x{height}: {sorted(active)}"
        )
    print(f"PASS  {MODE} {width}x{height}", flush=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--resolution", action="append", choices=RESOLUTIONS)
    parser.add_argument("--timeout", type=float, default=150.0)
    args = parser.parse_args()

    resolutions = tuple(args.resolution) if args.resolution else tuple(RESOLUTIONS)
    if not EDITOR.is_file():
        raise FileNotFoundError(EDITOR)
    if not PROJECT.is_file():
        raise FileNotFoundError(PROJECT)
    for name in resolutions:
        run_capture(*RESOLUTIONS[name], args.timeout)
    print("v0.8 UI baseline capture complete", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"FAIL {exc}", file=sys.stderr, flush=True)
        raise
