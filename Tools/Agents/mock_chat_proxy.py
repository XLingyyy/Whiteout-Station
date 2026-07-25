"""OpenAI-compatible deterministic mock for model-path regression."""

from __future__ import annotations

import sys

try:
    from .mock_server import run_from_cli
except ImportError:
    from mock_server import run_from_cli


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    raise SystemExit(run_from_cli(__doc__ or "Whiteout Station chat mock"))
