"""Drive the packaged v0.9 Demo through real keyboard and mouse input."""

from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
from ctypes import wintypes
from pathlib import Path
from typing import Any

import psutil
import win32gui
import win32ui
from PIL import Image, ImageGrab, ImageStat

try:
    from .v09_gate_common import MANIFEST_REL, RUN_ID_PATTERN
except ImportError:
    from v09_gate_common import MANIFEST_REL, RUN_ID_PATTERN


ARTIFACT_PREFIX = "WhiteoutStation-v0.9-Win64-"
EXECUTABLE_REL = Path("Windows/WhiteoutStation.exe")
OUTPUT_REL = Path("Validation/InputSmoke")
EVENT_LOG_REL = Path("Saved/Logs/WhiteoutStation_EventLog.json")
SMOKE_RES_X = 1280
SMOKE_RES_Y = 720

VK_ESCAPE = 0x1B
VK_RETURN = 0x0D
VK_SPACE = 0x20
VK_TAB = 0x09
VK_E = 0x45
VK_F = 0x46
VK_H = 0x48
VK_Q = 0x51
VK_W = 0x57
WM_CLOSE = 0x0010
WM_KEYDOWN = 0x0100
WM_KEYUP = 0x0101
WM_CHAR = 0x0102
WM_MOUSEMOVE = 0x0200
WM_LBUTTONDOWN = 0x0201
WM_LBUTTONUP = 0x0202
MK_LBUTTON = 0x0001
KEYEVENTF_KEYUP = 0x0002
KEYEVENTF_UNICODE = 0x0004
INPUT_KEYBOARD = 1
MOUSEEVENTF_LEFTDOWN = 0x0002
MOUSEEVENTF_LEFTUP = 0x0004
HWND_TOPMOST = -1
SWP_NOSIZE = 0x0001
SWP_NOMOVE = 0x0002
SWP_SHOWWINDOW = 0x0040

user32 = ctypes.windll.user32
user32.SetProcessDPIAware()


class SmokeError(RuntimeError):
    """Raised when input evidence is incomplete or inconsistent."""


ULONG_PTR = (
    ctypes.c_ulonglong
    if ctypes.sizeof(ctypes.c_void_p) == 8
    else ctypes.c_ulong
)


class KEYBDINPUT(ctypes.Structure):
    _fields_ = (
        ("wVk", wintypes.WORD),
        ("wScan", wintypes.WORD),
        ("dwFlags", wintypes.DWORD),
        ("time", wintypes.DWORD),
        ("dwExtraInfo", ULONG_PTR),
    )


class MOUSEINPUT(ctypes.Structure):
    _fields_ = (
        ("dx", wintypes.LONG),
        ("dy", wintypes.LONG),
        ("mouseData", wintypes.DWORD),
        ("dwFlags", wintypes.DWORD),
        ("time", wintypes.DWORD),
        ("dwExtraInfo", ULONG_PTR),
    )


class INPUTUNION(ctypes.Union):
    _fields_ = (
        ("ki", KEYBDINPUT),
        ("mi", MOUSEINPUT),
    )


class INPUT(ctypes.Structure):
    _fields_ = (("type", wintypes.DWORD), ("union", INPUTUNION))


def force_empty_credential_inputs(environment: dict[str, str]) -> None:
    environment["WHITEOUT_LLM_API_KEY"] = ""
    environment["WHITEOUT_LLM_ENABLED"] = ""


def activate_window(hwnd: int) -> bool:
    user32.ShowWindow(hwnd, 9)
    user32.SetWindowPos(
        hwnd,
        HWND_TOPMOST,
        0,
        0,
        0,
        0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW,
    )
    user32.BringWindowToTop(hwnd)
    user32.SetForegroundWindow(hwnd)
    return int(user32.GetForegroundWindow()) == int(hwnd)


def post_window_message(
    hwnd: int,
    message: int,
    wparam: int,
    lparam: int,
) -> None:
    if not user32.PostMessageW(hwnd, message, wparam, lparam):
        raise ctypes.WinError()


