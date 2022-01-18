#include "sakura_signals.h"

double calculate_dynamic_hedge_ratio(CircularBuffer *price1, CircularBuffer *price2, int lookback) {
    int size1 = cb_size(price1);
    int size2 = cb_size(price2);
    
    if (size1 != size2 || size1 < lookback || lookback < 5) {
        return 1.0; // fallback to 1:1 ratio
    }
    
    // use most recent lookback periods for dynamic calc
    int start_idx = size1 - lookback;
    
    // calc log returns for both assets
    double *ret1 = malloc((lookback-1) * sizeof(double));
    double *ret2 = malloc((lookback-1) * sizeof(double));
    
    if (!ret1 || !ret2) {
        free(ret1);
        free(ret2);
        return 1.0;
    }
    
    for (int i = 0; i < lookback-1; i++) {
        double p1_curr = cb_get(price1, start_idx + i + 1);
        double p1_prev = cb_get(price1, start_idx + i);
        double p2_curr = cb_get(price2, start_idx + i + 1);
        double p2_prev = cb_get(price2, start_idx + i);
        
        ret1[i] = log(p1_curr / p1_prev);
        ret2[i] = log(p2_curr / p2_prev);
    }
    
    // calc covariance and variance for beta
    double mean_ret1 = 0.0, mean_ret2 = 0.0;
    for (int i = 0; i < lookback-1; i++) {
        mean_ret1 += ret1[i];
        mean_ret2 += ret2[i];
    }
    mean_ret1 /= (lookback-1);
    mean_ret2 /= (lookback-1);
    
    double covariance = 0.0, variance2 = 0.0;
    for (int i = 0; i < lookback-1; i++) {
        double dev1 = ret1[i] - mean_ret1;
        double dev2 = ret2[i] - mean_ret2;
        covariance += dev1 * dev2;
        variance2 += dev2 * dev2;
    }
    
    double hedge_ratio = 1.0;
    if (variance2 > 1e-8) {
        hedge_ratio = covariance / variance2;
    }
    
    free(ret1);
    free(ret2);
    
    // clamp ratio to reasonable bounds
    if (hedge_ratio < 0.1) hedge_ratio = 0.1;
    if (hedge_ratio > 5.0) hedge_ratio = 5.0;
    
    return hedge_ratio;
}

double calculate_half_life(CircularBuffer *spread_buffer) {
    int size = cb_size(spread_buffer);
    if (size < 10) return 20.0; // default half-life
    
    // fit AR(1) model: spread[t] = alpha + beta * spread[t-1] + error
    double sum_y = 0.0, sum_x = 0.0, sum_xy = 0.0, sum_x2 = 0.0;
    int n = size - 1;
    
    for (int i = 0; i < n; i++) {
        double y = cb_get(spread_buffer, i + 1); // spread[t]
        double x = cb_get(spread_buffer, i);     // spread[t-1]
        
        sum_y += y;
        sum_x += x;
        sum_xy += x * y;
        sum_x2 += x * x;
    }
    
    // calc beta coefficient
    double beta = (n * sum_xy - sum_x * sum_y) / (n * sum_x2 - sum_x * sum_x);
    
    // half-life = -log(2) / log(beta)
    double half_life = 20.0; // default
    if (beta > 0 && beta < 1) {
        half_life = -log(2.0) / log(beta);
        
        // clamp to reasonable range
        if (half_life < 1.0) half_life = 1.0;
        if (half_life > 100.0) half_life = 100.0;
    }
    
    return half_life;
}

void update_dynamic_thresholds(PairTracker *tracker, double volatility_factor) {
    if (!tracker) return;
    
    double base_entry = 2.0;
    double base_exit = 0.5;
    
    // adjust thresholds based on regime
    if (tracker->regime_detector) {
        switch (tracker->regime_detector->current_regime) {
            case 0: // normal regime
                tracker->dynamic_entry_threshold = base_entry * volatility_factor;
                tracker->dynamic_exit_threshold = base_exit * volatility_factor;
                break;
            case 1: // stress regime
                tracker->dynamic_entry_threshold = base_entry * 1.5 * volatility_factor;
                tracker->dynamic_exit_threshold = base_exit * 1.2 * volatility_factor;
                break;
            case 2: // crisis regime
                tracker->dynamic_entry_threshold = base_entry * 2.5 * volatility_factor;
                tracker->dynamic_exit_threshold = base_exit * 2.0 * volatility_factor;
                break;
        }
    } else {
        tracker->dynamic_entry_threshold = base_entry * volatility_factor;
        tracker->dynamic_exit_threshold = base_exit * volatility_factor;
    }
    
    // adjust based on half-life (faster mean reversion = tighter thresholds)
    if (cb_size(tracker->spread_buffer) > 10) {
        double half_life = calculate_half_life(tracker->spread_buffer);
        double hl_factor = 20.0 / half_life; // normalize around 20 periods
        
        tracker->dynamic_entry_threshold *= hl_factor;
        tracker->dynamic_exit_threshold *= hl_factor;
    }
    
    // ensure minimum thresholds
    if (tracker->dynamic_entry_threshold < 0.5) tracker->dynamic_entry_threshold = 0.5;
    if (tracker->dynamic_exit_threshold < 0.1) tracker->dynamic_exit_threshold = 0.1;
}