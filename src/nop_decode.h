/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef NOP_DECODE_H
#define NOP_DECODE_H
#include <stddef.h>
#include <stdint.h>
/* Deliberately narrow: 0F 1F /0, ModRM.mod == 3, optionally REX.B (41).
 * No LOCK/REP/operand-size or other REX prefixes are accepted. */
static inline size_t nop_register_length(const uint8_t *p, size_t n) {
    size_t prefix = n && p[0] == 0x41 ? 1 : 0;
    return n >= prefix + 3 && p[prefix] == 0x0f && p[prefix+1] == 0x1f &&
           (p[prefix+2] & 0xf8) == 0xc0 ? prefix + 3 : 0;
}
#endif
