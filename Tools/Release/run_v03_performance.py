from __future__ import annotations

import json
import os
import shutil
import subprocess
import time
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
PACKAGE_ROOT = REPO_ROOT / "Builds" / "WhiteoutStation-v0.3-Win64"
EXECUTABLE = PACKAGE_ROOT / "Windows" / "WhiteoutStation.exe"
VALIDATION_ROOT = PACKAGE_ROOT / "Validation"
RUNTIME_LOG = Path(os.environ["LOCALAPPDATA"]) / "WhiteoutStation" / "Saved" / "Logs" / "WhiteoutStation.log"
PERFORMANCE_JSON = Path(os.environ["LOCALAPPDATA"]) / "WhiteoutStation" / "Saved" / "WhiteoutPerformance.json"


def main() -> None:
    if not EXECUTABLE.is_file():
        raise SystemExit(f"Missing packaged executable: {EXECUTABLE}")
    for output in (RUNTIME_LOG, PERFORMANCE_JSON):
        if output.exists():
            output.unlink()

    child_environment = os.environ.copy()
    child_environment.pop("WHITEOUT_LLM_API_KEY", None)
    child_environment.pop("WHITEOUT_LLM_ENABLED", None)
    started = time.time()
    completed = subprocess.run(
        [
            str(EXECUTABLE),
            "-WhiteoutLLMEnabled=false",
            "-WhiteoutPerformanceTest",
            "-ResX=1920",
            "-ResY=1080",
            "-ForceRes",
            "-WINDOWED",
            "-RenderOffscreen",
            "-nosplash",
            "-unattended",
            "-NoSound",
        ],
        cwd=EXECUTABLE.parent,
        env=child_environment,
        check=False,
    )
    if completed.returncode != 0:
        raise SystemExit(f"Performance process returned {completed.returncode}")
    if not PERFORMANCE_JSON.is_file() or PERFORMANCE_JSON.stat().st_mtime < started - 1.0:
        raise SystemExit("Fresh performance JSON missing")
    result = json.loads(PERFORMANCE_JSON.read_text(encoding="utf-8-sig"))

    VALIDATION_ROOT.mkdir(parents=True, exist_ok=True)
    shutil.copy2(PERFORMANCE_JSON, VALIDATION_ROOT / "performance_1080p.json")
    if RUNTIME_LOG.is_file():
        shutil.copy2(RUNTIME_LOG, VALIDATION_ROOT / "performance_Runtime.log")
    if not result["passed"]:
        raise SystemExit(f"Performance gate failed: {result}")
    print(
        "PERFORMANCE v0.3: PASS "
        f"samples={result['samples']} avg_fps={result['average_fps']:.2f} "
        f"one_percent_low_fps={result['one_percent_low_fps']:.2f} "
        f"p95_ms={result['p95_frame_ms']:.3f} p99_ms={result['p99_frame_ms']:.3f} "
        f"max_ms={result['max_frame_ms']:.3f}"
    )


if __name__ == "__main__":
    main()
