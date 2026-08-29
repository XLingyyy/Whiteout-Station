"""Offline contract, retry, endpoint, and mock-server tests."""

from __future__ import annotations

import json
import sys
import threading
import time
import urllib.error
import urllib.request
from pathlib import Path

import pytest


AGENT_DIR = Path(__file__).resolve().parent
if str(AGENT_DIR) not in sys.path:
    sys.path.insert(0, str(AGENT_DIR))

import mock_agent_server
import mock_chat_proxy
from agent_contract import (
    MODEL,
    ContractError,
    EndpointKind,
    build_request_payload,
    classify_endpoint,
    make_completion_envelope,
    validate_completion,
)
from mock_server import MockConfig, build_mock_content, create_server
from probe_deepseek import (
    ACTION_ID,
    build_http_request,
    print_sanitized_result,
    run_probe,
)


def valid_content(action_id: str = ACTION_ID) -> dict[str, object]:
    return {
        "persona_tail": "联调响应正常。",
        "emotion": "focused",
        "used_action_id": action_id,
        "referenced_fact_ids": [],
        "movement_intent": "stay",
        "reaction_action": "neutral",
    }


def encoded_envelope(
    content: dict[str, object] | None = None,
    *,
    finish_reason: str = "stop",
    empty_content: bool = False,
) -> bytes:
    envelope = make_completion_envelope(
        valid_content() if content is None else content,
        finish_reason=finish_reason,
        empty_content=empty_content,
    )
    return json.dumps(envelope, ensure_ascii=False).encode("utf-8")


class FakeResponse:
    def __init__(self, status: int, payload: bytes) -> None:
        self.status = status
        self.payload = payload
        self.closed = False

    def getcode(self) -> int:
        return self.status

    def read(self, amount: int = -1) -> bytes:
        return self.payload if amount < 0 else self.payload[:amount]

    def close(self) -> None:
        self.closed = True


class FakeOpener:
    def __init__(self, outcomes: list[FakeResponse | BaseException]) -> None:
        self.outcomes = outcomes
        self.requests: list[urllib.request.Request] = []

    def __call__(
        self,
        request: urllib.request.Request,
        *,
        timeout: float,
    ) -> FakeResponse:
        assert timeout > 0
        self.requests.append(request)
        outcome = self.outcomes.pop(0)
        if isinstance(outcome, BaseException):
            raise outcome
        return outcome


def assert_contract_error(code: str, payload: bytes) -> None:
    with pytest.raises(ContractError) as exc_info:
        validate_completion(payload, expected_action_id=ACTION_ID)
    assert exc_info.value.code == code


def test_request_uses_current_non_thinking_json_contract() -> None:
    payload = build_request_payload()
    assert payload["model"] == MODEL == "deepseek-v4-flash"
    assert payload["thinking"] == {"type": "disabled"}
    assert payload["response_format"] == {"type": "json_object"}
    assert payload["stream"] is False
    assert "Example JSON:" in payload["messages"][0]["content"]


@pytest.mark.parametrize(
    ("endpoint", "kind"),
    [
        ("https://api.deepseek.com/chat/completions", EndpointKind.OFFICIAL),
        ("https://api.deepseek.com/v1/chat/completions", EndpointKind.OFFICIAL),
        ("http://127.0.0.1:8765/chat/completions", EndpointKind.LOOPBACK),
        ("http://localhost:8765", EndpointKind.LOOPBACK),
        ("https://[::1]:8765/chat/completions", EndpointKind.LOOPBACK),
    ],
)
def test_endpoint_allowlist(endpoint: str, kind: EndpointKind) -> None:
    assert classify_endpoint(endpoint).kind is kind


@pytest.mark.parametrize(
    "endpoint",
    [
        "https://api.deepseek.com.evil.invalid/chat/completions",
        "http://api.deepseek.com/chat/completions",
        "https://example.com/chat/completions",
        "http://192.168.1.2:8765",
        "http://user:pass@localhost:8765",
        "http://localhost:8765?api_key=secret",
    ],
)
def test_endpoint_rejects_untrusted_or_credentialized_urls(endpoint: str) -> None:
    with pytest.raises(ContractError):
        classify_endpoint(endpoint)


