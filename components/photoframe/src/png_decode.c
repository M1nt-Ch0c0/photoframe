/*
 * SPDX-FileCopyrightText: 2026 M1nt-Ch0c0
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#include <png.h>

#include "photoframe.h"
#include "photoframe_internal.h"

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t offset;
} png_memory_reader_t;

typedef struct {
    bool failed;
} png_allocator_t;

static void png_read_memory(png_structp png_ptr,
                            png_bytep output,
                            png_size_t count)
{
    png_memory_reader_t *reader = png_get_io_ptr(png_ptr);
    if (reader == NULL || reader->offset > reader->size ||
        count > reader->size - reader->offset) {
        png_error(png_ptr, "truncated PNG input");
        return;
    }

    memcpy(output, reader->data + reader->offset, count);
    reader->offset += count;
}

static png_voidp png_module_malloc(png_structp png_ptr, png_alloc_size_t size)
{
    png_allocator_t *allocator = png_get_mem_ptr(png_ptr);
    void *memory = malloc((size_t)size);
    if (memory == NULL && allocator != NULL) {
        allocator->failed = true;
    }
    return memory;
}

static void png_module_free(png_structp png_ptr, png_voidp memory)
{
    (void)png_ptr;
    free(memory);
}

static void png_module_error(png_structp png_ptr, png_const_charp message)
{
    (void)message;
    png_longjmp(png_ptr, 1);
}

static void png_module_warning(png_structp png_ptr, png_const_charp message)
{
    (void)png_ptr;
    (void)message;
}

static int color_code(uint8_t red, uint8_t green, uint8_t blue)
{
    if (red == 0x00 && green == 0x00 && blue == 0x00) {
        return 0;
    }
    if (red == 0xff && green == 0xff && blue == 0xff) {
        return 1;
    }
    if (red == 0xff && green == 0xff && blue == 0x00) {
        return 2;
    }
    if (red == 0xff && green == 0x00 && blue == 0x00) {
        return 3;
    }
    if (red == 0x00 && green == 0x00 && blue == 0xff) {
        return 5;
    }
    if (red == 0x00 && green == 0xff && blue == 0x00) {
        return 6;
    }
    return -1;
}

static int decode_png(const uint8_t *png_data,
                          size_t png_size,
                          uint8_t **wire_data, bool logical)
{
    if (png_data == NULL || wire_data == NULL || png_size < 8u) {
        return PHOTOFRAME_ERR_ARGUMENT;
    }
    *wire_data = NULL;

    if (png_sig_cmp((png_const_bytep)png_data, 0, 8) != 0) {
        return PHOTOFRAME_ERR_PNG;
    }

    /* The allocation callbacks can mutate this state before libpng performs
     * a longjmp.  Keep it outside the function's automatic storage so its
     * value remains defined after setjmp/longjmp. */
    png_allocator_t *allocator = calloc(1, sizeof(*allocator));
    if (allocator == NULL) {
        return PHOTOFRAME_ERR_ALLOCATION;
    }
    png_structp png_ptr = png_create_read_struct_2(
        PNG_LIBPNG_VER_STRING,
        NULL,
        png_module_error,
        png_module_warning,
        allocator,
        png_module_malloc,
        png_module_free);
    if (png_ptr == NULL) {
        free(allocator);
        return PHOTOFRAME_ERR_ALLOCATION;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        free(allocator);
        return PHOTOFRAME_ERR_ALLOCATION;
    }

    png_memory_reader_t reader = {
        .data = png_data,
        .size = png_size,
        .offset = 0,
    };
    uint8_t *volatile row = NULL;
    uint8_t *volatile wire = NULL;
    int result = PHOTOFRAME_ERR_PNG;

    if (setjmp(png_jmpbuf(png_ptr)) != 0) {
        result = allocator->failed ? PHOTOFRAME_ERR_ALLOCATION
                                   : PHOTOFRAME_ERR_PNG;
        goto cleanup;
    }

    png_set_read_fn(png_ptr, &reader, png_read_memory);
    png_read_info(png_ptr, info_ptr);

    png_uint_32 width = 0;
    png_uint_32 height = 0;
    int bit_depth = 0;
    int color_type = 0;
    int interlace_type = 0;
    int compression_type = 0;
    int filter_method = 0;
    if (png_get_IHDR(png_ptr,
                     info_ptr,
                     &width,
                     &height,
                     &bit_depth,
                     &color_type,
                     &interlace_type,
                     &compression_type,
                     &filter_method) == 0) {
        result = PHOTOFRAME_ERR_PNG;
        goto cleanup;
    }

    if (width != PHOTOFRAME_WIDTH || height != PHOTOFRAME_HEIGHT ||
        interlace_type != PNG_INTERLACE_NONE) {
        result = PHOTOFRAME_ERR_GEOMETRY;
        goto cleanup;
    }

    if (bit_depth > 8 ||
        (color_type != PNG_COLOR_TYPE_GRAY &&
         color_type != PNG_COLOR_TYPE_GRAY_ALPHA &&
         color_type != PNG_COLOR_TYPE_PALETTE &&
         color_type != PNG_COLOR_TYPE_RGB &&
         color_type != PNG_COLOR_TYPE_RGB_ALPHA)) {
        result = PHOTOFRAME_ERR_PNG;
        goto cleanup;
    }

    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png_ptr);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(png_ptr);
    }
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS) != 0) {
        png_set_tRNS_to_alpha(png_ptr);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png_ptr);
    }

    png_read_update_info(png_ptr, info_ptr);
    int channels = png_get_channels(png_ptr, info_ptr);
    png_size_t row_bytes = png_get_rowbytes(png_ptr, info_ptr);
    if ((channels != 3 && channels != 4) ||
        row_bytes != (png_size_t)PHOTOFRAME_WIDTH * (png_size_t)channels) {
        result = PHOTOFRAME_ERR_PNG;
        goto cleanup;
    }

    row = malloc((size_t)row_bytes);
    wire = malloc(logical ? PHOTOFRAME_WIDTH*PHOTOFRAME_HEIGHT : PHOTOFRAME_FRAME_BYTES);
    if (row == NULL || wire == NULL) {
        result = PHOTOFRAME_ERR_ALLOCATION;
        goto cleanup;
    }

    for (png_uint_32 y = 0; y < PHOTOFRAME_HEIGHT; ++y) {
        png_read_row(png_ptr, (png_bytep)row, NULL);
        for (png_uint_32 x = 0; x < PHOTOFRAME_WIDTH; x += 2u) {
            size_t first_offset = (size_t)x * (size_t)channels;
            size_t second_offset = first_offset + (size_t)channels;
            if ((channels == 4 &&
                 (row[first_offset + 3u] != 0xff ||
                  row[second_offset + 3u] != 0xff))) {
                result = PHOTOFRAME_ERR_COLOR;
                goto cleanup;
            }

            int first = color_code(row[first_offset],
                                   row[first_offset + 1u],
                                   row[first_offset + 2u]);
            int second = color_code(row[second_offset],
                                    row[second_offset + 1u],
                                    row[second_offset + 2u]);
            if (first < 0 || second < 0) {
                result = PHOTOFRAME_ERR_COLOR;
                goto cleanup;
            }

            size_t source_byte =
                ((size_t)y * PHOTOFRAME_WIDTH + (size_t)x) / 2u;
            uint8_t packed = (uint8_t)(((uint8_t)first << 4) |
                                       (uint8_t)second);
            if (logical) {
                wire[source_byte*2] = first > 3 ? first-1 : first;
                wire[source_byte*2+1] = second > 3 ? second-1 : second;
            } else {
                wire[PHOTOFRAME_FRAME_BYTES - 1u - source_byte] =
                    (uint8_t)((packed << 4) | (packed >> 4));
            }
        }
    }

    png_read_end(png_ptr, info_ptr);
    if (reader.offset != reader.size) {
        result = PHOTOFRAME_ERR_PNG;
        goto cleanup;
    }

    *wire_data = (uint8_t *)wire;
    wire = NULL;
    result = PHOTOFRAME_OK;

cleanup:
    free((void *)row);
    free((void *)wire);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    free(allocator);
    return result;
}

int photoframe_decode_png(const uint8_t *data,size_t size,uint8_t **out) {
    return decode_png(data,size,out,false);
}
int photoframe_decode_indexed(const uint8_t *data,size_t size,uint8_t **out) {
    return decode_png(data,size,out,true);
}
