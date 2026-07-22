"""Local credential-free expression endpoint for Whiteout Station smoke tests."""

from __future__ import annotations

import argparse
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


class AgentHandler(BaseHTTPRequestHandler):
    server_version = "WhiteoutAgentMock/0.1"

    def do_POST(self) -> None:  # noqa: N802 - stdlib handler API
        length = int(self.headers.get("Content-Length", "0"))
        try:
            payload = json.loads(self.rfile.read(length).decode("utf-8"))
            messages = payload.get("messages", [])
            context = json.loads(messages[-1].get("content", "{}")) if messages else payload
            speaker = str(context["speaker"])
            response_type = str(context["response_type"])
            action_id = str(context["action_id"])
        except (UnicodeDecodeError, json.JSONDecodeError, KeyError, TypeError, ValueError):
            self.send_error(400, "invalid request")
            return

        response = {
            "utterance": f"{speaker}：收到。按当前决定继续处理 {action_id}。",
            "emotion": str(context.get("emotion", "focused")),
            "response_type": response_type,
            "referenced_fact_ids": [],
        }
        encoded = json.dumps(response, ensure_ascii=False).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(encoded)))
        self.end_headers()
        self.wfile.write(encoded)

    def log_message(self, message: str, *args: object) -> None:
        print(f"agent-mock: {self.address_string()} {message % args}", flush=True)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    server = ThreadingHTTPServer(("127.0.0.1", args.port), AgentHandler)
    print(f"Whiteout agent mock listening on http://127.0.0.1:{args.port}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