def visible_game_windows() -> dict[int, int]:
    windows: dict[int, int] = {}
    callback_type = ctypes.WINFUNCTYPE(
        wintypes.BOOL,
        wintypes.HWND,
        wintypes.LPARAM,
    )

    @callback_type
    def callback(hwnd: int, _lparam: int) -> bool:
        if not user32.IsWindowVisible(hwnd):
            return True
        process_id = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(process_id))
        try:
            process_name = psutil.Process(process_id.value).name().casefold()
        except (psutil.AccessDenied, psutil.NoSuchProcess):
            return True
        if process_name not in {
            "whiteoutstation.exe",
            "whiteoutstation-win64-shipping.exe",
        }:
            return True
        title_length = user32.GetWindowTextLengthW(hwnd)
        if title_length <= 0:
            return True
        title = ctypes.create_unicode_buffer(title_length + 1)
        user32.GetWindowTextW(hwnd, title, title_length + 1)
        if "whiteoutstation" in title.value.casefold():
            windows[int(hwnd)] = int(process_id.value)
        return True

    user32.EnumWindows(callback, 0)
    return windows


def wait_for_window(
    existing_handles: set[int],
    timeout_seconds: float = 45.0,
) -> tuple[int, int]:
    deadline = time.monotonic() + timeout_seconds
    while time.monotonic() < deadline:
        for hwnd, process_id in visible_game_windows().items():
            if hwnd not in existing_handles:
                activate_window(hwnd)
                return hwnd, process_id
        time.sleep(0.25)
    raise SmokeError("Packaged WhiteoutStation window did not appear")


def send_key(hwnd: int, virtual_key: int, settle: float = 0.4) -> None:
    if activate_window(hwnd):
        user32.keybd_event(virtual_key, 0, 0, 0)
        time.sleep(0.05)
        user32.keybd_event(virtual_key, 0, KEYEVENTF_KEYUP, 0)
    else:
        post_window_message(hwnd, WM_KEYDOWN, virtual_key, 0)
        time.sleep(0.05)
        post_window_message(hwnd, WM_KEYUP, virtual_key, 0)
    time.sleep(settle)


def hold_key(
    hwnd: int,
    virtual_key: int,
    duration: float,
    settle: float = 0.5,
) -> None:
    if activate_window(hwnd):
        user32.keybd_event(virtual_key, 0, 0, 0)
        time.sleep(duration)
        user32.keybd_event(virtual_key, 0, KEYEVENTF_KEYUP, 0)
    else:
        post_window_message(hwnd, WM_KEYDOWN, virtual_key, 0)
        time.sleep(duration)
        post_window_message(hwnd, WM_KEYUP, virtual_key, 0)
    time.sleep(settle)


def click_client(
    hwnd: int,
    x_ratio: float,
    y_ratio: float,
    settle: float = 0.5,
) -> None:
    has_foreground = activate_window(hwnd)
    left, top, right, bottom = client_bounds(hwnd)
    input_width = min(right - left, SMOKE_RES_X)
    input_height = min(bottom - top, SMOKE_RES_Y)
    client_x = round(input_width * x_ratio)
    client_y = round(input_height * y_ratio)
    if has_foreground:
        user32.SetCursorPos(left + client_x, top + client_y)
        user32.mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0)
        time.sleep(0.06)
        user32.mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0)
    else:
        position = (client_y << 16) | (client_x & 0xFFFF)
        post_window_message(hwnd, WM_MOUSEMOVE, 0, position)
        post_window_message(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, position)
        time.sleep(0.06)
        post_window_message(hwnd, WM_LBUTTONUP, 0, position)
    time.sleep(settle)


def send_unicode_text(hwnd: int, value: str) -> None:
    has_foreground = activate_window(hwnd)
    encoded = value.encode("utf-16-le")
    for index in range(0, len(encoded), 2):
        code_unit = int.from_bytes(encoded[index : index + 2], "little")
        if not has_foreground:
            post_window_message(hwnd, WM_CHAR, code_unit, 0)
            continue
        down = INPUT(
            INPUT_KEYBOARD,
            INPUTUNION(
                ki=KEYBDINPUT(
                    0,
                    code_unit,
                    KEYEVENTF_UNICODE,
                    0,
                    0,
                )
            ),
        )
        up = INPUT(
            INPUT_KEYBOARD,
            INPUTUNION(
                ki=KEYBDINPUT(
                    0,
                    code_unit,
                    KEYEVENTF_UNICODE | KEYEVENTF_KEYUP,
                    0,
                    0,
                )
            ),
        )
        inputs = (INPUT * 2)(down, up)
        if user32.SendInput(2, inputs, ctypes.sizeof(INPUT)) != 2:
            raise ctypes.WinError()
    time.sleep(0.25)


