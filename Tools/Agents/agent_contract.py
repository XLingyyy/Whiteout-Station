"""Shared DeepSeek request and response contract for Whiteout Station tools."""

from __future__ import annotations

import json
from dataclasses import dataclass
from enum import Enum
from typing import Any, Iterable
from urllib.parse import urlsplit


MODEL = "deepseek-v4-flash"
OFFICIAL_ENDPOINT = "https://api.deepseek.com/chat/completions"
MAX_RESPONSE_BYTES = 64 * 1024
NPC_RESPONSE_FIELDS = frozenset(
    {"npc_line", "emotion", "used_action_id", "referenced_fact_ids"}
)


class EndpointKind(str, Enum):
    OFFICIAL = "official"
    LOOPBACK = "loopback"


class ContractError(ValueError):
    """Validation failure with a stable, non-sensitive error code."""

    def __init__(self, code: str) -> None:
        super().__init__(code)
        self.code = code


@dataclass(frozen=True)
class EndpointPolicy:
    kind: EndpointKind
    endpoint: str
    requires_api_key: bool


@dataclass(frozen=True)
class NPCResponse:
    npc_line: str
    emotion: str
    used_action_id: str
    referenced_fact_ids: tuple[str, ...]


def classify_endpoint(endpoint: str) -> EndpointPolicy:
    """Allow the official HTTPS API and local development mocks only."""

    clean_endpoint = endpoint.strip()
    try:
        parsed = urlsplit(clean_endpoint)
        port = parsed.port
    except ValueError as exc:
        raise ContractError("endpoint_invalid") from exc

    if (
        not clean_endpoint
        or not parsed.scheme
        or not parsed.hostname
        or parsed.username is not None
        or parsed.password is not None
        or parsed.query
        or parsed.fragment
    ):
        raise ContractError("endpoint_invalid")

    scheme = parsed.scheme.lower()
    hostname = parsed.hostname.lower()
    path = parsed.path.rstrip("/")
    if (
        scheme == "https"
        and hostname == "api.deepseek.com"
        and port in (None, 443)
        and path in ("/chat/completions", "/v1/chat/completions")
    ):
        return EndpointPolicy(EndpointKind.OFFICIAL, clean_endpoint, True)

    if (
        scheme in ("http", "https")
        and hostname in ("localhost", "127.0.0.1", "::1")
    ):
        return EndpointPolicy(EndpointKind.LOOPBACK, clean_endpoint, False)

    raise ContractError("endpoint_not_allowed")


def build_request_payload(action_id: str = "probe_availability") -> dict[str, Any]:
    """Build the exact non-thinking JSON request used by the probe."""

    example = {
        "npc_line": "联调响应正常。",
        "emotion": "focused",
        "used_action_id": action_id,
        "referenced_fact_ids": [],
    }
    system_prompt = (
        "Return exactly one JSON object and no markdown. The JSON must contain "
        "exactly four fields: npc_line string, emotion string, used_action_id "
        "string equal to the supplied action_id, and referenced_fact_ids string "
        "array. Do not add facts. Example JSON: "
        + json.dumps(example, ensure_ascii=False, separators=(",", ":"))
    )
    user_prompt = json.dumps(
        {"action_id": action_id, "allowed_fact_ids": []},
        ensure_ascii=False,
        separators=(",", ":"),
    )
    return {
        "model": MODEL,
        "messages": [
            {"role": "system", "content": system_prompt},
            {"role": "user", "content": user_prompt},
        ],
        "temperature": 0,
        "max_tokens": 160,
        "stream": False,
        "thinking": {"type": "disabled"},
        "response_format": {"type": "json_object"},
    }


def encode_request_payload(action_id: str = "probe_availability") -> bytes:
    return json.dumps(
        build_request_payload(action_id),
        ensure_ascii=False,
        separators=(",", ":"),
    ).encode("utf-8")


