/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
#include <string.h>
typedef LARGE_INTEGER (WINAPI *physical_fn)(void *);
typedef void *(WINAPI *virtual_fn)(LARGE_INTEGER);
typedef LONG (WINAPI *capture_fn)(CONTEXT *, void *, ULONG, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, void *);
int main(void)
{
    HMODULE kernel = LoadLibraryA("ntoskrnl.exe");
    if (!kernel) return 1;
    physical_fn physical = (physical_fn)(void *)GetProcAddress(kernel, "MmGetPhysicalAddress");
    virtual_fn logical = (virtual_fn)(void *)GetProcAddress(kernel, "MmGetVirtualForPhysical");
    capture_fn capture = (capture_fn)(void *)GetProcAddress(kernel, "KeCapturePersistentThreadState");
    if (!physical || !logical || !capture) return 2;
    SYSTEM_INFO info; GetSystemInfo(&info);
    BYTE *region = VirtualAlloc(NULL, 2 * info.dwPageSize, MEM_RESERVE, PAGE_NOACCESS);
    if (!region || !VirtualAlloc(region, info.dwPageSize, MEM_COMMIT, PAGE_READWRITE)) return 3;
    LARGE_INTEGER p = physical(region + 123);
    if (logical(p) != region + 123) return 4;
    *(BYTE *)logical(p) = 0x5a;
    if (region[123] != 0x5a) return 5;
    p.QuadPart = (ULONG_PTR)(region + info.dwPageSize);
    if (logical(p)) return 6;
    DWORD old;
    if (!VirtualProtect(region, info.dwPageSize, PAGE_NOACCESS, &old)) return 7;
    p = physical(region);
    if (logical(p)) return 8;
    VirtualFree(region, 0, MEM_RELEASE);
    if (logical(p)) return 9;
    p.QuadPart = -1; if (logical(p)) return 10;
    p.QuadPart = 0; if (logical(p)) return 11;
    BYTE snapshot[512]; memset(snapshot, 0xa5, sizeof(snapshot));
    CONTEXT context = {0};
    LONG status = capture(&context, NULL, 0, 0, 0, 0, 0, snapshot);
    if (status != (LONG)0xc0000002) return 12;
    for (unsigned i = 0; i < sizeof(snapshot); ++i) if (snapshot[i] != 0xa5) return 13;
    puts("logical_mapping=roundtrip reserved/noaccess/freed/invalid=rejected capture=unsupported buffer=untouched");
    return 0;
}
