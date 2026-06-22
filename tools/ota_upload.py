#!/usr/bin/env python3
"""Upload a firmware image to pocketsynth WiFi Dev Mode OTA."""

from __future__ import annotations

import argparse
import pathlib
import sys
import urllib.error
import urllib.request


DEFAULT_TOKEN = "pocketsynth-dev"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Upload firmware.bin to pocketsynth Dev Mode OTA.")
    parser.add_argument("--host", required=True, help="Device IP address or host name.")
    parser.add_argument("--bin", required=True, type=pathlib.Path, help="Path to firmware.bin.")
    parser.add_argument("--token", default=DEFAULT_TOKEN, help="OTA token header value.")
    parser.add_argument("--timeout", type=float, default=60.0, help="Upload timeout in seconds.")
    return parser.parse_args()


def build_url(host: str) -> str:
    if host.startswith("http://") or host.startswith("https://"):
        base = host.rstrip("/")
    else:
        base = f"http://{host}"
    return f"{base}/ota"


def main() -> int:
    args = parse_args()
    firmware_path = args.bin
    if not firmware_path.is_file():
        print(f"firmware not found: {firmware_path}", file=sys.stderr)
        return 2

    firmware = firmware_path.read_bytes()
    request = urllib.request.Request(
        build_url(args.host),
        data=firmware,
        method="POST",
        headers={
            "Content-Type": "application/octet-stream",
            "Content-Length": str(len(firmware)),
            "X-PocketSynth-Token": args.token,
        },
    )

    print(f"Uploading {firmware_path} ({len(firmware)} bytes) to {request.full_url}")
    try:
        with urllib.request.urlopen(request, timeout=args.timeout) as response:
            body = response.read().decode("utf-8", errors="replace").strip()
            print(f"HTTP {response.status}: {body}")
    except urllib.error.HTTPError as exc:
        body = exc.read().decode("utf-8", errors="replace").strip()
        print(f"HTTP {exc.code}: {body}", file=sys.stderr)
        return 1
    except urllib.error.URLError as exc:
        print(f"upload failed: {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
