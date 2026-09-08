/* SPDX-License-Identifier: LGPL-2.1-or-later */
#if !defined(__APPLE__) || !defined(__x86_64__)
#error Build this experimental bridge for macOS x86_64.
#endif
#include <errno.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ucontext.h>
#include <unistd.h>
#include "nop_decode.h"
#include "priv_decode.h"

/* Each installed trampoline owns an immutable sigaction. This avoids a
 * global mutable 'previous handler' racing signals on other threads.
 * Slots are never recycled because an in-flight signal may still use one. */
#define EACH_SLOT(X) \
 X(0) X(1) X(2) X(3) X(4) X(5) X(6) X(7) \
 X(8) X(9) X(10) X(11) X(12) X(13) X(14) X(15) \
 X(16) X(17) X(18) X(19) X(20) X(21) X(22) X(23) \
 X(24) X(25) X(26) X(27) X(28) X(29) X(30) X(31)
static struct sigaction actions[33]; /* slot 32 is permanent SIG_DFL */
static _Atomic unsigned next_slot;
static _Atomic unsigned long hits;
static mach_port_t self_task;
static int trace_enabled;
static int privileged_enabled;
static void default_handler(int, siginfo_t *, void *);
_Static_assert(ATOMIC_LONG_LOCK_FREE == 2, "signal counter must be lock free");

unsigned long nop_bridge_hits(void) {
    return atomic_load_explicit(&hits, memory_order_relaxed);
}

static void dispatch(unsigned slot, int sig, siginfo_t *info, void *context) {
    const int saved_errno = errno;
    ucontext_t *uc = context;
    if (sig == SIGILL && info && info->si_code > 0 && uc && uc->uc_mcontext) {
        uint8_t bytes[4];
        mach_vm_size_t copied = 0;
        const uint64_t pc = uc->uc_mcontext->__ss.__rip;
        /* A bounded Mach read avoids recursively faulting on an unreadable
         * instruction or a page boundary. No memory allocation is involved.
         * This is Darwin-specific, not a claim of POSIX signal portability. */
        size_t length = 0;
        size_t privileged_length = 0;
        if (mach_vm_read_overwrite(self_task, pc, 3,
                                  (mach_vm_address_t)bytes, &copied) == KERN_SUCCESS && copied == 3) {
            size_t available = 3;
            if ((bytes[0] == 0x41 || (privileged_enabled && (bytes[0] & 0xf0) == 0x40)) &&
                mach_vm_read_overwrite(self_task, pc + 3, 1,
                                      (mach_vm_address_t)(bytes + 3), &copied) == KERN_SUCCESS && copied == 1)
                available = 4;
            length = nop_register_length(bytes, available);
            if (privileged_enabled) privileged_length = privileged_control_move_length(bytes, available);
        }
        if (length) {
            uc->uc_mcontext->__ss.__rip = pc + length;
            atomic_fetch_add_explicit(&hits, 1, memory_order_relaxed);
            if (trace_enabled) {
                static const char message[] = "[nop-bridge] emulated register NOP\n";
                (void)write(STDERR_FILENO, message, sizeof message - 1);
            }
            errno = saved_errno;
            return;
        }
        if (privileged_length && uc->uc_mcontext->__es.__trapno == 6) {
            /* Preserve RIP and every register. Forward the real fault as #GP
             * so Wine can deliver EXCEPTION_PRIV_INSTRUCTION to the caller. */
            uc->uc_mcontext->__es.__trapno = 13;
            uc->uc_mcontext->__es.__err = 0;
            if (trace_enabled) {
                static const char message[] = "[nop-bridge] classified privileged control-register fault\n";
                (void)write(STDERR_FILENO, message, sizeof message - 1);
            }
        }
    }
    const struct sigaction *original = &actions[slot];
    if (original->sa_flags & SA_RESETHAND) {
        struct sigaction def = actions[32];
        def.sa_flags = SA_SIGINFO;
        def.sa_sigaction = default_handler;
        sigaction(sig, &def, NULL);
    }
    errno = saved_errno;
    if (original->sa_handler == SIG_IGN) return;
    if (original->sa_handler == SIG_DFL) {
        /* Restore default and re-raise on this thread. SIGILL is normally
         * blocked here, so delivery occurs when the trampoline returns. */
        struct sigaction def = {0};
        def.sa_handler = SIG_DFL;
        sigemptyset(&def.sa_mask);
        sigaction(sig, &def, NULL);
        raise(sig);
        return;
    }
    if (original->sa_flags & SA_SIGINFO)
        original->sa_sigaction(sig, info, context);
    else
        original->sa_handler(sig);
}
static void default_handler(int s, siginfo_t *i, void *c) { dispatch(32, s, i, c); }
#define DECLARE_SLOT(n) \
 static void handler_##n(int s, siginfo_t *i, void *c) { dispatch(n, s, i, c); }
EACH_SLOT(DECLARE_SLOT)
#define SLOT_ADDRESS(n) handler_##n,
static void (*const handlers[])(int, siginfo_t *, void *) = {
    EACH_SLOT(SLOT_ADDRESS)
};

static void unwrap(struct sigaction *action) {
    if (!(action->sa_flags & SA_SIGINFO)) return;
    if (action->sa_sigaction == default_handler) { *action = actions[32]; return; }
    for (unsigned i = 0; i < 32; ++i) {
        if (action->sa_sigaction == handlers[i]) {
            *action = actions[i];
            return;
        }
    }
}

static int bridge_sigaction(int sig, const struct sigaction *action,
                            struct sigaction *old_action) {
    if (sig != SIGILL) return sigaction(sig, action, old_action);
    struct sigaction replacement;
    if (action) {
        unsigned slot = atomic_load_explicit(&next_slot, memory_order_relaxed);
        do {
            if (slot >= 32) { errno = ENOMEM; return -1; }
        } while (!atomic_compare_exchange_weak_explicit(&next_slot, &slot, slot + 1,
                     memory_order_relaxed, memory_order_relaxed));
        replacement = *action;
        unwrap(&replacement);
        actions[slot] = replacement;
        replacement.sa_flags |= SA_SIGINFO;
        /* A successfully emulated NOP must not consume a one-shot handler. */
        replacement.sa_flags &= ~SA_RESETHAND;
        replacement.sa_sigaction = handlers[slot];
    }
    int rc = sigaction(sig, action ? &replacement : NULL, old_action);
    if (rc == 0 && old_action) unwrap(old_action);
    return rc;
}

/* Calls within this image go to the original symbol; dyld interposes calls
 * from other images, including ntdll when it installs Wine's signal handler. */
__attribute__((used, section("__DATA,__interpose")))
static const struct { const void *replacement; const void *original; } interpose = {
    (const void *)bridge_sigaction, (const void *)sigaction
};

__attribute__((constructor)) static void initialize(void) {
    self_task = mach_task_self();
    actions[32].sa_handler = SIG_DFL;
    sigemptyset(&actions[32].sa_mask);
    trace_enabled = getenv("NOP_BRIDGE_TRACE") != NULL;
    privileged_enabled = getenv("NOP_BRIDGE_PRIVILEGED") != NULL;
    struct sigaction current;
    if (sigaction(SIGILL, NULL, &current) ||
        bridge_sigaction(SIGILL, &current, NULL)) {
        static const char message[] = "[nop-bridge] initialization failed\n";
        (void)write(STDERR_FILENO, message, sizeof message - 1);
        _exit(125);
    }
}
