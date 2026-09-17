#!/usr/bin/env python3
"""Assemble reproducible WLEDIMUUDP release artifacts from completed PlatformIO builds."""

from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def revision_from_git() -> str:
    result = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, check=True, capture_output=True, text=True
    )
    return result.stdout.strip()


def host_program(environment: str) -> Path:
    base = ROOT / ".pio" / "build" / environment
    windows = base / "program.exe"
    native = base / "program"
    if windows.is_file():
        return windows
    if native.is_file():
        return native
    raise FileNotFoundError(f"missing built host program for {environment}: {base}")


def copy(source: Path, destination: Path) -> Path:
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
    return destination


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", default="dist/WLEDIMUUDP-release")
    parser.add_argument("--revision", default=os.environ.get("SOURCE_REVISION"))
    parser.add_argument("--ci-revision", default=os.environ.get("GITHUB_SHA"))
    args = parser.parse_args()

    version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
    source_revision = args.revision or revision_from_git()
    ci_revision = args.ci_revision or source_revision
    output = (ROOT / args.output).resolve()

    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)

    built: list[tuple[str, Path]] = []
    built.append(
        (
            "firmware/firmware.bin",
            copy(
                ROOT / ".pio" / "build" / "esp32s3" / "firmware.bin",
                output / "firmware" / "firmware.bin",
            ),
        )
    )

    host = host_program("host_probe")
    mapper = host_program("mapping_probe")
    suffix = host.suffix
    built.append(
        (f"tools/host_probe{suffix}", copy(host, output / "tools" / f"host_probe{suffix}"))
    )
    built.append(
        (f"tools/mapping_probe{suffix}", copy(mapper, output / "tools" / f"mapping_probe{suffix}"))
    )

    copy(ROOT / "config" / "wifi.example.hpp", output / "config" / "wifi.example.hpp")
    copy(ROOT / "README.md", output / "README.md")
    copy(ROOT / "CHANGELOG.md", output / "CHANGELOG.md")
    copy(ROOT / "docs" / "rag" / "RELEASE.md", output / "docs" / "RELEASE.md")
    copy(
        ROOT / "docs" / "rag" / "WU006_IMPLEMENTATION.md",
        output / "docs" / "WU006_IMPLEMENTATION.md",
    )

    manifest = [
        "WLEDIMUUDP release manifest",
        f"version={version}",
        f"source_revision={source_revision}",
        f"ci_revision={ci_revision}",
        "release_schema=1",
        "protocol=AudioSync-V2/00002",
        "mapping=Balanced-v1",
        "board=waveshare-esp32-s3-matrix",
        "imu=QMI8658/QMI8658C",
        "default_target=239.0.0.1:11988",
        "default_packet_rate_hz=50",
        "physical_compatibility=see docs/WU006_IMPLEMENTATION.md; pending rows remain pending",
        "",
        "SHA256",
    ]
    for relative, path in built:
        manifest.append(f"{sha256(path)}  {relative}")
    (output / "MANIFEST.txt").write_text("\n".join(manifest) + "\n", encoding="utf-8")

    print(
        "release-bundle: "
        f"{output.relative_to(ROOT)} version={version} "
        f"source_revision={source_revision} ci_revision={ci_revision}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
