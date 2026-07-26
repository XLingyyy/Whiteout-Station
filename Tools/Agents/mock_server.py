"""OpenAI-compatible local mock server shared by Whiteout Station agent tools."""

from __future__ import annotations

import argparse
import json
import sys
import threading
import time
from dataclasses import dataclass
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any

try:
    from .agent_contract import MODEL, make_completion_envelope
except ImportError:
    from agent_contract import MODEL, make_completion_envelope


MAX_REQUEST_BYTES = 64 * 1024


@dataclass(frozen=True)
class MockConfig:
    delay_seconds: float = 0.0
    status_code: int = 200
    status_sequence: tuple[int, ...] = ()
    empty_content: bool = False
    finish_reason: str = "stop"
    malformed_content: bool = False
    extra_field: bool = False


def classify_intent(text: str) -> dict[str, object]:
    if any(token in text for token in ("修复", "发电机", "维修间")):
        return {
            "intent": "promise",
            "promise_condition": "heat_repair_room",
            "confidence": 0.97,
        }
    if any(token in text for token in ("保证", "记录", "不弃站")):
        return {
            "intent": "promise",
            "promise_condition": "keep_records",
            "confidence": 0.95,
        }
    if any(token in text for token in ("别怕", "放心", "冷静")):
        return {
            "intent": "reassure",
            "promise_condition": "none",
            "confidence": 0.94,
        }
    if any(token in text for token in ("撒谎", "证据", "隐瞒")):
        return {
            "intent": "challenge",
            "promise_condition": "none",
            "confidence": 0.93,
        }
    return {"intent": "ask", "promise_condition": "none", "confidence": 0.91}


def _message_text(message: Any) -> str:
    if not isinstance(message, dict):
        return ""
    content = message.get("content")
    return content if isinstance(content, str) else ""


def _request_context(user_text: str) -> dict[str, Any]:
    try:
        context = json.loads(user_text)
    except json.JSONDecodeError:
        return {}
    return context if isinstance(context, dict) else {}


def build_mock_content(request: dict[str, Any]) -> tuple[str, dict[str, object]]:
    messages = request.get("messages")
    if not isinstance(messages, list):
        raise ValueError("messages must be an array")
    system_text = _message_text(messages[0]) if messages else ""
    user_text = _message_text(messages[-1]) if messages else ""
    if "Classify" in system_text:
        return "intent", classify_intent(user_text)

    context = _request_context(user_text)
    action_id = context.get("action_id", "probe_availability")
    if not isinstance(action_id, str) or not action_id:
        action_id = "probe_availability"
    emotion = context.get("emotion", "focused")
    if not isinstance(emotion, str) or not emotion:
        emotion = "focused"
    preset = context.get("preset_utterance", "联调响应正常。")
    if not isinstance(preset, str) or not preset.strip():
        preset = "联调响应正常。"
    npc_line = f"【mock】{preset.strip()}"[:240]
    return (
        "npc_line",
        {
            "npc_line": npc_line,
            "emotion": emotion[:32],
            "used_action_id": action_id[:64],
            "referenced_fact_ids": [],
        },
    )


class WhiteoutMockServer(ThreadingHTTPServer):
    config: MockConfig
    audit_path: Path | None
    audit_lock: threading.Lock
    request_count: int
    request_lock: threading.Lock

    def next_status_code(self) -> int:
        with self.request_lock:
            index = self.request_count
            self.request_count += 1
        if self.config.status_sequence:
            return self.config.status_sequence[
                min(index, len(self.config.status_sequence) - 1)
            ]
        return self.config.status_code


