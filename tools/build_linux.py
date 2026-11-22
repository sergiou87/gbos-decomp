#!/usr/bin/env python3
"""Prepare and use the pinned original Linux SDCC in an isolated local runtime."""

import hashlib
import json
import re
import shutil
import subprocess
import tempfile
from pathlib import Path, PurePosixPath

import standin
from build_historical import download, extract
from context_compile import WINE_PROFILE_ID, context_flags, prepare_context, wrap_context
from rom import ROOT


PROFILE_ID = "3.3.0-linux-x86"
PERIOD_RUNTIME = "glibc213"
PERIOD_ROOT = "/opt/gbos-glibc213"
RUNTIME_SOURCE_ROOT = PurePosixPath(".tools/sdcc-r8604-src/device")
_VERIFIED_DOCKER_IMAGES = set()


class MissingRuntimeError(ValueError):
    """Raised when a pinned compiler binary, marker, or Docker image needs setup."""


def load_profile(runtime=None):
    releases = json.loads((ROOT / "config/compiler-candidates.json").read_text())["binary_releases"]
    profile = next(release for release in releases if release["id"] == PROFILE_ID)
    if runtime is None:
        return profile
    if runtime != PERIOD_RUNTIME or runtime not in profile.get("runtime_variants", {}):
        raise ValueError("Unknown pinned Linux compiler runtime: " + str(runtime))
    variant = profile["runtime_variants"][runtime]
    return dict(profile, runtime_id=runtime, runtime_archive=variant["archive"],
                runtime_environment=dict(profile["runtime_environment"],
                                         **variant["environment"]))


def compiler_directory():
    return ROOT / ".tools/sdcc-3.3.0-linux/sdcc-3.3.0"


def verify_binaries(profile, directory=None):
    directory = directory if directory is not None else compiler_directory()
    for name, key in (("sdcc", "compiler_sha256"), ("sdcpp", "preprocessor_sha256")):
        path = directory / "bin" / name
        if not path.is_file():
            raise MissingRuntimeError("Missing pinned Linux {}. Run make setup.".format(name))
        if hashlib.sha256(path.read_bytes()).hexdigest() != profile[key]:
            raise MissingRuntimeError("Pinned Linux compiler file changed: " + str(path))


def ensure_docker_image(image, label):
    if not isinstance(image, str) or not re.fullmatch(r"sha256:[0-9a-f]{64}", image):
        raise MissingRuntimeError(
            "Invalid pinned {} runtime image identity. Run make setup.".format(label))
    if image in _VERIFIED_DOCKER_IMAGES:
        return image
    result = subprocess.run(["docker", "image", "inspect", image, "--format", "{{.Id}}"],
                            capture_output=True, text=True)
    if result.returncode == 0 and result.stdout.strip() == image:
        _VERIFIED_DOCKER_IMAGES.add(image)
        return image
    stderr = (result.stderr or "").strip()
    lower = stderr.lower()
    if stderr and "no such image" not in lower and "not found" not in lower and (
            "cannot connect" in lower or "daemon" in lower or "permission denied" in lower):
        raise ValueError(
            "Docker daemon is unavailable ({}). Start Docker, then retry.".format(stderr))
    raise MissingRuntimeError(
        "Pinned {} runtime image {} is missing from Docker. Run make setup.".format(label, image))


def runtime_recipe(profile):
    if not re.fullmatch(r"[\w/.-]+@sha256:[0-9a-f]{64}", profile["runtime_image"]):
        raise ValueError("Linux runtime must use an image digest")
    packages = []
    for package, version in sorted(profile["runtime_packages"].items()):
        if (not re.fullmatch(r"[a-z0-9+.-]+", package)
                or not re.fullmatch(r"[A-Za-z0-9.+:~_-]+", version)):
            raise ValueError("Invalid pinned Linux runtime package")
        packages.append(package + "=" + version)
    if not packages:
        raise ValueError("Linux runtime packages must be pinned")
    recipe = ("FROM " + profile["runtime_image"] + "\n"
              "RUN apt-get update -qq"
              " && apt-get install -y --no-install-recommends " + " ".join(packages)
              + " && apt-get clean\n")
    if "runtime_archive" in profile:
        archive = profile["runtime_archive"]
        if (not re.fullmatch(r"[A-Za-z0-9+._-]+[.]deb", archive["filename"])
                or not re.fullmatch(r"[0-9a-f]{64}", archive["sha256"])):
            raise ValueError("Invalid pinned Linux runtime archive")
        package = PERIOD_ROOT + "/package/" + archive["filename"]
        recipe += ("COPY " + archive["filename"] + " " + package + "\n"
                   "RUN dpkg-deb --extract " + package + " " + PERIOD_ROOT
                   + " && test -f " + PERIOD_ROOT + "/lib32/ld-linux.so.2"
                   + " && ln -sfn " + PERIOD_ROOT
                   + "/lib32/ld-linux.so.2 /lib/ld-linux.so.2\n")
    return recipe


