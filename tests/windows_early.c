/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#ifdef BUILD_EARLY_DLL
static int initialized, faults;
static LONG CALLBACK diagnostic(EXCEPTION_POINTERS *e) {
    if (e->ExceptionRecord->ExceptionCode != EXCEPTION_ILLEGAL_INSTRUCTION)
        return EXCEPTION_CONTINUE_SEARCH;
    const unsigned char *pc = (void *)e->ContextRecord->Rip;
    if (pc[0] == 0x0f && pc[1] == 0x1f && pc[2] == 0xc2) {
        ++faults; e->ContextRecord->Rip += 3; return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (pc[0] == 0x41 && pc[1] == 0x0f && pc[2] == 0x1f && pc[3] == 0xc1) {
        ++faults; e->ContextRecord->Rip += 4; return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
__declspec(dllexport) int early_initialized(void) { return initialized; }
__declspec(dllexport) int early_faults(void) { return faults; }
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    (void)instance; (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        PVOID handle = AddVectoredExceptionHandler(1, diagnostic);
        if (!handle) return FALSE;
        __asm__ volatile(".byte 0x0f,0x1f,0xc2; .byte 0x41,0x0f,0x1f,0xc1");
        initialized = 42;
        RemoveVectoredExceptionHandler(handle);
    }
    return TRUE;
}
#else
__declspec(dllimport) int early_initialized(void);
__declspec(dllimport) int early_faults(void);
int main(int argc, char **argv) {
    int expect_bridge = argc > 1 && !strcmp(argv[1], "--expect-bridge");
    printf("dll_attach initialized=%d fallback_nops=%d main_reached=yes\n",
           early_initialized(), early_faults());
    return early_initialized() != 42 || (expect_bridge && early_faults() != 0);
}
#endif
