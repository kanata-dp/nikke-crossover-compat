/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

/* Treat the kernel object as opaque; keep ample aligned caller-owned storage. */
static DECLSPEC_ALIGN(16) unsigned char mutex_object[128];
typedef void (WINAPI *mutex_fn)(void *);
static mutex_fn acquire, release;
static volatile LONG inside, errors, entered;
static unsigned long counter;
static HANDLE gate;

static DWORD WINAPI worker(void *arg)
{
    unsigned iterations = (unsigned)(ULONG_PTR)arg;
    WaitForSingleObject(gate, INFINITE);
    for (unsigned i = 0; i < iterations; ++i)
    {
        acquire(mutex_object);
        InterlockedIncrement(&entered);
        if (InterlockedIncrement(&inside) != 1) InterlockedIncrement(&errors);
        unsigned long value = counter;
        if (!(i % 127)) SwitchToThread();
        counter = value + 1;
        if (InterlockedDecrement(&inside) != 0) InterlockedIncrement(&errors);
        release(mutex_object);
    }
    return 0;
}

int main(void)
{
    setbuf(stdout, NULL);
    HMODULE kernel = LoadLibraryA("ntoskrnl.exe");
    if (!kernel) { printf("LoadLibrary failed: %lu\n", GetLastError()); return 2; }
    mutex_fn initialize = (mutex_fn)(void *)GetProcAddress(kernel, "KeInitializeGuardedMutex");
    acquire = (mutex_fn)(void *)GetProcAddress(kernel, "KeAcquireGuardedMutex");
    release = (mutex_fn)(void *)GetProcAddress(kernel, "KeReleaseGuardedMutex");
    if (!initialize || !acquire || !release)
    {
        printf("guarded mutex exports missing: initialize=%d acquire=%d release=%d\n",
               !!initialize, !!acquire, !!release);
        return 3;
    }
    initialize(mutex_object);
    gate = CreateEventW(NULL, TRUE, FALSE, NULL);
    HANDLE threads[8];
    acquire(mutex_object);
    for (unsigned i = 0; i < 8; ++i)
    {
        threads[i] = CreateThread(NULL, 0, worker, (void *)(ULONG_PTR)10000, 0, NULL);
        if (!threads[i]) return 4;
    }
    SetEvent(gate);
    Sleep(100);
    if (entered) { puts("FAIL: waiter entered while mutex was held"); return 5; }
    release(mutex_object);
    DWORD result = WaitForMultipleObjects(8, threads, TRUE, 30000);
    if (result != WAIT_OBJECT_0) { printf("FAIL: wait returned %lu\n", result); return 6; }
    for (unsigned i = 0; i < 8; ++i) CloseHandle(threads[i]);
    CloseHandle(gate);
    printf("guarded mutex: counter=%lu expected=80000 overlap_errors=%ld\n", counter, errors);
    return counter != 80000 || errors ? 7 : 0;
}
