"""Offline v1.3 contract, retry, endpoint, and mock-server tests."""

from __future__ import annotations

import json
import socket
import sys
import threading
import time
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any

import pytest


AGENT_DIR = Path(__file__).resolve().parent
if str(AGENT_DIR) not in sys.path:
    sys.path.insert(0, str(AGENT_DIR))

import mock_agent_server
import mock_chat_proxy
from agent_contract import (
    MAX_LINE_CHARACTERS,
    MAX_OUTPUT_TOKENS,
    MODEL,
    NPC_RESPONSE_FIELDS,
    PROMPT_MODE,
    PROTOCOL_VERSION,
    PROBE_MUST_ATOMS,
    ContractError,
    EndpointKind,
    SemanticAtom,
    build_request_payload,
    classify_endpoint,
    make_completion_envelope,
    validate_completion,
)
from mock_server import MOCK_MODES, MockConfig, build_mock_content, create_server
from probe_deepseek import (
    ACTION_ID,
    build_http_request,
    print_sanitized_result,
    run_probe,
)


MUST_ATOMS = (
    SemanticAtom(
        "PLAYER_ASSISTANCE_NEEDED",
        "你留下来搭把手",
        ("搭把手", "搭手"),
    ),
    SemanticAtom(
        "GU_HENG_NEEDS_RECOVERY",
        "让我先缓口气",
        ("缓口气", "缓过来"),
    ),
    SemanticAtom(
        "REPAIR_ROOM_SHOULD_BE_WARM",
        "把维修间弄暖",
        ("维修间", "弄暖"),
    ),
)
MAY_ATOMS = (
    SemanticAtom(
        "BACKUP_POWER_DECLINING",
        "备用电还在掉",
        ("备用电",),
    ),
)
NATURAL_LINE = "你留下来搭把手，把维修间弄暖。让我先缓口气，我就动手。"


