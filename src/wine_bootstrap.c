/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Uses Wine's public loader ABI and address reservations described by
 * loader/main.c (Alexandre Julliard and Wine contributors, LGPL-2.1-or-later).
 * This executable is built locally; no Wine/CrossOver binaries are bundled.
 */
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/mman.h>
#include <unistd.h>
#include <CoreFoundation/CoreFoundation.h>

struct wine_preload_info { void *addr; size_t size; };
__asm__(".zerofill WINE_RESERVE,WINE_RESERVE");
static char low_addresses[0x1fffff000]
    __attribute__((section("WINE_RESERVE,WINE_RESERVE")));
__asm__(".zerofill WINE_TOP_DOWN,WINE_TOP_DOWN");
static char top_addresses[0x001ff0000]
    __attribute__((section("WINE_TOP_DOWN,WINE_TOP_DOWN")));
static const struct wine_preload_info reserved[] = {
    {low_addresses, sizeof low_addresses},
    {top_addresses, sizeof top_addresses},
    {NULL, 0}
};
__attribute__((visibility("default")))
const struct wine_preload_info *volatile wine_main_preload_info = reserved;

int main(int argc, char **argv) {
    const char *log = getenv("NOP_BRIDGE_LOG");
    if (log && log[0] == '/' && !freopen(log, "a", stderr)) return 125;
    char *app_argv[] = {argv[0], getenv("NOP_BRIDGE_APP_PROGRAM"), NULL};
    if (argc == 1 && app_argv[1]) {
        const char *cwd = getenv("NOP_BRIDGE_APP_CWD");
        if (cwd && chdir(cwd)) { perror("bootstrap: application directory"); return 125; }
        argc = 2;
        argv = app_argv;
    }
    const char *ntdll = getenv("NOP_BRIDGE_NTDLL");
    if (!ntdll || ntdll[0] != '/') {
        fputs("bootstrap: NOP_BRIDGE_NTDLL must be an absolute ntdll.so path\n", stderr);
        return 125;
    }
    const struct wine_preload_info *regions = wine_main_preload_info;
    for (size_t i = 0; regions[i].size; ++i) {
        void *p = mmap(regions[i].addr, regions[i].size, PROT_NONE,
                       MAP_FIXED | MAP_NORESERVE | MAP_PRIVATE | MAP_ANON, -1, 0);
        if (p == MAP_FAILED) { perror("bootstrap: reserve"); return 125; }
    }
    /* Only GUI processes should register under the app's bundle identity.
     * Wine's background services share this loader but have no AX windows. */
    if (argc > 1) {
        const char *base = strrchr(argv[1], '\\');
        if (!base) base = strrchr(argv[1], '/');
        base = base ? base + 1 : argv[1];
        if (!strcasecmp(base, "nikke_launcher.exe") || !strcasecmp(base, "nikke.exe"))
            (void)CFBundleGetMainBundle();
    }
    void *library = dlopen(ntdll, RTLD_NOW | RTLD_GLOBAL);
    if (!library) { fprintf(stderr, "bootstrap: %s\n", dlerror()); return 125; }
    void (*entry)(int, char **) = dlsym(library, "__wine_main");
    if (!entry) { fputs("bootstrap: __wine_main unavailable\n", stderr); return 125; }
    entry(argc, argv);
    return 125;
}
