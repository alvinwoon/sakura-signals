#include "sakura_signals.h"

CircularBuffer* create_circular_buffer(int capacity) {
    CircularBuffer *cb = malloc(sizeof(CircularBuffer));
    if (!cb) return NULL;
    
    cb->data = malloc(capacity * sizeof(double));
    if (!cb->data) {
        free(cb);
        return NULL;
    }
    
    // init buffer state
    cb->size = 0;
    cb->capacity = capacity;
    cb->head = 0;
    cb->is_full = false;
    
    return cb;
}

void destroy_circular_buffer(CircularBuffer *cb) {
    if (cb) {
        free(cb->data);
        free(cb);
    }
}

void cb_push(CircularBuffer *cb, double value) {
    cb->data[cb->head] = value;
    
    if (cb->is_full) {
        cb->head = (cb->head + 1) % cb->capacity;
    } else {
        cb->size++;
        if (cb->size == cb->capacity) {
            cb->is_full = true;
        }
        cb->head = (cb->head + 1) % cb->capacity;
    }
}

double cb_get(CircularBuffer *cb, int index) {
    if (index < 0 || index >= cb_size(cb)) {
        return 0.0; // invalid idx
    }
    
    int actual_index;
    if (cb->is_full) {
        actual_index = (cb->head + index) % cb->capacity;
    } else {
        actual_index = index;
    }
    
    return cb->data[actual_index];
}

int cb_size(CircularBuffer *cb) {
    return cb->size;
}
