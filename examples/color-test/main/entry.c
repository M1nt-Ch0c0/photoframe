/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include "photopainter_app.h"
#ifndef COLOR_TEST_REVERSE
#define COLOR_TEST_REVERSE 0
#endif
APP_MANIFEST("color-test","Six color test",0,APP_REQUIRES_DISPLAY);
int main(int argc,char **argv) {
    (void)argc;(void)argv;
    const app_context_v2_t *c=app_host_context_v2();if(!c)return APP_ERR_RUNTIME;
    int r=APP_OK;
    if(c->event==APP_EVENT_START) {
        uint8_t *frame=malloc(APP_FRAME_BYTES);
        if(!frame)r=APP_ERR_MEMORY;
        else {
            for(unsigned y=0;y<APP_FRAME_HEIGHT;y++)for(unsigned x=0;x<APP_FRAME_WIDTH;x++) {
                unsigned color=x*6/APP_FRAME_WIDTH;
                frame[y*APP_FRAME_WIDTH+x]=COLOR_TEST_REVERSE?5-color:color;
            }
            r=app_host_display_v2(frame,APP_FRAME_BYTES);free(frame);
        }
    } else if(c->event==APP_EVENT_INPUT)r=APP_ERR_UNSUPPORTED;
    app_complete(c,r,c->event==APP_EVENT_START&&r==APP_OK?APP_READY:0);return r;
}
