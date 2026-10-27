/* SPDX-License-Identifier: MIT */
#pragma once
#include "photopainter_app.h"
#define IMAGE_COMMAND 0u
#define IMAGE_STATUS 1u
#define IMAGE_STATUS_BYTES 64u
#define IMAGE_MAX_BYTES (5u*1024u*1024u)
#define IMAGE_IDLE_MS 60000u
enum { IMAGE_BEGIN=1,IMAGE_CHUNK,IMAGE_COMMIT,IMAGE_CANCEL,IMAGE_QUERY };
enum { IMAGE_IDLE,IMAGE_RECEIVING,IMAGE_PROCESSING,IMAGE_COMPLETE,IMAGE_FAILED,IMAGE_CANCELED,IMAGE_MISSING,IMAGE_REJECTED };
enum { IMAGE_OK,IMAGE_INVALID,IMAGE_BUSY,IMAGE_OFFSET,IMAGE_CONFLICT,IMAGE_MEMORY,IMAGE_HASH,IMAGE_PNG,IMAGE_DISPLAY,IMAGE_TIMEOUT,IMAGE_DISCONNECTED };
int ble_image_start(void);
void ble_image_stop(void);
int ble_image_event(const app_ble_event_v2_t *event);
int ble_image_coc_event(const app_coc_event_v22_t *event);
int ble_image_tick(void);
int ble_image_receiving(void);