def profile_fingerprint(profile):
    inputs = {
        "recipe": runtime_recipe(profile),
        "platform": profile["runtime_platform"],
        "archive": profile["sha256"],
        "compiler": profile["compiler_sha256"],
        "preprocessor": profile["preprocessor_sha256"],
        "environment": profile["runtime_environment"],
        "layout_version": 1,
    }
    if "runtime_archive" in profile:
        inputs["runtime_archive"] = profile["runtime_archive"]["sha256"]
    return hashlib.sha256(json.dumps(inputs, sort_keys=True).encode()).hexdigest()


def runtime_marker(profile=None):
    suffix = "-" + profile["runtime_id"] if profile and "runtime_id" in profile else ""
    return compiler_directory().parent / (".gbos-linux-runtime" + suffix + ".json")


def runtime_image(profile, check_docker=True):
    marker = runtime_marker(profile)
    if not marker.is_file():
        raise MissingRuntimeError("Missing pinned Linux runtime. Run make setup.")
    try:
        metadata = json.loads(marker.read_text())
    except json.JSONDecodeError as error:
        raise MissingRuntimeError("Invalid pinned Linux runtime marker. Run make setup.") from error
    if not isinstance(metadata, dict) or metadata.get("profile_sha256") != profile_fingerprint(profile):
        raise MissingRuntimeError("Pinned Linux runtime configuration changed. Run make setup.")
    image = metadata.get("image_id", "")
    if not isinstance(image, str) or not re.fullmatch(r"sha256:[0-9a-f]{64}", image):
        raise MissingRuntimeError("Invalid pinned Linux runtime image identity. Run make setup.")
    if check_docker:
        ensure_docker_image(image, "Linux")
    return image


def _csv_mount_field(field):
    if "\n" in field or "\r" in field:
        raise ValueError("Docker mount option must not contain newlines")
    if "," in field or '"' in field:
        return '"' + field.replace('"', '""') + '"'
    return field


def bind_mount(source, destination, readonly=False):
    fields = ["type=bind", _csv_mount_field("src=" + str(source)),
              _csv_mount_field("dst=" + str(destination))]
    if readonly:
        fields.append("readonly")
    return ["--mount", ",".join(fields)]


def docker_prefix(profile, mounts):
    command = [
        "docker", "run", "--rm", "--pull", "never",
        "--platform", profile["runtime_platform"], "--network", "none",
        "--read-only", "--tmpfs", "/tmp:size=64m", "--cap-drop", "ALL",
        "--security-opt", "no-new-privileges",
        *bind_mount(compiler_directory(), "/opt/sdcc", readonly=True),
        *mounts, "--workdir", "/workspace", "--env", "PATH=/opt/sdcc/bin:/usr/bin:/bin",
    ]
    for key, value in sorted(profile["runtime_environment"].items()):
        command.extend(["--env", key + "=" + value])
    return command


def compile_command(profile, image, region, assembly):
    source = PurePosixPath(region["source"])
    if not source.parts or source.is_absolute() or ".." in source.parts:
        raise ValueError("Invalid Linux C source path")
    if source.parts[0] == "src":
        source_root = PurePosixPath("src")
    elif (region.get("category") == "runtime" and source.suffix == ".c"
          and source.is_relative_to(RUNTIME_SOURCE_ROOT / "lib")):
        source_root = RUNTIME_SOURCE_ROOT
    else:
        raise ValueError("Linux C source must be inside src/ or the pinned runtime library")
    source_path = ROOT / source
    if not source_path.resolve().is_relative_to((ROOT / source_root).resolve()):
        raise ValueError("Linux C source escapes its declared source root")
    if source_root == RUNTIME_SOURCE_ROOT:
        expected = region.get("source_sha256")
        if not isinstance(expected, str) or not re.fullmatch(r"[0-9a-f]{64}", expected):
            raise ValueError("Linux runtime C source requires a SHA-256 pin")
        if hashlib.sha256(source_path.read_bytes()).hexdigest() != expected:
            raise ValueError("Pinned Linux runtime source changed: " + str(source))
    if standin.has_any(source_path.read_text()):
        raise ValueError("Symbol-phase stand-ins require profile " + WINE_PROFILE_ID)
    if not assembly.resolve().is_relative_to((ROOT / "build").resolve()):
        raise ValueError("Linux compiler output must be inside build/")
    optimization = region.get("optimization", "size")
    if optimization not in ("default", "size", "speed"):
        raise ValueError("Unknown Linux compiler optimization profile")
    flags = [] if optimization == "default" else ["--opt-code-" + optimization]
    command = docker_prefix(profile, [
        *bind_mount(ROOT / source_root, "/workspace/" + str(source_root), readonly=True),
        *bind_mount(assembly.parent.resolve(), "/output"),
    ])
    return command + [
        "--entrypoint", "/opt/sdcc/bin/sdcc", image,
        "-mgbz80", *flags, *region.get("compiler_flags", []),
        "--nostdlib", "--no-std-crt0",
        "--codeseg", region["area"], "--constseg", region["area"],
        "-S", source.as_posix(), "-o", "/output/" + assembly.name,
    ]


