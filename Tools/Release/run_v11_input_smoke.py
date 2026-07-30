"""Drive the packaged v1.1 build through real keyboard and mouse input.

The selected artifact must not have a release manifest yet because this gate
adds evidence that must be covered by the final manifest. Credential values
are never printed or persisted.
"""

from __future__ import annotations

import argparse
import contextlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterator

import psutil

try:
    from . import run_v10_input_smoke as v10
    from .v11_gate_common import MANIFEST_REL, RUN_ID_PATTERN
except ImportError:
    import run_v10_input_smoke as v10
    from v11_gate_common import MANIFEST_REL, RUN_ID_PATTERN


AGENT_TOOLS = Path(__file__).resolve().parents[1] / "Agents"
if str(AGENT_TOOLS) not in sys.path:
    sys.path.insert(0, str(AGENT_TOOLS))

from mock_server import MockConfig, create_server


ARTIFACT_PREFIX = "WhiteoutStation-v1.1-Win64-"
EXECUTABLE_REL = Path("Windows/WhiteoutStation.exe")
OUTPUT_REL = Path("Validation/InputSmokeV11")
EVENT_LOG_REL = Path("Saved/Logs/WhiteoutStation_EventLog.json")
MODEL_AUDIT_REL = Path("Saved/Logs/WhiteoutStation_ModelAudit.jsonl")
AUTOSAVE_REL = Path("Saved/SaveGames/WhiteoutStation_Autosave_v1_1.sav")
ANTENNA_READY_PATTERN = re.compile(r"AntennaInputSmokePrep:\s+ready=1\b")


class SmokeError(v10.SmokeError):
    """Raised when v1.1 real-input evidence is incomplete."""


@dataclass(frozen=True)
class GameHandle:
    process: subprocess.Popen[bytes]
    hwnd: int
    window_process_id: int
    log_path: Path


class MockLoopbackEndpoint:
    """Run the repository's deterministic mock on an ephemeral loopback port."""

    def __init__(self, audit_path: Path) -> None:
        self.audit_path = audit_path
        self.server: Any = None
        self.thread: threading.Thread | None = None
        self.port = 0

    @property
    def request_count(self) -> int:
        return int(self.server.request_count) if self.server is not None else 0

    def __enter__(self) -> "MockLoopbackEndpoint":
        self.server = create_server(
            "127.0.0.1",
            0,
            config=MockConfig(),
            audit_path=self.audit_path,
        )
        self.port = int(self.server.server_port)
        self.thread = threading.Thread(
            target=self.server.serve_forever,
            name="whiteout-v11-input-smoke-mock",
        )
        self.thread.start()
        if not self.thread.is_alive():
            self.server.server_close()
            raise SmokeError("Loopback mock server did not start")
        return self

    def __exit__(
        self,
        _exc_type: object,
        _exc: object,
        _traceback: object,
    ) -> None:
        if self.server is not None:
            self.server.shutdown()
            self.server.server_close()
        if self.thread is not None:
            self.thread.join(timeout=3.0)
            if self.thread.is_alive():
                raise SmokeError("Loopback mock server did not stop")


def _decode_text(payload: bytes, label: str) -> str:
    encodings: list[str]
    if payload.startswith(b"\xef\xbb\xbf"):
        encodings = ["utf-8-sig", "utf-8", "gb18030", "utf-16"]
    elif payload.startswith((b"\xff\xfe", b"\xfe\xff")):
        encodings = ["utf-16", "utf-8-sig", "utf-8", "gb18030"]
    else:
        encodings = ["utf-8-sig", "utf-8", "gb18030", "utf-16"]
    for encoding in encodings:
        try:
            text = payload.decode(encoding)
        except UnicodeDecodeError:
            continue
        if "\ufffd" not in text:
            return text
    raise SmokeError(f"{label}: cannot decode text safely")


def _read_text(path: Path, label: str) -> str:
    try:
        return _decode_text(path.read_bytes(), label)
    except OSError as exc:
        raise SmokeError(f"{label}: cannot read {path.name}") from exc


def _load_json(path: Path, label: str) -> Any:
    try:
        return json.loads(_read_text(path, label))
    except json.JSONDecodeError as exc:
        raise SmokeError(f"{label}: malformed JSON") from exc


