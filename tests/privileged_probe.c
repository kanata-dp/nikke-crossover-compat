/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static volatile LONG faults;
static volatile DWORD expected;
static volatile unsigned length;
static LONG CALLBACK handler(EXCEPTION_POINTERS *e)
{
    printf("fault=%08lx expected=%08lx\n", e->ExceptionRecord->ExceptionCode, expected);
    if (e->ExceptionRecord->ExceptionCode != expected) return EXCEPTION_CONTINUE_SEARCH;
    InterlockedIncrement(&faults);
    e->ContextRecord->Rip += length;
    return EXCEPTION_CONTINUE_EXECUTION;
}

int main(int argc, char **argv)
{
    setbuf(stdout, NULL);
    BOOL fixed = argc > 1 && !strcmp(argv[1], "fixed");
    PVOID cookie = AddVectoredExceptionHandler(1, handler);
    expected = fixed ? EXCEPTION_PRIV_INSTRUCTION : EXCEPTION_ILLEGAL_INSTRUCTION;
    length = 3;
    __asm__ volatile(".byte 0x0f,0x20,0xd8" ::: "rax", "memory"); /* MOV RAX, CR3 */
    length = 4;
    __asm__ volatile(".byte 0x41,0x0f,0x20,0xd8" ::: "r8", "memory"); /* MOV R8, CR3 */
    __asm__ volatile(".byte 0x44,0x0f,0x20,0xc0" ::: "rax", "memory"); /* MOV RAX, CR8 */
    length = 3;
    __asm__ volatile(".byte 0x0f,0x22,0xd8" ::: "memory"); /* MOV CR3, RAX */
    expected = EXCEPTION_ILLEGAL_INSTRUCTION;
    __asm__ volatile(".byte 0x0f,0x20,0xc8" ::: "rax", "memory"); /* Undefined CR1 */
    length = 2;
    expected = EXCEPTION_ILLEGAL_INSTRUCTION;
    __asm__ volatile("ud2" ::: "memory");
    RemoveVectoredExceptionHandler(cookie);
    printf("faults=%ld expected=6\n", faults);
    return faults != 6;
}
