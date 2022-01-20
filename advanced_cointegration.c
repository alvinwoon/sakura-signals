#include "sakura_signals.h"

// simplified johansen test implementation
double johansen_test(CircularBuffer *price1, CircularBuffer *price2) {
    int n = cb_size(price1);
    if (n != cb_size(price2) || n < 20) return 0.0;
    
    // convert prices to log returns for stationarity
    double *ret1 = malloc((n-1) * sizeof(double));
    double *ret2 = malloc((n-1) * sizeof(double));
    
    if (!ret1 || !ret2) {
        free(ret1);
        free(ret2);
        return 0.0;
    }
    
    for (int i = 0; i < n-1; i++) {
        double p1_curr = cb_get(price1, i+1);
        double p1_prev = cb_get(price1, i);
        double p2_curr = cb_get(price2, i+1);
        double p2_prev = cb_get(price2, i);
        
        ret1[i] = log(p1_curr / p1_prev);
        ret2[i] = log(p2_curr / p2_prev);
    }
    
    // simplified eigenvalue calc (would use LAPACK in production)
    // calculate covariance matrix
    double mean1 = 0.0, mean2 = 0.0;
    for (int i = 0; i < n-1; i++) {
        mean1 += ret1[i];
        mean2 += ret2[i];
    }
    mean1 /= (n-1);
    mean2 /= (n-1);
    
    double cov11 = 0.0, cov12 = 0.0, cov22 = 0.0;
    for (int i = 0; i < n-1; i++) {
        double d1 = ret1[i] - mean1;
        double d2 = ret2[i] - mean2;
        cov11 += d1 * d1;
        cov12 += d1 * d2;
        cov22 += d2 * d2;
    }
    cov11 /= (n-2);
    cov12 /= (n-2);
    cov22 /= (n-2);
    
    // simplified trace test statistic
    double det = cov11 * cov22 - cov12 * cov12;
    double trace = cov11 + cov22;
    
    // johansen trace statistic (simplified)
    double lambda_trace = -n * log(1 - (trace - sqrt(trace*trace - 4*det)) / 2);
    
    free(ret1);
    free(ret2);
    
    return lambda_trace;
}

double threshold_cointegration_test(CircularBuffer *spread, double threshold) {
    int n = cb_size(spread);
    if (n < 10) return 0.0;
    
    // fit threshold autoregressive model
    // spread[t] = alpha + beta * spread[t-1] * I(|spread[t-1]| > threshold) + error
    
    double sum_y = 0.0, sum_x = 0.0, sum_xy = 0.0, sum_x2 = 0.0;
    int valid_points = 0;
    
    for (int i = 1; i < n; i++) {
        double y = cb_get(spread, i);
        double x_prev = cb_get(spread, i-1);
        
        // apply threshold condition
        if (fabs(x_prev) > threshold) {
            double x = x_prev;
            
            sum_y += y;
            sum_x += x;
            sum_xy += x * y;
            sum_x2 += x * x;
            valid_points++;
        }
    }
    
    if (valid_points < 5) return 0.0;
    
    // calc beta coefficient for threshold model
    double beta = (valid_points * sum_xy - sum_x * sum_y) / 
                  (valid_points * sum_x2 - sum_x * sum_x);
    
    // threshold cointegration test stat
    double test_stat = fabs(beta - 1.0) * sqrt(valid_points);
    
    return test_stat;
}

// fractional cointegration test (simplified)
double fractional_cointegration_test(CircularBuffer *price1, CircularBuffer *price2) {
    int n = cb_size(price1);
    if (n != cb_size(price2) || n < 30) return 0.0;
    
    // calc log price spread
    double *spread = malloc(n * sizeof(double));
    if (!spread) return 0.0;
    
    for (int i = 0; i < n; i++) {
        double p1 = cb_get(price1, i);
        double p2 = cb_get(price2, i);
        spread[i] = log(p1) - log(p2);
    }
    
    // estimate fractional integration parameter d using R/S statistic
    double *cumsum = malloc(n * sizeof(double));
    if (!cumsum) {
        free(spread);
        return 0.0;
    }
    
    // calc cumulative sum of deviations from mean
    double mean_spread = 0.0;
    for (int i = 0; i < n; i++) {
        mean_spread += spread[i];
    }
    mean_spread /= n;
    
    cumsum[0] = spread[0] - mean_spread;
    for (int i = 1; i < n; i++) {
        cumsum[i] = cumsum[i-1] + (spread[i] - mean_spread);
    }
    
    // calc range and standard deviation
    double max_cumsum = cumsum[0], min_cumsum = cumsum[0];
    for (int i = 1; i < n; i++) {
        if (cumsum[i] > max_cumsum) max_cumsum = cumsum[i];
        if (cumsum[i] < min_cumsum) min_cumsum = cumsum[i];
    }
    double range = max_cumsum - min_cumsum;
    
    double std_dev = 0.0;
    for (int i = 0; i < n; i++) {
        double dev = spread[i] - mean_spread;
        std_dev += dev * dev;
    }
    std_dev = sqrt(std_dev / (n-1));
    
    // R/S statistic
    double rs_stat = (std_dev > 0) ? range / std_dev : 0.0;
    
    // hurst exponent estimate
    double hurst = (rs_stat > 0) ? log(rs_stat) / log(n) : 0.5;
    
    // fractional integration parameter d = H - 0.5
    double d_param = hurst - 0.5;
    
    free(spread);
    free(cumsum);
    
    // test statistic: significant deviation from d=0 (cointegration)
    return fabs(d_param) * sqrt(n);
}

// error correction model test
double error_correction_test(CircularBuffer *price1, CircularBuffer *price2, CircularBuffer *spread) {
    int n = cb_size(spread);
    if (n < 15 || cb_size(price1) != n || cb_size(price2) != n) return 0.0;
    
    // calc first differences of prices
    double *diff1 = malloc((n-1) * sizeof(double));
    double *diff2 = malloc((n-1) * sizeof(double));
    double *spread_lag = malloc((n-1) * sizeof(double));
    
    if (!diff1 || !diff2 || !spread_lag) {
        free(diff1);
        free(diff2);
        free(spread_lag);
        return 0.0;
    }
    
    for (int i = 0; i < n-1; i++) {
        diff1[i] = cb_get(price1, i+1) - cb_get(price1, i);
        diff2[i] = cb_get(price2, i+1) - cb_get(price2, i);
        spread_lag[i] = cb_get(spread, i); // lagged spread
    }
    
    // estimate error correction coeff for asset 1
    // diff1[t] = alpha + gamma * spread[t-1] + error
    double sum_y = 0.0, sum_x = 0.0, sum_xy = 0.0, sum_x2 = 0.0;
    
    for (int i = 0; i < n-1; i++) {
        sum_y += diff1[i];
        sum_x += spread_lag[i];
        sum_xy += spread_lag[i] * diff1[i];
        sum_x2 += spread_lag[i] * spread_lag[i];
    }
    
    double gamma = ((n-1) * sum_xy - sum_x * sum_y) / 
                   ((n-1) * sum_x2 - sum_x * sum_x);
    
    // error correction test: gamma should be negative and significant
    double test_stat = fabs(gamma) * sqrt(n-1);
    
    free(diff1);
    free(diff2);
    free(spread_lag);
    
    return test_stat;
}