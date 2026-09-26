# NIKKE CrossOver Compatibility

**Run the Windows PC version of GODDESS OF VICTORY: NIKKE on an Apple Silicon Mac through CrossOver.**

[简体中文](README.md) · [Validation](docs/VALIDATION.md) · [Architecture](docs/ARCHITECTURE.md)

This experimental patch set addresses startup compatibility problems, missing Wine APIs, and black background videos observed while running NIKKE. It also installs a persistent launcher entry inside CrossOver.

**2026-09-26 update: fix the blank “LEVEL INFINITE CAPTCHA NEW” login window.** Verification uses a separate browser host that the earlier launcher fix did not cover. The user confirmed the slider appeared and completed verification manually. **NIKKE PC Global 152.8.13** subsequently reached the lobby with responsive page navigation.

This **0.2.1 experimental source update** adds source, installation and restore tools for the verification browser. See the [CAPTCHA fix and installation guide](docs/UPDATE-2026-09-26.en.md). Wine kernel/media patches remain at 0.2.0; earlier combat and tower reports on 152.8.11 are in the [previous update](docs/UPDATE-2026-09-22.en.md).

## What does it address?

- **Startup compatibility:** handle specific register-NOP forms rejected by Rosetta on the tested machine, correct privileged-instruction exception classification, and supply several Wine kernel APIs used during startup.
- **Black background video:** expose an unsupported DXGI-video capability early enough for the application to take its software fallback.
- **Blank login CAPTCHA:** pass software-rendering options to the separate INTL WebView host so verification can display. Players still complete verification manually.
- **Repeatable launching:** keep the runtime in a persistent location and open the official NIKKE launcher from CrossOver's application list.
- **Blurry graphics:** use CrossOver's High Resolution Mode in the separate bottle. The tested game's stored window size increased from 1388×781 to 2202×1340.

These are observed results for the tested configuration, not a universal fix for NIKKE errors or other games using ACE.

## Requirements

The current tested environment is **Apple M4 Max, macOS 27.0, CrossOver 26.1, NIKKE Global 152.8.13**. Earlier combat reports used 152.8.11; older records used macOS 26.6.2. Other hardware, CrossOver versions, and future game updates have not been verified.

You need:

1. A working installation of CrossOver 26.1 and Rosetta.
2. An existing CrossOver bottle with NIKKE for Windows installed. Its official launcher must already open and allow sign-in. The default launcher directory is `C:\NIKKE\Launcher`.
3. Xcode Command Line Tools, Python 3, Bison 3, and MinGW-w64 to build the source.

