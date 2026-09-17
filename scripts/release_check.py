#!/usr/bin/env python3
"""Deterministic release/configuration sanity checks for WLEDIMUUDP."""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SEMVER = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+$")


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def tracked_files() -> list[str]:
    result = subprocess.run(
        ["git", "ls-files"], cwd=ROOT, check=True, capture_output=True, text=True
    )
    return [line.strip() for line in result.stdout.splitlines() if line.strip()]


def main() -> int:
    errors: list[str] = []

    version = read("VERSION").strip()
    if not SEMVER.fullmatch(version):
        errors.append(f"VERSION must be MAJOR.MINOR.PATCH, got {version!r}")

    firmware_header = read(
        "lib/WledImuUdpFirmware/include/wledimuudp/firmware_support.hpp"
    )
    if f'kProjectVersion[] = "{version}"' not in firmware_header:
        errors.append("firmware kProjectVersion does not match VERSION")
    if f'kFirmwareIdentity[] = "WLEDIMUUDP/{version}"' not in firmware_header:
        errors.append("firmware identity does not include the project VERSION")
    if 'kProtocolIdentity[] = "AudioSync-V2/00002"' not in firmware_header:
        errors.append("firmware protocol identity is missing or changed")

    config = read("config/wifi.example.hpp")
    required_config_tokens = (
        'kWifiSsid[] = "CHANGE_ME"',
        'kWifiPassword[] = "CHANGE_ME"',
        "kMulticastAddress{239U, 0U, 0U, 1U}",
        "kMulticastPort = 11988U",
        "kPacketRateHz = 50U",
        "kReconnectIntervalMs = 5000U",
    )
    for token in required_config_tokens:
        if token not in config:
            errors.append(f"safe configuration template missing expected token: {token}")

    tracked = tracked_files()
    forbidden_prefixes = (".pio/", "dist/", ".venv/")
    for path in tracked:
        if path == "config/wifi.local.hpp":
            errors.append("config/wifi.local.hpp must never be tracked")
        if path.startswith(forbidden_prefixes):
            errors.append(f"generated build/release debris is tracked: {path}")

    ignore = read(".gitignore")
    if "config/wifi.local.hpp" not in ignore:
        errors.append(".gitignore must protect config/wifi.local.hpp")
    if "dist/" not in ignore:
        errors.append(".gitignore must exclude generated release bundles under dist/")

    required_docs = (
        "README.md",
        "CHANGELOG.md",
        "docs/rag/RELEASE.md",
        "docs/rag/HOST_PROBE.md",
        "docs/rag/WU006_IMPLEMENTATION.md",
    )
    for path in required_docs:
        if not (ROOT / path).is_file():
            errors.append(f"required release document is missing: {path}")

    if errors:
        print("release-check: FAILED", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print(f"release-check: OK version={version} tracked_files={len(tracked)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
