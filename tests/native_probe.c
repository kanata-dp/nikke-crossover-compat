/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <dlfcn.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ucontext.h>
#include <sys/mman.h>
#include <unistd.h>

static volatile sig_atomic_t fallback_nops, ud2_seen, raised_seen, expecting_raise;
static void simple_handler(int sig) { if (sig == SIGILL) ++raised_seen; }
static void *worker(void *unused) {
    (void)unused;
    for (int i = 0; i < 100; ++i)
        __asm__ volatile(".byte 0x0f,0x1f,0xc0; .byte 0x0f,0x1f,0xc1;"
                         ".byte 0x0f,0x1f,0xc2; .byte 0x0f,0x1f,0xc3;"
                         ".byte 0x0f,0x1f,0xc4; .byte 0x0f,0x1f,0xc5;"
                         ".byte 0x0f,0x1f,0xc6; .byte 0x0f,0x1f,0xc7;"
                         ".byte 0x41,0x0f,0x1f,0xc0; .byte 0x41,0x0f,0x1f,0xc1;"
                         ".byte 0x41,0x0f,0x1f,0xc2; .byte 0x41,0x0f,0x1f,0xc3;"
                         ".byte 0x41,0x0f,0x1f,0xc4; .byte 0x41,0x0f,0x1f,0xc5;"
                         ".byte 0x41,0x0f,0x1f,0xc6; .byte 0x41,0x0f,0x1f,0xc7;");
    return NULL;
}
static void fallback(int sig, siginfo_t *info, void *ctx) {
    (void)sig;
    ucontext_t *uc = ctx;
    const unsigned char *pc = (void *)uc->uc_mcontext->__ss.__rip;
    if (expecting_raise) { ++raised_seen; return; }
    if (pc[0] == 0x0f && pc[1] == 0x0b) {
        ++ud2_seen; uc->uc_mcontext->__ss.__rip += 2; return;
    }
    size_t prefix = pc[0] == 0x41 ? 1 : 0;
    if (pc[prefix] == 0x0f && pc[prefix+1] == 0x1f && (pc[prefix+2] & 0xf8) == 0xc0) {
        ++fallback_nops; uc->uc_mcontext->__ss.__rip += prefix + 3; return;
    }
    char message[160];
    int length = snprintf(message, sizeof message, "unexpected SIGILL code=%d pc=%p bytes=%02x %02x %02x\n",
                          info->si_code, (void *)pc, pc[0], pc[1], pc[2]);
    (void)write(2, message, (size_t)length);
    _exit(99);
}

int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "--lock-nop")) {
        __asm__ volatile(".byte 0xf0,0x90"); return 99;
    }
    if (argc > 1 && !strcmp(argv[1], "--guard-ud2")) {
        size_t page = (size_t)getpagesize();
        unsigned char *p = mmap(NULL, page * 2, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANON, -1, 0);
        if (p == MAP_FAILED) return 98;
        p[page-2] = 0x0f; p[page-1] = 0x0b;
        if (mprotect(p, page, PROT_READ | PROT_EXEC) ||
            mprotect(p + page, page, PROT_NONE)) return 98;
        ((void (*)(void))(p + page - 2))();
        return 99;
    }
    if (argc > 1 && !strcmp(argv[1], "--unhandled")) {
        __asm__ volatile("ud2"); return 99;
    }
    struct sigaction action = {0}, query = {0};
    action.sa_sigaction = fallback;
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGILL, &action, NULL) || sigaction(SIGILL, NULL, &query)) return 2;
    if (query.sa_sigaction != fallback || !(query.sa_flags & SA_SIGINFO)) return 3;
    unsigned long before_flags, after_flags, result;
    errno = EDOM;
    __asm__ volatile(
        "movq $0x12345678, %%rdx\n\t"
        "cmpq %%rdx, %%rdx\n\t"
        "pushfq; popq %0\n\t"
        ".byte 0x0f,0x1f,0xc2\n\t"
        ".byte 0x41,0x0f,0x1f,0xc1\n\t"
        "pushfq; popq %1\n\t"
        "movq %%rdx, %2"
        : "=&r"(before_flags), "=&r"(after_flags), "=&r"(result)
        : : "rdx", "cc", "memory");
    if (before_flags != after_flags || result != 0x12345678 || errno != EDOM) return 4;
    __asm__ volatile(".byte 0x0f,0x1f,0xc0; .byte 0x0f,0x1f,0xc7; ud2");
    expecting_raise = 1;
    raise(SIGILL);
    expecting_raise = 0;
    unsigned long (*get_hits)(void) = dlsym(RTLD_DEFAULT, "nop_bridge_hits");
    unsigned long count = get_hits ? get_hits() : 0;
    printf("bridge=%s hits=%lu fallback_nops=%d ud2_forwarded=%d raised_forwarded=%d state_preserved=yes\n",
           get_hits ? "loaded" : "absent", count, fallback_nops, ud2_seen, raised_seen);
    if (ud2_seen != 1 || raised_seen != 1) return 5;
    if (get_hits && fallback_nops) return 6;
    if (argc > 1 && !strcmp(argv[1], "--threads")) {
        if (!get_hits) return 7;
        worker(NULL);
        unsigned long per_worker = get_hits() - count;
        count = get_hits();
        pthread_t threads[8];
        for (int i = 0; i < 8; ++i)
            if (pthread_create(&threads[i], NULL, worker, NULL)) return 8;
        for (int i = 0; i < 8; ++i)
            if (pthread_join(threads[i], NULL)) return 9;
        unsigned long additional = get_hits() - count;
        printf("threaded_nops=%lu threads=8\n", additional);
        if (additional != per_worker * 8) return 10;
    }
    if (argc > 1 && !strcmp(argv[1], "--exhaustion")) {
        if (!get_hits) return 11;
        unsigned installed = 0;
        while (installed < 40 && sigaction(SIGILL, &action, NULL) == 0) ++installed;
        if (installed != 30 || errno != ENOMEM) return 12;
        if (sigaction(SIGILL, NULL, &query) || query.sa_sigaction != fallback) return 13;
        __asm__ volatile(".byte 0x0f,0x1f,0xc2; ud2");
        printf("exhaustion=fail_closed handler_preserved=yes\n");
    }
    if (argc > 1 && !strcmp(argv[1], "--reset-hand")) {
        action.sa_flags |= SA_RESETHAND;
        if (sigaction(SIGILL, &action, NULL)) return 14;
        __asm__ volatile(".byte 0x0f,0x1f,0xc2");
        if (sigaction(SIGILL, NULL, &query) || query.sa_sigaction != fallback ||
            !(query.sa_flags & SA_RESETHAND)) return 15;
        __asm__ volatile("ud2");
        if (sigaction(SIGILL, NULL, &query) || query.sa_handler != SIG_DFL) return 16;
        __asm__ volatile("ud2");
        return 17;
    }
    if (argc > 1 && !strcmp(argv[1], "--simple-handler")) {
        struct sigaction previous;
        action.sa_handler = simple_handler;
        action.sa_flags = 0;
        sigaddset(&action.sa_mask, SIGUSR1);
        if (sigaction(SIGILL, &action, &previous) || previous.sa_sigaction != fallback) return 18;
        if (sigaction(SIGILL, NULL, &query) || query.sa_handler != simple_handler ||
            query.sa_flags & SA_SIGINFO || !sigismember(&query.sa_mask, SIGUSR1)) return 19;
        raise(SIGILL);
        if (raised_seen != 2 || sigaction(SIGILL, &previous, NULL)) return 20;
        puts("simple_handler_and_restore=pass");
    }
    return 0;
}
