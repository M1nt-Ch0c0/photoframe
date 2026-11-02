/* SPDX-License-Identifier: MIT */
#if PHOTOFRAME_APP_ENTRY && PHOTOFRAME_ABI_2
#include "ble_image.h"
#include "image_transfer.h"
#include "photoframe.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* a4fe2000-1536-4c88-a2ce-32ecdaac0471, command ...2001, status ...2002 */
static const app_ble_service_v2_t service={
    .size=sizeof(app_ble_service_v2_t),.count=2,
    .uuid={0xa4,0xfe,0x20,0,0x15,0x36,0x4c,0x88,0xa2,0xce,0x32,0xec,0xda,0xac,4,0x71},
    .characteristics={
        {.uuid={0xa4,0xfe,0x20,1,0x15,0x36,0x4c,0x88,0xa2,0xce,0x32,0xec,0xda,0xac,4,0x71},.properties=APP_BLE_WRITE,.max_bytes=128},
        {.uuid={0xa4,0xfe,0x20,2,0x15,0x36,0x4c,0x88,0xa2,0xce,0x32,0xec,0xda,0xac,4,0x71},.properties=APP_BLE_READ,.max_bytes=64}
    }
};
/* The same transaction core serves every transport. Only adapters hold peer IDs. */
static image_transfer transfer;
enum {INPUT_GATT,INPUT_COC,INPUT_CHANNEL};
static uint32_t input_session,input_transport,coc_session;
static uint8_t channel_buffer[APP_CHANNEL_MAX_MESSAGE];
static uint8_t coc_buffer[4096],coc_reply[64];
static bool coc_reply_pending;
static uint64_t now_ms(void *context) {(void)context;return app_host_monotonic_ms_v2();}
static int processing(void *context,const uint8_t receipt[64]) {
    (void)context;return app_host_ble_set_v2(IMAGE_STATUS,receipt,64);
}
static unsigned render(void *context,const uint8_t *png,uint32_t bytes) {
    (void)context;uint8_t *frame=NULL;
    int r=photoframe_decode_indexed(png,bytes,&frame);
    if(r!=APP_OK){free(frame);return r==APP_ERR_MEMORY?IMAGE_MEMORY:IMAGE_PNG;}
    r=app_host_display_v2(frame,APP_FRAME_BYTES);
    if(r==APP_OK)(void)app_host_cache_put_v2(1,frame,APP_FRAME_BYTES);
    free(frame);return r==APP_OK?IMAGE_OK:IMAGE_DISPLAY;
}
static const image_transfer_ops operations={now_ms,processing,render};
static int publish(void) {return app_host_ble_set_v2(IMAGE_STATUS,transfer.reply,64);}
int ble_image_receiving(void) {return transfer.input!=NULL;}
int ble_image_start(void) {
    image_transfer_clear(&transfer);image_transfer_init(&transfer,&operations,NULL);input_session=input_transport=0;
    int r=app_host_ble_register_v2(&service);
    if(r==APP_OK)r=publish();
    if(r==APP_OK)r=app_host_timer_v2(1,1000);
    if(r==APP_OK){
        const app_coc_config_v22_t c={sizeof(c),4096,4,4};
        r=app_host_coc_open_v22(&c);
    }
    if(r==APP_OK){
        app_channel_config_v23_t c={.size=sizeof(c),.max_message_bytes=APP_CHANNEL_MAX_MESSAGE,.max_reply_bytes=64};
        memcpy(c.service,service.uuid,16);r=app_host_channel_register_v23(&c);
    }
    if(r!=APP_OK)ble_image_stop();
    return r;
}
void ble_image_stop(void) {
    (void)app_host_channel_close_v23();
    (void)app_host_coc_close_v22();coc_session=0;coc_reply_pending=false;
    image_transfer_clear(&transfer);input_session=input_transport=0;
    (void)app_host_timer_v2(1,0);(void)app_host_ble_unregister_v2();
}
static int finish_event(void) {
    if(!transfer.input)input_session=input_transport=0;
    int r=publish();if(r!=APP_OK)ble_image_stop();return r;
}
int ble_image_tick(void) {image_transfer_expire(&transfer);if(!transfer.input)input_session=input_transport=0;return finish_event();}
int ble_image_event(const app_ble_event_v2_t *e) {
    image_transfer_expire(&transfer);if(!transfer.input)input_session=input_transport=0;
    if(e->type==APP_BLE_CONNECTED || e->type==APP_BLE_DISCONNECTED) {
        /* CONNECTED also cleans up if coalescing replaced a queued disconnect. */
        if(transfer.input)image_transfer_cancel(&transfer,IMAGE_DISCONNECTED);
    } else if(e->type==APP_BLE_ERROR) {
        ble_image_stop();return APP_ERR_RUNTIME;
    } else if(e->type==APP_BLE_WRITTEN) {
        if(e->characteristic!=IMAGE_COMMAND || e->data_size>128)image_transfer_reject(&transfer,NULL,0,IMAGE_INVALID);
        else if(transfer.input && input_transport!=INPUT_GATT)image_transfer_reject(&transfer,e->data_size>=18?e->data+2:NULL,e->data_size>1?e->data[1]:0,IMAGE_BUSY);
        else image_transfer_request(&transfer,e->data,e->data_size,128);
    }
    return finish_event();
}
static bool flush_coc_reply(void){
    if(!coc_reply_pending)return true;
    int r=app_host_coc_send_v22(coc_session,coc_reply,sizeof(coc_reply));
    if(r==APP_COC_AGAIN)return false;
    coc_reply_pending=false;
    return r==APP_COC_OK;
}
int ble_image_coc_event(const app_coc_event_v22_t *e){
    image_transfer_expire(&transfer);if(!transfer.input)input_session=input_transport=0;
    if(e->type&(APP_COC_CONNECTED|APP_COC_DISCONNECTED|APP_COC_ERROR)){
        if(transfer.input && input_transport==INPUT_COC)image_transfer_cancel(&transfer,IMAGE_DISCONNECTED);
        coc_reply_pending=false;
        coc_session=(e->type&APP_COC_CONNECTED)?e->session:0;
    }
    if(!coc_session || e->session!=coc_session)return finish_event();
    if(!flush_coc_reply())return finish_event();
    /* Four records per entry keeps Manager cooperative. Successful DATA has
     * no per-chunk response: CoC credits supply transport backpressure.
     * BEGIN/COMMIT/QUERY/CANCEL retain the same 64-byte application receipt. */
    for(unsigned budget=0;budget<4;budget++){
        uint32_t n=0;int r=app_host_coc_recv_v22(coc_session,coc_buffer,sizeof(coc_buffer),&n);
        if(r!=APP_COC_OK)break;
        unsigned op=n>1?coc_buffer[1]:0;
        if(transfer.input && (input_transport!=INPUT_COC || input_session!=coc_session)){
            image_transfer_reject(&transfer,n>=18?coc_buffer+2:NULL,op,IMAGE_BUSY);
        }else{
            image_transfer_request(&transfer,coc_buffer,n,sizeof(coc_buffer));
            if(transfer.input){input_session=coc_session;input_transport=INPUT_COC;}
            /* Remember a failed DATA result for the final COMMIT/QUERY; do
             * not flood the reverse channel with one reply per rejected block. */
            if(op==IMAGE_CHUNK && transfer.input && transfer.reply[4]==IMAGE_REJECTED && n>=18 && !memcmp(transfer.active+8,coc_buffer+2,16))
                image_transfer_fail(&transfer,transfer.reply[6]);
        }
        if(op!=IMAGE_CHUNK){
            memcpy(coc_reply,transfer.reply,64);coc_reply_pending=true;
            if(!flush_coc_reply())break;
        }
    }
    return finish_event();
}
int ble_image_channel_event(const app_channel_event_v23_t *e){
    image_transfer_expire(&transfer);
    if(!transfer.input)input_session=input_transport=0;
    if(e->type&(APP_CHANNEL_CONNECTED|APP_CHANNEL_DISCONNECTED|APP_CHANNEL_ERROR)){
        if(transfer.input && input_transport==INPUT_CHANNEL)image_transfer_cancel(&transfer,IMAGE_DISCONNECTED);
    }
    if(e->type&APP_CHANNEL_RX_READY){
        uint32_t n=0;int r=app_host_channel_recv_v23(e->session,channel_buffer,sizeof(channel_buffer),&n);
        if(r!=APP_CHANNEL_OK)return r==APP_CHANNEL_CLOSED?finish_event():APP_ERR_RUNTIME;
        if(transfer.input && (input_transport!=INPUT_CHANNEL || input_session!=e->session)){
            image_transfer_reject(&transfer,n>=18?channel_buffer+2:NULL,n>1?channel_buffer[1]:0,IMAGE_BUSY);
        }else{
            image_transfer_request(&transfer,channel_buffer,n,sizeof(channel_buffer));
            if(transfer.input){input_session=e->session;input_transport=INPUT_CHANNEL;}
        }
        /* A single authenticated exchange returns the application receipt;
         * no separate APP_READ transaction is needed. DATA acknowledges the
         * whole message's consumed offset, not an ATT fragment or queue copy. */
        r=app_host_channel_send_v23(e->session,transfer.reply,64);
        if(r!=APP_CHANNEL_OK && r!=APP_CHANNEL_CLOSED)return APP_ERR_RUNTIME;
    }
    return finish_event();
}

#endif
