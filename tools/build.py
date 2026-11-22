#!/usr/bin/env python3
"""Compile and link the complete fixed-address ROM, then verify its SHA-256."""

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

import standin
from rom import ROOT, load_config, verify_binary
from build_linux import PROFILE_ID as LINUX_PROFILE_ID, compile_c as compile_linux_c
from build_wine import PROFILE_ID as WINE_PROFILE_ID, compile_c as compile_wine_c
from context_compile import compile_context
from compiler_arena import arena_library

BUILD = ROOT / "build"


def historical_compiler_environment(bin_directory):
    env = os.environ.copy()
    env["PATH"] = str(bin_directory) + os.pathsep + env.get("PATH", "")
    env["LC_ALL"] = "C"
    if sys.platform != "darwin":
        raise ValueError("The pinned native compiler profile requires macOS")
    env["DYLD_INSERT_LIBRARIES"] = str(arena_library())
    env["MallocNanoZone"] = "0"
    env["MallocMaxMagazines"] = "1"
    return env


def parse_ihx(text, size):
    memory = {}
    base = 0
    eof = False
    for line in text.splitlines():
        if not line.strip():
            continue
        if eof:
            raise ValueError("Intel HEX data after EOF")
        if not line.startswith(":"):
            raise ValueError("Invalid Intel HEX record")
        record = bytes.fromhex(line[1:])
        if len(record) < 5 or len(record) != record[0] + 5 or sum(record) & 0xff:
            raise ValueError("Invalid Intel HEX length or checksum")
        count = record[0]
        offset = int.from_bytes(record[1:3], "big")
        kind = record[3]
        payload = record[4:-1]
        if kind == 0:
            for index, value in enumerate(payload):
                address = base + offset + index
                if not 0 <= address < size:
                    raise ValueError("Linked byte outside ROM: 0x{:x}".format(address))
                if address in memory:
                    raise ValueError("Overlapping linked byte at 0x{:04x}".format(address))
                memory[address] = value
        elif kind == 1 and count == 0 and offset == 0:
            eof = True
        elif kind in (2, 4) and count == 2 and offset == 0:
            base = int.from_bytes(payload, "big") << (4 if kind == 2 else 16)
        else:
            raise ValueError("Unsupported Intel HEX record type/shape: {}".format(kind))
    if not eof:
        raise ValueError("Intel HEX missing EOF")
    if len(memory) != size:
        missing = next(index for index in range(size) if index not in memory)
        raise ValueError("Unfilled ROM byte at 0x{:04x}".format(missing))
    return bytes(memory[index] for index in range(size))


def normalize_legacy_abi(text):
    normalized, count = re.subn(
        r"(?m)^[ \t]*[.]optsdcc -mgbz80[ \t]*$",
        "\t.optsdcc -msm83 sdcccall(0)", text)
    if count != 1:
        raise ValueError("Unexpected historical object ABI metadata")
    return normalized


def verify_source(region):
    if "source_sha256" not in region:
        return
    source = ROOT / region["source"]
    if not source.is_file():
        raise ValueError("Missing pinned source {}. Run make setup.".format(region["source"]))
    if hashlib.sha256(source.read_bytes()).hexdigest() != region["source_sha256"]:
        raise ValueError("Pinned source changed: " + region["source"])


def reproduce_file_menu_modification(binary, regions):
    start, end = 0x4b34, 0x4b37
    owners = [region for region in regions if region["start"] < end and region["end"] > start]
    if (len(owners) != 1 or owners[0]["kind"] != "c"
            or owners[0]["name"] != "file_open_menu"
            or owners[0]["start"] != 0x4ae1 or owners[0]["end"] != 0x4c0f):
        raise ValueError("Original file-menu modification requires its complete C region")
    preimage = bytes.fromhex("00 23 36")
    if binary[start:end] != preimage:
        raise ValueError("Original file-menu modification preimage mismatch at 0x4b34")
    if binary[0x0801:0x0804] != preimage:
        raise ValueError("Original file-menu restoration bytes mismatch at 0x0801")
    # The original image's guarded modification is reproduced only after linking.
    # Assembly/IHX stay unchanged; the final complete-image hash verifies the result.
    return binary[:start] + bytes.fromhex("c3 e5 08") + binary[end:]


def run(command, env=None, cwd=ROOT):
    print("+ " + " ".join(str(part) for part in command), flush=True)
    subprocess.run([str(part) for part in command], cwd=cwd, env=env, check=True)


