/* SPDX-License-Identifier: MIT */
#if PHOTOFRAME_APP_ENTRY && PHOTOFRAME_ABI_2
#include "ble_image.h"
#include "image_sha256.h"
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
/* All state is owned by this ELF. Only the manager calls these functions. */
static uint8_t active[64],receipts[4][64],reply[64];
static unsigned next_receipt;
static uint8_t *input;
static uint64_t last_activity;
static image_sha256_t hash;
static uint32_t u32(const uint8_t *p) {return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put32(uint8_t *p,uint32_t n) {for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
static void empty(uint8_t *r,const uint8_t *id) {memset(r,0,64);memcpy(r,"PFI1",4);if(id)memcpy(r+8,id,16);}
static void release_input(void) {free(input);input=NULL;memset(&hash,0,sizeof(hash));}
static void terminal(unsigned state,unsigned error) {
    active[4]=(uint8_t)state;active[6]=(uint8_t)error;active[7]=0;
    memcpy(receipts[next_receipt++%4],active,64);memcpy(reply,active,64);release_input();
}
static uint8_t *find(const uint8_t *id) {
    for(unsigned i=0;i<4;i++)if(receipts[i][4]&&!memcmp(receipts[i]+8,id,16))return receipts[i];
    return NULL;
}
static int publish(void) {return app_host_ble_set_v2(IMAGE_STATUS,reply,64);}
static void reject(const uint8_t *id,unsigned op,unsigned error) {
    empty(reply,id);reply[4]=IMAGE_REJECTED;reply[5]=(uint8_t)op;reply[6]=(uint8_t)error;
}
static void expire(void) {
    if(input && app_host_monotonic_ms_v2()-last_activity>=IMAGE_IDLE_MS)terminal(IMAGE_CANCELED,IMAGE_TIMEOUT);
}
int ble_image_receiving(void) {return input!=NULL;}
int ble_image_start(void) {
    release_input();memset(receipts,0,sizeof(receipts));empty(active,NULL);empty(reply,NULL);next_receipt=0;
    int r=app_host_ble_register_v2(&service);
    if(r==APP_OK)r=publish();
    if(r==APP_OK)r=app_host_timer_v2(1,1000);
    if(r!=APP_OK)ble_image_stop();
    return r;
}
void ble_image_stop(void) {
    release_input();memset(receipts,0,sizeof(receipts));memset(active,0,sizeof(active));memset(reply,0,sizeof(reply));
    (void)app_host_timer_v2(1,0);(void)app_host_ble_unregister_v2();
}
static int finish_event(void) {
    int r=publish();if(r!=APP_OK)ble_image_stop();return r;
}
int ble_image_tick(void) {expire();return finish_event();}
static void command(const uint8_t *p,uint32_t n) {
    unsigned op=n>1?p[1]:0;
    if(n<18 || n>128 || p[0]!=1 || op<IMAGE_BEGIN || op>IMAGE_QUERY) {reject(NULL,op,IMAGE_INVALID);return;}
    const uint8_t *id=p+2;
    uint8_t zero[16]={0};
    if(!memcmp(id,zero,16)){reject(id,op,IMAGE_INVALID);return;}
    uint8_t *stored=find(id);
    bool same=!memcmp(active+8,id,16);
    if(op==IMAGE_BEGIN) {
        if(n!=54 || u32(p+18)<8 || u32(p+18)>IMAGE_MAX_BYTES){reject(id,op,IMAGE_INVALID);return;}
        if(stored || (input && same)) {
            uint8_t *r=stored?stored:active;
            if(u32(r+24)!=u32(p+18)||memcmp(r+32,p+22,32)){reject(id,op,IMAGE_CONFLICT);return;}
            memcpy(reply,r,64);reply[5]=(uint8_t)op;
            if(input && same)last_activity=app_host_monotonic_ms_v2();
            return;
        }
        if(input){reject(id,op,IMAGE_BUSY);return;}
        /* The deployed host routes allocations above 512 bytes to PSRAM.
         * A multi-megabyte request cannot fit internal RAM; failure is explicit. */
        input=malloc(u32(p+18));
        if(!input){reject(id,op,IMAGE_MEMORY);return;}
        empty(active,id);active[4]=IMAGE_RECEIVING;active[5]=(uint8_t)op;
        put32(active+24,u32(p+18));memcpy(active+32,p+22,32);
        image_sha256_init(&hash);last_activity=app_host_monotonic_ms_v2();memcpy(reply,active,64);return;
    }
    if(op==IMAGE_QUERY) {
        if(n!=18){reject(id,op,IMAGE_INVALID);return;}
        if(input && same)memcpy(reply,active,64);
        else if(stored)memcpy(reply,stored,64);
        else {empty(reply,id);reply[4]=IMAGE_MISSING;}
        reply[5]=(uint8_t)op;return;
    }
    if(op==IMAGE_COMMIT && n==18 && stored){memcpy(reply,stored,64);reply[5]=(uint8_t)op;return;}
    if(!input || !same){reject(id,op,IMAGE_CONFLICT);return;}
    uint32_t total=u32(active+24),received=u32(active+28);
    active[5]=(uint8_t)op;
    if(op==IMAGE_CHUNK) {
        if(n<=22){reject(id,op,IMAGE_INVALID);return;}
        uint32_t offset=u32(p+18),bytes=n-22;
        if(offset>received || bytes>total || offset>total-bytes){reject(id,op,IMAGE_OFFSET);return;}
        if(offset<received) {
            if(bytes>received-offset || memcmp(input+offset,p+22,bytes)){reject(id,op,IMAGE_CONFLICT);return;}
        } else {
            memcpy(input+offset,p+22,bytes);image_sha256_update(&hash,p+22,bytes);put32(active+28,received+bytes);
        }
        last_activity=app_host_monotonic_ms_v2();memcpy(reply,active,64);return;
    }
    if(n!=18){reject(id,op,IMAGE_INVALID);return;}
    if(op==IMAGE_CANCEL){terminal(IMAGE_CANCELED,IMAGE_OK);return;}
    if(received!=total){reject(id,op,IMAGE_OFFSET);return;}
    uint8_t digest[32];image_sha256_final(&hash,digest);
    if(memcmp(digest,active+32,32)){terminal(IMAGE_FAILED,IMAGE_HASH);return;}
    uint8_t *frame=NULL;
    int r=photoframe_decode_indexed(input,total,&frame);
    if(r!=APP_OK){free(frame);terminal(IMAGE_FAILED,r==APP_ERR_MEMORY?IMAGE_MEMORY:IMAGE_PNG);return;}
    active[4]=IMAGE_PROCESSING;memcpy(reply,active,64);
    if(publish()!=APP_OK){free(frame);terminal(IMAGE_FAILED,IMAGE_DISPLAY);return;}
    /* Synchronous host display returns only after final POWER_OFF. */
    r=app_host_display_v2(frame,APP_FRAME_BYTES);
    if(r==APP_OK)(void)app_host_cache_put_v2(1,frame,APP_FRAME_BYTES);
    free(frame);terminal(r==APP_OK?IMAGE_COMPLETE:IMAGE_FAILED,r==APP_OK?IMAGE_OK:IMAGE_DISPLAY);
}
int ble_image_event(const app_ble_event_v2_t *e) {
    expire();
    if(e->type==APP_BLE_CONNECTED || e->type==APP_BLE_DISCONNECTED) {
        /* CONNECTED also cleans up if coalescing replaced a queued disconnect. */
        if(input)terminal(IMAGE_CANCELED,IMAGE_DISCONNECTED);
    } else if(e->type==APP_BLE_ERROR) {
        ble_image_stop();return APP_ERR_RUNTIME;
    } else if(e->type==APP_BLE_WRITTEN) {
        if(e->characteristic!=IMAGE_COMMAND || e->data_size>128)reject(NULL,0,IMAGE_INVALID);
        else command(e->data,e->data_size);
    }
    return finish_event();
}
#endif
