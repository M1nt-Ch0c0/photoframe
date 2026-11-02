/* SPDX-License-Identifier: MIT */
#pragma once
#include "image_protocol.h"
#include "image_sha256.h"
#include <stdbool.h>
#include <stdint.h>
/* Application-owned transaction core. No radio, SDK, filesystem or peer handle.
 * The sole Manager serializes calls. Transport owners enforce peer admission. */
typedef struct {
    uint64_t (*now_ms)(void *context);
    int (*processing)(void *context,const uint8_t receipt[64]);
    unsigned (*render)(void *context,const uint8_t *png,uint32_t bytes);
} image_transfer_ops;
typedef struct {
    uint8_t active[64],receipts[4][64],reply[64];
    unsigned next_receipt;
    uint8_t *input;
    uint64_t last_activity;
    image_sha256_t hash;
    const image_transfer_ops *ops;
    void *context;
} image_transfer;
void image_transfer_init(image_transfer *,const image_transfer_ops *,void *context);
void image_transfer_clear(image_transfer *);
void image_transfer_expire(image_transfer *);
void image_transfer_cancel(image_transfer *,unsigned error);
void image_transfer_fail(image_transfer *,unsigned error);
void image_transfer_reject(image_transfer *,const uint8_t *id,unsigned op,unsigned error);
void image_transfer_request(image_transfer *,const uint8_t *,uint32_t bytes,uint32_t maximum);
