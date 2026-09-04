/*
 * SPDX-FileCopyrightText: 2026 M1nt-Ch0c0
 * SPDX-License-Identifier: MIT
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <png.h>

#include "photoframe.h"
#include "photoframe_internal.h"

typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
} memory_png_t;

typedef enum {
    IMAGE_VALID,
    IMAGE_BAD_COLOR,
    IMAGE_TRANSPARENT,
} image_kind_t;

static int display_calls;
static int display_result;

int photoframe_e6_render(const uint8_t *wire_data, size_t wire_size)
{
    ++display_calls;
    assert(wire_data != NULL);
    assert(wire_size == PHOTOFRAME_FRAME_BYTES);
    assert(wire_data[0] == 0x62); /* final yellow/green pair, rotated */
    assert(wire_data[wire_size - 1] == 0x53); /* first red/blue pair */
    return display_result;
}

static void png_write_memory(png_structp png_ptr,
                             png_bytep bytes,
                             png_size_t count)
{
    memory_png_t *output = png_get_io_ptr(png_ptr);
    if (count > SIZE_MAX - output->size) {
        png_error(png_ptr, "test output overflow");
    }

    size_t needed = output->size + count;
    if (needed > output->capacity) {
        size_t capacity = output->capacity == 0 ? 4096 : output->capacity;
        while (capacity < needed) {
            if (capacity > SIZE_MAX / 2) {
                png_error(png_ptr, "test output overflow");
            }
            capacity *= 2;
        }
        void *data = realloc(output->data, capacity);
        if (data == NULL) {
            png_error(png_ptr, "test output allocation failed");
        }
        output->data = data;
        output->capacity = capacity;
    }

    memcpy(output->data + output->size, bytes, count);
    output->size += count;
}

static void png_flush_memory(png_structp png_ptr)
{
    (void)png_ptr;
}

static void set_rgb_pixel(uint8_t *row,
                          size_t x,
                          uint8_t red,
                          uint8_t green,
                          uint8_t blue,
                          uint8_t alpha,
                          bool has_alpha)
{
    size_t channels = has_alpha ? 4u : 3u;
    row[x * channels] = red;
    row[x * channels + 1u] = green;
    row[x * channels + 2u] = blue;
    if (has_alpha) {
        row[x * channels + 3u] = alpha;
    }
}

static memory_png_t make_rgb_png(png_uint_32 width,
                                 png_uint_32 height,
                                 int interlace,
                                 image_kind_t kind)
{
    memory_png_t output = {0};
    png_structp png_ptr =
        png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    assert(png_ptr != NULL);
    png_infop info_ptr = png_create_info_struct(png_ptr);
    assert(info_ptr != NULL);
    assert(setjmp(png_jmpbuf(png_ptr)) == 0);

    bool has_alpha = kind == IMAGE_TRANSPARENT;
    int color_type = has_alpha ? PNG_COLOR_TYPE_RGB_ALPHA
                               : PNG_COLOR_TYPE_RGB;
    png_set_write_fn(png_ptr, &output, png_write_memory, png_flush_memory);
    png_set_IHDR(png_ptr,
                 info_ptr,
                 width,
                 height,
                 8,
                 color_type,
                 interlace,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);

    size_t channels = has_alpha ? 4u : 3u;
    uint8_t *row = malloc((size_t)width * channels);
    assert(row != NULL);
    int passes = png_set_interlace_handling(png_ptr);
    for (int pass = 0; pass < passes; ++pass) {
        for (png_uint_32 y = 0; y < height; ++y) {
            for (png_uint_32 x = 0; x < width; ++x) {
                set_rgb_pixel(row, x, 0xff, 0xff, 0xff, 0xff, has_alpha);
            }
            if (width >= 2 && y == 0) {
                set_rgb_pixel(row, 0, 0xff, 0x00, 0x00,
                              has_alpha ? 0x00 : 0xff, has_alpha);
                set_rgb_pixel(row, 1, 0x00, 0x00, 0xff, 0xff, has_alpha);
                if (kind == IMAGE_BAD_COLOR) {
                    set_rgb_pixel(row, 0, 0x01, 0x02, 0x03, 0xff, false);
                }
            }
            if (width >= 2 && y + 1u == height) {
                set_rgb_pixel(row, width - 2u,
                              0xff, 0xff, 0x00, 0xff, has_alpha);
                set_rgb_pixel(row, width - 1u,
                              0x00, 0xff, 0x00, 0xff, has_alpha);
            }
            png_write_row(png_ptr, row);
        }
    }

    free(row);
    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return output;
}

