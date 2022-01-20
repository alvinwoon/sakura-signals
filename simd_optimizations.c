#include "sakura_signals.h"

// check if AVX2 is available (most modern CPUs support this)
#ifdef __AVX2__
#include <immintrin.h>
#define SIMD_AVAILABLE 1
#else
#define SIMD_AVAILABLE 0
#endif

double simd_rolling_mean(double *data, int size) {
    if (!data || size <= 0) return 0.0;
    
#if SIMD_AVAILABLE
    if (size >= 4) {
        __m256d sum_vec = _mm256_setzero_pd();
        int i = 0;
        
        // process 4 doubles at a time
        for (; i <= size - 4; i += 4) {
            __m256d data_vec = _mm256_loadu_pd(&data[i]);
            sum_vec = _mm256_add_pd(sum_vec, data_vec);
        }
        
        // horizontal sum of vector
        double sum_array[4];
        _mm256_storeu_pd(sum_array, sum_vec);
        double sum = sum_array[0] + sum_array[1] + sum_array[2] + sum_array[3];
        
        // handle remaining elements
        for (; i < size; i++) {
            sum += data[i];
        }
        
        return sum / size;
    }
#endif
    
    // fallback to scalar implementation
    double sum = 0.0;
    for (int i = 0; i < size; i++) {
        sum += data[i];
    }
    return sum / size;
}

double simd_rolling_std(double *data, int size) {
    if (!data || size <= 1) return 0.0;
    
    double mean = simd_rolling_mean(data, size);
    
#if SIMD_AVAILABLE
    if (size >= 4) {
        __m256d mean_vec = _mm256_set1_pd(mean);
        __m256d sum_sq_vec = _mm256_setzero_pd();
        int i = 0;
        
        // process 4 doubles at a time
        for (; i <= size - 4; i += 4) {
            __m256d data_vec = _mm256_loadu_pd(&data[i]);
            __m256d diff_vec = _mm256_sub_pd(data_vec, mean_vec);
            __m256d sq_vec = _mm256_mul_pd(diff_vec, diff_vec);
            sum_sq_vec = _mm256_add_pd(sum_sq_vec, sq_vec);
        }
        
        // horizontal sum
        double sum_sq_array[4];
        _mm256_storeu_pd(sum_sq_array, sum_sq_vec);
        double sum_sq = sum_sq_array[0] + sum_sq_array[1] + sum_sq_array[2] + sum_sq_array[3];
        
        // handle remaining elements
        for (; i < size; i++) {
            double diff = data[i] - mean;
            sum_sq += diff * diff;
        }
        
        return sqrt(sum_sq / (size - 1));
    }
#endif
    
    // fallback to scalar
    double sum_sq = 0.0;
    for (int i = 0; i < size; i++) {
        double diff = data[i] - mean;
        sum_sq += diff * diff;
    }
    return sqrt(sum_sq / (size - 1));
}

