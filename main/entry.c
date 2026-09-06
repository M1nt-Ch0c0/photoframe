/*
 * SPDX-FileCopyrightText: 2026 M1nt-Ch0c0
 * SPDX-License-Identifier: MIT
 */

#include <stddef.h>
#include <stdint.h>

#include "photoframe.h"

#if PHOTOFRAME_APP_ENTRY && PHOTOFRAME_ABI_2
#include "photopainter_app.h"
#include <stdlib.h>
#include <string.h>
APP_MANIFEST("photoframe","Photo frame",APP_INPUT_PNG,APP_REQUIRES_DISPLAY);
#define FRAME_CACHE_VERSION 1u
/* The waiting page and restoration policy belong to the app, never the host. */
static void waiting(uint8_t *frame) {
    static const char letters[]="WAITNGFOREM";
    static const uint8_t glyphs[][7]={
        {17,17,17,21,21,21,10},{14,17,17,31,17,17,17},
        {31,4,4,4,4,4,31},{31,4,4,4,4,4,4},
        {17,25,25,21,19,19,17},{14,17,16,23,17,17,14},
        {31,16,16,30,16,16,16},{14,17,17,17,17,17,14},
        {30,17,17,30,20,18,17},{31,16,16,30,16,16,31},
        {17,27,21,21,17,17,17}
    };
    const char *message="WAITING FOR IMAGE";
    memset(frame,APP_WHITE,APP_FRAME_BYTES);
    const unsigned scale=6,width=17*6*scale;
    unsigned x=(APP_FRAME_WIDTH-width)/2,y=(APP_FRAME_HEIGHT-7*scale)/2;
    for(const char *c=message;*c;c++,x+=6*scale) {
        const char *g=strchr(letters,*c);if(!g)continue;
        for(unsigned row=0;row<7;row++)for(unsigned col=0;col<5;col++)
            if(glyphs[g-letters][row]&(1u<<(4-col)))
                for(unsigned dy=0;dy<scale;dy++)for(unsigned dx=0;dx<scale;dx++)
                    frame[(y+row*scale+dy)*APP_FRAME_WIDTH+x+col*scale+dx]=APP_BLACK;
    }
}
int main(int argc,char **argv) {
    (void)argc;(void)argv;
    const app_context_v2_t *c=app_host_context_v2();if(!c)return APP_ERR_RUNTIME;
    int r=APP_OK;uint8_t *frame=NULL;
    if(c->event==APP_EVENT_START) {
        uint32_t bytes=0;const uint8_t *cached=app_host_cache_get_v2(FRAME_CACHE_VERSION,&bytes);
        if(cached&&bytes==APP_FRAME_BYTES)r=app_host_display_v2(cached,bytes);
        else {
            frame=malloc(APP_FRAME_BYTES);
            if(!frame)r=APP_ERR_MEMORY;
            else {waiting(frame);r=app_host_display_v2(frame,APP_FRAME_BYTES);}
        }
    } else if(c->event==APP_EVENT_INPUT) {
        if(c->input_type!=APP_INPUT_PNG)r=APP_ERR_UNSUPPORTED;
        else r=photoframe_decode_indexed(c->data,c->data_size,&frame);
        if(r==APP_OK) {
            r=app_host_display_v2(frame,APP_FRAME_BYTES);
            /* Display success is independent of optional cache admission. */
            if(r==APP_OK)(void)app_host_cache_put_v2(FRAME_CACHE_VERSION,frame,APP_FRAME_BYTES);
        }
    }
    free(frame);
    app_complete(c,r,c->event==APP_EVENT_START&&r==APP_OK?APP_READY:0);
    return r;
}
#elif PHOTOFRAME_APP_ENTRY

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
