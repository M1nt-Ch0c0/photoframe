/* SPDX-License-Identifier: MIT */
#pragma once
#include "photopainter_app.h"
#define IMAGE_COMMAND 0u
#define IMAGE_STATUS 1u
#include "image_protocol.h"
int ble_image_start(void);
void ble_image_stop(void);
int ble_image_event(const app_ble_event_v2_t *event);
int ble_image_coc_event(const app_coc_event_v22_t *event);
int ble_image_channel_event(const app_channel_event_v23_t *event);
int ble_image_tick(void);
int ble_image_receiving(void);
