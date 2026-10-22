/* SPDX-License-Identifier: MIT */
#include "photopainter_app.h"
APP_MANIFEST_BLE("ble-echo","Authenticated BLE echo",0,0);

/* Service a4fe1000-1536-4c88-a2ce-32ecdaac0471; characteristic ...1001...
 * The host copies this descriptor. It never calls a pointer into this ELF. */
static const app_ble_service_v2_t service={
    .size=sizeof(app_ble_service_v2_t),.count=1,
    .uuid={0xa4,0xfe,0x10,0x00,0x15,0x36,0x4c,0x88,0xa2,0xce,0x32,0xec,0xda,0xac,0x04,0x71},
    .characteristics={{
        .uuid={0xa4,0xfe,0x10,0x01,0x15,0x36,0x4c,0x88,0xa2,0xce,0x32,0xec,0xda,0xac,0x04,0x71},
        .properties=APP_BLE_READ|APP_BLE_WRITE|APP_BLE_NOTIFY,.max_bytes=APP_BLE_MAX_VALUE}}
};
int main(int argc,char **argv) {
    (void)argc;(void)argv;
    const app_context_v2_t *c=app_host_context_v2();if(!c)return APP_ERR_RUNTIME;
    int result=APP_OK;
    if(c->event==APP_EVENT_START) {
        result=app_host_ble_register_v2(&service);
        if(result==APP_OK)result=app_host_ble_set_v2(0,(const uint8_t *)"ready",5);
    } else if(c->event==APP_EVENT_BLE) {
        if(c->data_size!=sizeof(app_ble_event_v2_t) || !c->data)result=APP_ERR_ARGUMENT;
        else {
            const app_ble_event_v2_t *event=(const app_ble_event_v2_t *)c->data;
            if(event->type==APP_BLE_WRITTEN)result=app_host_ble_set_v2(event->characteristic,event->data,event->data_size);
            else if(event->type==APP_BLE_ERROR)result=app_host_ble_unregister_v2();
        }
    } else if(c->event==APP_EVENT_STOP)result=app_host_ble_unregister_v2();
    else if(c->event==APP_EVENT_INPUT)result=APP_ERR_UNSUPPORTED;
    app_complete(c,result,c->event==APP_EVENT_START && result==APP_OK?APP_READY:0);
    return result;
}
