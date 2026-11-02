/* SPDX-License-Identifier: MIT */
/* The legacy SO builder recursively compiles main/ without the APP's libc
 * configuration. Transactions belong only to the ABI 2 application profile. */
#if PHOTOFRAME_APP_ENTRY && PHOTOFRAME_ABI_2
#include "image_transfer.h"
#include <stdlib.h>
#include <string.h>
static uint32_t u32(const uint8_t *p) {return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put32(uint8_t *p,uint32_t n) {for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
static void empty(uint8_t *r,const uint8_t *id) {memset(r,0,64);memcpy(r,"PFI1",4);if(id)memcpy(r+8,id,16);}
static void release_input(image_transfer *t) {free(t->input);t->input=NULL;memset(&t->hash,0,sizeof(t->hash));}
static void terminal(image_transfer *t,unsigned state,unsigned error) {
    t->active[4]=(uint8_t)state;t->active[6]=(uint8_t)error;t->active[7]=0;
    memcpy(t->receipts[t->next_receipt++%4],t->active,64);memcpy(t->reply,t->active,64);release_input(t);
}
static uint8_t *find(image_transfer *t,const uint8_t *id) {
    for(unsigned i=0;i<4;i++)if(t->receipts[i][4]&&!memcmp(t->receipts[i]+8,id,16))return t->receipts[i];
    return NULL;
}
void image_transfer_reject(image_transfer *t,const uint8_t *id,unsigned op,unsigned error) {
    empty(t->reply,id);t->reply[4]=IMAGE_REJECTED;t->reply[5]=(uint8_t)op;t->reply[6]=(uint8_t)error;
}
void image_transfer_expire(image_transfer *t) {
    if(t->input && t->ops->now_ms(t->context)-t->last_activity>=IMAGE_IDLE_MS)terminal(t,IMAGE_CANCELED,IMAGE_TIMEOUT);
}

void image_transfer_init(image_transfer *t,const image_transfer_ops *ops,void *context) {
    memset(t,0,sizeof(*t));t->ops=ops;t->context=context;
    empty(t->active,NULL);empty(t->reply,NULL);
}
void image_transfer_clear(image_transfer *t) {
    release_input(t);
    const image_transfer_ops *ops=t->ops;void *context=t->context;
    image_transfer_init(t,ops,context);
}
void image_transfer_cancel(image_transfer *t,unsigned error) {
    if(t->input)terminal(t,IMAGE_CANCELED,error);
}
void image_transfer_fail(image_transfer *t,unsigned error) {
    if(t->input)terminal(t,IMAGE_FAILED,error);
}
void image_transfer_request(image_transfer *t,const uint8_t *p,uint32_t n,uint32_t maximum) {
    unsigned op=n>1?p[1]:0;
    if(n<18 || n>maximum || p[0]!=1 || op<IMAGE_BEGIN || op>IMAGE_QUERY) {image_transfer_reject(t,NULL,op,IMAGE_INVALID);return;}
    const uint8_t *id=p+2;
    uint8_t zero[16]={0};
    if(!memcmp(id,zero,16)){image_transfer_reject(t,id,op,IMAGE_INVALID);return;}
    uint8_t *stored=find(t,id);
    bool same=!memcmp(t->active+8,id,16);
    if(op==IMAGE_BEGIN) {
        if(n!=54 || u32(p+18)<8 || u32(p+18)>IMAGE_MAX_BYTES){image_transfer_reject(t,id,op,IMAGE_INVALID);return;}
        if(stored || (t->input && same)) {
            uint8_t *r=stored?stored:t->active;
            if(u32(r+24)!=u32(p+18)||memcmp(r+32,p+22,32)){image_transfer_reject(t,id,op,IMAGE_CONFLICT);return;}
            memcpy(t->reply,r,64);t->reply[5]=(uint8_t)op;
            if(t->input && same)t->last_activity=t->ops->now_ms(t->context);
            return;
        }
        if(t->input){image_transfer_reject(t,id,op,IMAGE_BUSY);return;}
        /* The deployed host routes allocations above 512 bytes to PSRAM.
         * A multi-megabyte request cannot fit internal RAM; failure is explicit. */
        t->input=malloc(u32(p+18));
        if(!t->input){image_transfer_reject(t,id,op,IMAGE_MEMORY);return;}
        empty(t->active,id);t->active[4]=IMAGE_RECEIVING;t->active[5]=(uint8_t)op;
        put32(t->active+24,u32(p+18));memcpy(t->active+32,p+22,32);
        image_sha256_init(&t->hash);t->last_activity=t->ops->now_ms(t->context);memcpy(t->reply,t->active,64);return;
    }
    if(op==IMAGE_QUERY) {
        if(n!=18){image_transfer_reject(t,id,op,IMAGE_INVALID);return;}
        if(t->input && same)memcpy(t->reply,t->active,64);
        else if(stored)memcpy(t->reply,stored,64);
        else {empty(t->reply,id);t->reply[4]=IMAGE_MISSING;}
        t->reply[5]=(uint8_t)op;return;
    }
    if(op==IMAGE_COMMIT && n==18 && stored){memcpy(t->reply,stored,64);t->reply[5]=(uint8_t)op;return;}
    if(!t->input || !same){image_transfer_reject(t,id,op,IMAGE_CONFLICT);return;}
    uint32_t total=u32(t->active+24),received=u32(t->active+28);
    t->active[5]=(uint8_t)op;
    if(op==IMAGE_CHUNK) {
        if(n<=22){image_transfer_reject(t,id,op,IMAGE_INVALID);return;}
        uint32_t offset=u32(p+18),bytes=n-22;
        if(offset>received || bytes>total || offset>total-bytes){image_transfer_reject(t,id,op,IMAGE_OFFSET);return;}
        if(offset<received) {
            if(bytes>received-offset || memcmp(t->input+offset,p+22,bytes)){image_transfer_reject(t,id,op,IMAGE_CONFLICT);return;}
        } else {
            memcpy(t->input+offset,p+22,bytes);image_sha256_update(&t->hash,p+22,bytes);put32(t->active+28,received+bytes);
        }
        t->last_activity=t->ops->now_ms(t->context);memcpy(t->reply,t->active,64);return;
    }
    if(n!=18){image_transfer_reject(t,id,op,IMAGE_INVALID);return;}
    if(op==IMAGE_CANCEL){terminal(t,IMAGE_CANCELED,IMAGE_OK);return;}
    if(received!=total){image_transfer_reject(t,id,op,IMAGE_OFFSET);return;}
    uint8_t digest[32];image_sha256_final(&t->hash,digest);
    if(memcmp(digest,t->active+32,32)){terminal(t,IMAGE_FAILED,IMAGE_HASH);return;}
    t->active[4]=IMAGE_PROCESSING;memcpy(t->reply,t->active,64);
    if(t->ops->processing(t->context,t->reply)!=0){terminal(t,IMAGE_FAILED,IMAGE_DISPLAY);return;}
    unsigned r=t->ops->render(t->context,t->input,total);
    terminal(t,r==IMAGE_OK?IMAGE_COMPLETE:IMAGE_FAILED,r);
}

#endif
