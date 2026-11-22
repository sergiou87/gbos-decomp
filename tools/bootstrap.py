#!/usr/bin/env python3
"""Install the checksum-pinned toolchain locally, without changing system tools."""

import hashlib
import json
import platform
import subprocess
import tarfile
import tempfile
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parents[1]


def install_toolchain():
    lock = json.loads((ROOT / "toolchain.lock.json").read_text())
    host = "{}-{}".format(platform.system(), platform.machine())
    if host not in lock["assets"]:
        raise SystemExit("Unsupported host: {}. See toolchain.lock.json.".format(host))
    asset = lock["assets"][host]
    tools = ROOT / ".tools"
    tools.mkdir(exist_ok=True)
    archive = tools / asset["filename"]
    if not archive.exists():
        with tempfile.TemporaryDirectory(dir=tools) as temporary:
            download = Path(temporary) / asset["filename"]
            subprocess.run(
                ["curl", "--fail", "--location", "--silent", "--show-error",
                 "--proto", "=https", "--proto-redir", "=https",
                 "--output", str(download), lock["download_base"] + asset["filename"]],
                check=True,
            )
            verify_archive(download, asset["sha256"])
            download.replace(archive)
    verify_archive(archive, asset["sha256"])
    destination = tools / ("gbdk-" + lock["version"])
    marker = destination / ".archive-sha256"
    if marker.exists() and marker.read_text().strip() == asset["sha256"]:
        for executable in ("sdcc", "sdasgb", "sdldgb"):
            if not (destination / "bin" / executable).is_file():
                raise SystemExit("Incomplete toolchain: {}".format(destination))
        print("Toolchain ready: {}".format(destination.relative_to(ROOT)))
        return
    if destination.exists():
        raise SystemExit("Unrecognized toolchain directory: {}".format(destination))
    with tempfile.TemporaryDirectory(dir=tools) as temporary:
        with tarfile.open(archive) as package:
            for member in package.getmembers():
                path = PurePosixPath(member.name)
                if (path.is_absolute() or ".." in path.parts
                        or not path.parts or path.parts[0] != "gbdk"
                        or not (member.isfile() or member.isdir())):
                    raise SystemExit("Unsafe archive member: {}".format(member.name))
            # Members were restricted to regular files/directories within gbdk/.
            package.extractall(temporary)
        extracted = Path(temporary) / "gbdk"
        for executable in ("sdcc", "sdasgb", "sdldgb"):
            if not (extracted / "bin" / executable).is_file():
                raise SystemExit("Toolchain archive lacks {}".format(executable))
        (extracted / ".archive-sha256").write_text(asset["sha256"] + "\n")
        extracted.replace(destination)
    print("Installed GBDK {} in {}".format(lock["version"], destination.relative_to(ROOT)))


def verify_archive(path, expected):
    actual = hashlib.sha256(path.read_bytes()).hexdigest()
    if actual != expected:
        raise SystemExit(
            "Toolchain archive checksum mismatch: {}\nExpected: {}\nActual:   {}"
            .format(path, expected, actual)
        )


if __name__ == "__main__":
    try:
        install_toolchain()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error))
