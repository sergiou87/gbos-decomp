"""Tests for Docker bind-mount CSV quoting and Wine shell command quoting."""

import csv
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import build_linux
import build_wine


class DockerQuotingTest(unittest.TestCase):
    def test_bind_mount_leaves_plain_paths_unquoted_and_csv_quotes_commas_and_quotes(self):
        self.assertEqual(
            build_linux.bind_mount("/opt/gbos/src", "/workspace/src", readonly=True),
            ["--mount", "type=bind,src=/opt/gbos/src,dst=/workspace/src,readonly"],
        )

        tricky_src = '/Users/dev/Work, Archive/"gbos"-decomp'
        mount = build_linux.bind_mount(tricky_src, "/work", readonly=True)
        self.assertEqual(mount[0], "--mount")
        parsed = next(csv.reader([mount[1]]))
        self.assertEqual(parsed, ["type=bind", "src=" + tricky_src, "dst=/work", "readonly"])

        with self.assertRaisesRegex(ValueError, "must not contain newlines"):
            build_linux.bind_mount("/tmp/bad\npath", "/work")

    def test_wine_compile_files_quotes_shell_arguments_and_mounts(self):
        profile = build_wine.load_profile()
        profile = dict(profile, xpheap=dict(profile["xpheap"], container_environment={
            "PATH": "/usr/bin:/bin",
            "WINEPATH": "C:\\a;echo INJECTED",
        }))
        captured = {}

        def fake_run(argv, **kwargs):
            captured["argv"] = argv
            unit_dir = Path(work_dir) / "unit"
            # Execute the inner wine invocation logic via /bin/sh with a stub wine script
            # to prove shell metacharacters in unit/flags/env cannot execute commands.
            (unit_dir / "out.asm").write_text("; ok\n")
            return subprocess.CompletedProcess(argv, 0)

        with tempfile.TemporaryDirectory(prefix="gbos,quote-") as temporary:
            work_dir = Path(temporary) / 'work"dir'
            compiler_dir = Path(temporary) / "sdcc,dir"
            compiler_dir.mkdir(parents=True)
            assembly = Path(temporary) / "result.s"
            unit_name = "odd;touch pwned.c"

            with mock.patch.object(build_wine, "load_profile", return_value=profile), \
                    mock.patch.object(build_wine, "image_id", return_value="sha256:" + "a" * 64), \
                    mock.patch.object(build_wine.subprocess, "run", side_effect=fake_run):
                build_wine.compile_files(
                    {unit_name: "void f(void) {}\n"},
                    unit_name,
                    ["--opt-code-size", "-DTITLE=hello world;uname"],
                    assembly,
                    compiler=compiler_dir,
                    work=work_dir,
                )

            argv = captured["argv"]
            shell_cmd = argv[-1]
            self.assertEqual(argv[-4:-1], ["/bin/sh", "sha256:" + "a" * 64, "-c"])
            # Parse-check with real /bin/sh -n and ensure metacharacters are single-quoted.
            subprocess.run(["/bin/sh", "-n", "-c", shell_cmd], check=True)
            self.assertIn("'odd;touch pwned.c'", shell_cmd)
            self.assertIn("'-DTITLE=hello world;uname'", shell_cmd)
            self.assertIn("'WINEPATH=C:\\a;echo INJECTED'", shell_cmd)

            mount_specs = [argv[i + 1] for i, token in enumerate(argv[:-1]) if token == "--mount"]
            self.assertEqual(len(mount_specs), 2)
            self.assertIn("src=" + str(work_dir.resolve()), next(csv.reader([mount_specs[0]])))
            self.assertIn("src=" + str(compiler_dir), next(csv.reader([mount_specs[1]])))

    def test_wine_compile_files_rejects_missing_unit_or_invalid_env_name(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError, "Compile unit is missing"):
                build_wine.compile_files({"a.c": ""}, "b.c", [], Path(temporary) / "out.s")

        bad_profile = build_wine.load_profile()
        bad_profile = dict(bad_profile, xpheap=dict(bad_profile["xpheap"], container_environment={
            "BAD;KEY": "1",
        }))
        with tempfile.TemporaryDirectory() as temporary, \
                mock.patch.object(build_wine, "load_profile", return_value=bad_profile), \
                mock.patch.object(build_wine, "image_id", return_value="sha256:" + "a" * 64):
            with self.assertRaisesRegex(ValueError, "Invalid environment variable name"):
                build_wine.compile_files(
                    {"a.c": ""}, "a.c", [], Path(temporary) / "out.s", work=Path(temporary) / "w"
                )


if __name__ == "__main__":
    unittest.main()
