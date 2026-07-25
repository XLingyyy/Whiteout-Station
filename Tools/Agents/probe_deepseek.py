"""Sanitized DeepSeek availability and response-contract probe."""

from __future__ import annotations

import argparse
import configparser
import datetime as dt
import json
import os
import sys
import time
import urllib.error
import urllib.request
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Mapping

try:
    from .agent_contract import (
        MAX_RESPONSE_BYTES,
        MODEL,
        OFFICIAL_ENDPOINT,
        ContractError,
        EndpointKind,
        build_request_payload,
        classify_endpoint,
        validate_completion,
    )
except ImportError:
    from agent_contract import (
        MAX_RESPONSE_BYTES,
        MODEL,
        OFFICIAL_ENDPOINT,
        ContractError,
        EndpointKind,
        build_request_payload,
        classify_endpoint,
        validate_completion,
    )


ROOT = Path(__file__).resolve().parents[2]
LOCAL_CONFIG = ROOT / "WhiteoutStation" / "LocalConfig" / "WhiteoutLLM.ini"
DEFAULT_ENDPOINT = OFFICIAL_ENDPOINT
ACTION_ID = "probe_availability"
RETRYABLE_HTTP_STATUSES = frozenset({429, 500, 503})


@dataclass(frozen=True)
class ProbeResult:
    success: bool
    exit_code: int
    result: str
    endpoint_kind: str
    credential_source: str
    attempts: int
    http_status: str
    error_code: str
    npc_line_chars: int = 0
    referenced_fact_count: int = 0


def load_settings(
    *,
    local_config: Path = LOCAL_CONFIG,
    environment: Mapping[str, str] | None = None,
) -> tuple[str, str, str]:
    endpoint = DEFAULT_ENDPOINT
    key_from_file = ""
    if local_config.is_file():
        config = configparser.ConfigParser(interpolation=None)
        config.read(local_config, encoding="utf-8-sig")
        endpoint = (
            config.get("WhiteoutLLM", "Endpoint", fallback=endpoint).strip()
            or endpoint
        )
        key_from_file = config.get(
            "WhiteoutLLM",
            "ApiKey",
            fallback="",
        ).strip()
    env = os.environ if environment is None else environment
    environment_key = env.get("WHITEOUT_LLM_API_KEY", "").strip()
    if environment_key:
        return endpoint, environment_key, "environment"
    if key_from_file:
        return endpoint, key_from_file, "local_ini"
    return endpoint, "", "none"


def build_http_request(
    endpoint: str,
    api_key: str,
    *,
    action_id: str = ACTION_ID,
) -> tuple[urllib.request.Request, EndpointKind]:
    policy = classify_endpoint(endpoint)
    clean_key = api_key.strip()
    if policy.requires_api_key and not clean_key:
        raise ContractError("missing_api_key")

    headers = {"Content-Type": "application/json; charset=utf-8"}
    if policy.kind is EndpointKind.OFFICIAL:
        headers["Authorization"] = f"Bearer {clean_key}"
    body = build_request_payload(action_id)
    request = urllib.request.Request(
        policy.endpoint,
        data=json.dumps(
            body,
            ensure_ascii=False,
            separators=(",", ":"),
        ).encode("utf-8"),
        method="POST",
        headers=headers,
    )
    return request, policy.kind


def _http_error_code(status: int) -> str:
    return {
        400: "http_bad_request",
        401: "http_auth",
        402: "http_balance",
        422: "http_invalid_parameters",
        429: "http_rate_limited",
        500: "http_server_error",
        503: "http_server_overloaded",
    }.get(status, "http_error")


def _configuration_failure(
    *,
    code: str,
    credential_source: str,
    endpoint_kind: str = "rejected",
) -> ProbeResult:
    return ProbeResult(
        success=False,
        exit_code=2,
        result="REJECTED",
        endpoint_kind=endpoint_kind,
        credential_source=credential_source,
        attempts=0,
        http_status="NO_REQUEST",
        error_code=code,
    )


