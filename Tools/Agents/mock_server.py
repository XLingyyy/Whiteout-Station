"""OpenAI-compatible deterministic v1.3 mock shared by agent tools."""

from __future__ import annotations

import argparse
import json
import sys
import threading
import time
from dataclasses import dataclass
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any, Mapping, Sequence

try:
    from .agent_contract import (
        EMOTIONS,
        MODEL,
        MOVEMENT_INTENTS,
        REACTION_ACTIONS,
        make_completion_envelope,
    )
except ImportError:
    from agent_contract import (
        EMOTIONS,
        MODEL,
        MOVEMENT_INTENTS,
        REACTION_ACTIONS,
        make_completion_envelope,
    )


MAX_REQUEST_BYTES = 64 * 1024
MOCK_MODES = (
    "valid_natural",
    "missing_atom",
    "forbidden_fact",
    "added_condition",
    "system_jargon",
    "invalid_json",
    "timeout",
)
LEGACY_MODE_MAP = {
    "valid": "valid_natural",
    "empty": "missing_atom",
    "added-condition": "added_condition",
    "topic-drift": "added_condition",
}


@dataclass(frozen=True)
class MockConfig:
    delay_seconds: float = 0.0
    status_code: int = 200
    status_sequence: tuple[int, ...] = ()
    empty_content: bool = False
    finish_reason: str = "stop"
    malformed_content: bool = False
    extra_field: bool = False
    mode: str = "valid_natural"
    # Deprecated v1.2 compatibility for old launch scripts.
    persona_tail_mode: str = ""

    @property
    def effective_mode(self) -> str:
        if self.persona_tail_mode:
            return LEGACY_MODE_MAP.get(self.persona_tail_mode, self.mode)
        return self.mode


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


def _allowed_token(context: Mapping[str, Any], field: str, default: str) -> str:
    values = context.get(field)
    if isinstance(values, list) and values:
        first = values[0]
        if isinstance(first, str) and first:
            return first
    return default