def test_official_endpoint_requires_key_and_sets_authorization() -> None:
    with pytest.raises(ContractError) as exc_info:
        build_http_request(
            "https://api.deepseek.com/chat/completions",
            "",
        )
    assert exc_info.value.code == "missing_api_key"

    request, kind = build_http_request(
        "https://api.deepseek.com/chat/completions",
        "dummy-secret",
    )
    assert kind is EndpointKind.OFFICIAL
    assert request.get_header("Authorization") == "Bearer dummy-secret"


@pytest.mark.parametrize(
    "malformed_key",
    [
        "标签：sk-test",
        "sk-test\nInjected: value",
        "密钥",
        "x" * 513,
    ],
)
def test_official_endpoint_rejects_non_header_safe_keys(
    malformed_key: str,
) -> None:
    with pytest.raises(ContractError) as exc_info:
        build_http_request(
            "https://api.deepseek.com/chat/completions",
            malformed_key,
        )
    assert exc_info.value.code == "invalid_api_key_format"

    opener = FakeOpener([FakeResponse(200, encoded_envelope())])
    result = run_probe(
        endpoint="https://api.deepseek.com/chat/completions",
        api_key=malformed_key,
        credential_source="environment",
        timeout=1,
        retry_delay=0,
        opener=opener,
        sleep=lambda _: None,
    )
    assert not result.success
    assert result.result == "REJECTED"
    assert result.attempts == 0
    assert result.error_code == "invalid_api_key_format"
    assert not opener.requests


def test_loopback_never_receives_deepseek_key() -> None:
    request, kind = build_http_request(
        "http://127.0.0.1:8765/chat/completions",
        "dummy-secret",
    )
    assert kind is EndpointKind.LOOPBACK
    assert request.get_header("Authorization") is None


def test_valid_completion_is_accepted() -> None:
    response = validate_completion(
        encoded_envelope(),
        expected_action_id=ACTION_ID,
    )
    assert response.persona_tail == "联调响应正常。"
    assert response.used_action_id == ACTION_ID
    assert response.referenced_fact_ids == ()
    assert response.movement_intent == "stay"
    assert response.reaction_action == "neutral"


@pytest.mark.parametrize(
    "finish_reason",
    ["length", "content_filter", "tool_calls", "insufficient_system_resource"],
)
def test_non_stop_finish_reason_is_rejected(finish_reason: str) -> None:
    assert_contract_error(
        f"provider_finish_{finish_reason}",
        encoded_envelope(finish_reason=finish_reason),
    )


def test_empty_content_is_rejected() -> None:
    assert_contract_error(
        "provider_empty_content",
        encoded_envelope(empty_content=True),
    )


def test_invalid_content_json_is_rejected() -> None:
    envelope = make_completion_envelope(valid_content())
    envelope["choices"][0]["message"]["content"] = "{broken"
    assert_contract_error(
        "content_invalid_json",
        json.dumps(envelope).encode("utf-8"),
    )


def test_missing_and_unknown_content_fields_are_rejected() -> None:
    missing = valid_content()
    missing.pop("emotion")
    assert_contract_error("content_missing_field", encoded_envelope(missing))

    extra = valid_content()
    extra["state_changes"] = {}
    assert_contract_error("content_unknown_field", encoded_envelope(extra))


def test_action_and_fact_guards_are_strict() -> None:
    wrong_action = valid_content("different_action")
    assert_contract_error(
        "used_action_id_mismatch",
        encoded_envelope(wrong_action),
    )

    unauthorized_fact = valid_content()
    unauthorized_fact["referenced_fact_ids"] = ["FACT_SECRET"]
    assert_contract_error(
        "referenced_fact_not_allowed",
        encoded_envelope(unauthorized_fact),
    )


def test_empty_persona_tail_is_valid_spine_only_output() -> None:
    content = valid_content()
    content["persona_tail"] = ""
    response = validate_completion(
        encoded_envelope(content),
        expected_action_id=ACTION_ID,
    )
    assert response.persona_tail == ""


def test_persona_tail_is_limited_to_48_characters() -> None:
    content = valid_content()
    content["persona_tail"] = "尾" * 49
    assert_contract_error(
        "persona_tail_invalid_length",
        encoded_envelope(content),
    )


def test_probe_retries_429_once_then_accepts() -> None:
    opener = FakeOpener(
        [
            FakeResponse(429, b"{}"),
            FakeResponse(200, encoded_envelope()),
        ]
    )
    sleeps: list[float] = []
    result = run_probe(
        endpoint="http://127.0.0.1:8765/chat/completions",
        api_key="",
        credential_source="none",
        timeout=1,
        retry_delay=0.01,
        opener=opener,
        sleep=sleeps.append,
    )
    assert result.success
    assert result.attempts == 2
    assert sleeps == [0.01]


