#!/usr/bin/env python3
"""Small dependency-free bootstrap helper for a clean WLEDIMUUDP checkout."""

from __future__ import annotations

import argparse
import importlib.metadata
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXAMPLE = ROOT / "config" / "wifi.example.hpp"
LOCAL = ROOT / "config" / "wifi.local.hpp"


def package_version(name: str) -> str | None:
    try:
        return importlib.metadata.version(name)
    except importlib.metadata.PackageNotFoundError:
        return None


def print_status() -> bool:
    expected_platformio = "6.1.18"
    expected_clang = "18.1.8"
    platformio = package_version("platformio")
    clang_format = package_version("clang-format")

    print("WLEDIMUUDP bootstrap status")
    print(f"Python: {sys.version.split()[0]} (CI reference: 3.12)")
    print(f"PlatformIO: {platformio or 'not installed'} (pinned: {expected_platformio})")
    print(f"clang-format: {clang_format or 'not installed'} (pinned: {expected_clang})")
    print(f"Local config: {'present' if LOCAL.exists() else 'missing'}")

    return platformio == expected_platformio and clang_format == expected_clang


def init_config() -> int:
    if LOCAL.exists():
        print(f"Refusing to overwrite existing {LOCAL.relative_to(ROOT)}")
        return 0
    shutil.copyfile(EXAMPLE, LOCAL)
    print(f"Created {LOCAL.relative_to(ROOT)} from the safe template.")
    print("Edit kWifiSsid/kWifiPassword in the local file only; it is ignored by Git.")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--init-config",
        action="store_true",
        help="create config/wifi.local.hpp from the committed safe template without overwriting",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="fail unless the pinned development dependencies are installed",
    )
    args = parser.parse_args()

    if args.init_config:
        result = init_config()
        if result != 0:
            return result

    dependencies_ok = print_status()
    if args.check and not dependencies_ok:
        print("Install pinned dependencies with: python -m pip install -r requirements-dev.txt")
        return 1

    if not args.init_config and not LOCAL.exists():
        print("Next: python scripts/bootstrap.py --init-config")
    print("Verification: python scripts/release_check.py")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
