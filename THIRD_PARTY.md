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
software-rendering wrapper. They are not bundled or installed by this project.
NIKKE assets, ACE files, Intel manuals and Microsoft documentation are not
redistributed. Product names identify test environments, not affiliation.
