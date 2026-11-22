#!/usr/bin/env python3
"""Prepare and use the original win32 SDCC 3.3.0 under Wine with an XP-style CRT heap.

SDCC assigns a function's stack slots in the address order of its symbol structures, so the
layout depends on the host heap. Profile 3.3.0-win32-xpheap runs the original compiler (pinned
installer) in a pinned Wine image; a patched copy of sdcc.exe imports tools/xpheap/xpheap.dll first,
which serves msvcrt's CRT heap from the Windows XP heap model at a fixed address. Every compile
uses the same container paths and a cleared environment, so msvcrt's startup allocations (and
therefore every later heap address) are reproducible.
"""

import hashlib
import json
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path, PurePosixPath

import standin
from build_historical import download
from build_linux import MissingRuntimeError, _VERIFIED_DOCKER_IMAGES, bind_mount, ensure_docker_image
from context_compile import (WINE_PROFILE_ID as PROFILE_ID, context_flags, function_span,
                             prepare_context, wrap_context)
from rom import ROOT

RELEASE_ID = "3.3.0-win32"
TOOLS = ROOT / ".tools"
INSTALLER = TOOLS / "sdcc-3.3.0-setup.exe"
PLAIN = TOOLS / "sdcc-3.3.0-win32"
EMULATED = TOOLS / "sdcc-3.3.0-win32-xpheap"
MARKER = EMULATED / ".gbos-xpheap.json"
WORK = ROOT / "build/wine"
SOURCES = ("tools/xpheap/xpheap.c", "tools/xpheap/xpheap_engine.h", "tools/xpheap/patch_imports.py")
EMULATED_FILES = ("bin/sdcc.exe", "bin/xpheap.dll", "bin/sdcpp.exe")
DLL_FLAGS = ["-O2", "-Wall", "-shared", "-ffreestanding", "-fno-builtin",
             "-fno-tree-loop-distribute-patterns", "-fno-stack-protector", "-mno-stack-arg-probe",
             "-nostdlib", "-Wl,-e,_DllMain@12", "-Wl,--image-base,0x50000000"]


def load_profile():
    releases = json.loads((ROOT / "config/compiler-candidates.json").read_text())["binary_releases"]
    return next(release for release in releases if release["id"] == RELEASE_ID)


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def runtime_recipe(profile):
    if not re.fullmatch(r"[\w/.-]+@sha256:[0-9a-f]{64}", profile["runtime_image"]):
        raise ValueError("Wine runtime must use an image digest")
    packages = []
    for package, version in sorted(profile["runtime_packages"].items()):
        if (not re.fullmatch(r"[a-z0-9+.:-]+", package)
                or not re.fullmatch(r"[A-Za-z0-9.+:~_-]+", version)):
            raise ValueError("Invalid pinned Wine runtime package")
        packages.append(package + "=" + version)
    return ("FROM " + profile["runtime_image"] + "\n"
            "RUN dpkg --add-architecture i386 && apt-get update -qq"
            " && apt-get install -y --no-install-recommends " + " ".join(packages)
            + " && apt-get clean\n"
            "RUN WINEPREFIX=/opt/gbos-wine-prefix WINEARCH=win32 WINEDEBUG=-all"
            " HOME=/tmp wineboot -i && WINEPREFIX=/opt/gbos-wine-prefix wineserver -w\n")


def fingerprint(profile):
    inputs = {
        "recipe": runtime_recipe(profile),
        "platform": profile["runtime_platform"],
        "installer": profile["sha256"],
        "compiler": profile["compiler_sha256"],
        "preprocessor": profile["preprocessor_sha256"],
        "xpheap": profile["xpheap"],
        "sources": {name: sha256(ROOT / name) for name in SOURCES},
        "dll_flags": DLL_FLAGS,
        "layout_version": 1,
    }
    return hashlib.sha256(json.dumps(inputs, sort_keys=True).encode()).hexdigest()


def require_docker():
    if shutil.which("docker") is None:
        raise ValueError("Docker is required for the Wine compiler. Install/start Docker, "
                         "then run make setup.")


def docker(profile, image, mounts, command, environment=None, workdir="/work"):
    run = ["docker", "run", "--rm", "--pull", "never", "--platform", profile["runtime_platform"],
           "--network", "none", "--read-only", "--tmpfs", "/tmp:size=256m", "--cap-drop", "ALL",
           "--security-opt", "no-new-privileges", *mounts, "--workdir", workdir]
    for key, value in sorted((environment or {}).items()):
        run += ["--env", key + "=" + value]
    return run + ["--entrypoint", "/bin/sh", image, "-c", command]


