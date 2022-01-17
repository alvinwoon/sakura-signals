#include "sakura_signals.h"

AttentionLayer* create_attention_layer(int input_dim, int attention_dim, int sequence_length) {
    AttentionLayer *layer = malloc(sizeof(AttentionLayer));
    if (!layer) return NULL;
    
    layer->input_dim = input_dim;
    layer->attention_dim = attention_dim;
    layer->sequence_length = sequence_length;
    
    // allocate weight matricies
    layer->query_weights = malloc(attention_dim * sizeof(double*));
    layer->key_weights = malloc(attention_dim * sizeof(double*));
    layer->value_weights = malloc(attention_dim * sizeof(double*));
    
    if (!layer->query_weights || !layer->key_weights || !layer->value_weights) {
        free(layer->query_weights);
        free(layer->key_weights);
        free(layer->value_weights);
        free(layer);
        return NULL;
    }
    
    // Initialize weight matrices with small random values
    srand(42); // Fixed seed for reproducibility
    for (int i = 0; i < attention_dim; i++) {
        layer->query_weights[i] = malloc(input_dim * sizeof(double));
        layer->key_weights[i] = malloc(input_dim * sizeof(double));
        layer->value_weights[i] = malloc(input_dim * sizeof(double));
        
        if (!layer->query_weights[i] || !layer->key_weights[i] || !layer->value_weights[i]) {
            // Cleanup on failure
            for (int j = 0; j <= i; j++) {
                free(layer->query_weights[j]);
                free(layer->key_weights[j]);
                free(layer->value_weights[j]);
            }
            free(layer->query_weights);
            free(layer->key_weights);
            free(layer->value_weights);
            free(layer);
            return NULL;
        }
        
        // xavier init
        double scale = sqrt(2.0 / (input_dim + attention_dim));
        for (int j = 0; j < input_dim; j++) {
            layer->query_weights[i][j] = ((double)rand() / RAND_MAX - 0.5) * 2 * scale;
            layer->key_weights[i][j] = ((double)rand() / RAND_MAX - 0.5) * 2 * scale;
            layer->value_weights[i][j] = ((double)rand() / RAND_MAX - 0.5) * 2 * scale;
        }
    }
    
    return layer;
}

void destroy_attention_layer(AttentionLayer *layer) {
    if (layer) {
        for (int i = 0; i < layer->attention_dim; i++) {
            free(layer->query_weights[i]);
            free(layer->key_weights[i]);
            free(layer->value_weights[i]);
        }
        free(layer->query_weights);
        free(layer->key_weights);
        free(layer->value_weights);
        free(layer);
    }
}

AttentionOutput* create_attention_output(int sequence_length, int feature_dim) {
    AttentionOutput *output = malloc(sizeof(AttentionOutput));
    if (!output) return NULL;
    
    output->attention_scores = calloc(sequence_length, sizeof(double));
    output->context_vector = calloc(feature_dim, sizeof(double));
    output->weighted_features = calloc(feature_dim, sizeof(double));
    output->sequence_length = sequence_length;
    output->feature_dim = feature_dim;
    
    if (!output->attention_scores || !output->context_vector || !output->weighted_features) {
        free(output->attention_scores);
        free(output->context_vector);
        free(output->weighted_features);
        free(output);
        return NULL;
    }
    
    return output;
}

void destroy_attention_output(AttentionOutput *output) {
    if (output) {
        free(output->attention_scores);
        free(output->context_vector);
        free(output->weighted_features);
        free(output);
    }
}

double* softmax(double *scores, int length) {
    double *result = malloc(length * sizeof(double));
    if (!result) return NULL;
    
    // Find max for numerical stability
    double max_score = scores[0];
    for (int i = 1; i < length; i++) {
        if (scores[i] > max_score) {
            max_score = scores[i];
        }
    }
    
    // Calculate exp and sum
    double sum = 0.0;
    for (int i = 0; i < length; i++) {
        result[i] = exp(scores[i] - max_score);
        sum += result[i];
    }
    
    // Normalize
    for (int i = 0; i < length; i++) {
        result[i] /= sum;
    }
    
    return result;
}

double* matrix_multiply(double **matrix, double *vector, int rows, int cols) {
    double *result = calloc(rows, sizeof(double));
    if (!result) return NULL;
    
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            result[i] += matrix[i][j] * vector[j];
        }
    }
    
    return result;
}