static memory_png_t make_palette_png(void)
{
    memory_png_t output = {0};
    png_structp png_ptr =
        png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    assert(png_ptr != NULL);
    png_infop info_ptr = png_create_info_struct(png_ptr);
    assert(info_ptr != NULL);
    assert(setjmp(png_jmpbuf(png_ptr)) == 0);

    png_color palette[6] = {
        {0x00, 0x00, 0x00},
        {0xff, 0xff, 0xff},
        {0xff, 0xff, 0x00},
        {0xff, 0x00, 0x00},
        {0x00, 0x00, 0xff},
        {0x00, 0xff, 0x00},
    };
    png_set_write_fn(png_ptr, &output, png_write_memory, png_flush_memory);
    png_set_IHDR(png_ptr,
                 info_ptr,
                 PHOTOFRAME_WIDTH,
                 PHOTOFRAME_HEIGHT,
                 8,
                 PNG_COLOR_TYPE_PALETTE,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_set_PLTE(png_ptr, info_ptr, palette, 6);
    png_write_info(png_ptr, info_ptr);

    uint8_t row[PHOTOFRAME_WIDTH];
    for (size_t y = 0; y < PHOTOFRAME_HEIGHT; ++y) {
        memset(row, 1, sizeof(row));
        if (y == 0) {
            row[0] = 3;
            row[1] = 4;
        }
        if (y + 1u == PHOTOFRAME_HEIGHT) {
            row[PHOTOFRAME_WIDTH - 2u] = 2;
            row[PHOTOFRAME_WIDTH - 1u] = 5;
        }
        png_write_row(png_ptr, row);
    }

    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return output;
}

static void reset_display_stub(int result)
{
    display_calls = 0;
    display_result = result;
}

static void expect_no_display(const memory_png_t *png, int expected)
{
    reset_display_stub(PHOTOFRAME_OK);
    assert(photoframe_render_png(png->data, png->size) == expected);
    assert(display_calls == 0);
}

int main(void)
{
    assert(photoframe_axp2101_aldo3_voltage_value(0xa3) == 0xbc);
    assert(photoframe_axp2101_aldo3_voltage_value(0x7f) == 0x7c);
    assert(photoframe_axp2101_aldo3_enable_value(0xa1) == 0xa5);

    reset_display_stub(PHOTOFRAME_OK);
    assert(photoframe_render_png(NULL, 1) == PHOTOFRAME_ERR_ARGUMENT);
    assert(display_calls == 0);

    const uint8_t not_png[8] = {0};
    reset_display_stub(PHOTOFRAME_OK);
    assert(photoframe_render_png(not_png, sizeof(not_png)) ==
           PHOTOFRAME_ERR_PNG);
    assert(display_calls == 0);

    memory_png_t valid = make_rgb_png(PHOTOFRAME_WIDTH,
                                      PHOTOFRAME_HEIGHT,
                                      PNG_INTERLACE_NONE,
                                      IMAGE_VALID);
    reset_display_stub(PHOTOFRAME_OK);
    assert(photoframe_render_png(valid.data, valid.size) == PHOTOFRAME_OK);
    assert(display_calls == 1);

    reset_display_stub(PHOTOFRAME_ERR_BUSY_TIMEOUT);
    assert(photoframe_render_png(valid.data, valid.size) ==
           PHOTOFRAME_ERR_BUSY_TIMEOUT);
    assert(display_calls == 1);

    memory_png_t palette = make_palette_png();
    reset_display_stub(PHOTOFRAME_OK);
    assert(photoframe_render_png(palette.data, palette.size) == PHOTOFRAME_OK);
    assert(display_calls == 1);

    memory_png_t wrong_size = make_rgb_png(PHOTOFRAME_WIDTH - 1u,
                                           PHOTOFRAME_HEIGHT,
                                           PNG_INTERLACE_NONE,
                                           IMAGE_VALID);
    expect_no_display(&wrong_size, PHOTOFRAME_ERR_GEOMETRY);

    memory_png_t interlaced = make_rgb_png(PHOTOFRAME_WIDTH,
                                           PHOTOFRAME_HEIGHT,
                                           PNG_INTERLACE_ADAM7,
                                           IMAGE_VALID);
    expect_no_display(&interlaced, PHOTOFRAME_ERR_GEOMETRY);

    memory_png_t bad_color = make_rgb_png(PHOTOFRAME_WIDTH,
                                          PHOTOFRAME_HEIGHT,
                                          PNG_INTERLACE_NONE,
                                          IMAGE_BAD_COLOR);
    expect_no_display(&bad_color, PHOTOFRAME_ERR_COLOR);

    memory_png_t transparent = make_rgb_png(PHOTOFRAME_WIDTH,
                                            PHOTOFRAME_HEIGHT,
                                            PNG_INTERLACE_NONE,
                                            IMAGE_TRANSPARENT);
    expect_no_display(&transparent, PHOTOFRAME_ERR_COLOR);

    memory_png_t truncated = valid;
    truncated.size -= 10u;
    expect_no_display(&truncated, PHOTOFRAME_ERR_PNG);

    memory_png_t trailing = {
        .data = malloc(valid.size + 1u),
        .size = valid.size + 1u,
        .capacity = valid.size + 1u,
    };
    assert(trailing.data != NULL);
    memcpy(trailing.data, valid.data, valid.size);
    trailing.data[valid.size] = 0;
    expect_no_display(&trailing, PHOTOFRAME_ERR_PNG);

    free(valid.data);
    free(palette.data);
    free(wrong_size.data);
    free(interlaced.data);
    free(bad_color.data);
    free(transparent.data);
    free(trailing.data);
    puts("photoframe native tests passed");
    return 0;
}
