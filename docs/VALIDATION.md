# Validation — 2026-09-08

## Result

The user reports actual NIKKE lobby and combat entry, about three minutes
without an ACE popup, normal perceived frame rate, and working background
animation. The user then closed the game normally. A persistent CrossOver
entry was installed from that working environment and used to restart it;
the user confirmed the game was running and that Retina mode made it
noticeably clearer. The user subsequently confirmed more than ten minutes
without an ACE popup in that restarted session. There is no quantitative
FPS measurement or independently verified completed mission. Longer-session
stability and additional game scenes remain untested.

Earlier runs showed ACE popups `13-131078-288` and `13-131079-63`.
The gameplay-tested runtime still logs unimplemented `PsGetThreadProcess`
calls in a driver process, including after the persistent-entry restart.
Short-session playability does not prove every ACE component/check succeeded.
The candidate implementation passes independent tests but is not installed
in that gameplay-tested runtime; it is a separate, opt-in source patch.

Environment: Apple M4 Max, macOS 26.6.2 (25G83), CrossOver 26.1.
The development tools were Apple Clang and MinGW-w64 GCC 16.2.0.
The game reported Unity 2021.3.56f2. Results do not establish compatibility on
different hosts or future game versions.

## Experiments and causal limits

1. The previous investigation reproduced `0f 1f c2` as an illegal instruction,
   both in `nikkeBase.dll` at RVA `0x1f0cf00` and in a standalone executable.
2. A native x86_64 macOS probe measured four fallback exceptions without the
   final bridge and zero with it. RDX, RFLAGS and errno were preserved.
3. Windows probes showed the same result. An imported DLL executed both NOP
   forms during process attach before main. The diagnostic VEH saw two faults
   without the bridge and zero with it; main observed initialized value 42.
4. A real Windows CreateProcess child passed after adding the local runtime
   view. Earlier attempts through the stock loader/wrapper lost the bridge in
   descendants. Repointing WINELOADER alone was insufficient on this runtime.
5. The unprefixed-only prototype executed 34,763 NOP emulations in one game
   run and reached Unity/D3D initialization. A later DXVK run exposed
   `41 0f 1f c1` in GameAssembly.dll at RVA `0xf5f9fae`.
6. After adding that REX.B form, a direct run logged 40,656 NOP emulations and
   no `c000001d` exceptions. It reached input and splash initialization,
   then reported launcher SDK initialization error `2020002`. This direct
   run deliberately had no launcher session arguments. It was not a gameplay
   test and is not evidence of successful login.
7. The official launcher was then started in the cloned bottle. The user
   clicked Start and supplied a screenshot of the real loading UI with the
   announcement and progress `[2/7]`. A second screenshot showed ACE error
   `13-131078-288`.

The screenshots were captured at 11:07:58 and 11:08:17 local time. Test-prefix
cleanup preceded the source edit at 11:09:15, after the ACE screenshot. That
cleanup therefore does not explain the already displayed ACE error.
An initial launcher/bootstrap process returning zero is not treated as game
completion: Wine can have surviving descendants.

## Background video

The local title MP4 was present and ffprobe identified H.264, 1600 × 1600,
YUV420P. The successful launcher-driven session's Player.log repeatedly
reported `WindowsVideoMedia` error `0x80070057`, with context:

> Failed getting shared handle from IDXGIResource

An independent program created its own shared 1600 × 1600 RGBA8 and NV12
textures, then attempted the legacy DXGI shared handle/open sequence:

| Backend override | Outcome |
| --- | --- |
| DXVK | Texture creation succeeded; GetSharedHandle returned 0x80070057 for both formats. Its log reported VK_KHR_EXTERNAL_MEMORY_WIN32 unsupported. |
| WineD3D | Texture creation succeeded; GetSharedHandle returned 0x80004001 for both formats. |
| D3DMetal | The combined probe hit a Metal pixel-format assertion. A subsequent isolated RGBA8 probe returned E_NOTIMPL for GetSharedHandle; NV12 sharing remains unsupported in this test. |

This strongly supports a graphics-resource sharing obstacle, independently
of NIKKE or ACE. It does not prove a complete fix or that every graphics
backend/version has the same limitation. The probe does not implement a
complete sharing/rendering solution.

The authored Media Foundation reader subsequently decoded a generated
640 × 360 H.264 clip. GPU and CPU paths each yielded 60 changing frames with
the same additive byte checksum, 2606208008. This is a useful content check,
not a cryptographic pixel identity proof or a presentation test.

Ignoring the source reader's D3D manager (`NOP_BRIDGE_MF_SOFTWARE=1`) produced
CPU frames in the probe. In the game, Unity continued to request
`IMFDXGIBuffer` and logged E_NOINTERFACE; this experiment failed to resolve
the black background.

Returning E_NOTIMPL from `MFCreateDXGIDeviceManager`
(`NOP_BRIDGE_MF_NO_DXGI=1`) allowed the probe's explicit CPU fallback to decode
the same 60 frames. The 11:59 game run logged that manager-creation failure
and a color-standard fallback, without the earlier shared-handle/buffer
interface errors. ACE interrupted this run. No new screenshot establishes
that the game actually displayed background animation in that early run.
In the later 12:43 run using the same no-DXGI option, the user explicitly
confirmed normal background animation during resource downloading. Several
kernel changes also occurred between runs, so this is a successful observed
configuration, not a controlled single-variable game A/B result.

