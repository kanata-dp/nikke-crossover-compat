/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef LONG (WINAPI *lookup_fn)(HANDLE, void **);
typedef char *(WINAPI *name_fn)(void *);
typedef void (WINAPI *dereference_fn)(void *);

int main(int argc, char **argv)
{
    setbuf(stdout, NULL);
    if (argc != 2) return 1;
    HMODULE kernel = LoadLibraryA("ntoskrnl.exe");
    if (!kernel) return 2;
    lookup_fn lookup = (lookup_fn)(void *)GetProcAddress(kernel, "PsLookupProcessByProcessId");
    name_fn name = (name_fn)(void *)GetProcAddress(kernel, "PsGetProcessImageFileName");
    dereference_fn dereference = (dereference_fn)(void *)GetProcAddress(kernel, "ObfDereferenceObject");
    if (!lookup || !name || !dereference) { puts("missing process query API"); return 3; }
    void *self;
    LONG status = lookup((HANDLE)(ULONG_PTR)GetCurrentProcessId(), &self);
    if (status) { printf("lookup self: %08lx\n", status); return 4; }
    char path[32768];
    GetModuleFileNameA(NULL, path, sizeof(path));
    const char *base = strrchr(path, '\\');
    char expected[16];
    lstrcpynA(expected, base ? base + 1 : path, sizeof(expected));
    char *self_name = name(self);
    printf("self name=%s expected=%s\n", self_name, expected);
    if (strcmp(self_name, expected)) return 5;
    STARTUPINFOA startup = { .cb = sizeof(startup) };
    PROCESS_INFORMATION child;
    if (!CreateProcessA(argv[1], NULL, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &startup, &child)) return 6;
    void *child_object;
    status = lookup((HANDLE)(ULONG_PTR)child.dwProcessId, &child_object);
    if (status) { printf("lookup child: %08lx\n", status); TerminateProcess(child.hProcess, 1); return 7; }
    base = strrchr(argv[1], '\\');
    lstrcpynA(expected, base ? base + 1 : argv[1], sizeof(expected));
    char *child_name = name(child_object);
    printf("child name=%s expected=%s\n", child_name, expected);
    int failed = strcmp(child_name, expected) || self_name == child_name;
    TerminateProcess(child.hProcess, 0);
    WaitForSingleObject(child.hProcess, 5000);
    if (strcmp(name(child_object), expected)) failed = 1;
    dereference(child_object); dereference(self);
    CloseHandle(child.hThread); CloseHandle(child.hProcess);
    return failed ? 8 : 0;
}
