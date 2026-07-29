#!/usr/bin/env python3
"""Capture v1.0 UX, NPC pose, and runtime-facing validation frames."""

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
MODES = ("v10ux", "v10characters", "v10lookat")


def matching_pids(mode: str) -> set[int]:
    result: set[int] = set()
    for process in psutil.process_iter(("pid", "name", "cmdline")):
        try:
            if (process.info["name"] or "").lower() != "unrealeditor.exe":
                continue
            arguments = process.info["cmdline"] or []
            if any(
                f"WhiteoutPresentationCapture={mode}" in argument
                for argument in arguments
            ):
                result.add(int(process.info["pid"]))
        except (psutil.AccessDenied, psutil.NoSuchProcess):
            continue
    return result


def run_capture(
    mode: str,
    width: int,
    height: int,
    timeout_seconds: float,
) -> None:
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
        "-WhiteoutV10Capture",
        "-NoSound",
        "-unattended",
        "-NoSplash",
    ]
    print(f"START {mode} {width}x{height}", flush=True)
    subprocess.Popen(
        arguments,
        cwd=ROOT,
        creationflags=subprocess.CREATE_NO_WINDOW,
    )

    deadline = time.monotonic() + timeout_seconds
    active: set[int] = set()
    while time.monotonic() < deadline:
        active = matching_pids(mode) - before
        if active:
            break
        time.sleep(0.25)
    if not active:
        raise RuntimeError(f"Unreal Editor did not start for {mode}")

    while time.monotonic() < deadline and any(
        psutil.pid_exists(pid) for pid in active
    ):
        time.sleep(1.0)
    if any(psutil.pid_exists(pid) for pid in active):
        raise TimeoutError(
            f"Capture timed out for {mode}: {sorted(active)}"
        )
    print(f"PASS  {mode} {width}x{height}", flush=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", action="append", choices=MODES)
    parser.add_argument("--width", type=int, default=1600)
    parser.add_argument("--height", type=int, default=1000)
    parser.add_argument("--timeout", type=float, default=240.0)
    args = parser.parse_args()

    if not EDITOR.is_file():
        raise FileNotFoundError(EDITOR)
    if not PROJECT.is_file():
        raise FileNotFoundError(PROJECT)
    for mode in tuple(args.mode) if args.mode else MODES:
        run_capture(
            mode,
            args.width,
            args.height,
            args.timeout,
        )
    print("v1.0 validation capture complete", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"FAIL {exc}", file=sys.stderr, flush=True)
        raise
