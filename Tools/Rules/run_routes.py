from __future__ import annotations

import json
import sys

from whiteout_rules import WhiteoutSimulator, run_route


def main() -> int:
    output = []
    for route_id in WhiteoutSimulator().rules["routes"]:
        output.append(run_route(WhiteoutSimulator(), route_id))
    print(json.dumps(output, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    raise SystemExit(main())
