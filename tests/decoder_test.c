/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <assert.h>
#include <stdio.h>
#include "../src/nop_decode.h"
int main(void) {
    unsigned recognized = 0, prefixed = 0;
    for (unsigned value = 0; value < 0x1000000; ++value) {
        uint8_t b[3] = {(uint8_t)(value >> 16), (uint8_t)(value >> 8), (uint8_t)value};
        size_t n = nop_register_length(b, 3);
        if (n) {
            assert(value >= 0x0f1fc0 && value <= 0x0f1fc7 && n == 3);
            ++recognized;
        }
        assert(nop_register_length(b, 0) == 0);
        assert(nop_register_length(b, 1) == 0);
        assert(nop_register_length(b, 2) == 0);
        uint8_t rex[4] = {0x41, b[0], b[1], b[2]};
        if (nop_register_length(rex, 4)) {
            assert(value >= 0x0f1fc0 && value <= 0x0f1fc7);
            ++prefixed;
        }
        assert(nop_register_length(rex, 3) == 0);
    }
    assert(recognized == 8 && prefixed == 8);
    puts("decoder: 33554432 candidate sequences checked; accepted 8 bare + 8 REX.B NOPs");
    return 0;
}