AttentionOutput* apply_temporal_attention(AttentionLayer *layer, CircularBuffer *sequence) {
    int seq_len = cb_size(sequence);
    if (seq_len < 2) return NULL;
    
    AttentionOutput *output = create_attention_output(seq_len, layer->attention_dim);
    if (!output) return NULL;
    
    // Create input matrix from circular buffer
    double *input_sequence = malloc(seq_len * sizeof(double));
    if (!input_sequence) {
        destroy_attention_output(output);
        return NULL;
    }
    
    for (int i = 0; i < seq_len; i++) {
        input_sequence[i] = cb_get(sequence, i);
    }
    
    // simplified attn calc for 1D time series
    // calc attn scores based on recency + magnitude
    double total_score = 0.0;
    for (int i = 0; i < seq_len; i++) {
        // recency bias: recent data gets higher attn
        double recency_weight = (double)(i + 1) / seq_len;
        
        // magnitude bias: larger abs values get more attn
        double magnitude_weight = fabs(input_sequence[i]);
        
        // combined score w/ learnable params (simplified)
        output->attention_scores[i] = recency_weight * 0.7 + magnitude_weight * 0.3;
        total_score += output->attention_scores[i];
    }
    
    // Normalize attention scores
    for (int i = 0; i < seq_len; i++) {
        output->attention_scores[i] /= total_score;
    }
    
    // Calculate weighted context vector
    for (int i = 0; i < seq_len; i++) {
        output->context_vector[0] += output->attention_scores[i] * input_sequence[i];
    }
    
    // Simple feature transformation
    output->weighted_features[0] = output->context_vector[0];
    if (layer->attention_dim > 1) {
        // Add momentum feature
        double momentum = 0.0;
        for (int i = 1; i < seq_len; i++) {
            momentum += output->attention_scores[i] * (input_sequence[i] - input_sequence[i-1]);
        }
        output->weighted_features[1] = momentum;
    }
    
    free(input_sequence);
    return output;
}

double calculate_attention_enhanced_zscore(CircularBuffer *spread_buffer, AttentionLayer *attention) {
    if (!attention || cb_size(spread_buffer) < 5) {
        // Fallback to traditional z-score
        double mean = rolling_mean(spread_buffer);
        double std = rolling_std(spread_buffer);
        if (cb_size(spread_buffer) > 0) {
            double current = cb_get(spread_buffer, cb_size(spread_buffer) - 1);
            return calculate_z_score(current, mean, std);
        }
        return 0.0;
    }
    
    AttentionOutput *att_output = apply_temporal_attention(attention, spread_buffer);
    if (!att_output) {
        // Fallback to traditional z-score
        double mean = rolling_mean(spread_buffer);
        double std = rolling_std(spread_buffer);
        if (cb_size(spread_buffer) > 0) {
            double current = cb_get(spread_buffer, cb_size(spread_buffer) - 1);
            return calculate_z_score(current, mean, std);
        }
        return 0.0;
    }
    
    // Use attention-weighted mean instead of simple rolling mean
    double attention_weighted_mean = att_output->context_vector[0];
    
    // Calculate attention-weighted standard deviation
    double weighted_variance = 0.0;
    int seq_len = cb_size(spread_buffer);
    
    for (int i = 0; i < seq_len; i++) {
        double diff = cb_get(spread_buffer, i) - attention_weighted_mean;
        weighted_variance += att_output->attention_scores[i] * diff * diff;
    }
    
    double attention_weighted_std = sqrt(weighted_variance);
    
    // Current value (most recent)
    double current_value = cb_get(spread_buffer, seq_len - 1);
    
    // Enhanced z-score with attention mechanism
    double enhanced_zscore = 0.0;
    if (attention_weighted_std > 0) {
        enhanced_zscore = (current_value - attention_weighted_mean) / attention_weighted_std;
        
        // Add momentum component if available
        if (attention->attention_dim > 1 && att_output->feature_dim > 1) {
            double momentum_factor = att_output->weighted_features[1];
            enhanced_zscore += momentum_factor * 0.1; // Small momentum contribution
        }
    }
    
    destroy_attention_output(att_output);
    return enhanced_zscore;
}
