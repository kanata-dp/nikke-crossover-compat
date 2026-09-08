/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
typedef LONG (WINAPI *lookup_fn)(HANDLE, void **);
typedef void *(WINAPI *owner_fn)(void *);
typedef HANDLE (WINAPI *id_fn)(void *);
typedef void (WINAPI *release_fn)(void *);
int main(void)
{
    HMODULE kernel = LoadLibraryA("ntoskrnl.exe");
    if (!kernel) return 1;
    lookup_fn lookup = (lookup_fn)(void *)GetProcAddress(kernel, "PsLookupThreadByThreadId");
    owner_fn owner = (owner_fn)(void *)GetProcAddress(kernel, "PsGetThreadProcess");
    id_fn pid = (id_fn)(void *)GetProcAddress(kernel, "PsGetProcessId");
    release_fn release = (release_fn)(void *)GetProcAddress(kernel, "ObfDereferenceObject");
    if (!lookup || !owner || !pid || !release) return 2;
    void *self;
    if (lookup((HANDLE)(ULONG_PTR)GetCurrentThreadId(), &self)) return 3;
    void *self_owner = owner(self);
    if (!self_owner || (ULONG_PTR)pid(self_owner) != GetCurrentProcessId()) return 4;
    char path[32768];
    if (!GetModuleFileNameA(NULL, path, sizeof(path))) return 5;
    STARTUPINFOA startup = {.cb = sizeof(startup)};
    PROCESS_INFORMATION child;
    if (!CreateProcessA(path, NULL, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &startup, &child)) return 6;
    void *other = NULL;
    LONG status = lookup((HANDLE)(ULONG_PTR)child.dwThreadId, &other);
    int failed = !!status;
    if (!status)
    {
        void *other_owner = owner(other);
        failed = !other_owner || other_owner == self_owner ||
            (ULONG_PTR)pid(other_owner) != child.dwProcessId || owner(other) != other_owner;
        release(other);
    }
    TerminateProcess(child.hProcess, 0);
    WaitForSingleObject(child.hProcess, 5000);
    CloseHandle(child.hThread); CloseHandle(child.hProcess); release(self);
    printf("thread_owner self_and_child=%s stable_borrowed_pointer=%s\n", failed ? "fail" : "pass", failed ? "fail" : "pass");
    return failed ? 7 : 0;
}
