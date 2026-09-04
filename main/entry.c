/*
 * SPDX-FileCopyrightText: 2026 M1nt-Ch0c0
 * SPDX-License-Identifier: MIT
 */

#include <stddef.h>
#include <stdint.h>

#include "photoframe.h"

#if PHOTOFRAME_APP_ENTRY

/* Resolved by the firmware host when the application ELF is loaded. */
extern const uint8_t *photoframe_host_png_data(void);
extern size_t photoframe_host_png_size(void);
extern void photoframe_host_report_result(int result);

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    const uint8_t *data = photoframe_host_png_data();
    size_t size = photoframe_host_png_size();
    int result = photoframe_render_png(data, size);

    /* esp_elf_request() does not propagate the ELF entry's return value. */
    photoframe_host_report_result(result);
    return result;
}

#else

/* project_so() scans every C source in main; keep this translation unit valid
 * without importing the app-only host ABI into photoframe.so. */
typedef int photoframe_so_entry_translation_unit_is_intentionally_empty;

#endif
