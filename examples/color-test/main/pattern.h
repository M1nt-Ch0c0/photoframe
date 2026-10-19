/* SPDX-License-Identifier: MIT */
#pragma once
#include <stddef.h>
#include <stdint.h>
/* Panel packing is identical to the shared decoder's 180-degree rotation. */
static inline void color_test_pattern(uint8_t *wire, int reverse)
{
    static const uint8_t colors[] = {0, 1, 2, 3, 5, 6};
    for (size_t y = 0; y < 480; ++y) {
        for (size_t x = 0; x < 800; x += 2) {
            size_t first = x * 6 / 800;
            size_t second = (x + 1) * 6 / 800;
            if (reverse) { first = 5 - first; second = 5 - second; }
            wire[192000 - 1 - (y * 800 + x) / 2] =
                (uint8_t)((colors[second] << 4) | colors[first]);
        }
    }
}
