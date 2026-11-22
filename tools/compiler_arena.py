#!/usr/bin/env python3
"""Prepare and verify the macOS process-lifetime historical-compiler allocator."""

import hashlib
import json
import platform
import subprocess
import sys

from rom import ROOT


SOURCE = ROOT / "tools/compiler_arena.c"
DIRECTORY = ROOT / ".tools/compiler-arena"
LIBRARY = DIRECTORY / "arena.dylib"
MARKER = DIRECTORY / "identity.json"


def source_identity():
    return {"source_sha256": hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
            "platform": sys.platform, "machine": platform.machine(), "version": 1}


def arena_library():
    if not LIBRARY.is_file() or not MARKER.is_file():
        raise ValueError("Missing deterministic compiler allocator. Run make setup.")
    marker = json.loads(MARKER.read_text())
    expected = dict(source_identity(),
                    library_sha256=hashlib.sha256(LIBRARY.read_bytes()).hexdigest())
    if marker != expected:
        raise ValueError("Deterministic compiler allocator changed. Run make setup.")
    return LIBRARY


def setup():
    if sys.platform != "darwin":
        return
    try:
        arena_library()
    except (ValueError, OSError):
        DIRECTORY.mkdir(parents=True, exist_ok=True)
        subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
                        "-dynamiclib", str(SOURCE), "-o", str(LIBRARY)],
                       cwd=ROOT, check=True)
        marker = dict(source_identity(),
                      library_sha256=hashlib.sha256(LIBRARY.read_bytes()).hexdigest())
        MARKER.write_text(json.dumps(marker, indent=2) + "\n")
    print("Deterministic historical compiler allocator ready")


if __name__ == "__main__":
    try:
        setup()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error))