def valid_content() -> dict[str, object]:
    return {
        "npc_line": NATURAL_LINE,
        "realized_atom_ids": [atom.atom_id for atom in MUST_ATOMS],
        "disclosed_fact_ids": [],
        "emotion": "guarded",
        "movement_intent": "stay",
        "reaction_action": "consider",
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


def validate_test_completion(payload: bytes, **kwargs: Any):
    return validate_completion(
        payload,
        must_atoms=kwargs.pop("must_atoms", MUST_ATOMS),
        may_atoms=kwargs.pop("may_atoms", MAY_ATOMS),
        **kwargs,
    )


def assert_contract_error(code: str, payload: bytes, **kwargs: Any) -> None:
    with pytest.raises(ContractError) as exc_info:
        validate_test_completion(payload, **kwargs)
    assert exc_info.value.code == code


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


def probe_content() -> dict[str, object]:
    return {
        "npc_line": "联调响应正常。",
        "realized_atom_ids": ["PROBE_ACKNOWLEDGED"],
        "disclosed_fact_ids": [],
        "emotion": "focused",
        "movement_intent": "stay",
        "reaction_action": "neutral",
    }


def encoded_probe_envelope(
    content: dict[str, object] | None = None,
    *,
    finish_reason: str = "stop",
    empty_content: bool = False,
) -> bytes:
    envelope = make_completion_envelope(
        probe_content() if content is None else content,
        finish_reason=finish_reason,
        empty_content=empty_content,
    )
    return json.dumps(envelope, ensure_ascii=False).encode("utf-8")


def test_request_uses_v13_full_line_non_thinking_json_contract() -> None:
    payload = build_request_payload()
    assert payload["model"] == MODEL == "deepseek-v4-flash"
    assert payload["thinking"] == {"type": "disabled"}
    assert payload["response_format"] == {"type": "json_object"}
    assert payload["stream"] is False
    assert payload["max_tokens"] == MAX_OUTPUT_TOKENS == 256
    system_prompt = payload["messages"][0]["content"]
    assert "Example JSON:" in system_prompt
    assert "恰好包含" in system_prompt

    context = json.loads(payload["messages"][-1]["content"])
    assert context["protocol_version"] == PROTOCOL_VERSION == "dialogue_epistemic_v3"
    assert context["prompt_mode"] == PROMPT_MODE == "semantic_atoms_full_line"
    assert context["persona"]["max_sentences"] == 2
    assert context["persona"]["max_characters"] == MAX_LINE_CHARACTERS == 96
    assert context["must_realize"][0]["id"] == "PROBE_ACKNOWLEDGED"
    assert "semantic_spine" not in context
    assert "persona_tail" not in context
    assert "private_knowledge" not in context


def test_v13_runtime_targets_full_line_protocol() -> None:
    runtime_path = (
        AGENT_DIR.parents[1]
        / "WhiteoutStation"
        / "Content"
        / "Agents"
        / "AgentRuntime.v1.3.json"
    )
    runtime = json.loads(runtime_path.read_text(encoding="utf-8"))
    assert runtime["schema_version"] == 6
    assert runtime["runtime_version"] == "1.3.0"
    assert runtime["protocol_version"] == PROTOCOL_VERSION
    assert runtime["prompt_mode"] == PROMPT_MODE
    assert runtime["max_sentences"] == 2
    assert runtime["max_line_chars"] == MAX_LINE_CHARACTERS
    assert runtime["max_output_tokens"] == MAX_OUTPUT_TOKENS
    assert "max_tail_chars" not in runtime
    assert "semantic_spine" not in runtime["notes"]


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
        build_http_request("https://api.deepseek.com/chat/completions", "")
    assert exc_info.value.code == "missing_api_key"

    request, kind = build_http_request(
        "https://api.deepseek.com/chat/completions",
        "dummy-secret",
    )
    assert kind is EndpointKind.OFFICIAL
    assert request.get_header("Authorization") == "Bearer dummy-secret"
    body = json.loads(request.data.decode("utf-8"))
    context = json.loads(body["messages"][-1]["content"])
    assert context["protocol_version"] == PROTOCOL_VERSION


@pytest.mark.parametrize(
    "malformed_key",
    ["标签：sk-test", "sk-test\nInjected: value", "密钥", "x" * 513],
)
def test_official_endpoint_rejects_non_header_safe_keys(malformed_key: str) -> None:
    with pytest.raises(ContractError) as exc_info:
        build_http_request(
            "https://api.deepseek.com/chat/completions",
            malformed_key,
        )
    assert exc_info.value.code == "invalid_api_key_format"

    opener = FakeOpener([FakeResponse(200, encoded_probe_envelope())])
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
    response = validate_test_completion(encoded_envelope())
    assert response.npc_line == NATURAL_LINE
    assert response.realized_atom_ids == tuple(atom.atom_id for atom in MUST_ATOMS)
    assert response.disclosed_fact_ids == ()
    assert response.movement_intent == "stay"
    assert response.reaction_action == "consider"


def test_exact_six_response_fields_are_required() -> None:
    assert frozenset(valid_content()) == NPC_RESPONSE_FIELDS
    missing = valid_content()
    missing.pop("emotion")
    assert_contract_error("content_missing_field", encoded_envelope(missing))

    extra = valid_content()
    extra["state_changes"] = {}
    assert_contract_error("content_unknown_field", encoded_envelope(extra))


@pytest.mark.parametrize(
    ("mutation", "code"),
    [
        (["PLAYER_ASSISTANCE_NEEDED"], "must_atom_missing"),
        (
            [
                "PLAYER_ASSISTANCE_NEEDED",
                "GU_HENG_NEEDS_RECOVERY",
                "REPAIR_ROOM_SHOULD_BE_WARM",
                "UNKNOWN_ATOM",
            ],
            "realized_atom_unknown",
        ),
        (
            [
                "PLAYER_ASSISTANCE_NEEDED",
                "GU_HENG_NEEDS_RECOVERY",
                "REPAIR_ROOM_SHOULD_BE_WARM",
                "REPAIR_ROOM_SHOULD_BE_WARM",
            ],
            "realized_atom_ids_duplicate",
        ),
    ],
)
def test_realized_atom_membership_is_strict(mutation: list[str], code: str) -> None:
    content = valid_content()
    content["realized_atom_ids"] = mutation
    assert_contract_error(code, encoded_envelope(content))


def test_may_atom_is_optional_and_cannot_carry_related_facts() -> None:
    content = valid_content()
    content["npc_line"] = NATURAL_LINE.rstrip("。") + "，备用电还在掉。"
    content["realized_atom_ids"] = [
        *(atom.atom_id for atom in MUST_ATOMS),
        "BACKUP_POWER_DECLINING",
    ]
    response = validate_test_completion(encoded_envelope(content))
    assert response.realized_atom_ids[-1] == "BACKUP_POWER_DECLINING"

    unsafe_may = (
        SemanticAtom(
            "HEAT_PACK_AVAILABLE",
            "还有一只保温包",
            ("保温包",),
            ("FACT_HEAT_PACK",),
        ),
    )
    assert_contract_error(
        "may_atom_has_related_fact",
        encoded_envelope(),
        may_atoms=unsafe_may,
    )


def test_disclosed_facts_must_exactly_equal_plan() -> None:
    planned = ("FACT_HAND_INJURY",)
    content = valid_content()
    content["npc_line"] = "你留下来搭把手，他的手伤已经影响精细操作。让我先缓口气。"
    content["realized_atom_ids"] = [
        "PLAYER_ASSISTANCE_NEEDED",
        "GU_HENG_NEEDS_RECOVERY",
        "REPAIR_ROOM_SHOULD_BE_WARM",
    ]
    # Keep the room atom visibly grounded while disclosing the planned fact.
    content["npc_line"] = "你留下来搭把手，把维修间弄暖。他的手伤影响精细操作，让我缓口气。"
    content["disclosed_fact_ids"] = list(planned)
    response = validate_test_completion(
        encoded_envelope(content),
        allowed_fact_ids=planned,
        planned_disclosed_fact_ids=planned,
    )
    assert response.disclosed_fact_ids == planned

    missing = valid_content()
    assert_contract_error(
        "disclosed_fact_plan_mismatch",
        encoded_envelope(missing),
        allowed_fact_ids=planned,
        planned_disclosed_fact_ids=planned,
    )

    extra = valid_content()
    extra["disclosed_fact_ids"] = ["FACT_HAND_INJURY"]
    assert_contract_error(
        "disclosed_fact_plan_mismatch",
        encoded_envelope(extra),
        allowed_fact_ids=planned,
    )


def test_duplicate_and_unknown_disclosed_facts_are_rejected() -> None:
    content = valid_content()
    content["disclosed_fact_ids"] = ["FACT_HAND_INJURY", "FACT_HAND_INJURY"]
    assert_contract_error(
        "disclosed_fact_ids_duplicate",
        encoded_envelope(content),
        allowed_fact_ids=("FACT_HAND_INJURY",),
        planned_disclosed_fact_ids=("FACT_HAND_INJURY",),
    )

    content["disclosed_fact_ids"] = ["FACT_SECRET"]
    assert_contract_error(
        "disclosed_fact_not_allowed",
        encoded_envelope(content),
        allowed_fact_ids=("FACT_HAND_INJURY",),
    )


def test_each_realized_atom_requires_a_surface_token() -> None:
    content = valid_content()
    content["npc_line"] = "你留下来搭把手，把维修间弄暖。现在就动手。"
    assert_contract_error("realized_atom_surface_missing", encoded_envelope(content))


def test_forbidden_fact_and_phrase_are_rejected() -> None:
    content = valid_content()
    content["npc_line"] = NATURAL_LINE.rstrip("。") + "，他的手伤也得处理。"
    assert_contract_error(
        "npc_line_forbidden_fact",
        encoded_envelope(content),
        forbidden_fact_ids=("FACT_HAND_INJURY",),
    )

    assert_contract_error(
        "npc_line_undisclosed_fact",
        encoded_envelope(content),
    )

    content["npc_line"] = NATURAL_LINE.rstrip("。") + "，条件 ID 写得很清楚。"
    assert_contract_error(
        "npc_line_system_jargon",
        encoded_envelope(content),
    )


@pytest.mark.parametrize(
    "jargon",
    [
        "AP",
        "Stamina",
        "至少两点",
        "至少2点",
        "阈值",
        "不会单独否决",
        "否决",
        "修正值",
        "+1",
        "REQ_STAMINA_2",
        "gu_heng_stamina_ready",
    ],
)
def test_system_jargon_is_rejected(jargon: str) -> None:
    content = valid_content()
    content["npc_line"] = NATURAL_LINE.rstrip("。") + f"，{jargon}。"
    assert_contract_error("npc_line_system_jargon", encoded_envelope(content))


def test_added_domain_concept_is_rejected_without_atom_or_fact() -> None:
    content = valid_content()
    content["npc_line"] = NATURAL_LINE.rstrip("。") + "，还得先修天线。"
    assert_contract_error("npc_line_added_domain_concept", encoded_envelope(content))


@pytest.mark.parametrize(
    ("line", "code"),
    [
        ("字" * 97, "npc_line_invalid_length"),
        (NATURAL_LINE + "\n再说一句。", "npc_line_invalid_length"),
        ("你留下来搭把手。把维修间弄暖。让我缓口气。", "npc_line_too_many_sentences"),
    ],
)
def test_line_length_newline_and_sentence_limits(line: str, code: str) -> None:
    content = valid_content()
    content["npc_line"] = line
    assert_contract_error(code, encoded_envelope(content))


@pytest.mark.parametrize(
    ("field", "value", "code"),
    [
        ("emotion", "invented", "emotion_invalid"),
        ("movement_intent", "teleport", "movement_intent_invalid"),
        ("reaction_action", "grant_item", "reaction_action_invalid"),
    ],
)
def test_performance_fields_use_enums(field: str, value: str, code: str) -> None:
    content = valid_content()
    content[field] = value
    assert_contract_error(code, encoded_envelope(content))


@pytest.mark.parametrize(
    "finish_reason",
    ["length", "content_filter", "tool_calls", "insufficient_system_resource"],
)
def test_non_stop_finish_reason_is_rejected(finish_reason: str) -> None:
    assert_contract_error(
        f"provider_finish_{finish_reason}",
        encoded_envelope(finish_reason=finish_reason),
    )


def test_empty_and_invalid_content_are_rejected() -> None:
    assert_contract_error(
        "provider_empty_content",
        encoded_envelope(empty_content=True),
    )
    envelope = make_completion_envelope(valid_content())
    envelope["choices"][0]["message"]["content"] = "{broken"
    assert_contract_error(
        "content_invalid_json",
        json.dumps(envelope).encode("utf-8"),
    )


def test_probe_accepts_v13_completion_and_reports_counts() -> None:
    opener = FakeOpener([FakeResponse(200, encoded_probe_envelope())])
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
    assert result.npc_line_chars == len("联调响应正常。")
    assert result.realized_atom_count == 1
    assert result.disclosed_fact_count == 0


def test_probe_retries_429_once_then_accepts() -> None:
    opener = FakeOpener(
        [FakeResponse(429, b"{}"), FakeResponse(200, encoded_probe_envelope())]
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
        [FakeResponse(status, b"{}"), FakeResponse(200, encoded_probe_envelope())]
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
            FakeResponse(200, encoded_probe_envelope()),
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
        (encoded_probe_envelope(empty_content=True), "provider_empty_content"),
        (
            encoded_probe_envelope(finish_reason="length"),
            "provider_finish_length",
        ),
        (
            b'{"choices":[{"finish_reason":"stop","message":{"content":"{"}}]}',
            "content_invalid_json",
        ),
        (
            encoded_probe_envelope(
                {
                    "npc_line": "联调响应正常。",
                    "emotion": "focused",
                    "movement_intent": "stay",
                }
            ),
            "content_missing_field",
        ),
    ],
)
def test_probe_rejects_contract_failures(payload: bytes, error_code: str) -> None:
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
    opener = FakeOpener([FakeResponse(200, encoded_probe_envelope())])
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
    assert "realized_atom_count=1" in output


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


