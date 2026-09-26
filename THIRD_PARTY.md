# Source and runtime provenance

The project is distributed under LGPL-2.1-or-later (see LICENSE).

`src/wine_bootstrap.c` follows Wine's public `__wine_main`/
`wine_main_preload_info` interface and macOS virtual address reservations.
Reference: Wine commit `36b6a2cf679fb395f668a917b76537190e212d9c`,
`loader/main.c` and `configure.ac`. The source credits Alexandre Julliard,
Copyright 2000, and Wine contributors, under LGPL-2.1-or-later.
Those attribution and licensing terms are retained here and in the bootstrap.

`patches/crossover-26.1-kernel.patch` and
`patches/crossover-26.1-mf-software.patch` modify Wine sources distributed by
CodeWeavers with CrossOver 26.1. Their context and modified source retain the
Wine contributors' LGPL-2.1-or-later licensing. The guarded-mutex operations
follow upstream Wine `dlls/ntoskrnl.exe/sync.c` at the reference commit above.
The process-name cache, callback-registration lifetime, memory-range ownership
and opt-in media fallback changes were authored for this investigation.

The upstream source archive is:
https://media.codeweavers.com/pub/crossover/source/crossover-sources-26.1.0.tar.gz
SHA-256: `e4ec87d5821a009dd1f1d2e36ffe2e24b8fcbae9516375ea42f95a16928ab8fa`.
It is fetched separately and is not included in the source package.

The public Endfield_FineWine project helped identify the Rosetta privileged
instruction classification problem and related missing Wine APIs:
https://github.com/stoicswe/Endfield_FineWine/tree/e5d4ccad235eefe32d912733e57e4c0bb53a5b58
Its README identifies scripts/documentation as MIT and patches as LGPL-2.1.
The strict instruction decoder and independently tested API implementations
here do not incorporate that project's dispatcher or signature-check changes.
The unsupported `KeCapturePersistentThreadState` return path and thread-owner
accessor follow Etaash Mathamsetty's Wine patches carried by that project,
commits `39beae3ce0e67e969cb90250f9e1c8be48278e34` and
`a0a4f472ed9ddcf2c8d6e66f03147946aaefc075`, under LGPL-2.1-or-later.
The logical physical inverse adds mapped-memory validation to the analogous
Wine-derived identity-mapping approach; it does not translate host hardware
addresses. The capture parameter widths use ULONG_PTR on x86_64.

Other new source and test files were authored for this investigation.
No implementation from proprietary game binaries or anti-cheat components is
included. Authored probes exercise public Windows/Wine API behavior.

Rosetta is an Apple runtime dependency. Wine/CrossOver and its graphics/media
components are separately installed dependencies with their own licenses.
`prepare_runtime.py` creates a local view of the user's existing installation;
the copied ntdll and links are not part of the distributable source package.
No CrossOver registration, licensing, signatures or application files are
altered by that script.

The prior launcher work referenced `Dorin130/li-miniloader-wine-fix` and a CEF
software-rendering wrapper for `tbs_browser.exe`. Those launcher dependencies
are not bundled or installed by this project. Version 0.2.1 adds an authored
wrapper for the separate `intl_service.exe` host, plus backup/restore tools.
Its source is LGPL-2.1-or-later; the official service remains a separately
installed dependency and is never included in the source package.
NIKKE assets, ACE files, Intel manuals and Microsoft documentation are not
redistributed. Product names identify test environments, not affiliation.

## September 2026 update

`patches/crossover-26.1-september-update.patch` adapts LGPL-2.1-or-later
work from [DW-Proton's Wine tree](https://dawn.wine/dawn-winery/wine-dwproton):

- Driver entry return-address compatibility: mkrsym1,
  [e332f9a4a3a8afbe4423bf77b260f3db08656af7](https://dawn.wine/dawn-winery/wine-dwproton/commit/e332f9a4a3a8afbe4423bf77b260f3db08656af7).
- Driver ServiceKeyName: shxrrydw,
  [57817da4e9e946a134561ef3aa0df9ab1c27dc54](https://dawn.wine/dawn-winery/wine-dwproton/commit/57817da4e9e946a134561ef3aa0df9ab1c27dc54).
- MmCopyMemory export and explicit unsupported result: shxrrydw,
  [e32f76019c5b577fa225f6c91a5616200c844454](https://dawn.wine/dawn-winery/wine-dwproton/commit/e32f76019c5b577fa225f6c91a5616200c844454).
- MDL mapping lifetime: shxrrydw,
  [429496a0ddbc2a4301ff3cc4387a9f2d7ad16b99](https://dawn.wine/dawn-winery/wine-dwproton/commit/429496a0ddbc2a4301ff3cc4387a9f2d7ad16b99),
  with local allocation-base matching, multiple-reference release and authored tests.
- PsGetContextThread entry thunk: shxrrydw,
  [363bac18bb83ee63d709359b3b2d0612e36ff1b4](https://dawn.wine/dawn-winery/wine-dwproton/commit/363bac18bb83ee63d709359b3b2d0612e36ff1b4).
- Minimal Wine system-process component `src/lsass.c`: Copyright 2026 bluechxin,
  [a55ca5689fbce9c6fe99d8d63a1de99aff4bb8dc](https://dawn.wine/dawn-winery/wine-dwproton/commit/a55ca5689fbce9c6fe99d8d63a1de99aff4bb8dc).
  Its license header is retained. It is not an implementation of Windows LSASS security services.

The local PsGetProcessExitStatus implementation queries the actual process;
it does not adopt the upstream fixed STATUS_PENDING placeholder. The existing
thread-owner patch retains its historical filename but is now included by default.