def client_bounds(hwnd: int) -> tuple[int, int, int, int]:
    rect = wintypes.RECT()
    if not user32.GetClientRect(hwnd, ctypes.byref(rect)):
        raise ctypes.WinError()
    point = wintypes.POINT(0, 0)
    if not user32.ClientToScreen(hwnd, ctypes.byref(point)):
        raise ctypes.WinError()
    return point.x, point.y, point.x + rect.right, point.y + rect.bottom


def move_cursor_away_from_center(hwnd: int) -> dict[str, list[int]]:
    left, top, right, bottom = client_bounds(hwnd)
    target = (
        left + round((right - left) * 0.82),
        top + round((bottom - top) * 0.78),
    )
    if not user32.SetCursorPos(*target):
        raise ctypes.WinError()
    time.sleep(0.08)
    actual = wintypes.POINT()
    if not user32.GetCursorPos(ctypes.byref(actual)):
        raise ctypes.WinError()
    return {
        "requested_screen": [target[0], target[1]],
        "actual_screen": [actual.x, actual.y],
    }


def assert_cursor_centered(
    hwnd: int,
    label: str,
    before: dict[str, list[int]],
) -> dict[str, Any]:
    left, top, right, bottom = client_bounds(hwnd)
    expected = (
        left + (right - left) // 2,
        top + (bottom - top) // 2,
    )
    actual = wintypes.POINT()
    if not user32.GetCursorPos(ctypes.byref(actual)):
        raise ctypes.WinError()
    error = (actual.x - expected[0], actual.y - expected[1])
    if abs(error[0]) > 4 or abs(error[1]) > 4:
        raise SmokeError(
            f"{label}: cursor did not return to client center; "
            f"expected={expected}, actual={(actual.x, actual.y)}"
        )
    return {
        "label": label,
        "before": before,
        "expected_screen": [expected[0], expected[1]],
        "actual_screen": [actual.x, actual.y],
        "error_px": [error[0], error[1]],
        "passed": True,
    }


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def capture(
    hwnd: int,
    output_root: Path,
    name: str,
) -> dict[str, Any]:
    activate_window(hwnd)
    window_left, window_top, window_right, window_bottom = (
        win32gui.GetWindowRect(hwnd)
    )
    width = window_right - window_left
    height = window_bottom - window_top
    window_dc_handle = win32gui.GetWindowDC(hwnd)
    window_dc = win32ui.CreateDCFromHandle(window_dc_handle)
    memory_dc = window_dc.CreateCompatibleDC()
    bitmap = win32ui.CreateBitmap()
    bitmap.CreateCompatibleBitmap(window_dc, width, height)
    memory_dc.SelectObject(bitmap)
    rendered = user32.PrintWindow(hwnd, memory_dc.GetSafeHdc(), 2)
    if rendered:
        bitmap_info = bitmap.GetInfo()
        bitmap_bits = bitmap.GetBitmapBits(True)
        image = Image.frombuffer(
            "RGB",
            (bitmap_info["bmWidth"], bitmap_info["bmHeight"]),
            bitmap_bits,
            "raw",
            "BGRX",
            0,
            1,
        )
        client_left, client_top, client_right, client_bottom = client_bounds(
            hwnd
        )
        image = image.crop(
            (
                client_left - window_left,
                client_top - window_top,
                client_right - window_left,
                client_bottom - window_top,
            )
        )
    else:
        image = ImageGrab.grab(bbox=client_bounds(hwnd), all_screens=True)
    win32gui.DeleteObject(bitmap.GetHandle())
    memory_dc.DeleteDC()
    window_dc.DeleteDC()
    win32gui.ReleaseDC(hwnd, window_dc_handle)
    width, height = image.size
    if (
        width < 1280
        or height < 720
        or abs((width / height) - (16 / 9)) > 0.01
    ):
        raise SmokeError(
            f"{name}: captured {image.size}, expected 16:9 at 1280x720 or higher"
        )
    output_path = output_root / f"{name}.png"
    image.save(output_path)
    if output_path.stat().st_size < 10_000:
        raise SmokeError(f"{name}: screenshot is unexpectedly small")
    luma = image.convert("L")
    mean_luma = float(ImageStat.Stat(luma).mean[0])
    histogram = luma.histogram()
    lit_pixel_fraction = sum(histogram[9:]) / sum(histogram)
    return {
        "name": name,
        "file": output_path.name,
        "size": list(image.size),
        "bytes": output_path.stat().st_size,
        "sha256": sha256_file(output_path),
        "mean_luma": round(mean_luma, 2),
        "lit_pixel_fraction": round(lit_pixel_fraction, 4),
    }