class Handler(BaseHTTPRequestHandler):
    server_version = "WhiteoutAgentMock/0.5"

    def log_message(self, _format: str, *_args: object) -> None:
        return

    def _write_json(self, status_code: int, payload: dict[str, Any]) -> None:
        encoded = json.dumps(
            payload,
            ensure_ascii=False,
            separators=(",", ":"),
        ).encode("utf-8")
        self.send_response(status_code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(encoded)))
        self.end_headers()
        self.wfile.write(encoded)

    def _audit(
        self,
        *,
        kind: str,
        request: dict[str, Any],
        request_bytes: int,
        status_code: int,
    ) -> None:
        server = self.server
        if not isinstance(server, WhiteoutMockServer) or server.audit_path is None:
            return
        messages = request.get("messages")
        user_text = _message_text(messages[-1]) if isinstance(messages, list) and messages else ""
        context = _request_context(user_text)
        safe_messages = messages if isinstance(messages, list) else []
        role_sequence = [
            message.get("role", "")
            for message in safe_messages
            if isinstance(message, dict)
        ]
        history_turns = (
            max(0, (len(safe_messages) - 2) // 2)
            if kind == "npc_line"
            else 0
        )
        thinking = request.get("thinking")
        response_format = request.get("response_format")
        record = {
            "timestamp_utc": time.time(),
            "kind": kind,
            "model_matches_expected": request.get("model") == MODEL,
            "thinking_disabled": thinking == {"type": "disabled"},
            "stream_disabled": request.get("stream") is False,
            "response_format_json_object": response_format == {"type": "json_object"},
            "action_id": context.get("action_id", ""),
            "dialogue_act": context.get("dialogue_act", ""),
            "has_player_text": bool(context.get("player_said")),
            "message_count": len(safe_messages),
            "role_sequence": role_sequence,
            "history_turns": history_turns,
            "request_bytes": request_bytes,
            "status_code": status_code,
            "finish_reason": server.config.finish_reason,
            "empty_content": server.config.empty_content,
            "authorization_present": bool(self.headers.get("Authorization")),
        }
        encoded = json.dumps(record, ensure_ascii=False, separators=(",", ":"))
        with server.audit_lock:
            with server.audit_path.open("a", encoding="utf-8") as handle:
                handle.write(encoded + "\n")

    def do_POST(self) -> None:  # noqa: N802 - stdlib handler API
        server = self.server
        if not isinstance(server, WhiteoutMockServer):
            self.send_error(500)
            return
        if server.config.delay_seconds > 0:
            time.sleep(server.config.delay_seconds)

        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            self._write_json(400, {"error": {"type": "invalid_request"}})
            return
        if length <= 0 or length > MAX_REQUEST_BYTES:
            self._write_json(400, {"error": {"type": "invalid_request"}})
            return
        try:
            request = json.loads(self.rfile.read(length).decode("utf-8"))
            if not isinstance(request, dict):
                raise ValueError("request must be an object")
            kind, content = build_mock_content(request)
        except (UnicodeDecodeError, json.JSONDecodeError, ValueError):
            self._write_json(400, {"error": {"type": "invalid_request"}})
            return

        status_code = server.next_status_code()
        if not 200 <= status_code < 300:
            payload = {
                "error": {
                    "type": "mock_http_error",
                    "code": f"mock_{status_code}",
                }
            }
        else:
            if server.config.extra_field and isinstance(content, dict):
                content["state_changes"] = {}
            payload = make_completion_envelope(
                content,
                model=MODEL,
                finish_reason=server.config.finish_reason,
                empty_content=server.config.empty_content,
            )
            if server.config.malformed_content:
                payload["choices"][0]["message"]["content"] = "{malformed-json"
        self._write_json(status_code, payload)
        self._audit(
            kind=kind,
            request=request,
            request_bytes=length,
            status_code=status_code,
        )


def create_server(
    host: str,
    port: int,
    *,
    config: MockConfig,
    audit_path: Path | None = None,
) -> WhiteoutMockServer:
    if audit_path is not None:
        audit_path.parent.mkdir(parents=True, exist_ok=True)
    server = WhiteoutMockServer((host, port), Handler)
    server.config = config
    server.audit_path = audit_path
    server.audit_lock = threading.Lock()
    server.request_count = 0
    server.request_lock = threading.Lock()
    return server


def build_parser(description: str) -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--audit", type=Path)
    parser.add_argument("--delay", type=float, default=0.0)
    parser.add_argument("--status-code", type=int, default=200)
    parser.add_argument(
        "--status-sequence",
        help="Comma-separated HTTP statuses, for example 429,200",
    )
    parser.add_argument("--empty-content", action="store_true")
    parser.add_argument("--malformed-content", action="store_true")
    parser.add_argument("--extra-field", action="store_true")
    parser.add_argument(
        "--finish-reason",
        default="stop",
        choices=(
            "stop",
            "length",
            "content_filter",
            "tool_calls",
            "insufficient_system_resource",
        ),
    )
    return parser


def run_from_cli(description: str) -> int:
    args = build_parser(description).parse_args()
    try:
        status_sequence = tuple(
            int(item.strip())
            for item in (args.status_sequence or "").split(",")
            if item.strip()
        )
    except ValueError:
        print("mock_config=INVALID", file=sys.stderr)
        return 2
    if (
        args.delay < 0
        or not 100 <= args.status_code <= 599
        or any(not 100 <= status <= 599 for status in status_sequence)
    ):
        print("mock_config=INVALID", file=sys.stderr)
        return 2
    server = create_server(
        args.host,
        args.port,
        config=MockConfig(
            delay_seconds=args.delay,
            status_code=args.status_code,
            status_sequence=status_sequence,
            empty_content=args.empty_content,
            finish_reason=args.finish_reason,
            malformed_content=args.malformed_content,
            extra_field=args.extra_field,
        ),
        audit_path=args.audit,
    )
    print(f"READY http://{args.host}:{server.server_port}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0
