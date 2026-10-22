/* SPDX-License-Identifier: MIT */
#pragma once
#include <stddef.h>
#include <stdint.h>
typedef struct {
    uint32_t h[8];
    uint64_t bytes;
    uint8_t block[64];
} image_sha256_t;
void image_sha256_init(image_sha256_t *s);
void image_sha256_update(image_sha256_t *s, const uint8_t *data, size_t size);
void image_sha256_final(image_sha256_t *s, uint8_t digest[32]);
