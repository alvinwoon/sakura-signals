#include "sakura_signals.h"

double rolling_mean(CircularBuffer *cb) {
    if (cb_size(cb) == 0) return 0.0;
    
    double sum = 0.0;
    int size = cb_size(cb);
    
    // sum all values in buffer
    for (int i = 0; i < size; i++) {
        sum += cb_get(cb, i);
    }
    
    return sum / size;
}

double rolling_std(CircularBuffer *cb) {
    int size = cb_size(cb);
    if (size <= 1) return 0.0;
    
    double mean = rolling_mean(cb);
    double sum_sq_diff = 0.0;
    
    for (int i = 0; i < size; i++) {
        double diff = cb_get(cb, i) - mean;
        sum_sq_diff += diff * diff;
    }
    
    return sqrt(sum_sq_diff / (size - 1));
}

double calculate_z_score(double value, double mean, double std) {
    if (std == 0.0) return 0.0;
    return (value - mean) / std;
}

double calculate_correlation(CircularBuffer *cb1, CircularBuffer *cb2) {
    int size1 = cb_size(cb1);
    int size2 = cb_size(cb2);
    
    if (size1 != size2 || size1 < 2) return 0.0;
    
    int n = size1;
    double mean1 = rolling_mean(cb1);
    double mean2 = rolling_mean(cb2);
    
    double numerator = 0.0;
    double sum_sq1 = 0.0;
    double sum_sq2 = 0.0;
    
    // calc pearson correlation
    for (int i = 0; i < n; i++) {
        double x1 = cb_get(cb1, i) - mean1;
        double x2 = cb_get(cb2, i) - mean2;
        
        numerator += x1 * x2;
        sum_sq1 += x1 * x1;
        sum_sq2 += x2 * x2;
    }
    
    double denominator = sqrt(sum_sq1 * sum_sq2);
    if (denominator == 0.0) return 0.0;
    
    return numerator / denominator;
}

double linear_regression_slope(CircularBuffer *y, CircularBuffer *x) {
    int n = cb_size(x);
    if (n != cb_size(y) || n < 2) return 0.0;
    
    double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_x2 = 0.0;
    
    for (int i = 0; i < n; i++) {
        double xi = cb_get(x, i);
        double yi = cb_get(y, i);
        
        sum_x += xi;
        sum_y += yi;
        sum_xy += xi * yi;
        sum_x2 += xi * xi;
    }
    
    double denominator = n * sum_x2 - sum_x * sum_x;
    if (fabs(denominator) < 1e-10) return 0.0;
    
    return (n * sum_xy - sum_x * sum_y) / denominator;
}