@pytest.mark.parametrize("status", [500, 503])
def test_probe_retries_server_errors_once(status: int) -> None:
    opener = FakeOpener(
        [
            FakeResponse(status, b"{}"),
            FakeResponse(200, encoded_envelope()),
        ]
    )
    result = run_probe(
        endpoint="http://127.0.0.1:8765/chat/completions",
        api_key="",
        credential_source="none",
        timeout=1,
        retry_delay=0,
        opener=opener,
        sleep=lambda _: None,
    )
    assert result.success
    assert result.attempts == 2


def test_probe_retries_connection_failure_once() -> None:
    opener = FakeOpener(
        [
            urllib.error.URLError("sensitive transport detail"),
            FakeResponse(200, encoded_envelope()),
        ]
    )
    result = run_probe(
        endpoint="http://localhost:8765/chat/completions",
        api_key="",
        credential_source="none",
        timeout=1,
        retry_delay=0,
        opener=opener,
        sleep=lambda _: None,
    )
    assert result.success
    assert result.attempts == 2


def test_probe_does_not_retry_non_retryable_status() -> None:
    opener = FakeOpener([FakeResponse(401, b"{}")])
    result = run_probe(
        endpoint="http://localhost:8765/chat/completions",
        api_key="",
        credential_source="none",
        timeout=1,
        retry_delay=0,
        opener=opener,
        sleep=lambda _: None,
    )
    assert not result.success
    assert result.attempts == 1
    assert result.error_code == "http_auth"
    assert len(opener.requests) == 1


@pytest.mark.parametrize(
    ("payload", "error_code"),
    [
        (encoded_envelope(empty_content=True), "provider_empty_content"),
        (
            encoded_envelope(finish_reason="length"),
            "provider_finish_length",
        ),
        (b'{"choices":[{"finish_reason":"stop","message":{"content":"{"}}]}',
         "content_invalid_json"),
        (
            encoded_envelope(
                {
                    "persona_tail": "缺字段。",
                    "emotion": "focused",
                    "used_action_id": ACTION_ID,
                }
            ),
            "content_missing_field",
        ),
    ],
)
def test_probe_rejects_contract_failures(
    payload: bytes,
    error_code: str,
) -> None:
    result = run_probe(
        endpoint="http://localhost:8765/chat/completions",
        api_key="",
        credential_source="none",
        timeout=1,
        retry_delay=0,
        opener=FakeOpener([FakeResponse(200, payload)]),
        sleep=lambda _: None,
    )
    assert not result.success
    assert result.result == "INVALID_RESPONSE"
    assert result.error_code == error_code


def test_sanitized_output_never_contains_key_or_authorization(
    capsys: pytest.CaptureFixture[str],
) -> None:
    secret = "dummy-secret-never-print"
    opener = FakeOpener([FakeResponse(200, encoded_envelope())])
    result = run_probe(
        endpoint="http://localhost:8765/chat/completions",
        api_key=secret,
        credential_source="environment",
        timeout=1,
        retry_delay=0,
        opener=opener,
        sleep=lambda _: None,
    )
    print_sanitized_result(result)
    output = capsys.readouterr().out
    assert secret not in output
    assert "Authorization" not in output
    assert "Bearer" not in output
    assert "credential_source=not_required" in output


def _start_mock(
    config: MockConfig,
    *,
    audit_path: Path | None = None,
) -> tuple[object, threading.Thread, str]:
    server = create_server(
        "127.0.0.1",
        0,
        config=config,
        audit_path=audit_path,
    )
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    endpoint = f"http://127.0.0.1:{server.server_port}/chat/completions"
    return server, thread, endpoint


def _stop_mock(server: object, thread: threading.Thread) -> None:
    server.shutdown()
    server.server_close()
    thread.join(timeout=2)


def test_both_mock_entrypoints_share_the_same_server() -> None:
    assert mock_agent_server.run_from_cli is mock_chat_proxy.run_from_cli


