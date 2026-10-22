#include "photopainter_app.h"
#include <assert.h>
#include <string.h>
int app_entry(int,char **);
extern const app_manifest_v2_t app_manifest;
static app_context_v2_t context;
static uint8_t value[128];static uint32_t size;static int registered,completed;
const app_context_v2_t *app_host_context_v2(void){return &context;}
void app_host_complete_v2(const app_result_v2_t *r){assert(r->event_id==context.event_id && r->generation==context.generation && r->status==APP_OK);completed++;}
int32_t app_host_ble_register_v2(const app_ble_service_v2_t *s){assert(s->count==1 && s->characteristics[0].max_bytes==128);registered++;return APP_OK;}
int32_t app_host_ble_unregister_v2(void){registered=0;return APP_OK;}
int32_t app_host_ble_set_v2(uint32_t index,const uint8_t *p,uint32_t n){assert(registered && index==0 && n<=128);memcpy(value,p,n);size=n;return APP_OK;}
int main(void){
    assert(app_manifest.abi_major==2 && app_manifest.abi_minor==1 && !strcmp(app_manifest.id,"ble-echo"));
    context=(app_context_v2_t){.size=sizeof(context),.abi_major=2,.abi_minor=1,.generation=7,.event_id=1,.event=APP_EVENT_START};
    assert(app_entry(0,0)==0 && registered==1 && size==5 && !memcmp(value,"ready",5));
    app_ble_event_v2_t event={.type=APP_BLE_WRITTEN,.data_size=5,.data={'h','e','l','l','o'}};
    context.event=APP_EVENT_BLE;context.event_id++;context.data=(const uint8_t *)&event;context.data_size=sizeof(event);
    assert(app_entry(0,0)==0 && size==5 && !memcmp(value,"hello",5));
    event.type=APP_BLE_CONNECTED;assert(app_entry(0,0)==0);
    event.type=APP_BLE_DISCONNECTED;assert(app_entry(0,0)==0);
    context.event=APP_EVENT_STOP;context.event_id++;assert(app_entry(0,0)==0 && !registered && completed==5);
    return 0;
}
