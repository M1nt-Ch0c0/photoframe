/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include "photoframe.h"
#include "photoframe_internal.h"
#include "pattern.h"
#ifndef COLOR_TEST_REVERSE
#define COLOR_TEST_REVERSE 0
#endif
extern const uint8_t *photoframe_host_png_data(void);
extern size_t photoframe_host_png_size(void);
extern void photoframe_host_report_result(int result);

/* A separate application entry and ELF: valid input triggers a calibration
 * chart instead of showing the supplied picture. Bad input never touches I/O. */
int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    uint8_t *wire = NULL;
    int result = photoframe_decode_png(photoframe_host_png_data(),
                                      photoframe_host_png_size(), &wire);
    if (result == PHOTOFRAME_OK) {
        color_test_pattern(wire, COLOR_TEST_REVERSE);
        result = photoframe_e6_render(wire, PHOTOFRAME_FRAME_BYTES);
    }
    free(wire);
    photoframe_host_report_result(result);
    return result;
}