def _verify_plain_binaries(profile, directory=None):
    directory = PLAIN if directory is None else directory
    for name, key in (("bin/sdcc.exe", "compiler_sha256"), ("bin/sdcpp.exe", "preprocessor_sha256")):
        path = directory / name
        if not path.is_file():
            raise MissingRuntimeError("Missing extracted Wine compiler file: " + name)
        if sha256(path) != profile[key]:
            raise MissingRuntimeError("Extracted Wine compiler file changed: " + name)


def image_id(check_docker=True):
    if not MARKER.is_file():
        raise MissingRuntimeError("Missing pinned Wine compiler. Run make setup.")
    try:
        marker = json.loads(MARKER.read_text())
    except json.JSONDecodeError as error:
        raise MissingRuntimeError("Invalid pinned Wine compiler marker. Run make setup.") from error
    profile = load_profile()
    if not isinstance(marker, dict) or marker.get("fingerprint") != fingerprint(profile):
        raise MissingRuntimeError("Wine compiler configuration changed. Run make setup.")
    files = marker.get("files")
    if not isinstance(files, dict) or set(files) != set(EMULATED_FILES):
        raise MissingRuntimeError("Invalid pinned Wine compiler file list. Run make setup.")
    for name in EMULATED_FILES:
        path = EMULATED / name
        if not path.is_file() or sha256(path) != files[name]:
            raise MissingRuntimeError("Pinned Wine compiler file changed: " + name)
    image = marker.get("image_id", "")
    if not isinstance(image, str) or not re.fullmatch(r"sha256:[0-9a-f]{64}", image):
        raise MissingRuntimeError("Invalid pinned Wine runtime image identity. Run make setup.")
    if check_docker:
        ensure_docker_image(image, "Wine")
    return image


def setup():
    require_docker()
    profile = load_profile()
    tag = "gbos-sdcc330-win32:" + fingerprint(profile)[:16]
    try:
        _verify_plain_binaries(profile)
        image_id(check_docker=True)
        print("Pinned Wine compiler ready: " + tag)
        return
    except MissingRuntimeError:
        pass
    TOOLS.mkdir(exist_ok=True)
    download(profile["url"], INSTALLER, profile["sha256"])
    subprocess.run(["docker", "build", "--platform", profile["runtime_platform"], "--tag", tag, "-"],
                   input=runtime_recipe(profile), cwd=ROOT, text=True, check=True)
    image = subprocess.run(["docker", "image", "inspect", tag, "--format", "{{.Id}}"],
                           capture_output=True, text=True, check=True).stdout.strip()
    if not re.fullmatch(r"sha256:[0-9a-f]{64}", image):
        raise ValueError("Docker returned an invalid Wine runtime image identity")
    with tempfile.TemporaryDirectory(dir=TOOLS) as temporary:
        staged = Path(temporary)
        (staged / "in").mkdir()
        shutil.copyfile(INSTALLER, staged / "in/setup.exe")
        for name in SOURCES:
            shutil.copyfile(ROOT / name, staged / "in" / Path(name).name)
        command = ("set -eu; 7z x -y -bso0 -bsp0 -o/stage/plain /stage/in/setup.exe"
                   " && rm -rf '/stage/plain/$PLUGINSDIR' /stage/plain/uninstall.exe"
                   " && cd /stage/in && i686-w64-mingw32-gcc "
                   + " ".join(shlex.quote(flag) for flag in DLL_FLAGS)
                   + " -o /stage/xpheap.dll xpheap.c -lkernel32 -lgcc")
        subprocess.run(docker(profile, image, bind_mount(staged, "/stage"),
                              command), check=True)
        plain = staged / "plain"
        _verify_plain_binaries(profile, plain)
        emulated = staged / "emulated"
        shutil.copytree(plain, emulated)
        sys.path.insert(0, str(ROOT / "tools/xpheap"))
        import patch_imports
        patch_imports.patch(plain / "bin/sdcc.exe", emulated / "bin/sdcc.exe")
        shutil.copyfile(staged / "xpheap.dll", emulated / "bin/xpheap.dll")
        for target, source in ((PLAIN, plain), (EMULATED, emulated)):
            if target.exists():
                shutil.rmtree(target)
            source.replace(target)
    _VERIFIED_DOCKER_IMAGES.add(image)
    files = {name: sha256(EMULATED / name) for name in EMULATED_FILES}
    MARKER.write_text(json.dumps({"fingerprint": fingerprint(profile), "image_id": image,
                                  "files": files}, indent=2) + "\n")
    print("Pinned Wine compiler ready: " + tag)


