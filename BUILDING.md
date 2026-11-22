# Building the byte-exact ROM

## Supported build host

The production build requires **macOS** and a working Docker daemon capable of
running **Linux/amd64 containers and 32-bit x86 executables**. The complete build
has been verified on macOS ARM64. Native Linux/Windows builds are not supported:
the native historical compiler's deterministic allocator uses macOS interposing.

Install:

- Python 3.9 or newer.
- Make, curl, and a host C/C++ compiler (Xcode Command Line Tools).
- flex and bison.
- Docker Desktop, or Docker CLI plus Colima/QEMU.

With Homebrew and existing Command Line Tools:

```sh
brew install flex bison
export PATH="$(brew --prefix flex)/bin:$(brew --prefix bison)/bin:$PATH"
export SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"
```

Start Docker Desktop before setup. Alternatively:

```sh
brew install colima docker docker-buildx qemu lima-additional-guestagents
mkdir -p "$HOME/.docker/cli-plugins"
ln -sfn "$(brew --prefix docker-buildx)/bin/docker-buildx" \
  "$HOME/.docker/cli-plugins/docker-buildx"
docker buildx version
colima start --vm-type qemu --arch x86_64 --cpu 2 --memory 4 --disk 20
docker info
```

`lima-additional-guestagents` supplies the Linux x86_64 guest agent needed when
running an x86_64 Colima VM on an ARM64 host.

The x86_64 QEMU VM can be slow on ARM64. GitHub Actions uses this mode because
ARM64 hosted runners do not support nested hardware virtualization. Allow several
GiB for downloaded toolchains, container images, and build intermediates.

## Setup and build

From the repository root:

```sh
make setup
make verify
```

`make setup` downloads checksum-pinned dependencies into `.tools/`, builds the
native historical compiler and host allocator, and prepares local Docker
runtimes. It needs network access; it installs no system-wide packages.
Dependencies are pinned in `toolchain.lock.json` and
`config/compiler-candidates.json`.

Normal builds need no network after setup, but Docker must remain running.
Each Wine compile uses a fresh prefix copied from the pinned image into a 1 GiB
container tmpfs. This keeps Wine's file ownership and permissions on native Linux
storage rather than Colima's host-mounted filesystem.
`make` and `make build` also verify the complete image before writing
`build/GBOS.GB`; `make verify` additionally runs the standalone verifier.
Expected output:

```text
MATCH: 32768 bytes, SHA-256 a4f92e994d86add9cc075aec73055aaa5419bea1233f6026dee299b305e466ae
```

Verify an existing output without rebuilding:

```sh
python3 tools/rom.py build/GBOS.GB
```

No original ROM is read, downloaded, or needed. Missing/overlapping source
ranges, unfilled/duplicate linked bytes, dependency mismatches, and an incorrect
size or SHA-256 all fail the build. Compiler diagnostics remain in the console
or alongside compiler intermediates; legacy-source warnings are intentional.

`make clean` removes only generated `build/` output. It retains downloaded
toolchains and Docker images, so the next build does not need setup again.
`make test` runs the host unit tests under `tools/`.

## Compiler profiles and SDK

| Tool/profile | Role |
| --- | --- |
| GBDK-2020 4.5.0 | Modern C units, ASxxxx/SDAS assembler and fixed-address linker |
| Native SDCC 3.3.0 r8604, Boost 1.55.0 | Historical C units and identified runtime source |
| Original Linux x86 SDCC 3.3.0 | Selected functions in pinned Debian/libc runtimes, including private glibc 2.13 |
| Original Win32 SDCC 3.3.0, Wine 8, `xpheap.dll` | Eight stack-layout-sensitive functions, including `settings_menu` |

The Apple SDK compiles host tools, not ROM code. No proprietary Game Boy SDK or
Windows XP installation is needed. Do not substitute system SDCC, add startup
libraries, fix cartridge headers automatically, or change compiler options.
`config/rom.json` preserves each function's exact address and measured profile.

Native SDCC uses a process-lifetime host arena to make symbol-pointer ordering
deterministic. The Win32 profile uses the validated Windows XP CRT heap model
in a fixed container layout. Build-generated unused `extern int symbolPhase...` declarations stand in for
unknown original declarations. Counts remain in `config/rom.json`; build tools
insert and check them without cluttering application sources. They emit no code,
data, or storage. Compilation contexts supply
allocation-sensitive original-file declarations; only the recovered function is
linked, and context input/unit hashes are verified.

Historical assembly is retained unchanged. A separate assembler adapter changes
only ABI metadata (or extracts the selected context function verbatim), not
instructions. The sole post-link change reproduces an observed original
three-byte file-menu modification at `0x4b34`: `00 23 36` becomes `c3 e5 08`.
It requires that exact preimage and saved restoration bytes `00 23 36` at
`0x0801`. Compiler assembly and linked IHX remain untouched; the final image
must still match the pinned complete-ROM SHA-256.

## GitHub Actions and releases

`.github/workflows/build.yml` runs on branch pushes (ignoring `v*` release tags,
which build through `.github/workflows/release.yml`), pull requests, manual
dispatches, and reusable workflow calls. It runs `make test` first, provisions
Docker through Colima/QEMU on macOS ARM64, runs setup and verification, and
uploads only the verified ROM.

`.github/workflows/release.yml` accepts numeric `v<major>.<minor>` tags, builds
the tagged commit through the same build workflow, verifies the downloaded
artifact again, and creates a release only after success. Release jobs alone
receive `contents: write`; ordinary builds use read-only repository permissions.
No external secrets are required beyond GitHub's automatic `GITHUB_TOKEN`.

A maintainer can trigger a release with:

```sh
git tag v1.0
git push origin v1.0
```

The ROM remains byte-identical regardless of tag. A source change that changes
ROM bytes will fail verification; version tags do not relax matching requirements.
