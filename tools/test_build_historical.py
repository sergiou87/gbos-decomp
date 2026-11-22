"""Tests for clean rebuild and partial-tree recovery in build_historical.py."""

import hashlib
import io
import json
import subprocess
import tarfile
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import build_historical


def _write_tar(path, compression, files):
    mode = "w:" + compression if compression else "w"
    with tarfile.open(path, mode) as archive:
        for name, content in files.items():
            raw = content.encode()
            info = tarfile.TarInfo(name=name)
            info.size = len(raw)
            info.mode = 0o755 if name.endswith(("configure", "sdcc", "sdcpp")) else 0o644
            archive.addfile(info, io.BytesIO(raw))
    return hashlib.sha256(path.read_bytes()).hexdigest()


class BuildHistoricalCleanRebuildTest(unittest.TestCase):
    def test_rebuild_removes_stale_source_objects_and_partial_boost_tree(self):
        with tempfile.TemporaryDirectory() as temporary:
            fake_root = Path(temporary)
            (fake_root / "config").mkdir()
            tools = fake_root / ".tools"
            tools.mkdir()

            sdcc_tar = tools / "sdcc-r8604.tar.gz"
            sdcc_sha = _write_tar(sdcc_tar, "gz", {
                "tags/sdcc/src/SDCCtree_dec.hpp": "#include <boost/graph/adjacency_list.hpp>\nboost::copy_graph(G, J);\nboost::copy_graph(G, G_sym);\n",
                "tags/sdcc/src/SDCCralloc.hpp": "boost::copy_graph(cfg, cfg2);\n",
                "tags/sdcc/support/cpp/placeholder.txt": "cpp\n",
            })
            boost_tar = tools / "boost_1_55_0.tar.bz2"
            boost_sha = _write_tar(boost_tar, "bz2", {
                "boost_1_55_0/boost/graph/adjacency_list.hpp": "// boost\n",
                "boost_1_55_0/boost/version.hpp": "// complete\n",
            })

            (fake_root / "config/compiler-candidates.json").write_text(json.dumps({
                "candidates": [{
                    "id": "r8604",
                    "url": "https://example.invalid/sdcc.tar.gz",
                    "sha256": sdcc_sha,
                    "build_dependencies": {
                        "boost": {
                            "version": "1.55.0",
                            "url": "https://example.invalid/boost.tar.bz2",
                            "sha256": boost_sha,
                        }
                    },
                }]
            }))

            # Pre-populate stale build artifacts and a corrupt build stamp in sdcc-r8604-src.
            stale_src = tools / "sdcc-r8604-src"
            (stale_src / "src").mkdir(parents=True)
            stale_obj = stale_src / "src/stale.o"
            stale_obj.write_text("stale object")
            (stale_src / ".gbos-build.json").write_text("{truncated")

            # Pre-populate an interrupted Boost extraction that had adjacency_list.hpp
            # and a leftover stray file, but lacked version.hpp and .gbos-boost.json.
            partial_boost = tools / "boost_1_55_0"
            (partial_boost / "boost/graph").mkdir(parents=True)
            (partial_boost / "boost/graph/adjacency_list.hpp").write_text("// partial\n")
            stray_boost = partial_boost / "stray.tmp"
            stray_boost.write_text("stray")

            def fake_run(cmd, **kwargs):
                # Assert stale object was wiped before ./configure or make runs.
                self.assertFalse((stale_src / "src/stale.o").exists())
                if cmd[0] == "make" and kwargs.get("cwd") == stale_src:
                    (stale_src / "bin").mkdir(exist_ok=True)
                    (stale_src / "bin/sdcc").write_text("#!/bin/sh\n")
                elif cmd[0] == "make" and cmd[-1] == "-j4" and kwargs.get("cwd") == stale_src / "support/cpp":
                    (stale_src / "bin").mkdir(exist_ok=True)
                    (stale_src / "bin/sdcpp").write_text("#!/bin/sh\n")
                return subprocess.CompletedProcess(cmd, 0)

            with mock.patch.object(build_historical, "ROOT", fake_root), \
                    mock.patch.object(build_historical.sys, "platform", "darwin"), \
                    mock.patch.object(build_historical.sys, "argv", ["build_historical.py", "r8604"]), \
                    mock.patch.object(build_historical.subprocess, "run", side_effect=fake_run):
                build_historical.main()

            self.assertFalse(stale_obj.exists())
            self.assertFalse(stray_boost.exists())
            self.assertTrue((partial_boost / "boost/version.hpp").is_file())
            self.assertTrue((partial_boost / ".gbos-boost.json").is_file())
            self.assertTrue((stale_src / ".gbos-build.json").is_file())

            # Second run with valid stamp and binaries must be a no-op.
            with mock.patch.object(build_historical, "ROOT", fake_root), \
                    mock.patch.object(build_historical.sys, "platform", "darwin"), \
                    mock.patch.object(build_historical.sys, "argv", ["build_historical.py", "r8604"]), \
                    mock.patch.object(build_historical.subprocess, "run",
                                      side_effect=AssertionError("rebuild should not run")):
                build_historical.main()


if __name__ == "__main__":
    unittest.main()
