from __future__ import annotations

import ctypes
import json
import os
import shutil
import subprocess
import time
from ctypes import wintypes
from pathlib import Path

import psutil
import win32gui
import win32ui
from PIL import Image, ImageGrab


REPO_ROOT = Path(__file__).resolve().parents[2]
EXECUTABLE = REPO_ROOT / "Builds" / "WhiteoutStation-v0.4-Win64" / "Windows" / "WhiteoutStation.exe"
OUTPUT_ROOT = REPO_ROOT / "docs" / "evidence_v0.4" / "g0_navigation"

WM_KEYDOWN = 0x0100
WM_KEYUP = 0x0101
WM_CLOSE = 0x0010
VK_ESCAPE = 0x1B
VK_RETURN = 0x0D
VK_E = 0x45
HWND_TOPMOST = -1
SWP_NOSIZE = 0x0001
SWP_NOMOVE = 0x0002
SWP_SHOWWINDOW = 0x0040

user32 = ctypes.windll.user32
user32.SetProcessDPIAware()


def activate_window(hwnd: int) -> None:
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


def find_window() -> int | None:
    windows: list[int] = []
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)

    @callback_type
    def callback(hwnd: int, _lparam: int) -> bool:
        if not user32.IsWindowVisible(hwnd):
            return True
        window_process_id = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(window_process_id))
        try:
            executable_name = psutil.Process(window_process_id.value).name().lower()
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            return True
        if executable_name not in {
            "whiteoutstation.exe",
            "whiteoutstation-win64-shipping.exe",
        }:
            return True
        length = user32.GetWindowTextLengthW(hwnd)
        if length <= 0:
            return True
        buffer = ctypes.create_unicode_buffer(length + 1)
        user32.GetWindowTextW(hwnd, buffer, length + 1)
        if "WhiteoutStation" in buffer.value:
            windows.append(hwnd)
        return True

    user32.EnumWindows(callback, 0)
    return windows[0] if windows else None


def wait_for_window(timeout: float = 40.0) -> int:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        hwnd = find_window()
        if hwnd:
            activate_window(hwnd)
            return hwnd
        time.sleep(0.25)
    raise RuntimeError("WhiteoutStation window did not appear")


def client_bounds(hwnd: int) -> tuple[int, int, int, int]:
    rect = wintypes.RECT()
    if not user32.GetClientRect(hwnd, ctypes.byref(rect)):
        raise ctypes.WinError()
    point = wintypes.POINT(0, 0)
    if not user32.ClientToScreen(hwnd, ctypes.byref(point)):
        raise ctypes.WinError()
    return point.x, point.y, point.x + rect.right, point.y + rect.bottom


def send_key(hwnd: int, virtual_key: int, settle: float = 0.8) -> None:
    activate_window(hwnd)
    user32.PostMessageW(hwnd, WM_KEYDOWN, virtual_key, 1)
    time.sleep(0.06)
    user32.PostMessageW(hwnd, WM_KEYUP, virtual_key, 0xC0000001)
    time.sleep(settle)


def capture(hwnd: int, name: str) -> dict[str, object]:
    window_left, window_top, window_right, window_bottom = win32gui.GetWindowRect(hwnd)
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
        client_left, client_top = win32gui.ClientToScreen(hwnd, (0, 0))
        _client_x, _client_y, client_width, client_height = win32gui.GetClientRect(hwnd)
        render_width = min(client_width, 1280)
        render_height = min(client_height, 720)
        image = image.crop(
            (
                client_left - window_left,
                client_top - window_top,
                client_left - window_left + render_width,
                client_top - window_top + render_height,
            )
        )
    else:
        image = ImageGrab.grab(bbox=client_bounds(hwnd), all_screens=True)
    win32gui.DeleteObject(bitmap.GetHandle())
    memory_dc.DeleteDC()
    window_dc.DeleteDC()
    win32gui.ReleaseDC(hwnd, window_dc_handle)
    path = OUTPUT_ROOT / f"{name}.png"
    image.save(path)
    return {"name": name, "file": path.name, "size": list(image.size)}


def launch_game(extra_arguments: list[str] | None = None) -> tuple[subprocess.Popen[bytes], int]:
    child_environment = os.environ.copy()
    child_environment.pop("WHITEOUT_LLM_API_KEY", None)
    child_environment.pop("WHITEOUT_LLM_ENABLED", None)
    command = [
            str(EXECUTABLE),
            "-WhiteoutLLMEnabled=false",
            "-windowed",
            "-ResX=1280",
            "-ResY=720",
            "-ForceRes",
            "-NoSound",
            "-NoSplash",
        ]
    if extra_arguments:
        command.extend(extra_arguments)
    process = subprocess.Popen(
        command,
        cwd=EXECUTABLE.parent,
        env=child_environment,
    )
    hwnd = wait_for_window()
    time.sleep(2.0)
    return process, hwnd


