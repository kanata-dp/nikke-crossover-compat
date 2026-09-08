/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
#include <string.h>
static LONG volatile fallback_nops, ud2_seen;
static LONG CALLBACK fallback(EXCEPTION_POINTERS *e) {
    if (e->ExceptionRecord->ExceptionCode != EXCEPTION_ILLEGAL_INSTRUCTION)
        return EXCEPTION_CONTINUE_SEARCH;
    const unsigned char *pc = (void *)e->ContextRecord->Rip;
    size_t prefix = pc[0] == 0x41 ? 1 : 0;
    if (pc[prefix] == 0x0f && pc[prefix+1] == 0x1f && (pc[prefix+2] & 0xf8) == 0xc0) {
        InterlockedIncrement(&fallback_nops);
        e->ContextRecord->Rip += prefix + 3;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (pc[0] == 0x0f && pc[1] == 0x0b) {
        InterlockedIncrement(&ud2_seen);
        e->ContextRecord->Rip += 2;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "--spawn-child")) {
        wchar_t path[32768], command[32768];
        STARTUPINFOW start = {0};
        PROCESS_INFORMATION child = {0};
        start.cb = sizeof start;
        DWORD length = GetModuleFileNameW(NULL, path, 32768);
        if (!length || length >= 32768) return 10;
        int n = _snwprintf(command, 32768, L"\"%ls\" --expect-bridge", path);
        if (n < 0 || n >= 32768) return 11;
        if (!CreateProcessW(path, command, NULL, NULL, FALSE, 0, NULL, NULL, &start, &child)) return 12;
        CloseHandle(child.hThread);
        if (WaitForSingleObject(child.hProcess, 20000) != WAIT_OBJECT_0) {
            TerminateProcess(child.hProcess, 13); CloseHandle(child.hProcess); return 13;
        }
        DWORD code = 14;
        GetExitCodeProcess(child.hProcess, &code);
        CloseHandle(child.hProcess);
        printf("child_exit=%lu\n", code);
        return (int)code;
    }
    int expect_bridge = argc > 1 && !strcmp(argv[1], "--expect-bridge");
    PVOID handler = AddVectoredExceptionHandler(1, fallback);
    if (!handler) return 2;
    unsigned long long before, after, result;
    __asm__ volatile(
        "movq $0x12345678, %%rdx; cmpq %%rdx, %%rdx; pushfq; popq %0;"
        ".byte 0x0f,0x1f,0xc2; .byte 0x41,0x0f,0x1f,0xc1; pushfq; popq %1; movq %%rdx, %2"
        : "=&r"(before), "=&r"(after), "=&r"(result)
        : : "rdx", "cc", "memory");
    __asm__ volatile(".byte 0x0f,0x1f,0xc0; .byte 0x0f,0x1f,0xc7; ud2");
    int preserved = before == after && result == 0x12345678;
    printf("windows fallback_nops=%ld ud2_forwarded=%ld state_preserved=%s\n",
           fallback_nops, ud2_seen, preserved ? "yes" : "no");
    RemoveVectoredExceptionHandler(handler);
    return !preserved || ud2_seen != 1 || (expect_bridge && fallback_nops != 0);
}
