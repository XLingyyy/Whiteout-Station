"""Run the v0.5 Shipping routes and AI fail-safe smoke scenarios.

The selected artifact must not have a release manifest yet because the
evidence produced here must be included in that manifest's checksums.
Credential values are never read, copied, printed, or persisted.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any

try:
    from .v05_gate_common import MANIFEST_REL, RUN_ID_PATTERN
except ImportError:
    from v05_gate_common import MANIFEST_REL, RUN_ID_PATTERN


ARTIFACT_PREFIX = "WhiteoutStation-v0.5-Win64-"
EXECUTABLE_REL = Path("Windows/WhiteoutStation.exe")
AGENT_RUNTIME_REL = Path(
    "Windows/WhiteoutStation/Content/Agents/AgentRuntime.v0.5.json"
)
OUTPUT_REL = Path("Validation/ShippingSmoke")
EVENT_LOG_REL = Path("Saved/Logs/WhiteoutStation_EventLog.json")
MODEL_AUDIT_REL = Path("Saved/Logs/WhiteoutStation_ModelAudit.jsonl")
SCREENSHOT_REL = Path("Saved/WhiteoutRuntimeSmoke.png")

EXPECTED_ROUTES = {
    "medical": {
        "actions": [
            "talk_ye_cheng",
            "heat_medical_room",
            "treat_gu_heng",
            "talk_gu_heng",
            "heat_repair_room",
            "repair_generator",
            "calibrate_antenna",
            "send_signal",
        ],
        "remaining_ap": 0,
        "score": 76.76,
    },
    "technical": {
        "actions": [
            "investigate_generator_log",
            "inspect_control_cabinet",
            "talk_gu_heng",
            "dismantle_kitchen_heater",
            "heat_repair_room",
            "repair_generator",
            "calibrate_antenna",
            "send_signal",
        ],
        "remaining_ap": 0,
        "score": 72.02,
    },
    "quick": {
        "actions": [
            "heat_repair_room",
            "distribute_food",
            "repair_generator",
            "repair_generator",
            "calibrate_antenna",
            "send_signal",
        ],
        "remaining_ap": 2,
        "score": 72.06,
    },
}


class SmokeError(RuntimeError):
    """Raised when the Shipping smoke cannot produce trustworthy evidence."""


@dataclass(frozen=True)
class Scenario:
    scenario_id: str
    route: str
    llm_mode: str
    expected_model_calls: int


SCENARIOS = (
    Scenario("default_offline_medical", "medical", "default_offline", 0),
    Scenario("default_offline_technical", "technical", "default_offline", 0),
    Scenario("default_offline_quick", "quick", "default_offline", 0),
    Scenario("enabled_no_key_medical", "medical", "enabled_no_key", 0),
    Scenario(
        "unreachable_endpoint_technical",
        "technical",
        "unreachable_endpoint",
        1,
    ),
)


def force_empty_credential_inputs() -> None:
    """Prevent child inheritance without retrieving any prior value."""

    os.environ["WHITEOUT_LLM_API_KEY"] = ""
    os.environ["WHITEOUT_LLM_ENABLED"] = ""


class DroppingLoopbackEndpoint:
    """Accept TCP connections and close them without reading request bytes."""

    def __init__(self) -> None:
        self._listener: socket.socket | None = None
        self._stop = threading.Event()
        self._thread: threading.Thread | None = None
        self.attempts = 0
        self.port = 0

    def __enter__(self) -> "DroppingLoopbackEndpoint":
        listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        listener.bind(("127.0.0.1", 0))
        listener.listen(4)
        listener.settimeout(0.2)
        self._listener = listener
        self.port = int(listener.getsockname()[1])
        self._thread = threading.Thread(target=self._serve, daemon=True)
        self._thread.start()
        return self

    def _serve(self) -> None:
        assert self._listener is not None
        while not self._stop.is_set():
            try:
                connection, _address = self._listener.accept()
            except TimeoutError:
                continue
            except OSError:
                break
            self.attempts += 1
            try:
                connection.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            finally:
                connection.close()

    def __exit__(self, _exc_type: object, _exc: object, _traceback: object) -> None:
        self._stop.set()
        if self._listener is not None:
            self._listener.close()
        if self._thread is not None:
            self._thread.join(timeout=2.0)


def load_json(path: Path) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise SmokeError(f"Cannot read JSON {path}: {exc}") from exc


def validate_artifact_root(artifact_root: Path) -> tuple[Path, Path]:
    try:
        root = artifact_root.resolve(strict=True)
    except OSError as exc:
        raise SmokeError(f"Artifact root does not exist: {exc}") from exc
    if not root.is_dir() or root.is_symlink():
        raise SmokeError(f"Artifact root must be a regular directory: {root}")
    if not root.name.startswith(ARTIFACT_PREFIX):
        raise SmokeError(f"Artifact root is not a unique v0.5 archive: {root.name}")
    run_id = root.name[len(ARTIFACT_PREFIX) :]
    if not RUN_ID_PATTERN.fullmatch(run_id):
        raise SmokeError(f"Artifact root has an invalid v0.5 run id: {run_id}")
    if (root / MANIFEST_REL).exists():
        raise SmokeError(
            f"{MANIFEST_REL} already exists; smoke evidence must precede manifest creation"
        )
    if (root / OUTPUT_REL).exists():
        raise SmokeError(f"Refusing to mix with existing smoke evidence: {OUTPUT_REL}")

    executable = root / EXECUTABLE_REL
    if not executable.is_file():
        raise SmokeError(f"Missing packaged executable: {EXECUTABLE_REL}")
    agent_path = root / AGENT_RUNTIME_REL
    agent = load_json(agent_path)
    if not isinstance(agent, dict):
        raise SmokeError("Packaged Agent runtime root must be an object")
    if agent.get("runtime_version") != "0.5.0":
        raise SmokeError("Packaged Agent runtime_version must be 0.5.0")
    if agent.get("llm_enabled") is not False:
        raise SmokeError("Packaged Agent runtime must default to offline")
    if agent.get("endpoint") != "https://api.deepseek.com/chat/completions":
        raise SmokeError("Packaged Agent runtime endpoint must be the official endpoint")

    local_configs = [
        path
        for path in root.rglob("*")
        if path.is_file() and path.name.casefold() == "whiteoutllm.ini"
    ]
    if local_configs:
        raise SmokeError(
            "Artifact contains LocalConfig/WhiteoutLLM.ini; refusing credential-capable input"
        )
    return root, executable


def png_size(path: Path) -> tuple[int, int]:
    data = path.read_bytes()[:24]
    if len(data) != 24 or data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SmokeError(f"Output is not a PNG: {path.name}")
    return struct.unpack(">II", data[16:24])


def validate_event_log(
    scenario: Scenario,
    event_log: dict[str, Any],
) -> dict[str, Any]:
    expected = EXPECTED_ROUTES[scenario.route]
    events = event_log.get("events")
    if not isinstance(events, list):
        raise SmokeError(f"{scenario.scenario_id}: events must be an array")
    action_ids = [event.get("action_id") for event in events if isinstance(event, dict)]
    if action_ids != expected["actions"]:
        raise SmokeError(f"{scenario.scenario_id}: action sequence mismatch")
    if any(
        event.get("reason_code") != "Committed"
        for event in events
        if isinstance(event, dict)
    ):
        raise SmokeError(f"{scenario.scenario_id}: non-committed event found")
    if event_log.get("rules_version") != "0.5.0":
        raise SmokeError(f"{scenario.scenario_id}: rules_version mismatch")
    if event_log.get("ending") != "TaskSuccess" or event_log.get("signal_sent") is not True:
        raise SmokeError(f"{scenario.scenario_id}: successful ending missing")
    if event_log.get("remaining_ap") != expected["remaining_ap"]:
        raise SmokeError(f"{scenario.scenario_id}: remaining AP mismatch")
    try:
        score = float(event_log["score"])
    except (KeyError, TypeError, ValueError) as exc:
        raise SmokeError(f"{scenario.scenario_id}: invalid score") from exc
    if abs(score - float(expected["score"])) > 0.02:
        raise SmokeError(
            f"{scenario.scenario_id}: score mismatch "
            f"({score:.2f} != {expected['score']:.2f})"
        )
    if event_log.get("model_calls") != scenario.expected_model_calls:
        raise SmokeError(f"{scenario.scenario_id}: model call count mismatch")
    return {
        "ending": "TaskSuccess",
        "events": len(events),
        "remaining_ap": event_log["remaining_ap"],
        "score": score,
        "model_calls": event_log["model_calls"],
    }


def audit_key_is_forbidden(key: str) -> bool:
    normalized = "".join(character for character in key.casefold() if character.isalnum())
    return normalized in {
        "apikey",
        "authorization",
        "authorizationheader",
        "bearertoken",
        "credential",
        "credentials",
        "secret",
    }


def validate_audit(scenario: Scenario, audit_path: Path) -> dict[str, Any] | None:
    if scenario.llm_mode != "unreachable_endpoint":
        if audit_path.exists() and audit_path.stat().st_size:
            raise SmokeError(f"{scenario.scenario_id}: unexpected live-provider audit")
        return None
    if not audit_path.is_file():
        raise SmokeError(f"{scenario.scenario_id}: missing transport-failure audit")

    records: list[dict[str, Any]] = []
    for line in audit_path.read_text(encoding="utf-8-sig").splitlines():
        if not line.strip():
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError as exc:
            raise SmokeError(f"{scenario.scenario_id}: malformed model audit") from exc
        if not isinstance(record, dict):
            raise SmokeError(f"{scenario.scenario_id}: model audit must contain objects")
        if any(audit_key_is_forbidden(str(key)) for key in record):
            raise SmokeError(f"{scenario.scenario_id}: credential-like audit field found")
        records.append(record)
    if len(records) != 1:
        raise SmokeError(f"{scenario.scenario_id}: expected one final audit record")
    record = records[0]
    if record.get("kind") != "expression" or record.get("action_id") != "talk_gu_heng":
        raise SmokeError(f"{scenario.scenario_id}: unexpected audit identity")
    if record.get("outcome") != "transport_error":
        raise SmokeError(f"{scenario.scenario_id}: endpoint loss did not safely degrade")
    if record.get("transport_attempt_limit") != 2:
        raise SmokeError(f"{scenario.scenario_id}: transport attempt limit mismatch")
    return {
        "kind": record["kind"],
        "action_id": record["action_id"],
        "http_status": record.get("http_status"),
        "outcome": record["outcome"],
        "transport_attempt_limit": record.get("transport_attempt_limit"),
    }


def build_command(
    executable: Path,
    runtime_root: Path,
    scenario: Scenario,
    endpoint_port: int | None,
) -> list[str]:
    command = [
        str(executable),
        f"-WhiteoutAutoRoute={scenario.route}",
        "-WhiteoutAutoCapture",
        f"-UserDir={runtime_root.resolve()}",
        "-ResX=1280",
        "-ResY=720",
        "-ForceRes",
        "-WINDOWED",
        "-RenderOffscreen",
        "-nosplash",
        "-unattended",
        "-NoSound",
    ]
    if scenario.llm_mode in {"enabled_no_key", "unreachable_endpoint"}:
        command.append("-WhiteoutLLMEnabled=true")
    if scenario.llm_mode == "unreachable_endpoint":
        if endpoint_port is None:
            raise SmokeError("Unreachable endpoint scenario has no loopback port")
        command.append(
            f"-WhiteoutAgentEndpoint=http://127.0.0.1:{endpoint_port}/chat/completions"
        )
    return command


def run_process(command: list[str], cwd: Path, timeout_seconds: float) -> int:
    completed = subprocess.run(
        command,
        cwd=cwd,
        check=False,
        timeout=timeout_seconds,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return completed.returncode


def run_scenario(
    executable: Path,
    scenario: Scenario,
    staging_root: Path,
    timeout_seconds: float,
) -> dict[str, Any]:
    runtime_root = staging_root / "Runtime" / scenario.scenario_id
    evidence_root = staging_root / "Evidence"
    runtime_root.mkdir(parents=True)
    evidence_root.mkdir(parents=True, exist_ok=True)
    force_empty_credential_inputs()

    endpoint: DroppingLoopbackEndpoint | None = None
    started = time.monotonic()
    if scenario.llm_mode == "unreachable_endpoint":
        endpoint = DroppingLoopbackEndpoint()
        endpoint.__enter__()
    try:
        command = build_command(
            executable,
            runtime_root,
            scenario,
            endpoint.port if endpoint is not None else None,
        )
        return_code = run_process(command, executable.parent, timeout_seconds)
    except subprocess.TimeoutExpired as exc:
        raise SmokeError(f"{scenario.scenario_id}: Shipping process timed out") from exc
    finally:
        if endpoint is not None:
            endpoint.__exit__(None, None, None)
    elapsed_ms = round((time.monotonic() - started) * 1000.0)
    if return_code != 0:
        raise SmokeError(
            f"{scenario.scenario_id}: Shipping process returned {return_code}"
        )

    event_path = runtime_root / EVENT_LOG_REL
    screenshot_path = runtime_root / SCREENSHOT_REL
    if not event_path.is_file() or not screenshot_path.is_file():
        raise SmokeError(f"{scenario.scenario_id}: fresh runtime evidence is missing")
    event_log = load_json(event_path)
    if not isinstance(event_log, dict):
        raise SmokeError(f"{scenario.scenario_id}: event log root must be an object")
    route_summary = validate_event_log(scenario, event_log)
    dimensions = png_size(screenshot_path)
    if dimensions != (1280, 720):
        raise SmokeError(
            f"{scenario.scenario_id}: screenshot is {dimensions}, expected 1280x720"
        )
    audit_summary = validate_audit(scenario, runtime_root / MODEL_AUDIT_REL)
    if endpoint is not None and endpoint.attempts != 2:
        raise SmokeError(
            f"{scenario.scenario_id}: expected exactly two bounded transport attempts"
        )

    event_name = f"{scenario.scenario_id}_EventLog.json"
    screenshot_name = f"{scenario.scenario_id}_RuntimeSmoke.png"
    shutil.copy2(event_path, evidence_root / event_name)
    shutil.copy2(screenshot_path, evidence_root / screenshot_name)
    summary = {
        "scenario_id": scenario.scenario_id,
        "route": scenario.route,
        "llm_mode": scenario.llm_mode,
        "passed": True,
        "exit_code": return_code,
        "duration_ms": elapsed_ms,
        **route_summary,
        "screenshot": {"width": dimensions[0], "height": dimensions[1]},
        "event_log": event_name,
        "runtime_screenshot": screenshot_name,
        "api_key_supplied": False,
    }
    if audit_summary is not None:
        summary["degradation"] = audit_summary
        summary["loopback_connections_dropped"] = endpoint.attempts
    print(
        f"{scenario.scenario_id}=PASS route={scenario.route} "
        f"ending={summary['ending']} score={summary['score']:.2f} "
        f"model_calls={summary['model_calls']}"
    )
    return summary


def run_shipping_smoke(
    artifact_root: Path,
    *,
    timeout_seconds: float = 90.0,
) -> Path:
    if not 10.0 <= timeout_seconds <= 300.0:
        raise SmokeError("timeout_seconds must be between 10 and 300")
    root, executable = validate_artifact_root(artifact_root)
    validation_root = root / "Validation"
    validation_root.mkdir(parents=True, exist_ok=True)
    output_root = root / OUTPUT_REL

    with tempfile.TemporaryDirectory(
        prefix=".shipping-smoke-",
        dir=validation_root,
    ) as temporary:
        staging_root = Path(temporary)
        summaries = [
            run_scenario(executable, scenario, staging_root, timeout_seconds)
            for scenario in SCENARIOS
        ]
        evidence_root = staging_root / "Evidence"
        report = {
            "schema": "whiteout.v0.5.shipping-smoke.v1",
            "passed": True,
            "artifact_root_name": root.name,
            "credential_policy": {
                "api_key_value_read": False,
                "api_key_value_persisted": False,
                "child_api_key_forced_empty": True,
                "local_credential_config_accepted": False,
            },
            "scenarios": summaries,
        }
        (evidence_root / "shipping_smoke_summary.json").write_text(
            json.dumps(report, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )
        evidence_root.replace(output_root)
    return output_root / "shipping_smoke_summary.json"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--artifact-root",
        type=Path,
        required=True,
        help="Exact unique WhiteoutStation-v0.5-Win64-<run_id> artifact root",
    )
    parser.add_argument("--timeout-seconds", type=float, default=90.0)
    args = parser.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")
    force_empty_credential_inputs()
    try:
        summary_path = run_shipping_smoke(
            args.artifact_root,
            timeout_seconds=args.timeout_seconds,
        )
    except (SmokeError, OSError) as exc:
        print(f"SHIPPING SMOKE v0.5: FAIL: {exc}")
        return 1
    print(f"SHIPPING SMOKE v0.5: PASS (5/5) summary={summary_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