def test_mock_exposes_all_v13_modes_and_legacy_alias() -> None:
    assert set(MOCK_MODES) == {
        "valid_natural",
        "missing_atom",
        "forbidden_fact",
        "added_condition",
        "system_jargon",
        "invalid_json",
        "timeout",
    }
    assert MockConfig(persona_tail_mode="valid").effective_mode == "valid_natural"
    assert (
        MockConfig(persona_tail_mode="added-condition").effective_mode
        == "added_condition"
    )


def test_valid_natural_mock_realizes_exact_probe_contract() -> None:
    request = build_request_payload()
    kind, content = build_mock_content(request, "valid_natural")
    assert kind == "npc_line"
    payload = encoded_probe_envelope(content)
    response = validate_completion(payload, must_atoms=PROBE_MUST_ATOMS)
    assert response.npc_line == "联调响应正常。"
    assert response.realized_atom_ids == ("PROBE_ACKNOWLEDGED",)


@pytest.mark.parametrize(
    ("mode", "reason"),
    [
        ("missing_atom", "must_atom_missing"),
        ("forbidden_fact", "npc_line_forbidden_fact"),
        ("added_condition", "npc_line_added_domain_concept"),
        ("system_jargon", "npc_line_system_jargon"),
    ],
)
def test_invalid_mock_modes_are_deterministically_rejected(
    mode: str,
    reason: str,
) -> None:
    _, content = build_mock_content(build_request_payload(), mode)
    with pytest.raises(ContractError) as exc_info:
        validate_completion(
            encoded_probe_envelope(content),
            must_atoms=PROBE_MUST_ATOMS,
            forbidden_fact_ids=("FACT_HAND_INJURY",)
            if mode == "forbidden_fact"
            else (),
        )
    assert exc_info.value.code == reason


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
    context = json.loads(build_request_payload()["messages"][-1]["content"])
    context.update(
        {
            "action_id": "talk_gu_heng",
            "player_said": player_said,
            "allowed_movement_intents": [
                "stay",
                "step_closer",
                "step_back",
                "return_to_post",
            ],
            "allowed_reaction_actions": [
                "neutral",
                "acknowledge",
                "consider",
                "reassure",
                "reject",
                "alarmed",
            ],
        }
    )
    request = build_request_payload()
    request["messages"][-1]["content"] = json.dumps(context, ensure_ascii=False)
    kind, content = build_mock_content(request)
    assert kind == "npc_line"
    assert content["movement_intent"] == movement
    assert content["reaction_action"] == reaction