def _shell_environment(mapping):
    assignments = []
    for key, value in sorted(mapping.items()):
        if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", key):
            raise ValueError("Invalid environment variable name: " + repr(key))
        assignments.append(shlex.quote("{}={}".format(key, value)))
    return " ".join(assignments)


def compile_files(files, unit, flags, assembly, compiler=EMULATED, work=WORK):
    """Compile `unit` among `files` ({name: text}) at the fixed container path; write `assembly`.
    `work` holds the unit directory (one compile at a time per directory).
    Each container gets a fresh Wine prefix on a native Linux tmpfs."""
    if unit not in files:
        raise ValueError("Compile unit is missing from unit files: " + str(unit))
    profile = load_profile()
    image = image_id()
    settings = profile["xpheap"]
    work = Path(work).resolve()
    work.mkdir(parents=True, exist_ok=True)
    directory = work / "unit"
    if directory.exists():
        shutil.rmtree(directory)
    directory.mkdir(parents=True)
    for name, text in files.items():
        if "/" in name or "\x00" in name or name.startswith("."):
            raise ValueError("Invalid unit file name: " + name)
        (directory / name).write_text(text)
    environment = _shell_environment(settings["container_environment"])
    env_command = "env -i " + environment if environment else "env -i"
    flag_args = (" " + " ".join(shlex.quote(str(flag)) for flag in flags)) if flags else ""
    compiler_exe = shlex.quote(
        PurePosixPath(settings["compiler_directory"], "bin/sdcc.exe").as_posix())
    command = ("cp -a /opt/gbos-wine-prefix/. /work/prefix/"
               " && cd {unit} && {env} wine {compiler} -mgbz80{flags} -I . -S {name}"
               " -o out.asm > compile.log 2>&1; status=$?; {env} wineserver -k; exit $status").format(
        unit=shlex.quote(settings["unit_directory"]), env=env_command, compiler=compiler_exe,
        flags=flag_args, name=shlex.quote(unit))
    mounts = [*bind_mount(work, "/work"),
              "--tmpfs", "/work/prefix:size=1g",
              *bind_mount(compiler, settings["compiler_directory"], readonly=True)]
    print("+ wine sdcc.exe -mgbz80 " + " ".join(flags) + " -S " + unit + " (" + PROFILE_ID + ")",
          flush=True)
    result = subprocess.run(docker(profile, image, mounts, command))
    if result.returncode:
        log = (directory / "compile.log").read_text(errors="replace") if (directory / "compile.log").exists() else ""
        raise ValueError("Wine compile failed ({}):\n{}".format(result.returncode, log[-2000:]))
    shutil.copyfile(directory / "out.asm", assembly)


def region_files(region, assembly, check_standins=True):
    """Files, unit name and flags for a region, as the native profiles would compile it."""
    if "context" in region:
        directory, unit = prepare_context(
            region, assembly, source_root=ROOT, check_standins=check_standins)
        files = {path.name: path.read_text() for path in directory.iterdir() if path.is_file()}
        return files, unit.name, list(context_flags(region))
    source = ROOT / region["source"]
    text = source.read_text()
    text = standin.insert_top(text, region.get("symbol_phase_top_standins", 0))
    text = standin.insert(text, function_span(text, region["name"])[0],
                         region.get("symbol_phase_standins", 0))
    if check_standins:
        found = standin.count_before(text, function_span(text, region["name"])[0])
        if found != region.get("symbol_phase_standins", 0):
            raise ValueError("{} has {} symbol-phase stand-ins, config records {}".format(
                region["source"], found, region.get("symbol_phase_standins", 0)))
        found = standin.count_top(text)
        if found != region.get("symbol_phase_top_standins", 0):
            raise ValueError("{} opens with {} symbol-phase stand-ins, config records {}".format(
                region["source"], found, region.get("symbol_phase_top_standins", 0)))
    files = {path.name: path.read_text() for path in source.parent.glob("*.h")}
    files[source.name] = text
    optimization = region.get("optimization", "size")
    flags = [] if optimization == "default" else ["--opt-code-" + optimization]
    flags += [*region.get("compiler_flags", []), "--nostdlib", "--no-std-crt0",
              "--codeseg", region["area"], "--constseg", region["area"]]
    return files, source.name, flags


def compile_c(region, assembly):
    require_docker()
    files, unit, flags = region_files(region, assembly)
    compile_files(files, unit, flags, assembly)
    if "context" in region:
        wrap_context(region, assembly)


if __name__ == "__main__":
    try:
        setup()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error))