def native_compiler():
    candidates = json.loads((ROOT / "config/compiler-candidates.json").read_text())
    candidate = next(item for item in candidates["candidates"] if item["id"] == "r8604")
    historical = ROOT / ".tools/sdcc-r8604-src"
    marker = historical / ".gbos-build.json"
    stamp = json.loads(marker.read_text()) if marker.is_file() else {}
    if (stamp.get("source_sha256") != candidate["sha256"]
            or stamp.get("slot_order_patch")
            or not all((historical / "bin" / name).is_file() for name in ("sdcc", "sdcpp"))):
        raise ValueError("Missing pinned historical compiler. Run make setup.")
    return historical


def reject_non_wine_standins(region):
    if region["kind"] != "c" or region.get("compiler") == WINE_PROFILE_ID:
        return
    if standin.has_any((ROOT / region["source"]).read_text()):
        raise ValueError("Symbol-phase stand-ins require profile " + WINE_PROFILE_ID)


def compile_historical_c(region, assembly):
    compiler_id = region["compiler"]
    reject_non_wine_standins(region)
    if compiler_id == LINUX_PROFILE_ID:
        compile_linux_c(region, assembly)
    elif compiler_id == WINE_PROFILE_ID:
        compile_wine_c(region, assembly)
    else:
        historical = native_compiler()
        env = historical_compiler_environment(historical / "bin")
        compiler = historical / "bin/sdcc"
        if "context" in region:
            compile_context(region, assembly, compiler, env, run)
        else:
            optimization = region.get("optimization", "size")
            flags = [] if optimization == "default" else ["--opt-code-" + optimization]
            run([compiler, "-mgbz80", *flags, *region.get("compiler_flags", []),
                 "--nostdlib", "--no-std-crt0", "--codeseg", region["area"],
                 "--constseg", region["area"], "-S", region["source"], "-o", assembly], env=env)
    adapter = assembly.with_suffix(".sdas.s")
    if "context" not in region:
        adapter.write_text(normalize_legacy_abi(assembly.read_text()))
    return adapter


def build():
    if BUILD.is_symlink():
        raise ValueError("Refusing to write to a symlinked build directory")
    BUILD.mkdir(exist_ok=True)
    config = load_config()
    output = BUILD / config["output"]
    output.unlink(missing_ok=True)
    for region in config["regions"]:
        verify_source(region)
    lock = json.loads((ROOT / "toolchain.lock.json").read_text())
    toolchain = ROOT / ".tools" / ("gbdk-" + lock["version"]) / "bin"
    for tool in ("sdcc", "sdasgb", "sdldgb"):
        if not (toolchain / tool).is_file():
            raise ValueError("Missing pinned toolchain. Run make setup first.")
    objects, sources, areas = [], set(), []
    for region in config["regions"]:
        source = region["source"]
        if source in sources:
            continue
        sources.add(source)
        obj = BUILD / "obj" / Path(source).with_suffix(".rel")
        obj.parent.mkdir(parents=True, exist_ok=True)
        if region["kind"] == "asm":
            run([toolchain / "sdasgb", "-plos", obj, source])
        elif "compiler" in region:
            adapter = compile_historical_c(region, obj.with_suffix(".asm"))
            run([toolchain / "sdasgb", "-plosg", obj, adapter])
        else:
            reject_non_wine_standins(region)
            run([toolchain / "sdcc", *config["compiler_flags"],
                 *region.get("compiler_flags", []),
                 "--codeseg", region["area"], "--constseg", region["area"],
                 "-c", source, "-o", obj])
        if "area" in region:
            areas.extend(["-b", "_{}=0x{:04x}".format(region["area"], region["start"])])
        objects.append(obj)
    run([toolchain / "sdldgb", "-n", "-i", "-m", "-w", "-j", *areas,
         BUILD / "gbos", *objects])
    linked = parse_ihx((BUILD / "gbos.ihx").read_text(), config["size"])
    binary = reproduce_file_menu_modification(linked, config["regions"])
    verify_binary(binary, config)
    output.write_bytes(binary)
    print("Built and verified " + str(output.relative_to(ROOT)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clean", action="store_true")
    args = parser.parse_args()
    if args.clean:
        if BUILD.is_symlink():
            raise ValueError("Refusing to clean a symlinked build directory")
        if BUILD.exists():
            shutil.rmtree(BUILD)
    else:
        build()


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error))
