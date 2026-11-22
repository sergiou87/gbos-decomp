# GBOS decompilation

A complete, byte-for-byte matching decompilation of the **GB PRO+ OS V4**
Game Boy clone flash-cartridge operating system. This is cartridge firmware and
its menu, not a Game Boy game.

| Property | Value |
| --- | --- |
| Build output | `build/GBOS.GB` |
| Original dump name | `GBOS.GB` |
| Size | 32,768 bytes (32 KiB) |
| SHA-256 | `a4f92e994d86add9cc075aec73055aaa5419bea1233f6026dee299b305e466ae` |
| Source-backed coverage | 32,768 / 32,768 bytes (100%) |

All code, data, headers, and padding rebuild from recovered C, assembly, and
identified SDCC runtime sources. No original ROM, reference-byte fallback,
research scripts, or generated disassembly is required. `GBOS.GB` has exactly
the same bytes and filename as the original dump.
Byte identity is checked before any successful build output is written.

The ROM provides file browsing, game loading, cheat editing, cartridge settings,
device information, and save-RAM loading/backup.

## Build

See [BUILDING.md](BUILDING.md) for prerequisites and reproducibility details.

```sh
make setup
make verify
```

`src/` contains production C and fixed-address ASxxxx assembly. Settings fields
name their mapper, ROM-size, and RAM-size roles; shared headers describe SD
commands, storage geometry, and FAT directory fields. `config/` records layout, compiler profiles,
and dependency hashes. `tools/` contains only setup/build/verification support.
Display initialization names LCD/palette registers and detects CGB capabilities
before OS initialization switches CPU speed.
Pinned historical compilation templates live in `tools/context/`, separate from
application sources. Toolchains and build intermediates are downloaded/generated
locally and ignored by Git.
