#!/usr/bin/env python3
"""Read pocketsynth WiFi Dev Mode bounded diagnostic logs."""

from __future__ import annotations

import argparse
import sys
import urllib.error
import urllib.request


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Fetch pocketsynth Dev Mode /logs output.")
    parser.add_argument("--host", required=True, help="Device IP address or host name.")
    parser.add_argument("--timeout", type=float, default=5.0, help="Request timeout in seconds.")
    return parser.parse_args()


def build_url(host: str) -> str:
    if host.startswith("http://") or host.startswith("https://"):
        return f"{host.rstrip('/')}/logs"
    return f"http://{host}/logs"


def main() -> int:
    args = parse_args()
    url = build_url(args.host)

    try:
        with urllib.request.urlopen(url, timeout=args.timeout) as response:
            print(response.read().decode("utf-8", errors="replace"), end="")
    except urllib.error.URLError as exc:
        print(f"logs request failed: {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
