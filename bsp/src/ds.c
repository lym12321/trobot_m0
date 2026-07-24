/**
 *  @brief Some Useful DataStruct
 *  @author tiny_fish
 *  @date 2025-11-20
 */
#include "bsp/ds.h"

bool ds_rq_init(ds_rq_t *queue, uint8_t *buffer, size_t size) {
    if (queue == NULL || buffer == NULL || size < 2u) {
        return false;
    }
    queue->buf = buffer;
    queue->size = size;
    queue->left = 0;
    queue->right = 0;
    return true;
}

size_t ds_rq_size(const ds_rq_t *queue) {
    if (queue->right >= queue->left) {
        return queue->right - queue->left;
    }
    return queue->size - queue->left + queue->right;
}

size_t ds_rq_avail(const ds_rq_t *queue) {
    return queue->size - 1u - ds_rq_size(queue);
}

bool ds_rq_push(
    ds_rq_t *queue, const uint8_t *data, size_t length) {
    if ((data == NULL && length != 0u) ||
        length > ds_rq_avail(queue)) {
        return false;
    }
    for (size_t i = 0; i < length; i++) {
        queue->buf[queue->right] = data[i];
        queue->right = (queue->right + 1u) % queue->size;
    }
    return true;
}

bool ds_rq_pop(
    ds_rq_t *queue, uint8_t *result, size_t length) {
    if ((result == NULL && length != 0u) ||
        length > ds_rq_size(queue)) {
        return false;
    }
    for (size_t i = 0; i < length; i++) {
        result[i] = queue->buf[queue->left];
        queue->left = (queue->left + 1u) % queue->size;
    }
    return true;
}
