/*
 * SPDX-FileCopyrightText: 2026 M1nt-Ch0c0
 * SPDX-License-Identifier: MIT
 */

#include <stdlib.h>

#include "photoframe.h"
#include "photoframe_internal.h"

int photoframe_render_png(const uint8_t *png_data, size_t png_size)
{
    if (png_data == NULL || png_size == 0 ||
        png_size > PHOTOFRAME_MAX_PNG_BYTES) {
        return PHOTOFRAME_ERR_ARGUMENT;
    }

    uint8_t *wire_data = NULL;
    int result = photoframe_decode_png(png_data, png_size, &wire_data);
    if (result != PHOTOFRAME_OK) {
        return result;
    }

    /* No GPIO or SPI function is reachable before decoding and validation have
     * completed and the full 192000-byte wire image has been allocated. */
    result = photoframe_e6_render(wire_data, PHOTOFRAME_FRAME_BYTES);
    free(wire_data);
    return result;
}