def _select_performance(
    context: Mapping[str, Any],
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


def _atom_id(value: Any) -> str:
    if isinstance(value, str):
        return value.strip()
    if isinstance(value, Mapping):
        candidate = value.get("id", value.get("atom_id", ""))
        return candidate.strip() if isinstance(candidate, str) else ""
    return ""


def _atom_fallback(value: Any) -> str:
    if isinstance(value, Mapping):
        candidate = value.get("fallback", value.get("natural_fallback", ""))
        if isinstance(candidate, str) and candidate.strip():
            return candidate.strip()
        for field in (
            "required_surface_tokens",
            "required_concept_tokens",
            "surface_tokens",
        ):
            tokens = value.get(field)
            if isinstance(tokens, list):
                for token in tokens:
                    if isinstance(token, str) and token.strip():
                        return token.strip()
    atom_id = _atom_id(value)
    return atom_id if atom_id else "我听见了"


def _atom_entries(context: Mapping[str, Any], field: str) -> list[Any]:
    values = context.get(field)
    return values if isinstance(values, list) else []


def _compact_natural_line(atoms: Sequence[Any]) -> str:
    pieces: list[str] = []
    for atom in atoms:
        piece = _atom_fallback(atom).strip().rstrip("。！？.!?；;，,")
        if piece:
            pieces.append(piece)
    if not pieces:
        return "我听见了。"
    line = "，".join(pieces) + "。"
    if len(line) <= 96:
        return line

    compact: list[str] = []
    for atom in atoms:
        if isinstance(atom, Mapping):
            tokens = atom.get(
                "required_surface_tokens",
                atom.get("required_concept_tokens", atom.get("surface_tokens", [])),
            )
            if isinstance(tokens, list) and tokens and isinstance(tokens[0], str):
                compact.append(tokens[0].strip())
                continue
        compact.append(_atom_fallback(atom).strip()[:12])
    return ("，".join(filter(None, compact)) + "。")[:96]


def _string_list(context: Mapping[str, Any], *fields: str) -> list[str]:
    for field in fields:
        values = context.get(field)
        if isinstance(values, list):
            return [value for value in values if isinstance(value, str) and value]
    return []


def build_mock_content(
    request: dict[str, Any],
    mode: str = "valid_natural",
) -> tuple[str, dict[str, object]]:
    messages = request.get("messages")
    if not isinstance(messages, list):
        raise ValueError("messages must be an array")
    system_text = _message_text(messages[0]) if messages else ""
    user_text = _message_text(messages[-1]) if messages else ""
    if "Classify" in system_text:
        return "intent", classify_intent(user_text)

    effective_mode = LEGACY_MODE_MAP.get(mode, mode)
    if effective_mode not in MOCK_MODES:
        raise ValueError("unknown mock mode")
    context = _request_context(user_text)
    must_atoms = _atom_entries(context, "must_realize")
    realized_atoms = must_atoms.copy()
    if effective_mode == "missing_atom" and realized_atoms:
        realized_atoms = realized_atoms[1:]

    npc_line = _compact_natural_line(realized_atoms)
    if effective_mode == "forbidden_fact":
        npc_line = npc_line.rstrip("。") + "，他的手伤已经影响维修。"
    elif effective_mode == "added_condition":
        npc_line = npc_line.rstrip("。") + "，你还得先把天线修好。"
    elif effective_mode == "system_jargon":
        npc_line = npc_line.rstrip("。") + "，这会消耗 AP，至少两点体力才行。"

    emotion = context.get("emotion", "")
    allowed_emotions = _string_list(context, "allowed_emotions")
    if not isinstance(emotion, str) or emotion not in EMOTIONS:
        emotion = allowed_emotions[0] if allowed_emotions else "focused"

    movement = context.get(
        "preset_movement_intent",
        _allowed_token(context, "allowed_movement_intents", "stay"),
    )
    if not isinstance(movement, str) or movement not in MOVEMENT_INTENTS:
        movement = "stay"
    reaction = context.get(
        "preset_reaction_action",
        _allowed_token(context, "allowed_reaction_actions", "neutral"),
    )
    if not isinstance(reaction, str) or reaction not in REACTION_ACTIONS:
        reaction = "neutral"
    movement, reaction = _select_performance(context, movement, reaction)

    planned_facts = _string_list(
        context,
        "planned_disclosure_fact_ids",
        "planned_knowledge_upgrade_fact_ids",
    )
    return (
        "npc_line",
        {
            "npc_line": npc_line,
            "realized_atom_ids": [
                atom_id for atom in realized_atoms if (atom_id := _atom_id(atom))
            ],
            "disclosed_fact_ids": planned_facts,
            "emotion": emotion,
            "movement_intent": movement,
            "reaction_action": reaction,
        },
    )


class WhiteoutMockServer(ThreadingHTTPServer):
    daemon_threads = True
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
    server_version = "WhiteoutAgentMock/1.3"

    def log_message(self, _format: str, *_args: object) -> None:
        return

    def _write_json(self, status_code: int, payload: dict[str, Any]) -> None:
        encoded = json.dumps(
            payload,
            ensure_ascii=False,
            separators=(",", ":"),
        ).encode("utf-8")
        try:
            self.send_response(status_code)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(encoded)))
            self.end_headers()
            self.wfile.write(encoded)
        except (BrokenPipeError, ConnectionResetError):
            return

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
        user_text = (
            _message_text(messages[-1])
            if isinstance(messages, list) and messages
            else ""
        )
        context = _request_context(user_text)
        safe_messages = messages if isinstance(messages, list) else []
        role_sequence = [
            message.get("role", "")
            for message in safe_messages
            if isinstance(message, dict)
        ]
        history_turns = (
            max(0, (len(safe_messages) - 2) // 2) if kind == "npc_line" else 0
        )
        thinking = request.get("thinking")
        response_format = request.get("response_format")
        record = {
            "timestamp_utc": time.time(),
            "kind": kind,
            "mock_mode": server.config.effective_mode,
            "model_matches_expected": request.get("model") == MODEL,
            "thinking_disabled": thinking == {"type": "disabled"},
            "stream_disabled": request.get("stream") is False,
            "response_format_json_object": response_format == {"type": "json_object"},
            "protocol_version": context.get("protocol_version", ""),
            "prompt_mode": context.get("prompt_mode", ""),
            "action_id": context.get("action_id", ""),
            "has_player_text": bool(context.get("player_said")),
            "must_atom_count": len(_atom_entries(context, "must_realize")),
            "may_atom_count": len(_atom_entries(context, "may_realize")),
            "planned_disclosure_count": len(
                _string_list(context, "planned_disclosure_fact_ids")
            ),
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
        delay = server.config.delay_seconds
        if server.config.effective_mode == "timeout" and delay <= 0:
            delay = 30.0
        if delay > 0:
            time.sleep(delay)

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
            kind, content = build_mock_content(request, server.config.effective_mode)
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
            if (
                server.config.malformed_content
                or server.config.effective_mode == "invalid_json"
            ):
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
    if config.effective_mode not in MOCK_MODES:
        raise ValueError("unknown mock mode")
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
    parser.add_argument("--mode", default="valid_natural", choices=MOCK_MODES)
    parser.add_argument(
        "--persona-tail-mode",
        choices=tuple(LEGACY_MODE_MAP),
        help=argparse.SUPPRESS,
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
            mode=args.mode,
            persona_tail_mode=args.persona_tail_mode or "",
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
