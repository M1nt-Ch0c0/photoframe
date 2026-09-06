/* SPDX-License-Identifier: MIT */
#include "photopainter_app.h"
APP_MANIFEST("lifecycle","Lifecycle timer test",0,0);
static uint32_t ticks;
int main(int argc,char **argv) {
    (void)argc;(void)argv;
    const app_context_v2_t *c=app_host_context_v2();if(!c)return APP_ERR_RUNTIME;
    int r=APP_OK;
    if(c->event==APP_EVENT_START){ticks=0;r=app_host_timer_v2(1,1000);}
    else if(c->event==APP_EVENT_TIMER) {
        ++ticks;(void)app_host_monotonic_ms_v2();(void)app_host_wall_time_v2();
        r=app_host_cache_put_v2(1,(const uint8_t *)&ticks,sizeof(ticks));
    } else if(c->event==APP_EVENT_STOP)r=app_host_timer_v2(1,0);
    else if(c->event==APP_EVENT_INPUT)r=APP_ERR_UNSUPPORTED;
    app_complete(c,r,c->event==APP_EVENT_START&&r==APP_OK?APP_READY:0);return r;
}