def close_game(
    process: subprocess.Popen[bytes],
    hwnd: int,
    window_process_id: int,
) -> None:
    if user32.IsWindow(hwnd):
        user32.PostMessageW(hwnd, WM_CLOSE, 0, 0)
    deadline = time.monotonic() + 8.0
    while time.monotonic() < deadline and user32.IsWindow(hwnd):
        time.sleep(0.2)
    if user32.IsWindow(hwnd):
        try:
            psutil.Process(window_process_id).terminate()
        except psutil.NoSuchProcess:
            pass
    try:
        process.wait(timeout=5.0)
    except subprocess.TimeoutExpired:
        process.terminate()
        process.wait(timeout=5.0)


def launch_game(
    executable: Path,
    runtime_root: Path,
    target_action: str,
) -> tuple[subprocess.Popen[bytes], int, int]:
    existing_handles = set(visible_game_windows())
    environment = os.environ.copy()
    force_empty_credential_inputs(environment)
    command = [
        str(executable),
        f"-WhiteoutInputSmokeTarget={target_action}",
        "-WhiteoutLLMEnabled=false",
        f"-UserDir={runtime_root.resolve()}",
        "-windowed",
        f"-ResX={SMOKE_RES_X}",
        f"-ResY={SMOKE_RES_Y}",
        "-ForceRes",
        "-NoSound",
        "-NoSplash",
    ]
    process = subprocess.Popen(
        command,
        cwd=executable.parent,
        env=environment,
    )
    hwnd, window_process_id = wait_for_window(existing_handles)
    time.sleep(2.0)
    return process, hwnd, window_process_id


def load_event_log(
    runtime_root: Path,
    expected_actions: list[str],
    scenario_id: str,
) -> dict[str, Any]:
    path = runtime_root / EVENT_LOG_REL
    if not path.is_file():
        raise SmokeError(f"{scenario_id}: missing exported event log")
    try:
        event_log = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise SmokeError(f"{scenario_id}: invalid event log") from exc
    events = event_log.get("events")
    if not isinstance(events, list):
        raise SmokeError(f"{scenario_id}: event array missing")
    action_ids = [
        event.get("action_id")
        for event in events
        if isinstance(event, dict)
    ]
    if action_ids != expected_actions:
        raise SmokeError(
            f"{scenario_id}: action path mismatch {action_ids!r}"
        )
    if (
        event_log.get("rules_version") != "0.9.0"
        or event_log.get("ending") != "SurvivalWait"
        or event_log.get("signal_sent") is not False
    ):
        raise SmokeError(f"{scenario_id}: settlement mismatch")
    return {
        "file": f"{scenario_id}_EventLog.json",
        "actions": action_ids,
        "ending": event_log["ending"],
        "score": event_log.get("score"),
        "remaining_ap": event_log.get("remaining_ap"),
        "model_calls": event_log.get("model_calls"),
    }


def advance_opening(
    hwnd: int,
    output_root: Path,
    scenario_id: str,
    captures: list[dict[str, Any]],
) -> None:
    captures.append(capture(hwnd, output_root, f"{scenario_id}_story_01"))
    click_client(hwnd, 0.5, 0.5, 1.25)
    captures.append(capture(hwnd, output_root, f"{scenario_id}_story_02"))
    for next_stage in range(3, 11):
        send_key(hwnd, VK_SPACE, 1.25)
        if next_stage in {4, 7, 10}:
            captures.append(
                capture(
                    hwnd,
                    output_root,
                    f"{scenario_id}_story_{next_stage:02d}",
                )
            )
    send_key(hwnd, VK_SPACE, 3.2)
    captures.append(capture(hwnd, output_root, f"{scenario_id}_game"))
    if captures[-1]["lit_pixel_fraction"] < 0.05:
        raise SmokeError(
            f"{scenario_id}: station reveal stayed black "
            f"(lit_pixel_fraction={captures[-1]['lit_pixel_fraction']:.4f})"
        )