def context_compile_command(profile, image, region, directory, unit, assembly):
    """Compile a composed context unit as its own read-only working directory."""
    build = (ROOT / "build").resolve()
    for path in (directory, assembly):
        if not path.resolve().is_relative_to(build):
            raise ValueError("Linux context compilation must stay inside build/")
    if unit.parent != directory:
        raise ValueError("Linux context unit must be inside its context directory")
    command = docker_prefix(profile, [
        *bind_mount(directory.resolve(), "/workspace", readonly=True),
        *bind_mount(assembly.parent.resolve(), "/output"),
    ])
    return command + [
        "--entrypoint", "/opt/sdcc/bin/sdcc", image,
        "-mgbz80", *context_flags(region),
        "-S", unit.name, "-o", "/output/" + assembly.name,
    ]


def require_docker():
    if shutil.which("docker") is None:
        raise ValueError("Docker is required for the pinned Linux compiler. "
                         "Install/start Docker, then run make setup.")


def compile_c(region, assembly, runner=None):
    require_docker()
    profile = load_profile(region.get("compiler_runtime"))
    verify_binaries(profile)
    image = runtime_image(profile, check_docker=(runner is None))
    if "context" in region:
        directory, unit = prepare_context(region, assembly)
        command = context_compile_command(profile, image, region, directory, unit, assembly)
    else:
        command = compile_command(profile, image, region, assembly)
    if runner is None:
        print("+ " + " ".join(command), flush=True)
        subprocess.run(command, cwd=ROOT, check=True)
    else:
        runner(command)
    if "context" in region:
        wrap_context(region, assembly)


def setup(runtime=None):
    require_docker()
    profile = load_profile(runtime)
    fingerprint = profile_fingerprint(profile)
    tag = "gbos-sdcc330-linux:" + fingerprint[:16]
    try:
        verify_binaries(profile)
        runtime_image(profile, check_docker=True)
        print("Pinned Linux compiler ready: " + tag)
        return
    except MissingRuntimeError:
        pass
    tools = ROOT / ".tools"
    tools.mkdir(exist_ok=True)
    destination = compiler_directory().parent
    try:
        verify_binaries(profile)
    except MissingRuntimeError:
        archive = tools / "sdcc-3.3.0-linux.tar.bz2"
        download(profile["url"], archive, profile["sha256"])
        with tempfile.TemporaryDirectory(dir=tools) as temporary:
            staged = Path(temporary) / "install"
            staged.mkdir()
            extracted = staged / "sdcc-3.3.0"
            extract(archive, extracted, None)
            verify_binaries(profile, extracted)
            if destination.exists():
                shutil.rmtree(destination)
            staged.replace(destination)
    verify_binaries(profile)
    recipe = runtime_recipe(profile)
    command = ["docker", "build", "--platform", profile["runtime_platform"], "--tag", tag]
    if "runtime_archive" in profile:
        dependency = profile["runtime_archive"]
        archive = tools / dependency["filename"]
        download(dependency["url"], archive, dependency["sha256"])
        with tempfile.TemporaryDirectory(dir=tools) as temporary:
            context = Path(temporary)
            (context / "Dockerfile").write_text(recipe)
            shutil.copyfile(archive, context / archive.name)
            subprocess.run(command + [str(context)], cwd=ROOT, check=True)
    else:
        subprocess.run(command + ["-"], input=recipe, cwd=ROOT, text=True, check=True)
    result = subprocess.run(["docker", "image", "inspect", tag, "--format", "{{.Id}}"],
                            capture_output=True, text=True, check=True)
    image = result.stdout.strip()
    if not re.fullmatch(r"sha256:[0-9a-f]{64}", image):
        raise ValueError("Docker returned an invalid Linux runtime image identity")
    _VERIFIED_DOCKER_IMAGES.add(image)
    runtime_marker(profile).write_text(json.dumps({
        "profile_sha256": fingerprint, "image_id": image,
    }, indent=2) + "\n")
    print("Pinned Linux compiler ready: " + tag)


if __name__ == "__main__":
    try:
        setup()
        setup(PERIOD_RUNTIME)
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error))
