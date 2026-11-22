#!/usr/bin/env python3
"""Build the pinned native SDCC 3.3.0 compiler and preprocessor."""

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tarfile
from pathlib import Path, PurePosixPath

from rom import ROOT


def download(url, path, expected):
    if not path.exists():
        temporary = path.with_suffix(".download")
        subprocess.run(["curl", "--fail", "--location", "--silent", "--show-error",
                        "--proto", "=https", "--proto-redir", "=https",
                        "-o", str(temporary), url], check=True)
        if hashlib.sha256(temporary.read_bytes()).hexdigest() != expected:
            raise ValueError("Downloaded archive checksum mismatch: " + str(temporary))
        temporary.replace(path)
    if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
        raise ValueError("Cached archive checksum mismatch: " + str(path))


def extract(archive, destination, subtree):
    destination.mkdir(exist_ok=True)
    with tarfile.open(archive) as package:
        for member in package.getmembers():
            parts = PurePosixPath(member.name).parts
            if not parts or ".." in parts or PurePosixPath(member.name).is_absolute():
                raise ValueError("Unsafe archive path: " + member.name)
            if subtree:
                if len(parts) < 2 or parts[1] != subtree:
                    continue
                parts = parts[2:]
            else:
                parts = parts[1:]
            if not parts:
                continue
            path = destination.joinpath(*parts)
            if member.isdir():
                path.mkdir(parents=True, exist_ok=True)
            elif member.isfile():
                path.parent.mkdir(parents=True, exist_ok=True)
                with package.extractfile(member) as source, path.open("wb") as output:
                    output.write(source.read())
                path.chmod(member.mode & 0o777)
            else:
                raise ValueError("Unsupported archive member: " + member.name)


def boost_compatibility(source):
    header = source / "src/SDCCtree_dec.hpp"
    original = header.read_text()
    if "struct forget_properties" in original:
        return  # This snapshot already has the upstream compatibility fix.
    if "struct discard_graph_properties" not in original:
        anchor = "#include <boost/graph/adjacency_list.hpp>"
        if original.count(anchor) != 1:
            raise ValueError("Unrecognized graph header; refusing speculative patch")
        original = original.replace(anchor, anchor + """

struct discard_graph_properties
{
  template <class Source, class Destination>
  void operator()(const Source &, const Destination &) const {}
};
""")
        header.write_text(original)
    for filename, calls in (
        ("SDCCtree_dec.hpp", ("G, J", "G, G_sym")),
        ("SDCCralloc.hpp", ("cfg, cfg2",)),
    ):
        path = source / "src" / filename
        text = path.read_text()
        for call in calls:
            text = text.replace(
                "boost::copy_graph({});".format(call),
                "boost::copy_graph({}, boost::vertex_copy(discard_graph_properties())"
                ".edge_copy(discard_graph_properties()));".format(call))
        path.write_text(text)


def valid_stamp(marker, expected):
    if not marker.is_file():
        return False
    try:
        return json.loads(marker.read_text()) == expected
    except ValueError:
        return False


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    candidates = json.loads((ROOT / "config/compiler-candidates.json").read_text())["candidates"]
    parser.add_argument("candidate", choices=[item["id"] for item in candidates if "id" in item])
    args = parser.parse_args()
    if sys.platform != "darwin":
        raise ValueError("The pinned native compiler profile requires macOS")
    config = next(item for item in candidates if item.get("id") == args.candidate)
    tools = ROOT / ".tools"
    tools.mkdir(exist_ok=True)
    archive = tools / ("sdcc-" + args.candidate + ".tar.gz")
    download(config["url"], archive, config["sha256"])
    source = tools / ("sdcc-" + args.candidate + "-src")
    boost = config["build_dependencies"]["boost"]
    boost_archive = tools / ("boost_" + boost["version"].replace(".", "_") + ".tar.bz2")
    download(boost["url"], boost_archive, boost["sha256"])
    stamp = {"source_sha256": config["sha256"], "boost_sha256": boost["sha256"],
             "host_compatibility_patch": 1, "native_preprocessor": True, "locale": "C"}
    marker = source / ".gbos-build.json"
    if (valid_stamp(marker, stamp)
            and all((source / "bin" / name).is_file() for name in ("sdcc", "sdcpp"))):
        print("Historical compiler ready: " + args.candidate)
        return
    # Remove any partial or stale build artifacts before re-extracting pinned source.
    if source.exists():
        shutil.rmtree(source)
    extract(archive, source, "sdcc")
    boost_source = tools / ("boost_" + boost["version"].replace(".", "_"))
    boost_stamp = {"sha256": boost["sha256"]}
    boost_marker = boost_source / ".gbos-boost.json"
    if (not valid_stamp(boost_marker, boost_stamp)
            or not (boost_source / "boost/graph/adjacency_list.hpp").is_file()):
        if boost_source.exists():
            shutil.rmtree(boost_source)
        extract(boost_archive, boost_source, None)
        boost_marker.write_text(json.dumps(boost_stamp, indent=2) + "\n")
    boost_compatibility(source)
    env = os.environ.copy()
    env.update(LC_ALL="C", CPPFLAGS="-I" + str(boost_source), CXXFLAGS="-std=c++11 -O2",
               CFLAGS="-O2 -Wno-implicit-function-declaration -Wno-int-conversion")
    configure = ["./configure"] + [
        "--disable-" + feature for feature in (
            "mcs51-port", "z80-port", "z180-port", "r2k-port", "r3ka-port",
            "ds390-port", "ds400-port", "pic14-port", "pic16-port", "hc08-port",
            "s08-port", "stm8-port", "ucsim", "device-lib", "packihx", "sdcpp",
            "sdcdb", "sdbinutils",
        )
    ]
    for command, name in ((configure, "configure-gbos.log"),
                          (["make", "-C", "src", "-j4"], "build-gbos.log")):
        log = source / name
        print("Running {}; log: {}".format(command[0], log), flush=True)
        with log.open("w") as output:
            result = subprocess.run(command, cwd=source, env=env, stdout=output,
                                    stderr=subprocess.STDOUT)
        if result.returncode:
            raise ValueError("{} failed; read {}".format(command[0], log))
    cpp = source / "support/cpp"
    # C collation is essential: the preprocessor binary-searches generated options.
    for command, name in ((["./configure"], "configure-gbos.log"),
                          (["make", "clean"], "clean-gbos.log"),
                          (["make", "-j4"], "build-gbos.log")):
        log = cpp / name
        with log.open("w") as output:
            result = subprocess.run(command, cwd=cpp, env=env, stdout=output,
                                    stderr=subprocess.STDOUT)
        if result.returncode:
            raise ValueError("Native preprocessor build failed; read " + str(log))
    subprocess.run([str(source / "bin/sdcc"), "--version"], check=True)
    if not (source / "bin/sdcpp").is_file():
        raise ValueError("Native preprocessor binary missing after build")
    marker.write_text(json.dumps(stamp, indent=2) + "\n")
    print("Pinned historical compiler built locally. System tools unchanged.")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error))
