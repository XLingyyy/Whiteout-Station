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
    persona_tail_mode: str = "valid"


def classify_intent(text: str) -> dict[str, object]:
    target_character = "ye_cheng" if "叶澄" in text else "gu_heng"
    target_action_id = "repair_generator" if "发电机" in text else "none"
    if any(token in text for token in ("要怎么样", "什么条件", "要我做什么", "才肯")):
        return {
            "speech_act": "ask",
            "query_type": "requirements",
            "target_action_id": "repair_generator",
            "target_fact_id": "none",
            "target_character": target_character,
            "confidence": 0.98,
        }
    if any(token in text for token in ("保证", "记录", "不弃站")):
        return {
            "speech_act": "promise",
            "query_type": "unknown",
            "target_action_id": target_action_id,
            "target_fact_id": "none",
            "target_character": target_character,
            "confidence": 0.95,
        }
    if any(token in text for token in ("别怕", "放心", "冷静")):
        return {
            "speech_act": "reassure",
            "query_type": "unknown",
            "target_action_id": target_action_id,
            "target_fact_id": "none",
            "target_character": target_character,
            "confidence": 0.94,
        }
    if any(token in text for token in ("撒谎", "证据", "隐瞒")):
        return {
            "speech_act": "challenge",
            "query_type": "evidence" if "证据" in text else "unknown",
            "target_action_id": target_action_id,
            "target_fact_id": "none",
            "target_character": target_character,
            "confidence": 0.93,
        }
    return {
        "speech_act": "ask",
        "query_type": "unknown",
        "target_action_id": target_action_id,
        "target_fact_id": "none",
        "target_character": target_character,
        "confidence": 0.91,
    }


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


def _select_performance(
    context: dict[str, Any],
    preset_movement: str,
    preset_reaction: str,
) -> tuple[str, str]:
    if context.get("action_id") not in {"talk_gu_heng", "talk_ye_cheng"}:
        return "stay", preset_reaction

    player_said = context.get("player_said", "")
    if not isinstance(player_said, str):
        return preset_movement, preset_reaction
    if any(token in player_said for token in ("退后", "离我远", "别靠近")):
        return "step_back", "reject"
    if any(token in player_said for token in ("回去", "归位", "回原位")):
        return "return_to_post", "consider"
    if any(token in player_said for token in ("过来", "靠近", "来我这")):
        return "step_closer", "acknowledge"
    if any(token in player_said for token in ("别怕", "放心", "冷静", "撑住")):
        return "stay", "reassure"
    if any(token in player_said for token in ("撒谎", "骗我", "隐瞒")):
        return "step_back", "alarmed"
    return preset_movement, preset_reaction


def build_mock_content(
    request: dict[str, Any],
    persona_tail_mode: str = "valid",
) -> tuple[str, dict[str, object]]:
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
    semantic_spine = context.get("semantic_spine", "联调语义骨架。")
    if not isinstance(semantic_spine, str) or not semantic_spine.strip():
        semantic_spine = "联调语义骨架。"
    movement_intent = context.get("preset_movement_intent", "stay")
    if movement_intent not in {"stay", "step_closer", "step_back", "return_to_post"}:
        movement_intent = "stay"
    reaction_action = context.get("preset_reaction_action", "neutral")
    if reaction_action not in {
        "neutral",
        "acknowledge",
        "consider",
        "reassure",
        "reject",
        "alarmed",
    }:
        reaction_action = "neutral"
    movement_intent, reaction_action = _select_performance(
        context,
        movement_intent,
        reaction_action,
    )
    persona_tail = "【mock】我听见了。"
    if persona_tail_mode == "empty":
        persona_tail = ""
    elif persona_tail_mode == "added-condition":
        persona_tail = "你还必须先把天线修好。"
    elif persona_tail_mode == "topic-drift":
        persona_tail = "食物和药品也归我安排。"
    return (
        "npc_line",
        {
            "persona_tail": persona_tail,
            "emotion": emotion[:32],
            "used_action_id": action_id[:64],
            "referenced_fact_ids": [],
            "movement_intent": movement_intent,
            "reaction_action": reaction_action,
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
            kind, content = build_mock_content(request, server.config.persona_tail_mode)
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
        "--persona-tail-mode",
        default="valid",
        choices=("valid", "empty", "added-condition", "topic-drift"),
    )
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
            persona_tail_mode=args.persona_tail_mode,
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