def make_completion_envelope(
    content: dict[str, Any],
    *,
    model: str = MODEL,
    finish_reason: str = "stop",
    empty_content: bool = False,
) -> dict[str, Any]:
    """Create a minimal OpenAI-compatible non-streaming response envelope."""

    encoded_content = (
        ""
        if empty_content
        else json.dumps(content, ensure_ascii=False, separators=(",", ":"))
    )
    return {
        "id": "whiteout-mock-v05",
        "object": "chat.completion",
        "created": 0,
        "model": model,
        "choices": [
            {
                "index": 0,
                "message": {"role": "assistant", "content": encoded_content},
                "finish_reason": finish_reason,
            }
        ],
        "usage": {
            "prompt_tokens": 0,
            "completion_tokens": 0,
            "total_tokens": 0,
        },
    }


def _validate_fact_ids(
    value: Any,
    allowed_fact_ids: Iterable[str],
) -> tuple[str, ...]:
    if not isinstance(value, list):
        raise ContractError("referenced_fact_ids_not_array")

    facts: list[str] = []
    for item in value:
        if not isinstance(item, str) or not item or len(item) > 64:
            raise ContractError("referenced_fact_id_invalid")
        if item in facts:
            raise ContractError("referenced_fact_id_duplicate")
        facts.append(item)

    allowed = frozenset(allowed_fact_ids)
    if any(fact not in allowed for fact in facts):
        raise ContractError("referenced_fact_not_allowed")
    return tuple(facts)


def validate_completion(
    payload: bytes,
    *,
    expected_action_id: str,
    allowed_fact_ids: Iterable[str] = (),
) -> NPCResponse:
    """Validate the provider envelope and the exact NPC response object."""

    if len(payload) > MAX_RESPONSE_BYTES:
        raise ContractError("response_too_large")
    try:
        root = json.loads(payload.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ContractError("provider_invalid_json") from exc
    if not isinstance(root, dict):
        raise ContractError("provider_invalid_envelope")

    choices = root.get("choices")
    if not isinstance(choices, list) or not choices or not isinstance(choices[0], dict):
        raise ContractError("provider_invalid_envelope")
    choice = choices[0]
    finish_reason = choice.get("finish_reason")
    if not isinstance(finish_reason, str):
        raise ContractError("provider_missing_finish_reason")
    if finish_reason != "stop":
        known_reasons = {
            "length",
            "content_filter",
            "tool_calls",
            "insufficient_system_resource",
        }
        suffix = finish_reason if finish_reason in known_reasons else "other"
        raise ContractError(f"provider_finish_{suffix}")

    message = choice.get("message")
    if not isinstance(message, dict):
        raise ContractError("provider_invalid_message")
    content = message.get("content")
    if not isinstance(content, str) or not content.strip():
        raise ContractError("provider_empty_content")
    try:
        response = json.loads(content)
    except json.JSONDecodeError as exc:
        raise ContractError("content_invalid_json") from exc
    if not isinstance(response, dict):
        raise ContractError("content_not_object")

    keys = frozenset(response)
    if keys != NPC_RESPONSE_FIELDS:
        if NPC_RESPONSE_FIELDS - keys:
            raise ContractError("content_missing_field")
        raise ContractError("content_unknown_field")

    npc_line = response["npc_line"]
    emotion = response["emotion"]
    used_action_id = response["used_action_id"]
    if not isinstance(npc_line, str) or not npc_line.strip():
        raise ContractError("npc_line_invalid")
    if len(npc_line) > 240:
        raise ContractError("npc_line_too_long")
    if not isinstance(emotion, str) or not emotion.strip() or len(emotion) > 32:
        raise ContractError("emotion_invalid")
    if (
        not isinstance(used_action_id, str)
        or not used_action_id
        or len(used_action_id) > 64
    ):
        raise ContractError("used_action_id_invalid")
    if used_action_id != expected_action_id:
        raise ContractError("used_action_id_mismatch")

    referenced_fact_ids = _validate_fact_ids(
        response["referenced_fact_ids"],
        allowed_fact_ids,
    )
    return NPCResponse(
        npc_line=npc_line.strip(),
        emotion=emotion.strip(),
        used_action_id=used_action_id,
        referenced_fact_ids=referenced_fact_ids,
    )