## ACE

The official ACE service was installed in the clone. A separate normal
Service Control Manager start returned RUNNING with zero error, and the next
query returned STOPPED with zero error. The idle standalone service may exit
when not attached to its expected game session; this observation does not
identify the reason for the popup. The error-code mapping was not found in
the inspected public vendor material.

Subsequent Wine logs identified these fatal unimplemented exports in order:

| Game test | Fatal missing export | Follow-up |
| --- | --- | --- |
| NOP bridge | KeAcquireGuardedMutex | Backported upstream mutex operations |
| Mutex overlay | PsGetProcessImageFileName | Added per-process cached actual short name |
| Process-name overlay | KeRegisterBugCheckReasonCallback | Added registration/removal lifecycle |
| Callback + privileged-fault overlay | MmGetPhysicalMemoryRanges | Added caller-owned terminated range array |
| Memory-range overlay | KeCapturePersistentThreadState, MmGetVirtualForPhysical | Explicit unsupported capture result; bounded logical address inverse |
| Logical mapping + capture fallback | PsGetThreadProcess | Game reached resource download, background reported normal; thread-owner query candidate prepared separately |

The recurring popup code is not sufficient to identify these individual
failures; the Wine stub-call logs supply the causal evidence. Optional
`MmGetSystemRoutineAddress` queries also returned NULL for other exports.
Those queries alone do not justify adding success-returning stubs.

After the callback and privileged-fault changes, a separate ordinary SCM
start of the ACE-BASE driver returned RUNNING with zero errors. The next
launcher-driven game run nevertheless called the missing physical-memory
query. A successful idle driver load is therefore not an ACE handshake test.

No ACE executable, driver, response, checks or game code was modified. No
successful anti-cheat result was fabricated. The short gameplay report above
does not establish complete anti-cheat compatibility.

## Additional API probes

- Guarded mutex: eight threads, 80,000 protected updates, no observed overlap;
  workers remained blocked while the main thread held the mutex.
- Process image name: actual self and child basenames, 15-character truncation,
  independent storage and stable cache while retaining the object after exit.
- Bugcheck callback registration: invalid inputs, duplicate rejection,
  independent records, deregistration and reuse. Callback execution during
  kernel crashes is not implemented or tested.
- Physical ranges: separate allocations, zero terminator, page-aligned base
  4096, length 38654701568 matching GlobalMemoryStatusEx; freed with ExFreePool.
  This tests Wine's logical memory report, not raw host physical access.
- Privileged moves: four valid CR forms change from C000001D to C0000096;
  undefined CR1 and UD2 retain C000001D. Probe handlers consume the exceptions;
  the compatibility layer itself preserves the faulting instruction pointer.
- Logical address inverse: a live committed page round-trips and can be read/
  written; reserved, inaccessible, freed, null and invalid addresses are rejected.
  The snapshot fallback returns STATUS_NOT_IMPLEMENTED and leaves a sentinel
  output buffer unchanged. This does not verify full Windows snapshot semantics.

## Native launcher and build reproducibility

The native app entry originally became an intermediate `start.exe` for the
32-bit launcher. `WINEARCH=wow64` kept that launcher in the registered app
process, allowing the desktop automation tool to obtain its window.
Only the launcher window has been verified accessible this way.

The working runtime and downloaded resources were subsequently APFS-cloned
into a persistent, separate CrossOver bottle. CrossOver's supported raw-menu
interface launches a locally signed app, which supplies the Wine bootstrap
and compatibility environment. The installed kernel/media/bridge hashes
match the preceding working runtime; signing the relocated app changes its
bootstrap code-signature bytes. Runtime links and launch environment no
longer reference temporary paths. The original bottle remains untouched.

CrossOver's High Resolution Mode was off. Enabling it through the CrossOver
UI set `Software\\Wine\\Mac Driver\\RetinaMode` to `y` and per-user DPI to 192.
The game subsequently saved window dimensions 2202×1340, versus 1388×781
before. The user explicitly confirmed improved clarity. These are stored
window dimensions, not a measurement of all internal render targets.

Some later executions under Downloads stalled in system directory access;
the sampled dsymutil stack ended inside CoreFoundation's directory opening.
A relocated temporary build with CFLAGS/OBJCFLAGS=-O2 completed from the
fixed-hash official archive, including the memory-range patch. The same
temporary-path regression passed real Windows child creation. The precise
macOS cause of the original directory stall was not established.

## Regression coverage

- Decoder enumeration: 2 × 16,777,216 candidate sequences; 16 accepted forms.
- Native A/B: fallback counts 4 → 0.
- Windows A/B: fallback counts 4 → 0.
- Imported DLL process attach: fallback counts 2 → 0.
- Real Windows child process inheritance.
- Eight concurrent threads: 12,800 NOP emulations, with a single-thread
  calibration allowing a host that already executes these NOPs natively.
- UD2 and LOCK NOP still terminate an unhandled process with SIGILL.
- UD2 at a readable page boundary adjacent to PROT_NONE is forwarded safely.
- sigaction query/restore, one-argument handlers, masks, SA_RESETHAND semantics.
- Fixed slot exhaustion fails with ENOMEM without replacing the last handler.

Raw game/launcher logs and screenshots remain local and are not in the source
package. Tests use authored fixtures. The original CrossOver installation and
original NIKKE bottle were not patched.
