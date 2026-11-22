"""Tests for Docker runtime image verification and idempotent setup."""

import hashlib
import json
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import build_linux
import build_wine


def _sha256_bytes(data):
    return hashlib.sha256(data).hexdigest()


class DockerRuntimeVerificationTest(unittest.TestCase):
    def setUp(self):
        build_linux._VERIFIED_DOCKER_IMAGES.clear()

    def tearDown(self):
        build_linux._VERIFIED_DOCKER_IMAGES.clear()

    def test_linux_runtime_image_verifies_docker_and_caches_inspection(self):
        image = "sha256:" + "b" * 64
        profile = dict(build_linux.load_profile(), compiler_sha256=_sha256_bytes(b"sdcc"),
                       preprocessor_sha256=_sha256_bytes(b"sdcpp"))
        with tempfile.TemporaryDirectory() as temporary:
            fake_root = Path(temporary)
            marker = fake_root / ".tools/sdcc-3.3.0-linux/.gbos-linux-runtime.json"
            marker.parent.mkdir(parents=True)
            marker.write_text(json.dumps({
                "profile_sha256": build_linux.profile_fingerprint(profile),
                "image_id": image,
            }))

            inspect_ok = subprocess.CompletedProcess(
                ["docker"], 0, stdout=image + "\n", stderr=""
            )
            with mock.patch.object(build_linux, "ROOT", fake_root), \
                    mock.patch.object(build_linux.subprocess, "run", return_value=inspect_ok) as run_mock:
                self.assertEqual(build_linux.runtime_image(profile), image)
                self.assertEqual(build_linux.runtime_image(profile), image)
                self.assertEqual(run_mock.call_count, 1)

            build_linux._VERIFIED_DOCKER_IMAGES.clear()
            pruned = subprocess.CompletedProcess(
                ["docker"], 1, stdout="", stderr="Error: No such image: " + image
            )
            with mock.patch.object(build_linux, "ROOT", fake_root), \
                    mock.patch.object(build_linux.subprocess, "run", return_value=pruned):
                with self.assertRaisesRegex(build_linux.MissingRuntimeError, "missing from Docker"):
                    build_linux.runtime_image(profile)

            daemon_down = subprocess.CompletedProcess(
                ["docker"], 1, stdout="",
                stderr="Cannot connect to the Docker daemon at unix:///var/run/docker.sock."
            )
            with mock.patch.object(build_linux, "ROOT", fake_root), \
                    mock.patch.object(build_linux.subprocess, "run", return_value=daemon_down):
                with self.assertRaisesRegex(ValueError, "Docker daemon is unavailable") as ctx:
                    build_linux.runtime_image(profile)
                self.assertNotIsInstance(ctx.exception, build_linux.MissingRuntimeError)

    def test_linux_and_wine_setup_are_idempotent_when_image_is_present(self):
        image = "sha256:" + "c" * 64
        inspect_ok = subprocess.CompletedProcess(["docker"], 0, stdout=image + "\n", stderr="")

        with tempfile.TemporaryDirectory() as temporary:
            fake_root = Path(temporary)
            # Prepare valid Linux binaries + marker.
            linux_profile = dict(build_linux.load_profile(),
                                 compiler_sha256=_sha256_bytes(b"linux-sdcc"),
                                 preprocessor_sha256=_sha256_bytes(b"linux-sdcpp"))
            linux_bin = fake_root / ".tools/sdcc-3.3.0-linux/sdcc-3.3.0/bin"
            linux_bin.mkdir(parents=True)
            (linux_bin / "sdcc").write_bytes(b"linux-sdcc")
            (linux_bin / "sdcpp").write_bytes(b"linux-sdcpp")
            (fake_root / ".tools/sdcc-3.3.0-linux/.gbos-linux-runtime.json").write_text(json.dumps({
                "profile_sha256": build_linux.profile_fingerprint(linux_profile),
                "image_id": image,
            }))

            with mock.patch.object(build_linux, "ROOT", fake_root), \
                    mock.patch.object(build_linux, "require_docker"), \
                    mock.patch.object(build_linux, "load_profile", return_value=linux_profile), \
                    mock.patch.object(build_linux, "download",
                                      side_effect=AssertionError("download must not run")), \
                    mock.patch.object(build_linux.subprocess, "run", return_value=inspect_ok) as run_mock:
                build_linux.setup()
                # Only `docker image inspect` should have executed (no `docker build`).
                self.assertEqual(run_mock.call_count, 1)
                self.assertEqual(run_mock.call_args[0][0][:3], ["docker", "image", "inspect"])

            # Prepare valid Wine plain + emulated binaries + marker.
            build_linux._VERIFIED_DOCKER_IMAGES.clear()
            wine_profile = dict(build_wine.load_profile(),
                                compiler_sha256=_sha256_bytes(b"win-sdcc"),
                                preprocessor_sha256=_sha256_bytes(b"win-sdcpp"))
            plain = fake_root / ".tools/sdcc-3.3.0-win32"
            emulated = fake_root / ".tools/sdcc-3.3.0-win32-xpheap"
            (plain / "bin").mkdir(parents=True)
            (emulated / "bin").mkdir(parents=True)
            (plain / "bin/sdcc.exe").write_bytes(b"win-sdcc")
            (plain / "bin/sdcpp.exe").write_bytes(b"win-sdcpp")
            (emulated / "bin/sdcc.exe").write_bytes(b"win-sdcc-patched")
            (emulated / "bin/xpheap.dll").write_bytes(b"xpheap-dll")
            (emulated / "bin/sdcpp.exe").write_bytes(b"win-sdcpp")
            marker = emulated / ".gbos-xpheap.json"
            marker.write_text(json.dumps({
                "fingerprint": build_wine.fingerprint(wine_profile),
                "image_id": image,
                "files": {
                    "bin/sdcc.exe": _sha256_bytes(b"win-sdcc-patched"),
                    "bin/xpheap.dll": _sha256_bytes(b"xpheap-dll"),
                    "bin/sdcpp.exe": _sha256_bytes(b"win-sdcpp"),
                },
            }))

            with mock.patch.object(build_wine, "PLAIN", plain), \
                    mock.patch.object(build_wine, "EMULATED", emulated), \
                    mock.patch.object(build_wine, "MARKER", marker), \
                    mock.patch.object(build_wine, "require_docker"), \
                    mock.patch.object(build_wine, "load_profile", return_value=wine_profile), \
                    mock.patch.object(build_wine, "download",
                                      side_effect=AssertionError("download must not run")), \
                    mock.patch.object(subprocess, "run", return_value=inspect_ok) as run_mock:
                build_wine.setup()
                self.assertEqual(run_mock.call_count, 1)
                self.assertEqual(run_mock.call_args[0][0][:3], ["docker", "image", "inspect"])


if __name__ == "__main__":
    unittest.main()
