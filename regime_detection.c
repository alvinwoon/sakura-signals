#include "sakura_signals.h"
#include <sys/time.h>

RegimeDetector* create_regime_detector(int volatility_window) {
    RegimeDetector *detector = malloc(sizeof(RegimeDetector));
    if (!detector) return NULL;
    
    detector->volatility_buffer = create_circular_buffer(volatility_window);
    detector->correlation_buffer = create_circular_buffer(volatility_window);
    
    if (!detector->volatility_buffer || !detector->correlation_buffer) {
        destroy_circular_buffer(detector->volatility_buffer);
        destroy_circular_buffer(detector->correlation_buffer);
        free(detector);
        return NULL;
    }
    
    // init regime state
    detector->current_regime = 0; // normal
    detector->regime_confidence = 1.0;
    detector->last_regime_change = 0;
    
    // init regime probs
    detector->regime_probabilities[0] = 0.8;  // normal
    detector->regime_probabilities[1] = 0.15; // stress
    detector->regime_probabilities[2] = 0.05; // crisis
    
    // init transition matrix (simplified HMM)
    // transition from normal
    detector->transition_matrix[0][0] = 0.95; // stay normal
    detector->transition_matrix[0][1] = 0.04; // to stress
    detector->transition_matrix[0][2] = 0.01; // to crisis
    
    // transition from stress
    detector->transition_matrix[1][0] = 0.60; // back to normal
    detector->transition_matrix[1][1] = 0.30; // stay stress
    detector->transition_matrix[1][2] = 0.10; // to crisis
    
    // transition from crisis
    detector->transition_matrix[2][0] = 0.20; // back to normal
    detector->transition_matrix[2][1] = 0.50; // to stress
    detector->transition_matrix[2][2] = 0.30; // stay crisis
    
    return detector;
}

void destroy_regime_detector(RegimeDetector *detector) {
    if (detector) {
        destroy_circular_buffer(detector->volatility_buffer);
        destroy_circular_buffer(detector->correlation_buffer);
        free(detector);
    }
}

void update_regime(RegimeDetector *detector, double price1, double price2, double correlation) {
    if (!detector) return;
    
    // calc log returns for volatility
    static double last_price1 = 0, last_price2 = 0;
    if (last_price1 > 0 && last_price2 > 0) {
        double ret1 = log(price1 / last_price1);
        double ret2 = log(price2 / last_price2);
        double combined_vol = sqrt(ret1*ret1 + ret2*ret2);
        
        cb_push(detector->volatility_buffer, combined_vol);
    }
    
    cb_push(detector->correlation_buffer, correlation);
    last_price1 = price1;
    last_price2 = price2;
    
    if (cb_size(detector->volatility_buffer) < 10) return;
    
    // calc regime indicators
    double current_vol = rolling_mean(detector->volatility_buffer);
    double vol_percentile = 0.0;
    int vol_size = cb_size(detector->volatility_buffer);
    
    // calc percentile rank of current vol
    int count_below = 0;
    for (int i = 0; i < vol_size; i++) {
        if (cb_get(detector->volatility_buffer, i) < current_vol) {
            count_below++;
        }
    }
    vol_percentile = (double)count_below / vol_size;
    
    // calc correlation stability
    double corr_std = rolling_std(detector->correlation_buffer);
    
    // regime classification logic
    int new_regime = 0;
    if (vol_percentile > 0.95 || corr_std > 0.3) {
        new_regime = 2; // crisis
    } else if (vol_percentile > 0.80 || corr_std > 0.15) {
        new_regime = 1; // stress
    } else {
        new_regime = 0; // normal
    }
    
    // update regime probs using simple bayes
    double evidence = 1.0;
    if (new_regime == 2) evidence = vol_percentile * 2.0; // crisis evidence
    else if (new_regime == 1) evidence = vol_percentile * 1.5; // stress evidence
    
    // apply transition probs
    double new_probs[MAX_REGIMES];
    for (int i = 0; i < MAX_REGIMES; i++) {
        new_probs[i] = detector->transition_matrix[detector->current_regime][i] * evidence;
    }
    
    // normalize
    double total = new_probs[0] + new_probs[1] + new_probs[2];
    if (total > 0) {
        for (int i = 0; i < MAX_REGIMES; i++) {
            detector->regime_probabilities[i] = new_probs[i] / total;
        }
    }
    
    // update current regime based on highest prob
    int max_regime = 0;
    double max_prob = detector->regime_probabilities[0];
    for (int i = 1; i < MAX_REGIMES; i++) {
        if (detector->regime_probabilities[i] > max_prob) {
            max_prob = detector->regime_probabilities[i];
            max_regime = i;
        }
    }
    
    if (max_regime != detector->current_regime) {
        detector->last_regime_change = 0; // reset counter
        detector->current_regime = max_regime;
    } else {
        detector->last_regime_change++;
    }
    
    detector->regime_confidence = max_prob;
}

int detect_regime_change(RegimeDetector *detector, double threshold) {
    if (!detector) return 0;
    
    // return 1 if regime changed recently and confidence is high
    return (detector->last_regime_change < 5 && detector->regime_confidence > threshold) ? 1 : 0;
}