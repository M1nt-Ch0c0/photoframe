/* SPDX-License-Identifier: MIT */
#include "ble_image.h"
#include "image_sha256.h"
#include <assert.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int app_entry(int,char **);
extern const app_manifest_v2_t app_manifest;
static app_context_v2_t context={.size=sizeof(context),.abi_major=2,.abi_minor=1,.generation=1};
static uint8_t status[64],id[16]={1},digest[32];
static uint64_t now;
static int registered,completed,displays,display_result,fail_alloc;
static void *allocations[16];
void *image_test_malloc(size_t n) {
    if(fail_alloc)return NULL;
    void *p=malloc(n);assert(p);
    for(unsigned i=0;i<16;i++)if(!allocations[i]){allocations[i]=p;return p;}
    abort();
}
void image_test_free(void *p) {
    if(!p)return;
    for(unsigned i=0;i<16;i++)if(allocations[i]==p){allocations[i]=NULL;break;}
    free(p);
}
static unsigned live(void) {unsigned n=0;for(unsigned i=0;i<16;i++)n+=allocations[i]!=NULL;return n;}
const app_context_v2_t *app_host_context_v2(void){return &context;}
void app_host_complete_v2(const app_result_v2_t *r) {
    assert(r->size==sizeof(*r)&&r->event_id==context.event_id&&r->generation==context.generation);
    assert(r->flags==(context.event==APP_EVENT_START&&r->status==0?APP_READY:0));completed++;
}
int32_t app_host_display_v2(const uint8_t *p,uint32_t n) {
    assert(n==APP_FRAME_BYTES && p && context.event!=APP_EVENT_STOP);
    if(context.event==APP_EVENT_BLE)assert(status[4]==IMAGE_PROCESSING);
    displays++;return display_result;
}
const uint8_t *app_host_cache_get_v2(uint32_t v,uint32_t *n){assert(v==1);*n=0;return NULL;}
int32_t app_host_cache_put_v2(uint32_t v,const uint8_t *p,uint32_t n){assert(v==1&&p&&n==APP_FRAME_BYTES);return APP_ERR_MEMORY;}
int32_t app_host_timer_v2(uint32_t id_,uint32_t ms){assert(id_==1&&(ms==1000||ms==0));return 0;}
uint64_t app_host_monotonic_ms_v2(void){return now;}
int32_t app_host_ble_register_v2(const app_ble_service_v2_t *s) {
    assert(!registered && s->count==2 && s->characteristics[0].properties==APP_BLE_WRITE);
    assert(s->characteristics[0].max_bytes==128 && s->characteristics[1].properties==APP_BLE_READ);
    registered=1;return 0;
}
int32_t app_host_ble_unregister_v2(void){registered=0;return 0;}
int32_t app_host_ble_set_v2(uint32_t i,const uint8_t *p,uint32_t n){assert(registered&&i==1&&n==64);memcpy(status,p,64);return 0;}
static int event(unsigned type,const void *p,uint32_t n) {
    context.event=type;context.event_id++;context.data=p;context.data_size=n;
    int before=completed;int r=app_entry(0,NULL);assert(completed==before+1);return r;
}
static void boundary(unsigned type) {app_ble_event_v2_t e={.type=type};assert(event(APP_EVENT_BLE,&e,sizeof(e))==0);}
static void put(uint8_t *p,uint32_t x){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(x>>(8*i));}
static uint32_t get(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void send(unsigned op,const uint8_t *tail,unsigned n) {
    app_ble_event_v2_t e={.type=APP_BLE_WRITTEN,.characteristic=0,.data_size=18+n,.data={1}};
    assert(n<=110);e.data[1]=(uint8_t)op;memcpy(e.data+2,id,16);if(n)memcpy(e.data+18,tail,n);
    assert(event(APP_EVENT_BLE,&e,sizeof(e))==0);assert(!memcmp(status,"PFI1",4));
}
static void begin(uint32_t n) {uint8_t tail[36];put(tail,n);memcpy(tail+4,digest,32);send(IMAGE_BEGIN,tail,36);}
static void chunk(uint32_t offset,const uint8_t *p,unsigned n) {uint8_t tail[110];assert(n<=106);put(tail,offset);memcpy(tail+4,p,n);send(IMAGE_CHUNK,tail,n+4);}
static void upload(const uint8_t *p,uint32_t n) {
    begin(n);assert(status[4]==IMAGE_RECEIVING && live()==1);
    for(uint32_t offset=0;offset<n;) {unsigned take=n-offset;if(take>106)take=106;chunk(offset,p+offset,take);offset+=take;assert(get(status+28)==offset&&status[6]==0);}
}
static void hash_data(const uint8_t *p,size_t n) {image_sha256_t s;image_sha256_init(&s);image_sha256_update(&s,p,n);image_sha256_final(&s,digest);}
static void sha_vectors(void) {
    const char *messages[]={"","abc","abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"};
    const char *expected[]={"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"};
    for(unsigned i=0;i<3;i++) {
        hash_data((const uint8_t *)messages[i],strlen(messages[i]));char hex[65];
        for(unsigned j=0;j<32;j++)snprintf(hex+j*2,3,"%02x",digest[j]);assert(!strcmp(hex,expected[i]));
    }
    image_sha256_t s;image_sha256_init(&s);
    for(unsigned i=0;i<1000000;i++)image_sha256_update(&s,(const uint8_t *)"a",1);
    image_sha256_final(&s,digest);char hex[65];for(unsigned j=0;j<32;j++)snprintf(hex+j*2,3,"%02x",digest[j]);
    assert(!strcmp(hex,"cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
}
int main(void) {
    sha_vectors();
    assert(app_manifest.abi_minor==1 && !strcmp(app_manifest.id,"photoframe"));
    assert(event(APP_EVENT_START,NULL,0)==0 && registered && displays==1 && live()==0);
    boundary(APP_BLE_CONNECTED);
    png_image image={.version=PNG_IMAGE_VERSION,.width=800,.height=480,.format=PNG_FORMAT_RGB};
    uint8_t *pixels=calloc(800*480,3);assert(pixels);png_alloc_size_t n=0;
    assert(png_image_write_to_memory(&image,NULL,&n,0,pixels,0,NULL));
    uint8_t *png=malloc(n+1);assert(png);assert(png_image_write_to_memory(&image,png,&n,0,pixels,0,NULL));free(pixels);
    hash_data(png,n);
    begin(IMAGE_MAX_BYTES+1);assert(status[6]==IMAGE_INVALID&&live()==0);
    fail_alloc=1;begin(n);assert(status[6]==IMAGE_MEMORY);fail_alloc=0;
    begin(IMAGE_MAX_BYTES);assert(status[4]==IMAGE_RECEIVING && live()==1);send(IMAGE_CANCEL,NULL,0);assert(live()==0);
    id[0]++;begin(n);chunk(1,png,1);assert(status[6]==IMAGE_OFFSET);
    chunk(0,png,106);chunk(0,png,106);assert(get(status+28)==106);
    uint8_t bad[106];memcpy(bad,png,106);bad[0]^=1;chunk(0,bad,106);assert(status[6]==IMAGE_CONFLICT);
    chunk(105,png+105,2);assert(status[6]==IMAGE_CONFLICT);
    id[0]++;begin(n);assert(status[6]==IMAGE_BUSY);id[0]--;
    send(IMAGE_COMMIT,NULL,0);assert(status[6]==IMAGE_OFFSET&&displays==1);
    context.input_type=APP_INPUT_PNG;assert(event(APP_EVENT_INPUT,png,n)==APP_ERR_INPUT&&live()==1&&displays==1);
    boundary(APP_BLE_DISCONNECTED);assert(live()==0);send(IMAGE_QUERY,NULL,0);assert(status[4]==IMAGE_CANCELED);
    id[0]++;begin(n);boundary(APP_BLE_CONNECTED);assert(live()==0); /* coalesced disconnect */
    id[0]++;begin(n);now+=IMAGE_IDLE_MS;assert(event(APP_EVENT_TIMER,NULL,0)==0&&live()==0&&status[6]==IMAGE_TIMEOUT);
    id[0]++;hash_data(png,n);upload(png,n);digest[0]^=1; /* conflict must not replace active metadata */
    begin(n);assert(status[6]==IMAGE_CONFLICT);digest[0]^=1;
    send(IMAGE_COMMIT,NULL,0);assert(status[4]==IMAGE_COMPLETE&&displays==2&&live()==0);
    send(IMAGE_COMMIT,NULL,0);assert(displays==2&&status[4]==IMAGE_COMPLETE);
    boundary(APP_BLE_DISCONNECTED);boundary(APP_BLE_CONNECTED);send(IMAGE_QUERY,NULL,0);assert(status[4]==IMAGE_COMPLETE);
    begin(n);assert(status[4]==IMAGE_COMPLETE&&displays==2&&live()==0);
    id[0]++;digest[0]^=1;upload(png,n);send(IMAGE_COMMIT,NULL,0);assert(status[6]==IMAGE_HASH&&live()==0&&displays==2);digest[0]^=1;
    id[0]++;png[n]=0;hash_data(png,n+1);upload(png,n+1);send(IMAGE_COMMIT,NULL,0);assert(status[6]==IMAGE_PNG&&displays==2&&live()==0);
    id[0]++;hash_data(png,n);display_result=APP_ERR_DISPLAY;upload(png,n);send(IMAGE_COMMIT,NULL,0);
    assert(status[4]==IMAGE_FAILED&&status[6]==IMAGE_DISPLAY&&displays==3&&live()==0);display_result=0;
    send(IMAGE_COMMIT,NULL,0);assert(displays==3);
    id[0]++;begin(n);assert(live()==1);assert(event(APP_EVENT_STOP,NULL,0)==0&&!registered&&live()==0);
    assert(event(APP_EVENT_START,NULL,0)==0);send(IMAGE_QUERY,NULL,0);assert(status[4]==IMAGE_MISSING);
    begin(n);assert(live()==1);
    app_ble_event_v2_t error={.type=APP_BLE_ERROR};
    assert(event(APP_EVENT_BLE,&error,sizeof(error))==APP_ERR_RUNTIME && !registered && live()==0);
    free(png);return 0;
}