def test_mock_is_openai_compatible_and_audit_is_metadata_only(tmp_path: Path) -> None:
    audit_path = tmp_path / "mock-audit.jsonl"
    server, thread, endpoint = _start_mock(MockConfig(), audit_path=audit_path)
    try:
        request, _ = build_http_request(endpoint, "dummy-secret")
        with urllib.request.urlopen(request, timeout=2) as response:
            payload = response.read()
        parsed = json.loads(payload.decode("utf-8"))
        assert parsed["choices"][0]["finish_reason"] == "stop"
        validate_completion(payload, must_atoms=PROBE_MUST_ATOMS)
    finally:
        _stop_mock(server, thread)

    audit_text = audit_path.read_text(encoding="utf-8")
    assert "dummy-secret" not in audit_text
    assert "Bearer" not in audit_text
    assert "请确认自然台词协议可用" not in audit_text
    audit = json.loads(audit_text)
    assert audit["authorization_present"] is False
    assert audit["thinking_disabled"] is True
    assert audit["stream_disabled"] is True
    assert audit["response_format_json_object"] is True
    assert audit["protocol_version"] == PROTOCOL_VERSION
    assert audit["prompt_mode"] == PROMPT_MODE
    assert audit["must_atom_count"] == 1
    assert audit["message_count"] == 2
    assert audit["role_sequence"] == ["system", "user"]
    assert audit["history_turns"] == 0