def settle_run(
    hwnd: int,
    output_root: Path,
    scenario_id: str,
    captures: list[dict[str, Any]],
) -> None:
    send_key(hwnd, VK_RETURN, 0.55)
    captures.append(
        capture(hwnd, output_root, f"{scenario_id}_settle_warning")
    )
    send_key(hwnd, VK_RETURN, 0.8)
    captures.append(
        capture(hwnd, output_root, f"{scenario_id}_ending_cinematic")
    )
    time.sleep(4.0)
    captures.append(capture(hwnd, output_root, f"{scenario_id}_results"))


def run_food_scenario(
    executable: Path,
    staging_root: Path,
) -> dict[str, Any]:
    scenario_id = "survival_controls"
    runtime_root = staging_root / "Runtime" / scenario_id
    output_root = staging_root / "Evidence"
    runtime_root.mkdir(parents=True)
    output_root.mkdir(parents=True, exist_ok=True)
    captures: list[dict[str, Any]] = []
    cursor_checks: list[dict[str, Any]] = []
    process, hwnd, window_process_id = launch_game(
        executable,
        runtime_root,
        "distribute_food",
    )
    try:
        advance_opening(hwnd, output_root, scenario_id, captures)
        before = move_cursor_away_from_center(hwnd)
        send_key(hwnd, VK_H, 0.8)
        cursor_checks.append(
            assert_cursor_centered(hwnd, "guide_open", before)
        )
        captures.append(capture(hwnd, output_root, f"{scenario_id}_guide"))
        before = move_cursor_away_from_center(hwnd)
        send_key(hwnd, VK_ESCAPE, 0.55)
        cursor_checks.append(
            assert_cursor_centered(hwnd, "guide_close", before)
        )
        before = move_cursor_away_from_center(hwnd)
        send_key(hwnd, VK_E, 0.8)
        cursor_checks.append(
            assert_cursor_centered(hwnd, "evidence_open", before)
        )
        captures.append(
            capture(hwnd, output_root, f"{scenario_id}_evidence")
        )
        before = move_cursor_away_from_center(hwnd)
        send_key(hwnd, VK_E, 0.55)
        cursor_checks.append(
            assert_cursor_centered(hwnd, "evidence_close", before)
        )
        hold_key(hwnd, VK_W, 0.35, 0.8)
        captures.append(capture(hwnd, output_root, f"{scenario_id}_focus"))
        send_key(hwnd, VK_F, 0.75)
        captures.append(capture(hwnd, output_root, f"{scenario_id}_preview"))
        send_key(hwnd, VK_Q, 0.55)
        captures.append(
            capture(hwnd, output_root, f"{scenario_id}_option_q")
        )
        send_key(hwnd, VK_F, 0.9)
        captures.append(
            capture(hwnd, output_root, f"{scenario_id}_committed")
        )
        settle_run(hwnd, output_root, scenario_id, captures)
        event_summary = load_event_log(
            runtime_root,
            ["distribute_food"],
            scenario_id,
        )
        shutil.copy2(
            runtime_root / EVENT_LOG_REL,
            output_root / event_summary["file"],
        )
        return {
            "scenario_id": scenario_id,
            "passed": True,
            "real_inputs": [
                "left_mouse_opening",
                "space_opening",
                "H",
                "Escape",
                "E",
                "W_hold",
                "F_preview",
                "Q_option",
                "F_commit",
                "Enter_confirm",
                "Enter_settle",
            ],
            "cursor_center_checks": cursor_checks,
            "event_log": event_summary,
            "captures": captures,
        }
    finally:
        close_game(process, hwnd, window_process_id)


