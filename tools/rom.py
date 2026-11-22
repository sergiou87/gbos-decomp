#!/usr/bin/env python3
"""Validate complete source ownership and the resulting ROM's pinned identity."""

import argparse
import hashlib
import json
import re
from pathlib import Path

import standin
from context_compile import WINE_PROFILE_ID, validate_context

ROOT = Path(__file__).resolve().parents[1]


def number(value):
    return int(value, 0) if isinstance(value, str) else int(value)


def load_config():
    config = json.loads((ROOT / "config/rom.json").read_text())
    if "reference" in config or config.get("inexact"):
        raise ValueError("Production builds cannot use reference bytes or inexact sources")
    if (type(config["size"]) is not int or config["size"] <= 0
            or not re.fullmatch(r"[0-9a-f]{64}", config["sha256"])):
        raise ValueError("Invalid ROM identity")
    if Path(config["output"]).name != config["output"]:
        raise ValueError("ROM output must be a file name")
    config["regions"] = validate_regions(config["regions"], config["size"])
    return config


def validate_regions(regions, size):
    result = []
    names, c_sources, areas = set(), set(), set()
    for original in regions:
        region = dict(original)
        region["start"], region["end"] = number(region["start"]), number(region["end"])
        if not 0 <= region["start"] < region["end"] <= size:
            raise ValueError("Invalid bounds for region " + region["name"])
        if region["kind"] not in ("c", "asm"):
            raise ValueError("Unknown region kind: " + region["kind"])
        if not isinstance(region["name"], str) or not region["name"] or region["name"] in names:
            raise ValueError("Duplicate or invalid region name")
        names.add(region["name"])
        source = Path(region["source"])
        if source.is_absolute() or ".." in source.parts:
            raise ValueError("Source must be a project-relative path")
        if region.get("category") not in (None, "data", "runtime"):
            raise ValueError("Unknown source category")
        if "composition" in region or "slot_order" in region:
            raise ValueError("Research-only compilation is not supported")
        if "subregions" in region:
            if not isinstance(region["subregions"], list) or region.get("category") is not None:
                raise ValueError("Invalid subregions")
            children = []
            for original_child in region["subregions"]:
                child = dict(original_child)
                if (set(child) != {"name", "start", "end", "category"}
                        or child["category"] != "data"):
                    raise ValueError("Subregions only classify data, not link ownership")
                child["start"], child["end"] = number(child["start"]), number(child["end"])
                if not region["start"] <= child["start"] < child["end"] <= region["end"]:
                    raise ValueError("Invalid subregion bounds")
                if not isinstance(child["name"], str) or not child["name"] or child["name"] in names:
                    raise ValueError("Duplicate or invalid subregion name")
                names.add(child["name"])
                children.append(child)
            children.sort(key=lambda child: child["start"])
            for previous, current in zip(children, children[1:]):
                if previous["end"] > current["start"]:
                    raise ValueError("Overlapping subregions")
            region["subregions"] = children
        if "context" in region:
            validate_context(region)
        for key in ("symbol_phase_standins", "symbol_phase_top_standins"):
            if key not in region:
                continue
            count = region[key]
            if (region["kind"] != "c" or region.get("compiler") != WINE_PROFILE_ID
                    or type(count) is not int or not 1 <= count <= standin.MAXIMUM):
                raise ValueError("Invalid symbol-phase stand-in profile/count")
        if "symbol_phase_top_standins" in region and "context" in region:
            raise ValueError("Top-of-unit stand-ins require a plain region")
        if region.get("category") == "runtime" and "source_sha256" not in region:
            raise ValueError("Upstream runtime requires a pinned source hash")
        if "source_sha256" in region and not re.fullmatch(r"[0-9a-f]{64}", region["source_sha256"]):
            raise ValueError("Invalid source SHA-256")
        if "compiler_flags" in region:
            flags = region["compiler_flags"]
            if (region["kind"] != "c" or not isinstance(flags, list)
                    or not all(isinstance(flag, str) and flag for flag in flags)):
                raise ValueError("Invalid C compiler flags")
        if region["kind"] == "c":
            if region.get("optimization", "size") not in ("default", "size", "speed"):
                raise ValueError("Unknown optimization profile")
            if region["source"] in c_sources or "area" not in region:
                raise ValueError("Each C translation unit needs its own region and link area")
            if region.get("compiler", "gbdk") not in ("gbdk", "r8604", "3.3.0-linux-x86",
                                                       WINE_PROFILE_ID):
                raise ValueError("Unknown production compiler")
            c_sources.add(region["source"])
        if "area" in region:
            if region["area"] in areas:
                raise ValueError("Each link area must have a single fixed base")
            areas.add(region["area"])
        result.append(region)
    result.sort(key=lambda region: region["start"])
    position = 0
    for region in result:
        if region["start"] != position:
            raise ValueError("Source gap or overlap at 0x{:04x}".format(position))
        position = region["end"]
    if position != size:
        raise ValueError("Source regions do not cover the complete ROM")
    return result


def verify_binary(data, config):
    actual = hashlib.sha256(data).hexdigest()
    if len(data) != config["size"] or actual != config["sha256"]:
        raise ValueError(
            "ROM identity mismatch.\nExpected: {} ({} bytes)\nActual:   {} ({} bytes)"
            .format(config["sha256"], config["size"], actual, len(data)))
    return actual


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", nargs="?", type=Path)
    args = parser.parse_args()
    config = load_config()
    path = args.path if args.path is not None else ROOT / "build" / config["output"]
    digest = verify_binary(path.read_bytes(), config)
    print("MATCH: {} bytes, SHA-256 {}".format(config["size"], digest))


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError) as error:
        raise SystemExit(str(error))