def test_mock_supports_empty_content_and_status_codes() -> None:
    server, thread, endpoint = _start_mock(MockConfig(empty_content=True))
    try:
        request, _ = build_http_request(endpoint, "")
        with urllib.request.urlopen(request, timeout=2) as response:
            payload = response.read()
        with pytest.raises(ContractError) as exc_info:
            validate_completion(payload, must_atoms=PROBE_MUST_ATOMS)
        assert exc_info.value.code == "provider_empty_content"
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


def test_mock_supports_invalid_json_and_extra_field_content() -> None:
    for config, expected_reason in (
        (MockConfig(mode="invalid_json"), "content_invalid_json"),
        (MockConfig(extra_field=True), "content_unknown_field"),
    ):
        server, thread, endpoint = _start_mock(config)
        try:
            request, _ = build_http_request(endpoint, "")
            with urllib.request.urlopen(request, timeout=2) as response:
                payload = response.read()
            with pytest.raises(ContractError) as exc_info:
                validate_completion(payload, must_atoms=PROBE_MUST_ATOMS)
            assert exc_info.value.code == expected_reason
        finally:
            _stop_mock(server, thread)


def test_timeout_mode_exceeds_caller_deadline() -> None:
    server, thread, endpoint = _start_mock(
        MockConfig(mode="timeout", delay_seconds=0.05)
    )
    try:
        request, _ = build_http_request(endpoint, "")
        with pytest.raises((TimeoutError, socket.timeout, urllib.error.URLError)):
            urllib.request.urlopen(request, timeout=0.01)
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
        validate_completion(payload, must_atoms=PROBE_MUST_ATOMS)
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