@pytest.mark.parametrize(
    ("player_said", "movement", "reaction"),
    (
        ("请过来一点", "step_closer", "acknowledge"),
        ("退后，离我远点", "step_back", "reject"),
        ("回原位去", "return_to_post", "consider"),
        ("别怕，先冷静", "stay", "reassure"),
        ("你一直在隐瞒什么？", "step_back", "alarmed"),
    ),
)
def test_mock_selects_constrained_performance(
    player_said: str,
    movement: str,
    reaction: str,
) -> None:
    context = {
        "action_id": "talk_gu_heng",
        "emotion": "focused",
        "semantic_spine": "我听见了。",
        "preset_movement_intent": "stay",
        "preset_reaction_action": "neutral",
        "player_said": player_said,
    }
    kind, content = build_mock_content(
        {
            "messages": [
                {"role": "system", "content": "Return json dialogue."},
                {"role": "user", "content": json.dumps(context, ensure_ascii=False)},
            ]
        }
    )
    assert kind == "npc_line"
    assert content["persona_tail"] == "【mock】我听见了。"
    assert content["movement_intent"] == movement
    assert content["reaction_action"] == reaction


def test_mock_is_openai_compatible_and_drops_authorization(
    tmp_path: Path,
) -> None:
    audit_path = tmp_path / "mock-audit.jsonl"
    server, thread, endpoint = _start_mock(
        MockConfig(),
        audit_path=audit_path,
    )
    try:
        request, _ = build_http_request(endpoint, "dummy-secret")
        with urllib.request.urlopen(request, timeout=2) as response:
            payload = response.read()
        parsed = json.loads(payload.decode("utf-8"))
        assert parsed["choices"][0]["finish_reason"] == "stop"
        validate_completion(payload, expected_action_id=ACTION_ID)
    finally:
        _stop_mock(server, thread)

    audit_text = audit_path.read_text(encoding="utf-8")
    assert "dummy-secret" not in audit_text
    assert "Bearer" not in audit_text
    audit = json.loads(audit_text)
    assert audit["authorization_present"] is False
    assert audit["thinking_disabled"] is True
    assert audit["stream_disabled"] is True
    assert audit["response_format_json_object"] is True
    assert audit["message_count"] == 2
    assert audit["role_sequence"] == ["system", "user"]
    assert audit["history_turns"] == 0


def test_mock_supports_empty_content_and_status_codes() -> None:
    server, thread, endpoint = _start_mock(MockConfig(empty_content=True))
    try:
        request, _ = build_http_request(endpoint, "")
        with urllib.request.urlopen(request, timeout=2) as response:
            payload = response.read()
        assert_contract_error("provider_empty_content", payload)
    finally:
        _stop_mock(server, thread)

    server, thread, endpoint = _start_mock(MockConfig(status_code=503))
    try:
        request, _ = build_http_request(endpoint, "")
        with pytest.raises(urllib.error.HTTPError) as exc_info:
            urllib.request.urlopen(request, timeout=2)
        assert exc_info.value.code == 503
        exc_info.value.close()
    finally:
        _stop_mock(server, thread)


def test_mock_supports_malformed_and_extra_field_content() -> None:
    for config, expected_reason in (
        (MockConfig(malformed_content=True), "content_invalid_json"),
        (MockConfig(extra_field=True), "content_unknown_field"),
    ):
        server, thread, endpoint = _start_mock(config)
        try:
            request, _ = build_http_request(endpoint, "")
            with urllib.request.urlopen(request, timeout=2) as response:
                payload = response.read()
            assert_contract_error(expected_reason, payload)
        finally:
            _stop_mock(server, thread)


def test_mock_supports_retry_status_sequence() -> None:
    server, thread, endpoint = _start_mock(
        MockConfig(status_sequence=(429, 200))
    )
    try:
        request, _ = build_http_request(endpoint, "")
        with pytest.raises(urllib.error.HTTPError) as exc_info:
            urllib.request.urlopen(request, timeout=2)
        assert exc_info.value.code == 429
        exc_info.value.close()

        request, _ = build_http_request(endpoint, "")
        with urllib.request.urlopen(request, timeout=2) as response:
            payload = response.read()
        validate_completion(payload, expected_action_id=ACTION_ID)
    finally:
        _stop_mock(server, thread)


def test_mock_supports_configurable_delay() -> None:
    server, thread, endpoint = _start_mock(MockConfig(delay_seconds=0.03))
    try:
        request, _ = build_http_request(endpoint, "")
        started = time.perf_counter()
        with urllib.request.urlopen(request, timeout=2) as response:
            response.read()
        elapsed = time.perf_counter() - started
        assert elapsed >= 0.02
    finally:
        _stop_mock(server, thread)
