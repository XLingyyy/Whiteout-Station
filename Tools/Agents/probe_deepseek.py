"""Minimal, sanitized availability probe for the fixed v0.3 DeepSeek model."""

from __future__ import annotations

import argparse
import configparser
import datetime as dt
import json
import os
import sys
import urllib.error
import urllib.request
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
LOCAL_CONFIG = ROOT / "WhiteoutStation" / "LocalConfig" / "WhiteoutLLM.ini"
DEFAULT_ENDPOINT = "https://api.deepseek.com/chat/completions"
MODEL = "deepseek-v4-flash"


def load_settings() -> tuple[str, str, str]:
    endpoint = DEFAULT_ENDPOINT
    key_from_file = ""
    if LOCAL_CONFIG.is_file():
        config = configparser.ConfigParser()
        config.read(LOCAL_CONFIG, encoding="utf-8-sig")
        endpoint = config.get("WhiteoutLLM", "Endpoint", fallback=endpoint).strip() or endpoint
        key_from_file = config.get("WhiteoutLLM", "ApiKey", fallback="").strip()
    environment_key = os.environ.get("WHITEOUT_LLM_API_KEY", "").strip()
    if environment_key:
        return endpoint, environment_key, "environment"
    if key_from_file:
        return endpoint, key_from_file, "local_ini"
    return endpoint, "", "none"


def sanitized_sample(payload: bytes) -> str:
    try:
        root = json.loads(payload.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError):
        return "<non-json response>"
    if not isinstance(root, dict):
        return "<unexpected response shape>"
    choices = root.get("choices")
    if isinstance(choices, list) and choices and isinstance(choices[0], dict):
        message = choices[0].get("message")
        if isinstance(message, dict):
            content = str(message.get("content", "")).replace("\r", " ").replace("\n", " ")
            return content[:160] or "<empty model content>"
    error = root.get("error")
    if isinstance(error, dict):
        return f"error_type={error.get('type', 'unknown')} code={error.get('code', 'unknown')}"
    return "<json response without choices>"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--timeout", type=float, default=12.0)
    args = parser.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")
    endpoint, api_key, source = load_settings()
    timestamp = dt.datetime.now(dt.timezone.utc).isoformat()
    print(f"timestamp_utc={timestamp}")
    print(f"endpoint={endpoint}")
    print(f"model={MODEL}")
    print(f"credential_source={source}")
    if not api_key:
        print("result=SKIPPED_NO_SAFE_CREDENTIAL")
        return 2

    body = json.dumps(
        {
            "model": MODEL,
            "messages": [
                {"role": "system", "content": "Reply with exactly: WHITEOUT_PROBE_OK"},
                {"role": "user", "content": "Availability probe."},
            ],
            "temperature": 0,
            "max_tokens": 16,
            "stream": False,
        },
        ensure_ascii=False,
    ).encode("utf-8")
    request = urllib.request.Request(
        endpoint,
        data=body,
        method="POST",
        headers={
            "Authorization": f"Bearer {api_key}",
            "Content-Type": "application/json; charset=utf-8",
        },
    )
    try:
        with urllib.request.urlopen(request, timeout=args.timeout) as response:
            payload = response.read()
            print(f"http_status={response.status}")
            print(f"sample_response={sanitized_sample(payload)}")
            print("result=AVAILABLE" if 200 <= response.status < 300 else "result=UNAVAILABLE")
            return 0 if 200 <= response.status < 300 else 1
    except urllib.error.HTTPError as exc:
        payload = exc.read()
        print(f"http_status={exc.code}")
        print(f"sample_response={sanitized_sample(payload)}")
        print("result=UNAVAILABLE")
        return 1
    except (urllib.error.URLError, TimeoutError) as exc:
        print("http_status=NO_RESPONSE")
        print(f"transport_error={type(exc).__name__}")
        print("result=UNAVAILABLE")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
