# NIKKE CrossOver Compatibility

**Run the Windows PC version of GODDESS OF VICTORY: NIKKE on an Apple Silicon Mac through CrossOver.**

[简体中文](README.md) · [Validation](docs/VALIDATION.md) · [Architecture](docs/ARCHITECTURE.md)

This experimental patch set addresses startup compatibility problems, missing Wine APIs, and black background videos observed while running NIKKE. It also installs a persistent launcher entry inside CrossOver.

**Tested on one machine:** the user entered the lobby and combat with working background animation and normal perceived frame rate. After restarting through the persistent entry with High Resolution Mode enabled, the user confirmed more than ten minutes without an ACE popup and noticeably clearer graphics. Quantitative FPS measurements and longer sessions remain untested.

## What does it address?

- **Startup compatibility:** handle specific register-NOP forms rejected by Rosetta on the tested machine, correct privileged-instruction exception classification, and supply several Wine kernel APIs used during startup.
- **Black background video:** expose an unsupported DXGI-video capability early enough for the application to take its software fallback.
- **Repeatable launching:** keep the runtime in a persistent location and open the official NIKKE launcher from CrossOver's application list.
- **Blurry graphics:** use CrossOver's High Resolution Mode in the separate bottle. The tested game's stored window size increased from 1388×781 to 2202×1340.

These are observed results for the tested configuration, not a universal fix for NIKKE errors or other games using ACE.

## Requirements

The tested environment is **Apple M4 Max, macOS 26.6.2, CrossOver 26.1**. Other hardware, CrossOver versions, and future game updates have not been verified.

You need:

1. A working installation of CrossOver 26.1 and Rosetta.
2. An existing CrossOver bottle with NIKKE for Windows installed. Its official launcher must already open and allow sign-in. The default launcher directory is `C:\NIKKE\Launcher`.
3. Xcode Command Line Tools, Python 3, Bison 3, and MinGW-w64 to build the source.

This repository contains source code only. It does not include game assets, ACE files, CrossOver binaries, or account data. The tested bottle already used [li-miniloader-wine-fix](https://github.com/Dorin130/li-miniloader-wine-fix) and a CEF launcher fix. This project does not install those dependencies. Fix the official launcher first if it cannot open.

## Installation

Run these commands from the repository root. Completely close the source bottle's game, launcher, and background processes before copying it.

### 1. Build the compatibility bridge

```sh
make
make test
```

### 2. Build the patched Wine modules

Download the [official CrossOver 26.1 source archive](https://media.codeweavers.com/pub/crossover/source/crossover-sources-26.1.0.tar.gz) from CodeWeavers, then run:

```sh
python3 scripts/build_wine_modules.py \
    --archive /absolute/path/to/crossover-sources-26.1.0.tar.gz

python3 scripts/prepare_runtime.py \
    --output local/runtime-modules \
    --modules local/wine-modules/build
```

The builder verifies a pinned SHA-256 before extracting the archive. Its default Bison path is `/opt/homebrew/opt/bison/bin/bison`; use `--bison` for another location.

### 3. Install a persistent CrossOver entry

Replace `YOUR_NIKKE_BOTTLE` with the name of your existing NIKKE bottle:

```sh
python3 scripts/install_crossover_entry.py \
    --source-prefix "$HOME/Library/Application Support/CrossOver/Bottles/YOUR_NIKKE_BOTTLE" \
    --source-runtime local/runtime-modules \
    --source-app build/NopBridgeLab.app \
    --source-bridge build/libnop_bridge.dylib
```

The installer creates a separate **NIKKE-Compatibility** bottle using APFS file clones, preserving downloaded resources. It keeps the original bottle and refuses to overwrite existing destinations. Copied sign-in state remains local.

The runtime lives under `~/Library/Application Support/NIKKE Compatibility` and depends on your existing CrossOver installation. Keep that directory; launching no longer depends on a temporary test directory.

## Launching and resolution

Reopen CrossOver, then use:

**NIKKE-Compatibility → NIKKE Compatibility → Start Game in the official launcher**

For clearer graphics, enable **High Resolution Mode** in that bottle's sidebar and accept the bottle restart. You can customize the entry name with the installer's `--menu-name` option.

The dedicated launch profile uses the tested **DXVK** backend. Selecting another backend in CrossOver does not automatically change that profile; other backends require separate configuration and testing.

## Known limitations

- Validation covers lobby/combat entry and more than ten minutes without an ACE popup after restarting. It is not a long-term stability guarantee.
- The gameplay-tested runtime still logs missing `PsGetThreadProcess` calls in a driver process. Short-session playability does not prove every ACE component or check succeeded.
- A candidate implementation passes independent API tests but has not been tested in the game and is disabled by default. Developers can build it with `build_wine_modules.py --with-thread-process`.
- Some Wine kernel semantics remain incomplete. Video recovery depends on an application supporting software fallback; not every cutscene has been verified.
- Each emulated NOP incurs signal-handling overhead. Its performance cost has not been quantified.
- CrossOver or game updates may require further compatibility work.

See [Validation](docs/VALIDATION.md) for the experiments, negative cases, and evidence. This project works at the runtime/API layer. It does not modify game or ACE binaries or fabricate successful anti-cheat results.

## Development and contributions

Native regression tests:

```sh
make test
```

Windows/Wine API tests require a separate disposable test bottle:

```sh
python3 scripts/test_windows.py --prefix /absolute/path/to/test-bottle
python3 scripts/test_wine_modules.py \
    --prefix /absolute/path/to/test-bottle \
    --runtime local/runtime-modules
```

Add `--with-thread-process` when testing the candidate thread API. To package source only:

```sh
python3 scripts/package_source.py
```

When reporting a problem, include your Mac model, macOS/CrossOver versions, graphics backend, and reproduction steps. Remove credentials, tokens, and account identifiers from any log excerpts.

## License and credits

Licensed under **LGPL-2.1-or-later**. See [LICENSE](LICENSE) and [Third-party provenance](THIRD_PARTY.md).

Thanks to Wine, CodeWeavers, Endfield_FineWine, and the earlier launcher-fix projects for their public work. NIKKE, CrossOver, and Rosetta belong to their respective rights holders. This is an independent community compatibility project.
