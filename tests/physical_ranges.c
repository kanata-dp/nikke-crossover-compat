/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
struct range { LARGE_INTEGER base, bytes; };
typedef struct range *(WINAPI *ranges_fn)(void);
typedef void (WINAPI *free_fn)(void *);
int main(void) {
    HMODULE kernel = LoadLibraryA("ntoskrnl.exe");
    if (!kernel) return 1;
    ranges_fn get = (ranges_fn)(void *)GetProcAddress(kernel, "MmGetPhysicalMemoryRanges");
    free_fn release = (free_fn)(void *)GetProcAddress(kernel, "ExFreePool");
    if (!get || !release) return 2;
    struct range *a = get(), *b = get();
    if (!a || !b || a == b) return 3;
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    MEMORYSTATUSEX memory = { .dwLength = sizeof(memory) };
    if (!GlobalMemoryStatusEx(&memory)) return 4;
    int failed = a[0].base.QuadPart < 0 || a[0].base.QuadPart % info.dwPageSize || a[0].bytes.QuadPart <= 0 ||
        a[0].bytes.QuadPart % info.dwPageSize || a[1].base.QuadPart || a[1].bytes.QuadPart ||
        a[0].bytes.QuadPart != b[0].bytes.QuadPart ||
        (ULONGLONG)a[0].bytes.QuadPart != memory.ullTotalPhys;
    printf("physical base=%llu bytes=%llu reported total=%llu allocated_separately=1 terminated=%d\n",
        (ULONGLONG)a[0].base.QuadPart, (ULONGLONG)a[0].bytes.QuadPart,
        memory.ullTotalPhys, !a[1].base.QuadPart && !a[1].bytes.QuadPart);
    release(a); release(b);
    return failed ? 5 : 0;
}
