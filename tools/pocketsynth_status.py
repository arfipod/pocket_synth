#!/usr/bin/env python3
"""Read pocketsynth WiFi Dev Mode status."""

from __future__ import annotations

import argparse
import json
import sys
import urllib.error
import urllib.request


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Fetch pocketsynth Dev Mode /status JSON.")
    parser.add_argument("--host", required=True, help="Device IP address or host name.")
    parser.add_argument("--timeout", type=float, default=5.0, help="Request timeout in seconds.")
    return parser.parse_args()


def build_url(host: str) -> str:
    if host.startswith("http://") or host.startswith("https://"):
        return f"{host.rstrip('/')}/status"
    return f"http://{host}/status"


def main() -> int:
    args = parse_args()
    url = build_url(args.host)

    try:
        with urllib.request.urlopen(url, timeout=args.timeout) as response:
            data = response.read().decode("utf-8", errors="replace")
    except urllib.error.URLError as exc:
        print(f"status request failed: {exc}", file=sys.stderr)
        return 1

    try:
        parsed = json.loads(data)
    except json.JSONDecodeError:
        print(data)
        return 1

    print(json.dumps(parsed, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
