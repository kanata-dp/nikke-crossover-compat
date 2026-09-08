/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
struct record {
    LIST_ENTRY entry;
    void (WINAPI *routine)(ULONG, void *, void *, ULONG);
    unsigned char *component;
    ULONG_PTR checksum;
    ULONG reason;
    UCHAR state;
};
static void WINAPI callback(ULONG reason, void *record, void *data, ULONG length)
{ (void)reason; (void)record; (void)data; (void)length; }
typedef BOOLEAN (WINAPI *register_fn)(struct record *, void (WINAPI *)(ULONG, void *, void *, ULONG), ULONG, unsigned char *);
typedef BOOLEAN (WINAPI *remove_fn)(struct record *);
#define CHECK(expr) do { if (!(expr)) { puts("FAIL: " #expr); return 1; } } while (0)
int main(void) {
    HMODULE kernel = LoadLibraryA("ntoskrnl.exe");
    CHECK(kernel);
    register_fn reg = (register_fn)(void *)GetProcAddress(kernel, "KeRegisterBugCheckReasonCallback");
    remove_fn remove = (remove_fn)(void *)GetProcAddress(kernel, "KeDeregisterBugCheckReasonCallback");
    CHECK(reg && remove);
    struct record a = {0}, b = {0};
    CHECK(!reg(&a, callback, 0, (unsigned char *)"probe"));
    CHECK(!reg(&a, NULL, 2, (unsigned char *)"probe"));
    CHECK(reg(&a, callback, 2, (unsigned char *)"probe"));
    CHECK(a.state == 1 && a.routine == callback && a.reason == 2);
    CHECK(!reg(&a, callback, 2, (unsigned char *)"duplicate"));
    CHECK(reg(&b, callback, 3, (unsigned char *)"second"));
    CHECK(remove(&a) && a.state == 0);
    CHECK(!remove(&a));
    CHECK(remove(&b) && b.state == 0);
    CHECK(reg(&a, callback, 2, (unsigned char *)"reuse") && remove(&a));
    puts("callback registration: validation, duplicate rejection, independent records, removal, reuse PASS");
    return 0;
}