This repository contains source code only. It does not include game assets, ACE files, CrossOver binaries, or account data. The tested bottle already used [li-miniloader-wine-fix](https://github.com/Dorin130/li-miniloader-wine-fix) and a CEF launcher fix. This project does not install those dependencies. Fix the official launcher first if it cannot open.

## Only the CAPTCHA window is blank?

If the launcher opens but login verification shows an empty white window, install the [standalone CAPTCHA fix](docs/UPDATE-2026-09-26.en.md). It needs Python 3 and MinGW-w64, without rebuilding Wine or downloading the game again. Keep using your existing CrossOver entry afterward. Users who already applied this fix and can complete verification do not need to reinstall it.

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
    --archive /absolute/path/to/crossover-sources-26.1.0.tar.gz \
    --output local/wine-modules-0.2.0

python3 scripts/prepare_runtime.py \
    --output local/runtime-0.2.0 \
    --modules local/wine-modules-0.2.0/build
```

The builder includes the September changes, thread-owner API and minimal Wine `lsass.exe` component by default, and verifies a pinned SHA-256 before extracting the archive. Its default Bison path is `/opt/homebrew/opt/bison/bin/bison`; use `--bison` for another location.

### 3. Install a persistent CrossOver entry

Replace `YOUR_NIKKE_BOTTLE` with the name of your existing NIKKE bottle:

```sh
python3 scripts/install_crossover_entry.py \
    --source-prefix "$HOME/Library/Application Support/CrossOver/Bottles/YOUR_NIKKE_BOTTLE" \
    --source-runtime local/runtime-0.2.0 \
    --source-app build/NopBridgeLab.app \
    --source-bridge build/libnop_bridge.dylib
```

The installer creates a separate **NIKKE-Compatibility** bottle using APFS file clones, preserving downloaded resources. It keeps the original bottle and refuses to overwrite existing destinations. Copied sign-in state remains local.

The runtime lives under `~/Library/Application Support/NIKKE Compatibility` and depends on your existing CrossOver installation. Keep that directory; launching no longer depends on a temporary test directory.

## Updating an existing installation

A GitHub source update does not replace your local runtime automatically. Exit the old bottle, rebuild into new output directories, then use the following installation command, replacing the source bottle name:

**Updating the game, updating this repository and updating the runtime behind a saved entry are separate operations.** An old entry can still point to an older runtime; the CAPTCHA fix does not upgrade it. If you already use 0.2.0 and gameplay works, only follow the separate CAPTCHA instructions for this update.

```sh
python3 scripts/install_crossover_entry.py \
    --source-prefix "$HOME/Library/Application Support/CrossOver/Bottles/YOUR_NIKKE_BOTTLE" \
    --source-runtime local/runtime-0.2.0 \
    --source-app build/NopBridgeLab.app \
    --source-bridge build/libnop_bridge.dylib \
    --bottle-name NIKKE-Compatibility-152 \
    --menu-name "NIKKE Compatibility 152" \
    --support "$HOME/Library/Application Support/NIKKE Compatibility 152"
```

Keep your original entry/runtime until the new one works. Installation configures the Wine system-process component inside the copied bottle; it does not change macOS services or the original bottle.

## Launching and resolution

Reopen CrossOver, then use:

**NIKKE-Compatibility → NIKKE Compatibility → Start Game in the official launcher**

For clearer graphics, enable **High Resolution Mode** in that bottle's sidebar and accept the bottle restart. You can customize the entry name with the installer's `--menu-name` option.

The dedicated launch profile uses the tested **DXVK** backend. Selecting another backend in CrossOver does not automatically change that profile; other backends require separate configuration and testing.

## Known limitations

- The current configuration is playable in the reported sessions, but two background ACE CORE driver processes still exit abnormally. Playability does not establish that all protection components/checks are healthy or that this configuration is officially supported.
- Temporary diagnostics and memory-snapshot code are excluded. Clean-build API tests and user gameplay reports are recorded separately. The new installation flow has not been verified end to end through combat; see [Validation](docs/VALIDATION.md).
- Long sessions, quantitative FPS and every cutscene have not been tested. CrossOver or game updates may need further adaptation.

The project modifies Wine compatibility behavior. It does not distribute or modify game/ACE binaries or replace failing API queries with fixed success values. Some APIs still explicitly report unsupported behavior.

## Development and contributions

Native regression tests:

```sh
make test
```

Windows/Wine API tests require a separate disposable test bottle:

```sh
python3 scripts/test_windows.py --prefix /absolute/path/to/test-bottle \
    --runtime local/runtime-0.2.0
python3 scripts/test_wine_modules.py \
    --prefix /absolute/path/to/test-bottle \
    --runtime local/runtime-0.2.0
```

Thread ownership, real process exit status, thread context and mapping-lifetime tests are included by default. To package source only:

```sh
python3 scripts/package_source.py
```

When reporting a problem, include your Mac model, macOS/CrossOver versions, graphics backend, and reproduction steps. Remove credentials, tokens, and account identifiers from any log excerpts.

## License and credits

Licensed under **LGPL-2.1-or-later**. See [LICENSE](LICENSE) and [Third-party provenance](THIRD_PARTY.md).

Thanks to Wine, CodeWeavers, DW-Proton, Endfield_FineWine, and the earlier launcher-fix projects for their public work. NIKKE, CrossOver, and Rosetta belong to their respective rights holders. This is an independent community compatibility project.