def run_probe(
    *,
    endpoint: str,
    api_key: str,
    credential_source: str,
    timeout: float,
    retry_delay: float,
    opener: Callable[..., object] | None = None,
    sleep: Callable[[float], None] = time.sleep,
) -> ProbeResult:
    try:
        policy = classify_endpoint(endpoint)
    except ContractError as exc:
        return _configuration_failure(
            code=exc.code,
            credential_source=credential_source,
        )
    if policy.requires_api_key and not api_key.strip():
        return _configuration_failure(
            code="missing_api_key",
            credential_source=credential_source,
            endpoint_kind=policy.kind.value,
        )
    endpoint_kind = policy.kind

    known_sources = frozenset({"environment", "local_ini", "none"})
    normalized_source = (
        credential_source if credential_source in known_sources else "unknown"
    )
    safe_credential_source = (
        normalized_source
        if endpoint_kind is EndpointKind.OFFICIAL
        else "not_required"
    )
    open_request = urllib.request.urlopen if opener is None else opener
    last_status = "NO_RESPONSE"
    last_error = "transport_connection"

    for attempt in range(1, 3):
        request, _ = build_http_request(endpoint, api_key)
        try:
            response = open_request(request, timeout=timeout)
            try:
                raw_status = getattr(response, "status", None)
                status = int(response.getcode() if raw_status is None else raw_status)
                payload = response.read(MAX_RESPONSE_BYTES + 1)
            finally:
                response.close()
            last_status = str(status)
            if not 200 <= status < 300:
                last_error = _http_error_code(status)
                if status in RETRYABLE_HTTP_STATUSES and attempt == 1:
                    sleep(retry_delay)
                    continue
                return ProbeResult(
                    False,
                    1,
                    "UNAVAILABLE",
                    endpoint_kind.value,
                    safe_credential_source,
                    attempt,
                    last_status,
                    last_error,
                )
            try:
                npc_response = validate_completion(
                    payload,
                    expected_action_id=ACTION_ID,
                    allowed_fact_ids=(),
                )
            except ContractError as exc:
                return ProbeResult(
                    False,
                    1,
                    "INVALID_RESPONSE",
                    endpoint_kind.value,
                    safe_credential_source,
                    attempt,
                    last_status,
                    exc.code,
                )
            return ProbeResult(
                True,
                0,
                "AVAILABLE",
                endpoint_kind.value,
                safe_credential_source,
                attempt,
                last_status,
                "ok",
                len(npc_response.npc_line),
                len(npc_response.referenced_fact_ids),
            )
        except urllib.error.HTTPError as exc:
            status = int(exc.code)
            exc.close()
            last_status = str(status)
            last_error = _http_error_code(status)
            if status in RETRYABLE_HTTP_STATUSES and attempt == 1:
                sleep(retry_delay)
                continue
            return ProbeResult(
                False,
                1,
                "UNAVAILABLE",
                endpoint_kind.value,
                safe_credential_source,
                attempt,
                last_status,
                last_error,
            )
        except urllib.error.URLError as exc:
            last_error = (
                "transport_timeout"
                if isinstance(exc.reason, TimeoutError)
                else "transport_connection"
            )
            if attempt == 1:
                sleep(retry_delay)
                continue
            return ProbeResult(
                False,
                1,
                "UNAVAILABLE",
                endpoint_kind.value,
                safe_credential_source,
                attempt,
                last_status,
                last_error,
            )
        except TimeoutError:
            last_error = "transport_timeout"
            if attempt == 1:
                sleep(retry_delay)
                continue
            return ProbeResult(
                False,
                1,
                "UNAVAILABLE",
                endpoint_kind.value,
                safe_credential_source,
                attempt,
                last_status,
                last_error,
            )
        except OSError:
            last_error = "transport_connection"
            if attempt == 1:
                sleep(retry_delay)
                continue
            return ProbeResult(
                False,
                1,
                "UNAVAILABLE",
                endpoint_kind.value,
                safe_credential_source,
                attempt,
                last_status,
                last_error,
            )

    return ProbeResult(
        False,
        1,
        "UNAVAILABLE",
        endpoint_kind.value,
        safe_credential_source,
        2,
        last_status,
        last_error,
    )


def print_sanitized_result(result: ProbeResult) -> None:
    timestamp = dt.datetime.now(dt.timezone.utc).isoformat()
    print(f"timestamp_utc={timestamp}")
    print(f"endpoint_kind={result.endpoint_kind}")
    print(f"model={MODEL}")
    print(f"credential_source={result.credential_source}")
    print(f"attempts={result.attempts}")
    print(f"http_status={result.http_status}")
    print(f"error_code={result.error_code}")
    if result.success:
        print(f"npc_line_chars={result.npc_line_chars}")
        print(f"referenced_fact_count={result.referenced_fact_count}")
    print(f"result={result.result}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--timeout", type=float, default=8.0)
    parser.add_argument("--retry-delay", type=float, default=0.2)
    parser.add_argument(
        "--endpoint",
        help="Override with an allowlisted official or loopback endpoint.",
    )
    args = parser.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")
    if args.timeout <= 0 or args.retry_delay < 0:
        print_sanitized_result(
            _configuration_failure(
                code="invalid_probe_arguments",
                credential_source="none",
            )
        )
        return 2

    if args.endpoint:
        endpoint = args.endpoint
        try:
            override_policy = classify_endpoint(endpoint)
        except ContractError:
            api_key = ""
            source = "none"
        else:
            if override_policy.kind is EndpointKind.LOOPBACK:
                api_key = ""
                source = "none"
            else:
                _, api_key, source = load_settings()
    else:
        endpoint, api_key, source = load_settings()
    result = run_probe(
        endpoint=endpoint,
        api_key=api_key,
        credential_source=source,
        timeout=args.timeout,
        retry_delay=args.retry_delay,
    )
    print_sanitized_result(result)
    return result.exit_code


if __name__ == "__main__":
    raise SystemExit(main())