def _load_json_lines(path: Path, label: str) -> list[dict[str, Any]]:
    if not path.is_file():
        raise SmokeError(f"{label}: missing JSONL audit")
    records: list[dict[str, Any]] = []
    for line in _read_text(path, label).splitlines():
        if not line.strip():
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError as exc:
            raise SmokeError(f"{label}: malformed JSONL audit") from exc
        if not isinstance(record, dict):
            raise SmokeError(f"{label}: audit entries must be objects")
        records.append(record)
    return records


def _wait_for_json_lines(
    path: Path,
    *,
    minimum: int,
    label: str,
    timeout_seconds: float = 18.0,
) -> list[dict[str, Any]]:
    deadline = time.monotonic() + timeout_seconds
    last_error: SmokeError | None = None
    while time.monotonic() < deadline:
        if path.is_file():
            try:
                records = _load_json_lines(path, label)
            except SmokeError as exc:
                last_error = exc
            else:
                if len(records) >= minimum:
                    return records
        time.sleep(0.15)
    if last_error is not None:
        raise last_error
    raise SmokeError(f"{label}: timed out waiting for {minimum} record(s)")


def _wait_for_file(
    path: Path,
    label: str,
    timeout_seconds: float = 8.0,
) -> Path:
    deadline = time.monotonic() + timeout_seconds
    while time.monotonic() < deadline:
        if path.is_file() and path.stat().st_size > 0:
            return path
        time.sleep(0.15)
    raise SmokeError(f"{label}: fresh file did not appear")


def _file_sha256(path: Path) -> str | None:
    return v10.sha256_file(path) if path.is_file() else None


def _wait_for_hash_change(
    path: Path,
    previous_sha256: str | None,
    label: str,
    timeout_seconds: float = 8.0,
) -> str:
    deadline = time.monotonic() + timeout_seconds
    while time.monotonic() < deadline:
        current = _file_sha256(path)
        if current is not None and current != previous_sha256:
            return current
        time.sleep(0.15)
    raise SmokeError(f"{label}: autosave did not change after real input")


def _copy_evidence(
    source: Path,
    evidence_root: Path,
    output_name: str,
) -> dict[str, Any]:
    destination = evidence_root / output_name
    shutil.copy2(source, destination)
    return {
        "file": output_name,
        "bytes": destination.stat().st_size,
        "sha256": v10.sha256_file(destination),
    }


def _terminate_process_ids(process_ids: list[int]) -> None:
    processes: dict[int, psutil.Process] = {}
    for process_id in process_ids:
        try:
            root = psutil.Process(process_id)
        except psutil.NoSuchProcess:
            continue
        try:
            for child in root.children(recursive=True):
                processes[child.pid] = child
        except (psutil.AccessDenied, psutil.NoSuchProcess):
            pass
        processes[root.pid] = root
    targets = list(processes.values())
    for target in targets:
        try:
            target.terminate()
        except (psutil.AccessDenied, psutil.NoSuchProcess):
            pass
    _gone, alive = psutil.wait_procs(targets, timeout=3.0)
    for target in alive:
        try:
            target.kill()
        except (psutil.AccessDenied, psutil.NoSuchProcess):
            pass
    if alive:
        psutil.wait_procs(alive, timeout=3.0)


def _close_game(handle: GameHandle) -> None:
    if v10.user32.IsWindow(handle.hwnd):
        v10.user32.PostMessageW(handle.hwnd, v10.WM_CLOSE, 0, 0)
    deadline = time.monotonic() + 8.0
    while time.monotonic() < deadline and v10.user32.IsWindow(handle.hwnd):
        time.sleep(0.2)
    try:
        handle.process.wait(timeout=3.0)
    except subprocess.TimeoutExpired:
        pass
    if handle.process.poll() is None or v10.user32.IsWindow(handle.hwnd):
        _terminate_process_ids([handle.window_process_id, handle.process.pid])
    if handle.process.poll() is None:
        try:
            handle.process.wait(timeout=3.0)
        except subprocess.TimeoutExpired as exc:
            raise SmokeError("Packaged game process did not stop") from exc