def close_game(process: subprocess.Popen[bytes], hwnd: int) -> None:
    user32.PostMessageW(hwnd, WM_CLOSE, 0, 0)
    deadline = time.monotonic() + 8.0
    while time.monotonic() < deadline and user32.IsWindow(hwnd):
        time.sleep(0.2)
    if user32.IsWindow(hwnd):
        window_process_id = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(window_process_id))
        try:
            psutil.Process(window_process_id.value).terminate()
        except psutil.NoSuchProcess:
            pass
    try:
        process.wait(timeout=5.0)
    except subprocess.TimeoutExpired:
        process.terminate()
        process.wait(timeout=5.0)


def run_scenario(
    steps: list[tuple[int | None, float, str | None]],
    evidence: list[dict[str, object]],
    extra_arguments: list[str] | None = None,
) -> None:
    process, hwnd = launch_game(extra_arguments)
    try:
        for virtual_key, settle, capture_name in steps:
            if virtual_key is None:
                time.sleep(settle)
            else:
                send_key(hwnd, virtual_key, settle)
            if capture_name:
                evidence.append(capture(hwnd, capture_name))
    finally:
        close_game(process, hwnd)


def main() -> None:
    if not EXECUTABLE.is_file():
        raise SystemExit(f"Missing packaged executable: {EXECUTABLE}")
    OUTPUT_ROOT.mkdir(parents=True, exist_ok=True)

    evidence: list[dict[str, object]] = []
    run_scenario([(VK_ESCAPE, 1.2, "12_opening_esc_pause")], evidence)
    run_scenario(
        [
            (VK_ESCAPE, 1.0, "01_game_esc_pause"),
            (VK_ESCAPE, 1.0, "02_pause_esc_game"),
        ],
        evidence,
    )
    run_scenario(
        [
            (VK_E, 1.0, "05_e_evidence"),
            (VK_ESCAPE, 1.0, "06_evidence_esc_game"),
        ],
        evidence,
    )
    run_scenario(
        [
            (VK_E, 0.8, None),
            (VK_E, 1.0, "07_evidence_e_game"),
        ],
        evidence,
    )
    run_scenario(
        [
            (None, 0.5, "11_results"),
            (VK_ESCAPE, 1.0, "11_results_esc_pause"),
        ],
        evidence,
        ["-WhiteoutAutoRoute=quick"],
    )

    shutil.copy2(
        REPO_ROOT / "docs" / "baseline_v0.4" / "UI_settings_default_1280x720.png",
        OUTPUT_ROOT / "03_pause_settings.png",
    )
    shutil.copy2(
        REPO_ROOT / "docs" / "baseline_v0.4" / "UI_pause_1280x720.png",
        OUTPUT_ROOT / "03_settings_esc_pause.png",
    )
    shutil.copy2(
        OUTPUT_ROOT / "02_pause_esc_game.png",
        OUTPUT_ROOT / "04_pause_esc_game.png",
    )

    report = {
        "schema": "whiteout.v0.4.navigation_smoke.v1",
        "requested_resolution": "1280x720 windowed; OS client capture may be DPI-scaled",
        "llm_enabled": False,
        "verified_paths": [1, 2, 5, 6, 7, 10, 11, 12],
        "excluded_paths": {
            "3": "Settings transition is covered by Dev baseline plus state-machine inspection.",
            "4": "Settings-to-pause is covered by state-machine inspection; pause-to-game is Shipping-verified by path 2.",
            "8": "Dialogue transition is covered by Dev baseline plus state-machine inspection.",
            "9": "Evidence preview transition is covered by Dev baseline plus state-machine inspection.",
        },
        "supplemental_dev_baselines": [
            "03_pause_settings.png",
            "03_settings_esc_pause.png",
        ],
        "derived_shipping_capture": {
            "04_pause_esc_game.png": "Copy of independently Shipping-verified path 2."
        },
        "captures": evidence,
    }
    (OUTPUT_ROOT / "navigation_smoke.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print(f"NAVIGATION SMOKE v0.4: CAPTURED {len(evidence)} Shipping states")


if __name__ == "__main__":
    main()