double simd_correlation(double *data1, double *data2, int size) {
    if (!data1 || !data2 || size < 2) return 0.0;
    
    double mean1 = simd_rolling_mean(data1, size);
    double mean2 = simd_rolling_mean(data2, size);
    
#if SIMD_AVAILABLE
    if (size >= 4) {
        __m256d mean1_vec = _mm256_set1_pd(mean1);
        __m256d mean2_vec = _mm256_set1_pd(mean2);
        __m256d sum_xy_vec = _mm256_setzero_pd();
        __m256d sum_x2_vec = _mm256_setzero_pd();
        __m256d sum_y2_vec = _mm256_setzero_pd();
        int i = 0;
        
        // vectorized correlation calc
        for (; i <= size - 4; i += 4) {
            __m256d x_vec = _mm256_loadu_pd(&data1[i]);
            __m256d y_vec = _mm256_loadu_pd(&data2[i]);
            
            __m256d x_diff = _mm256_sub_pd(x_vec, mean1_vec);
            __m256d y_diff = _mm256_sub_pd(y_vec, mean2_vec);
            
            __m256d xy_vec = _mm256_mul_pd(x_diff, y_diff);
            __m256d x2_vec = _mm256_mul_pd(x_diff, x_diff);
            __m256d y2_vec = _mm256_mul_pd(y_diff, y_diff);
            
            sum_xy_vec = _mm256_add_pd(sum_xy_vec, xy_vec);
            sum_x2_vec = _mm256_add_pd(sum_x2_vec, x2_vec);
            sum_y2_vec = _mm256_add_pd(sum_y2_vec, y2_vec);
        }
        
        // horizontal sums
        double xy_array[4], x2_array[4], y2_array[4];
        _mm256_storeu_pd(xy_array, sum_xy_vec);
        _mm256_storeu_pd(x2_array, sum_x2_vec);
        _mm256_storeu_pd(y2_array, sum_y2_vec);
        
        double sum_xy = xy_array[0] + xy_array[1] + xy_array[2] + xy_array[3];
        double sum_x2 = x2_array[0] + x2_array[1] + x2_array[2] + x2_array[3];
        double sum_y2 = y2_array[0] + y2_array[1] + y2_array[2] + y2_array[3];
        
        // handle remaining elements
        for (; i < size; i++) {
            double x_diff = data1[i] - mean1;
            double y_diff = data2[i] - mean2;
            sum_xy += x_diff * y_diff;
            sum_x2 += x_diff * x_diff;
            sum_y2 += y_diff * y_diff;
        }
        
        double denominator = sqrt(sum_x2 * sum_y2);
        return (denominator > 0) ? sum_xy / denominator : 0.0;
    }
#endif
    
    // fallback to scalar
    double sum_xy = 0.0, sum_x2 = 0.0, sum_y2 = 0.0;
    for (int i = 0; i < size; i++) {
        double x_diff = data1[i] - mean1;
        double y_diff = data2[i] - mean2;
        sum_xy += x_diff * y_diff;
        sum_x2 += x_diff * x_diff;
        sum_y2 += y_diff * y_diff;
    }
    
    double denominator = sqrt(sum_x2 * sum_y2);
    return (denominator > 0) ? sum_xy / denominator : 0.0;
}

// SIMD-optimized circular buffer operations
double simd_cb_rolling_mean(CircularBuffer *cb) {
    if (!cb || cb_size(cb) == 0) return 0.0;
    
    int size = cb_size(cb);
    
    // extract data to contiguous array for SIMD
    double *temp_data = malloc(size * sizeof(double));
    if (!temp_data) return rolling_mean(cb); // fallback
    
    for (int i = 0; i < size; i++) {
        temp_data[i] = cb_get(cb, i);
    }
    
    double result = simd_rolling_mean(temp_data, size);
    free(temp_data);
    
    return result;
}

double simd_cb_rolling_std(CircularBuffer *cb) {
    if (!cb || cb_size(cb) <= 1) return 0.0;
    
    int size = cb_size(cb);
    
    double *temp_data = malloc(size * sizeof(double));
    if (!temp_data) return rolling_std(cb); // fallback
    
    for (int i = 0; i < size; i++) {
        temp_data[i] = cb_get(cb, i);
    }
    
    double result = simd_rolling_std(temp_data, size);
    free(temp_data);
    
    return result;
}

double simd_cb_correlation(CircularBuffer *cb1, CircularBuffer *cb2) {
    int size1 = cb_size(cb1);
    int size2 = cb_size(cb2);
    
    if (size1 != size2 || size1 < 2) return 0.0;
    
    double *data1 = malloc(size1 * sizeof(double));
    double *data2 = malloc(size1 * sizeof(double));
    
    if (!data1 || !data2) {
        free(data1);
        free(data2);
        return calculate_correlation(cb1, cb2); // fallback
    }
    
    for (int i = 0; i < size1; i++) {
        data1[i] = cb_get(cb1, i);
        data2[i] = cb_get(cb2, i);
    }
    
    double result = simd_correlation(data1, data2, size1);
    
    free(data1);
    free(data2);
    
    return result;
}