def _launch_game(
    executable: Path,
    runtime_root: Path,
    target_action: str,
    *,
    log_name: str,
    extra_args: list[str] | None = None,
    llm_enabled: bool = False,
) -> GameHandle:
    existing_handles = set(v10.visible_game_windows())
    environment = os.environ.copy()
    v10.force_empty_credential_inputs(environment)
    log_path = runtime_root / "Saved" / "Logs" / log_name
    log_path.parent.mkdir(parents=True, exist_ok=True)
    command = [
        str(executable),
        f"-WhiteoutInputSmokeTarget={target_action}",
        f"-WhiteoutLLMEnabled={'true' if llm_enabled else 'false'}",
        f"-UserDir={runtime_root.resolve()}",
        f"-abslog={log_path.resolve()}",
        "-windowed",
        f"-ResX={v10.SMOKE_RES_X}",
        f"-ResY={v10.SMOKE_RES_Y}",
        "-ForceRes",
        "-NoSound",
        "-NoSplash",
    ]
    if extra_args:
        command.extend(extra_args)
    process = subprocess.Popen(
        command,
        cwd=executable.parent,
        env=environment,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    try:
        hwnd, window_process_id = v10.wait_for_window(existing_handles)
        time.sleep(2.0)
    except BaseException:
        _terminate_process_ids([process.pid])
        try:
            process.wait(timeout=3.0)
        except subprocess.TimeoutExpired:
            pass
        raise
    return GameHandle(process, hwnd, window_process_id, log_path)


@contextlib.contextmanager
def _game_session(
    executable: Path,
    runtime_root: Path,
    target_action: str,
    *,
    log_name: str,
    extra_args: list[str] | None = None,
    llm_enabled: bool = False,
) -> Iterator[GameHandle]:
    handle = _launch_game(
        executable,
        runtime_root,
        target_action,
        log_name=log_name,
        extra_args=extra_args,
        llm_enabled=llm_enabled,
    )
    try:
        yield handle
    finally:
        _close_game(handle)


def _prepare_phase_with_real_input(
    executable: Path,
    runtime_root: Path,
    evidence_root: Path,
    captures: list[dict[str, Any]],
    scenario_id: str,
) -> dict[str, Any]:
    autosave_path = runtime_root / AUTOSAVE_REL
    with _game_session(
        executable,
        runtime_root,
        "heat_control_room",
        log_name=f"{scenario_id}_phase_prep.log",
    ) as game:
        v10.advance_opening(
            game.hwnd,
            evidence_root,
            f"{scenario_id}_phase_prep",
            captures,
        )
        before_sha256 = _file_sha256(autosave_path)
        v10.send_key(game.hwnd, v10.VK_F, 0.7)
        captures.append(
            v10.capture(
                game.hwnd,
                evidence_root,
                f"{scenario_id}_phase_preview",
            )
        )
        v10.send_key(game.hwnd, v10.VK_F, 0.9)
        after_sha256 = _wait_for_hash_change(
            autosave_path,
            before_sha256,
            f"{scenario_id} phase prerequisite",
        )
        captures.append(
            v10.capture(
                game.hwnd,
                evidence_root,
                f"{scenario_id}_phase_committed",
            )
        )
    return {
        "method": "real_input_autosave_continue",
        "heating_action": "heat_control_room",
        "real_inputs": ["left_mouse_opening", "space_opening", "F_preview", "F_commit"],
        "autosave_changed": True,
        "autosave_sha256_after": after_sha256,
    }


def _validate_antenna_log(log_path: Path) -> dict[str, Any]:
    if not log_path.is_file():
        return {"shipping_log_available": False}
    text = _read_text(log_path, "antenna game log")
    if not ANTENNA_READY_PATTERN.search(text):
        raise SmokeError(
            "antenna_calibration: game log lacks " "AntennaInputSmokePrep ready=1"
        )
    if "InputSmokeSetup: target=calibrate_antenna" not in text:
        raise SmokeError("antenna_calibration: final antenna proxy was not targeted")
    return {
        "shipping_log_available": True,
        "antenna_prep_ready": True,
        "input_target_ready": True,
    }


def _validate_antenna_event_log(path: Path) -> dict[str, Any]:
    event_log = _load_json(path, "antenna event log")
    if not isinstance(event_log, dict):
        raise SmokeError("antenna_calibration: event log root must be an object")
    if event_log.get("rules_version") != "1.1.0":
        raise SmokeError("antenna_calibration: event log is not v1.1")
    events = event_log.get("events")
    if not isinstance(events, list):
        raise SmokeError("antenna_calibration: event array missing")
    calibration_events = [
        event
        for event in events
        if isinstance(event, dict) and event.get("action_id") == "calibrate_antenna"
    ]
    if len(calibration_events) != 1:
        raise SmokeError("antenna_calibration: expected one calibration commit")
    calibration = calibration_events[0]
    ap_before = calibration.get("ap_before")
    ap_after = calibration.get("ap_after")
    if (
        calibration.get("reason_code") != "Committed"
        or not isinstance(ap_before, (int, float))
        or not isinstance(ap_after, (int, float))
        or ap_after >= ap_before
    ):
        raise SmokeError("antenna_calibration: exported event did not confirm success")
    return {
        "rules_version": event_log["rules_version"],
        "action_id": "calibrate_antenna",
        "reason_code": "Committed",
        "ap_before": ap_before,
        "ap_after": ap_after,
    }


def run_antenna_scenario(
    executable: Path,
    staging_root: Path,
) -> dict[str, Any]:
    scenario_id = "antenna_calibration"
    runtime_root = staging_root / "Runtime" / scenario_id
    evidence_root = staging_root / "Evidence"
    runtime_root.mkdir(parents=True)
    evidence_root.mkdir(parents=True, exist_ok=True)
    captures: list[dict[str, Any]] = []
    autosave_path = runtime_root / AUTOSAVE_REL

    with _game_session(
        executable,
        runtime_root,
        "calibrate_antenna",
        log_name="antenna_input_smoke.log",
        extra_args=["-WhiteoutAntennaInputSmokeReady"],
    ) as game:
        v10.advance_opening(
            game.hwnd,
            evidence_root,
            scenario_id,
            captures,
        )
        captures.append(
            v10.capture(
                game.hwnd,
                evidence_root,
                f"{scenario_id}_final_proxy_focus",
            )
        )
        _wait_for_file(autosave_path, "antenna prepared autosave")
        before_sha256 = v10.sha256_file(autosave_path)
        v10.send_key(game.hwnd, v10.VK_F, 0.7)
        captures.append(
            v10.capture(
                game.hwnd,
                evidence_root,
                f"{scenario_id}_preview",
            )
        )
        v10.send_key(game.hwnd, v10.VK_F, 0.9)
        calibration_sha256 = _wait_for_hash_change(
            autosave_path,
            before_sha256,
            "antenna calibration",
        )
        captures.append(
            v10.capture(
                game.hwnd,
                evidence_root,
                f"{scenario_id}_committed",
            )
        )
        v10.send_key(game.hwnd, v10.VK_RETURN, 0.55)
        v10.send_key(game.hwnd, v10.VK_RETURN, 0.9)
        _wait_for_hash_change(
            autosave_path,
            calibration_sha256,
            "antenna afternoon settlement",
        )

    log_checks = _validate_antenna_log(game.log_path)
    log_evidence = (
        _copy_evidence(
            game.log_path,
            evidence_root,
            f"{scenario_id}_Game.log",
        )
        if game.log_path.is_file()
        else {}
    )

    with _game_session(
        executable,
        runtime_root,
        "heat_control_room",
        log_name="antenna_finalize.log",
        extra_args=["-WhiteoutContinue"],
    ) as finalizer:
        v10.advance_opening(
            finalizer.hwnd,
            evidence_root,
            f"{scenario_id}_finalize",
            captures,
        )
        before_heat_sha256 = v10.sha256_file(autosave_path)
        v10.send_key(finalizer.hwnd, v10.VK_F, 0.55)
        v10.send_key(finalizer.hwnd, v10.VK_F, 0.75)
        _wait_for_hash_change(
            autosave_path,
            before_heat_sha256,
            "antenna dusk heating selection",
        )
        v10.send_key(finalizer.hwnd, v10.VK_RETURN, 0.55)
        v10.send_key(finalizer.hwnd, v10.VK_RETURN, 1.3)
        event_path = _wait_for_file(
            runtime_root / EVENT_LOG_REL,
            "antenna exported event log",
        )
        time.sleep(1.5)
        captures.append(
            v10.capture(
                finalizer.hwnd,
                evidence_root,
                f"{scenario_id}_results",
            )
        )

    calibration_event = _validate_antenna_event_log(event_path)
    event_evidence = _copy_evidence(
        event_path,
        evidence_root,
        f"{scenario_id}_EventLog.json",
    )
    return {
        "scenario_id": scenario_id,
        "passed": True,
        "real_inputs": [
            "left_mouse_opening",
            "space_opening",
            "F_preview",
            "F_commit",
        ],
        "game_log": {
            **log_evidence,
            **log_checks,
            "antenna_prep_ready": True,
            "input_target_ready": True,
            "readiness_source": (
                "shipping_log"
                if log_checks["shipping_log_available"]
                else "prepared_autosave_and_calibration_event"
            ),
        },
        "calibration_event": {
            **event_evidence,
            **calibration_event,
        },
        "autosave_changed_on_commit": True,
        "captures": captures,
    }


def _validate_dialogue_audits(
    mock_records: list[dict[str, Any]],
    game_records: list[dict[str, Any]],
) -> tuple[dict[str, Any], dict[str, Any]]:
    if len(mock_records) != 1:
        raise SmokeError("dialogue_free_text: expected exactly one mock request")
    mock_record = mock_records[0]
    if (
        mock_record.get("kind") != "npc_line"
        or mock_record.get("action_id") != "talk_gu_heng"
        or mock_record.get("has_player_text") is not True
        or mock_record.get("response_format_json_object") is not True
        or mock_record.get("authorization_present") is not False
        or mock_record.get("status_code") != 200
    ):
        raise SmokeError("dialogue_free_text: mock request contract was not satisfied")
    matching_game_records = [
        record
        for record in game_records
        if record.get("kind") == "expression"
        and record.get("action_id") == "talk_gu_heng"
        and record.get("outcome") == "accepted"
        and record.get("http_status") == 200
    ]
    if len(game_records) != 1 or len(matching_game_records) != 1:
        raise SmokeError(
            "dialogue_free_text: game ModelAudit lacks one accepted HTTP 200"
        )
    return (
        {
            "requests": 1,
            "has_player_text": True,
            "response_format_json_object": True,
            "authorization_present": False,
            "status_code": 200,
        },
        {
            "records": 1,
            "kind": "expression",
            "action_id": "talk_gu_heng",
            "outcome": "accepted",
            "http_status": 200,
        },
    )


def run_dialogue_scenario(
    executable: Path,
    staging_root: Path,
) -> dict[str, Any]:
    scenario_id = "dialogue_free_text"
    runtime_root = staging_root / "Runtime" / scenario_id
    evidence_root = staging_root / "Evidence"
    runtime_root.mkdir(parents=True)
    evidence_root.mkdir(parents=True, exist_ok=True)
    captures: list[dict[str, Any]] = []
    phase_prerequisite = _prepare_phase_with_real_input(
        executable,
        runtime_root,
        evidence_root,
        captures,
        scenario_id,
    )

    mock_audit_path = runtime_root / ("Saved/Logs/WhiteoutStation_LoopbackAudit.jsonl")
    model_audit_path = runtime_root / MODEL_AUDIT_REL
    player_text = "继电器烧毁后还有什么替代方案？"
    mock_records: list[dict[str, Any]]
    game_records: list[dict[str, Any]]
    with MockLoopbackEndpoint(mock_audit_path) as endpoint:
        with _game_session(
            executable,
            runtime_root,
            "talk_gu_heng",
            log_name="dialogue_input_smoke.log",
            extra_args=[
                "-WhiteoutContinue",
                (
                    "-WhiteoutAgentEndpoint="
                    f"http://127.0.0.1:{endpoint.port}/chat/completions"
                ),
            ],
            llm_enabled=True,
        ) as game:
            v10.advance_opening(
                game.hwnd,
                evidence_root,
                scenario_id,
                captures,
            )
            v10.send_key(game.hwnd, v10.VK_F, 0.9)
            captures.append(
                v10.capture(
                    game.hwnd,
                    evidence_root,
                    f"{scenario_id}_intent",
                )
            )
            v10.send_key(game.hwnd, v10.VK_TAB, 0.3)
            v10.send_key(game.hwnd, v10.VK_RETURN, 0.55)
            captures.append(
                v10.capture(
                    game.hwnd,
                    evidence_root,
                    f"{scenario_id}_ask_text_entry",
                )
            )
            v10.send_unicode_text(game.hwnd, player_text)
            captures.append(
                v10.capture(
                    game.hwnd,
                    evidence_root,
                    f"{scenario_id}_chinese_typed",
                )
            )
            v10.send_key(game.hwnd, v10.VK_RETURN, 0.4)
            mock_records = _wait_for_json_lines(
                mock_audit_path,
                minimum=1,
                label="dialogue mock audit",
            )
            game_records = _wait_for_json_lines(
                model_audit_path,
                minimum=1,
                label="dialogue game ModelAudit",
            )
            time.sleep(0.8)
            captures.append(
                v10.capture(
                    game.hwnd,
                    evidence_root,
                    f"{scenario_id}_model_reply",
                )
            )
            v10.send_key(game.hwnd, v10.VK_ESCAPE, 0.8)
            captures.append(
                v10.capture(
                    game.hwnd,
                    evidence_root,
                    f"{scenario_id}_closed",
                )
            )
        request_count = endpoint.request_count

    if request_count != 1:
        raise SmokeError(
            "dialogue_free_text: loopback request count changed during cleanup"
        )
    mock_summary, game_summary = _validate_dialogue_audits(
        mock_records,
        game_records,
    )
    mock_evidence = _copy_evidence(
        mock_audit_path,
        evidence_root,
        f"{scenario_id}_LoopbackAudit.jsonl",
    )
    game_evidence = _copy_evidence(
        model_audit_path,
        evidence_root,
        f"{scenario_id}_ModelAudit.jsonl",
    )
    return {
        "scenario_id": scenario_id,
        "passed": True,
        "phase_prerequisite": phase_prerequisite,
        "real_inputs": [
            "left_mouse_opening",
            "space_opening",
            "F_dialogue",
            "Tab_focus_ask",
            "Enter_select_ask",
            "unicode_chinese_free_text",
            "Enter_submit",
            "Escape_leave",
        ],
        "player_text_characters": len(player_text),
        "mock_audit": {**mock_evidence, **mock_summary},
        "game_model_audit": {**game_evidence, **game_summary},
        "captures": captures,
    }


def validate_artifact_root(artifact_root: Path) -> tuple[Path, Path]:
    if artifact_root.is_symlink():
        raise SmokeError("Artifact root must not be a symbolic link")
    try:
        root = artifact_root.resolve(strict=True)
    except OSError as exc:
        raise SmokeError(f"Artifact root does not exist: {exc}") from exc
    if not root.is_dir():
        raise SmokeError("Artifact root must be a regular directory")
    if not root.name.startswith(ARTIFACT_PREFIX):
        raise SmokeError("Artifact root is not a unique v1.1 archive")
    run_id = root.name[len(ARTIFACT_PREFIX) :]
    if not RUN_ID_PATTERN.fullmatch(run_id):
        raise SmokeError("Artifact root has an invalid v1.1 run id")
    if (root / MANIFEST_REL).exists():
        raise SmokeError("Input smoke must run before manifest creation")
    if (root / OUTPUT_REL).exists():
        raise SmokeError("Refusing to mix with existing v1.1 input evidence")
    executable = root / EXECUTABLE_REL
    if not executable.is_file():
        raise SmokeError(f"Missing packaged executable: {EXECUTABLE_REL}")
    return root, executable


def run_input_smoke(artifact_root: Path) -> Path:
    root, executable = validate_artifact_root(artifact_root)
    validation_root = root / "Validation"
    validation_root.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix=".input-smoke-v11-",
        dir=validation_root,
    ) as temporary:
        staging_root = Path(temporary)
        scenarios = [
            run_antenna_scenario(executable, staging_root),
            run_dialogue_scenario(executable, staging_root),
        ]
        evidence_root = staging_root / "Evidence"
        report = {
            "schema": "whiteout.v1.1.real-input-smoke.v1",
            "passed": all(scenario.get("passed") is True for scenario in scenarios),
            "artifact_root_name": root.name,
            "credential_policy": {
                "api_key_value_logged": False,
                "api_key_value_persisted": False,
                "child_api_key_forced_empty": True,
                "loopback_authorization_present": False,
            },
            "scenarios": scenarios,
        }
        if report["passed"] is not True:
            raise SmokeError("One or more v1.1 input scenarios failed")
        summary_path = evidence_root / "input_smoke_summary.json"
        summary_path.write_text(
            json.dumps(report, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )
        evidence_root.replace(root / OUTPUT_REL)
    return root / OUTPUT_REL / "input_smoke_summary.json"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--artifact-root", type=Path, required=True)
    args = parser.parse_args()
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    try:
        summary_path = run_input_smoke(args.artifact_root)
    except (
        SmokeError,
        v10.SmokeError,
        OSError,
        subprocess.SubprocessError,
        psutil.Error,
    ) as exc:
        print(f"REAL INPUT SMOKE v1.1: FAIL: {exc}")
        return 1
    print(f"REAL INPUT SMOKE v1.1: PASS (2/2) summary={summary_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
