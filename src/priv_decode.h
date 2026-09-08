/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef PRIV_DECODE_H
#define PRIV_DECODE_H
#include <stddef.h>
#include <stdint.h>

/* Valid register-form MOV to/from CR0, CR2, CR3, CR4, or CR8 raises #GP
 * at CPL3. Undefined control registers and other opcodes stay untouched. */
static inline size_t privileged_control_move_length(const uint8_t *p, size_t n)
{
    unsigned offset = 0, rex = 0;
    if (!n) return 0;
    if ((p[0] & 0xf0) == 0x40) { rex = p[0]; offset = 1; }
    if (n < offset + 3 || p[offset] != 0x0f ||
        (p[offset + 1] != 0x20 && p[offset + 1] != 0x22) ||
        (p[offset + 2] & 0xc0) != 0xc0) return 0;
    unsigned cr = ((p[offset + 2] >> 3) & 7) | ((rex & 4) << 1);
    return cr == 0 || cr == 2 || cr == 3 || cr == 4 || cr == 8 ? offset + 3 : 0;
}
#endif