def run_dialogue_scenario(
    executable: Path,
    staging_root: Path,
) -> dict[str, Any]:
    scenario_id = "dialogue_free_text"
    runtime_root = staging_root / "Runtime" / scenario_id
    output_root = staging_root / "Evidence"
    runtime_root.mkdir(parents=True)
    output_root.mkdir(parents=True, exist_ok=True)
    captures: list[dict[str, Any]] = []
    cursor_checks: list[dict[str, Any]] = []
    process, hwnd, window_process_id = launch_game(
        executable,
        runtime_root,
        "talk_gu_heng",
    )
    try:
        advance_opening(hwnd, output_root, scenario_id, captures)
        before = move_cursor_away_from_center(hwnd)
        send_key(hwnd, VK_F, 0.9)
        cursor_checks.append(
            assert_cursor_centered(hwnd, "dialogue_open", before)
        )
        captures.append(
            capture(hwnd, output_root, f"{scenario_id}_intent")
        )
        send_key(hwnd, VK_TAB, 0.3)
        send_key(hwnd, VK_RETURN, 0.55)
        captures.append(
            capture(hwnd, output_root, f"{scenario_id}_text_entry")
        )
        send_unicode_text(hwnd, "继电器怎么会烧毁？")
        send_key(hwnd, VK_RETURN, 1.5)
        captures.append(
            capture(hwnd, output_root, f"{scenario_id}_reply")
        )
        before = move_cursor_away_from_center(hwnd)
        send_key(hwnd, VK_ESCAPE, 0.8)
        cursor_checks.append(
            assert_cursor_centered(hwnd, "dialogue_close", before)
        )
        captures.append(
            capture(hwnd, output_root, f"{scenario_id}_closed")
        )
        settle_run(hwnd, output_root, scenario_id, captures)
        event_summary = load_event_log(
            runtime_root,
            ["talk_gu_heng"],
            scenario_id,
        )
        shutil.copy2(
            runtime_root / EVENT_LOG_REL,
            output_root / event_summary["file"],
        )
        return {
            "scenario_id": scenario_id,
            "passed": True,
            "real_inputs": [
                "left_mouse_opening",
                "space_opening",
                "F_dialogue",
                "Tab_focus_ask",
                "Enter_select_ask",
                "unicode_free_text",
                "Enter_submit",
                "Escape_leave",
                "Enter_confirm",
                "Enter_settle",
            ],
            "cursor_center_checks": cursor_checks,
            "event_log": event_summary,
            "captures": captures,
        }
    finally:
        close_game(process, hwnd, window_process_id)


def validate_artifact_root(artifact_root: Path) -> tuple[Path, Path]:
    try:
        root = artifact_root.resolve(strict=True)
    except OSError as exc:
        raise SmokeError(f"Artifact root does not exist: {exc}") from exc
    if not root.is_dir() or root.is_symlink():
        raise SmokeError("Artifact root must be a regular directory")
    if not root.name.startswith(ARTIFACT_PREFIX):
        raise SmokeError("Artifact root is not a unique v0.9 archive")
    run_id = root.name[len(ARTIFACT_PREFIX) :]
    if not RUN_ID_PATTERN.fullmatch(run_id):
        raise SmokeError("Artifact root has an invalid v0.9 run id")
    if (root / MANIFEST_REL).exists():
        raise SmokeError("Input smoke must run before manifest creation")
    if (root / OUTPUT_REL).exists():
        raise SmokeError("Refusing to mix with existing input evidence")
    executable = root / EXECUTABLE_REL
    if not executable.is_file():
        raise SmokeError(f"Missing packaged executable: {EXECUTABLE_REL}")
    return root, executable


def run_input_smoke(artifact_root: Path) -> Path:
    root, executable = validate_artifact_root(artifact_root)
    validation_root = root / "Validation"
    validation_root.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix=".input-smoke-",
        dir=validation_root,
    ) as temporary:
        staging_root = Path(temporary)
        scenarios = [
            run_food_scenario(executable, staging_root),
            run_dialogue_scenario(executable, staging_root),
        ]
        evidence_root = staging_root / "Evidence"
        report = {
            "schema": "whiteout.v0.9.real-input-smoke.v1",
            "passed": True,
            "artifact_root_name": root.name,
            "credential_policy": {
                "api_key_value_read": False,
                "api_key_value_persisted": False,
                "child_api_key_forced_empty": True,
            },
            "scenarios": scenarios,
        }
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
    sys.stdout.reconfigure(encoding="utf-8")
    try:
        summary_path = run_input_smoke(args.artifact_root)
    except (SmokeError, OSError, subprocess.SubprocessError) as exc:
        print(f"REAL INPUT SMOKE v0.9: FAIL: {exc}")
        return 1
    print(f"REAL INPUT SMOKE v0.9: PASS (2/2) summary={summary_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
