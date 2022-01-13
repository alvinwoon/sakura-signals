#include "sakura_signals.h"

double engle_granger_test(CircularBuffer *y, CircularBuffer *x) {
    int n = cb_size(x);
    if (n != cb_size(y) || n < 10) return 0.0;
    
    // step 1: calc beta
    double beta = linear_regression_slope(y, x);
    
    // step 2: residuals
    CircularBuffer *residuals = create_circular_buffer(n);
    if (!residuals) return 0.0;
    
    double mean_x = rolling_mean(x);
    double mean_y = rolling_mean(y);
    double alpha = mean_y - beta * mean_x;
    
    for (int i = 0; i < n; i++) {
        double predicted = alpha + beta * cb_get(x, i);
        double residual = cb_get(y, i) - predicted;
        cb_push(residuals, residual);
    }
    
    // step 3: ADF test (simplified)
    // first diffs
    CircularBuffer *diff_residuals = create_circular_buffer(n - 1);
    if (!diff_residuals) {
        destroy_circular_buffer(residuals);
        return 0.0;
    }
    
    for (int i = 1; i < n; i++) {
        double diff = cb_get(residuals, i) - cb_get(residuals, i - 1);
        cb_push(diff_residuals, diff);
    }
    
    // simple ADF stat calc
    double mean_residual = rolling_mean(residuals);
    double std_residual = rolling_std(residuals);
    
    // test stat
    double test_stat = 0.0;
    if (std_residual > 0) {
        test_stat = mean_residual / std_residual * sqrt(n);
    }
    
    destroy_circular_buffer(residuals);
    destroy_circular_buffer(diff_residuals);
    
    return test_stat;
}

bool test_cointegration(double test_stat, double critical_value) {
    // For Engle-Granger test, we reject null hypothesis if test_stat < critical_value
    // Critical values are typically negative (e.g., -3.34 for 5% significance)
    return test_stat < critical_value;
}
