"""Run v1.2 Shipping routes and dialogue fail-safe scenarios.

This reuses the mature v1.1 route runner while overriding only the versioned
artifact contract and the v1.2 expression-action set.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

try:
    from . import run_v11_shipping_smoke as smoke
except ImportError:
    import run_v11_shipping_smoke as smoke


def configure_v12_contract() -> None:
    smoke.ARTIFACT_PREFIX = "WhiteoutStation-v1.2-Win64-"
    smoke.RELEASE_LABEL = "v1.2"
    smoke.AGENT_RUNTIME_REL = Path(
        "Windows/WhiteoutStation/Content/Agents/AgentRuntime.v1.2.json"
    )
    smoke.OUTPUT_REL = Path("Validation/ShippingSmokeV12")
    smoke.AGENT_RUNTIME_VERSION = "1.2.0"
    smoke.AGENT_SCHEMA_VERSION = 5
    smoke.SMOKE_SCHEMA = "whiteout.v1.2.shipping-smoke.v1"
    smoke.EXPRESSION_ACTION_IDS = frozenset(
        {"talk_gu_heng", "talk_ye_cheng", "repair_generator"}
    )
    smoke.PERFORMANCE_VALIDATION_REASON = "persona_tail_accepted"
    smoke.RUN_TIMEOUT_PROBE = True
    smoke.SCENARIOS = (
        smoke.Scenario("missing_key_medical", "medical", "default_missing_key", 0),
        smoke.Scenario("missing_key_technical", "technical", "default_missing_key", 0),
        smoke.Scenario("missing_key_quick", "quick", "default_missing_key", 0),
        smoke.Scenario("missing_key_wait", "wait", "default_missing_key", 0),
        smoke.Scenario("missing_key_collapse", "collapse", "default_missing_key", 0),
        smoke.Scenario("explicit_offline_medical", "medical", "explicit_offline", 0),
        smoke.Scenario("loopback_online_quick", "quick", "loopback_mock", 3),
        smoke.Scenario("loopback_online_technical", "technical", "loopback_mock", 3),
        smoke.Scenario(
            "unreachable_endpoint_quick", "quick", "unreachable_endpoint", 3
        ),
        smoke.Scenario("invalid_credential_quick", "quick", "provider_rejected", 3),
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--artifact-root",
        type=Path,
        required=True,
        help="Exact unique WhiteoutStation-v1.2-Win64-<run_id> artifact root",
    )
    parser.add_argument("--timeout-seconds", type=float, default=90.0)
    args = parser.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")
    configure_v12_contract()
    smoke.force_empty_credential_inputs()
    try:
        summary_path = smoke.run_shipping_smoke(
            args.artifact_root,
            timeout_seconds=args.timeout_seconds,
        )
    except (smoke.SmokeError, OSError) as exc:
        print(f"SHIPPING SMOKE v1.2: FAIL: {exc}")
        return 1
    print(f"SHIPPING SMOKE v1.2: PASS (14/14) summary={summary_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
