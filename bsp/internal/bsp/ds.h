/**
 *  @brief Some Useful DataStruct
 *  @author tiny_fish
 *  @date 2025-11-20
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Ring Queue
typedef struct {
    uint8_t *buf;
    size_t size;
    size_t left;
    size_t right;
} ds_rq_t;

bool ds_rq_init(ds_rq_t *queue, uint8_t *buffer, size_t size);
size_t ds_rq_avail(const ds_rq_t *queue);
size_t ds_rq_size(const ds_rq_t *queue);
bool ds_rq_push(
    ds_rq_t *queue, const uint8_t *data, size_t length);
bool ds_rq_pop(ds_rq_t *queue, uint8_t *result, size_t length);
