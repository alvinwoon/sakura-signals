#include "sakura_signals.h"

CorrelationMatrix* create_correlation_matrix(int size) {
    CorrelationMatrix *cm = malloc(sizeof(CorrelationMatrix));
    if (!cm) return NULL;
    
    cm->matrix = malloc(size * sizeof(double*));
    if (!cm->matrix) {
        free(cm);
        return NULL;
    }
    
    for (int i = 0; i < size; i++) {
        cm->matrix[i] = calloc(size, sizeof(double));
        if (!cm->matrix[i]) {
            // cleanup on fail
            for (int j = 0; j < i; j++) {
                free(cm->matrix[j]);
            }
            free(cm->matrix);
            free(cm);
            return NULL;
        }
    }
    
    cm->size = size;
    return cm;
}

void destroy_correlation_matrix(CorrelationMatrix *cm) {
    if (cm) {
        for (int i = 0; i < cm->size; i++) {
            free(cm->matrix[i]);
        }
        free(cm->matrix);
        free(cm);
    }
}

void update_correlation_matrix(CorrelationMatrix *cm, CircularBuffer **buffers, int n_series) {
    if (!cm || !buffers || n_series > cm->size) return;
    
    // fill corr matrix
    for (int i = 0; i < n_series; i++) {
        for (int j = 0; j < n_series; j++) {
            if (i == j) {
                cm->matrix[i][j] = 1.0; // diag = 1
            } else {
                cm->matrix[i][j] = calculate_correlation(buffers[i], buffers[j]);
            }
        }
    }
}
