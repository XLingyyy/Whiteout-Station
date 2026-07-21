"""OpenAI-compatible deterministic mock for WhiteoutStation model-path regression."""

from __future__ import annotations

import argparse
import json
import sys
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


def classify(text: str) -> dict[str, object]:
    if any(token in text for token in ("修复", "发电机", "维修间")):
        return {"intent": "promise", "promise_condition": "heat_repair_room", "confidence": 0.97}
    if any(token in text for token in ("保证", "记录", "不弃站")):
        return {"intent": "promise", "promise_condition": "keep_records", "confidence": 0.95}
    if any(token in text for token in ("别怕", "放心", "冷静")):
        return {"intent": "reassure", "promise_condition": "none", "confidence": 0.94}
    if any(token in text for token in ("撒谎", "证据", "隐瞒")):
        return {"intent": "challenge", "promise_condition": "none", "confidence": 0.93}
    return {"intent": "ask", "promise_condition": "none", "confidence": 0.91}


class Handler(BaseHTTPRequestHandler):
    server_version = "WhiteoutMock/0.3"

    def log_message(self, _format: str, *_args: object) -> None:
        return

    def do_POST(self) -> None:  # noqa: N802 - stdlib callback name
        length = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(length).decode("utf-8")
        request = json.loads(raw)
        messages = request.get("messages", [])
        system_text = str(messages[0].get("content", "")) if messages else ""
        user_text = str(messages[-1].get("content", "")) if messages else ""
        if "Classify" in system_text:
            content = classify(user_text)
            kind = "intent"
        else:
            context = json.loads(user_text)
            content = {
                "utterance": f"【mock】{context['preset_utterance']}",
                "emotion": context["emotion"],
                "response_type": context["response_type"],
                "referenced_fact_ids": [],
            }
            kind = "expression"
        response = {
            "id": "whiteout-mock-v03",
            "model": request.get("model", "deepseek-v4-flash"),
            "choices": [{"index": 0, "message": {"role": "assistant", "content": json.dumps(content, ensure_ascii=False)}}],
        }
        body = json.dumps(response, ensure_ascii=False).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)
        record = {
            "timestamp_utc": datetime.now(timezone.utc).isoformat(),
            "kind": kind,
            "request": request,
            "response": response,
        }
        with self.server.audit_path.open("a", encoding="utf-8") as handle:  # type: ignore[attr-defined]
            handle.write(json.dumps(record, ensure_ascii=False) + "\n")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--audit", type=Path, required=True)
    args = parser.parse_args()
    args.audit.parent.mkdir(parents=True, exist_ok=True)
    server = ThreadingHTTPServer((args.host, args.port), Handler)
    server.audit_path = args.audit  # type: ignore[attr-defined]
    print(f"READY http://{args.host}:{args.port}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    raise SystemExit(main())
