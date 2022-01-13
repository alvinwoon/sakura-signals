#include "sakura_signals.h"

PairSignal generate_pairs_signal(PairTracker *tracker, double current_price1, double current_price2) {
    PairSignal signal = {0};
    
    if (!tracker || !tracker->price_buffer1 || !tracker->price_buffer2) {
        return signal;
    }
    
    // update buffers
    cb_push(tracker->price_buffer1, current_price1);
    cb_push(tracker->price_buffer2, current_price2);
    
    // calc spread
    double current_spread = log(current_price1) - log(current_price2);
    cb_push(tracker->spread_buffer, current_spread);
    
    // rolling stats
    tracker->mean_spread = rolling_mean(tracker->spread_buffer);
    tracker->std_spread = rolling_std(tracker->spread_buffer);
    tracker->correlation = calculate_correlation(tracker->price_buffer1, tracker->price_buffer2);
    
    // Calculate z-score
    double z_score = calculate_z_score(current_spread, tracker->mean_spread, tracker->std_spread);
    
    // Generate signal based on z-score thresholds
    int trade_signal = mean_reversion_signal(z_score, 2.0, 0.5);
    
    // Fill signal structure
    signal.spread = current_spread;
    signal.z_score = z_score;
    signal.correlation = tracker->correlation;
    signal.signal = trade_signal;
    
    // Calculate cointegration test if we have enough data
    if (cb_size(tracker->price_buffer1) >= 20) {
        signal.cointegration_stat = engle_granger_test(tracker->price_buffer1, tracker->price_buffer2);
    }
    
    return signal;
}

PairSignal generate_pairs_signal_with_attention(PairTracker *tracker, double current_price1, double current_price2) {
    PairSignal signal = {0};
    
    if (!tracker || !tracker->price_buffer1 || !tracker->price_buffer2) {
        return signal;
    }
    
    // Update price buffers
    cb_push(tracker->price_buffer1, current_price1);
    cb_push(tracker->price_buffer2, current_price2);
    
    // Calculate current spread
    double current_spread = log(current_price1) - log(current_price2);
    cb_push(tracker->spread_buffer, current_spread);
    
    // Update rolling statistics
    tracker->mean_spread = rolling_mean(tracker->spread_buffer);
    tracker->std_spread = rolling_std(tracker->spread_buffer);
    tracker->correlation = calculate_correlation(tracker->price_buffer1, tracker->price_buffer2);
    
    // Calculate z-score (traditional and attention-enhanced)
    double z_score = calculate_z_score(current_spread, tracker->mean_spread, tracker->std_spread);
    
    // enhanced z-score w/ attention if enabled  
    if (tracker->use_attention && tracker->temporal_attention && cb_size(tracker->spread_buffer) >= 10) {
        tracker->attention_enhanced_zscore = calculate_attention_enhanced_zscore(
            tracker->spread_buffer, tracker->temporal_attention);
        
        // blend trad + attention z-scores
        double blend_factor = 0.7; // 70% attn, 30% trad
        z_score = blend_factor * tracker->attention_enhanced_zscore + (1 - blend_factor) * z_score;
    } else {
        tracker->attention_enhanced_zscore = z_score;
    }
    
    // Generate signal based on enhanced z-score thresholds
    int trade_signal = mean_reversion_signal(z_score, 2.0, 0.5);
    
    // Fill signal structure
    signal.spread = current_spread;
    signal.z_score = z_score;
    signal.correlation = tracker->correlation;
    signal.signal = trade_signal;
    
    // Calculate cointegration test if we have enough data
    if (cb_size(tracker->price_buffer1) >= 20) {
        signal.cointegration_stat = engle_granger_test(tracker->price_buffer1, tracker->price_buffer2);
    }
    
    return signal;
}

int mean_reversion_signal(double z_score, double entry_threshold, double exit_threshold) {
    static int current_position = 0; // 0: no position, 1: long, -1: short
    
    // Entry signals
    if (current_position == 0) {
        if (z_score > entry_threshold) {
            current_position = -1; // Short spread (short asset1, long asset2)
            return -1;
        } else if (z_score < -entry_threshold) {
            current_position = 1;  // Long spread (long asset1, short asset2)
            return 1;
        }
    }
    // Exit signals
    else if (current_position == 1) {
        if (z_score > -exit_threshold) {
            current_position = 0;
            return 0; // Close long position
        }
    }
    else if (current_position == -1) {
        if (z_score < exit_threshold) {
            current_position = 0;
            return 0; // Close short position
        }
    }
    
    return current_position; // Hold current position
}

void print_pair_signal(PairSignal *signal) {
    const char* signal_str;
    switch (signal->signal) {
        case -1: signal_str = "SHORT"; break;
        case 0:  signal_str = "NEUTRAL"; break;
        case 1:  signal_str = "LONG"; break;
        default: signal_str = "UNKNOWN"; break;
    }
    
    printf("Pair: %s/%s | Spread: %.6f | Z-Score: %.3f | Corr: %.3f | Signal: %s\n",
           signal->symbol1, signal->symbol2, signal->spread, signal->z_score, 
           signal->correlation, signal_str);
}

PairSignal generate_enhanced_pairs_signal(PairTracker *tracker, double price1, double price2,
                                        double bid1, double ask1, double bid2, double ask2, long timestamp_micro) {
    PairSignal signal = {0};
    
    if (!tracker || !tracker->price_buffer1 || !tracker->price_buffer2) {
        return signal;
    }
    
    // update price buffers
    cb_push(tracker->price_buffer1, price1);
    cb_push(tracker->price_buffer2, price2);
    
    // calc dynamic hedge ratio if enabled
    if (tracker->use_dynamic_hedging && cb_size(tracker->price_buffer1) >= 20) {
        tracker->current_hedge_ratio = calculate_dynamic_hedge_ratio(
            tracker->price_buffer1, tracker->price_buffer2, 20);
        cb_push(tracker->hedge_ratio_buffer, tracker->current_hedge_ratio);
    } else {
        tracker->current_hedge_ratio = 1.0;
    }
    
    // calc spread using dynamic hedge ratio
    double current_spread = price1 - tracker->current_hedge_ratio * price2;
    cb_push(tracker->spread_buffer, current_spread);
    
    // calc volatilities for both assets
    if (cb_size(tracker->price_buffer1) >= 2) {
        double ret1 = log(price1 / cb_get(tracker->price_buffer1, cb_size(tracker->price_buffer1) - 2));
        double ret2 = log(price2 / cb_get(tracker->price_buffer2, cb_size(tracker->price_buffer2) - 2));
        cb_push(tracker->volatility1_buffer, ret1 * ret1);
        cb_push(tracker->volatility2_buffer, ret2 * ret2);
    }
    
    // update rolling stats with SIMD if available
    tracker->mean_spread = simd_cb_rolling_mean(tracker->spread_buffer);
    tracker->std_spread = simd_cb_rolling_std(tracker->spread_buffer);
    tracker->correlation = simd_cb_correlation(tracker->price_buffer1, tracker->price_buffer2);
    
    // update regime detection
    if (tracker->use_regime_detection && tracker->regime_detector) {
        update_regime(tracker->regime_detector, price1, price2, tracker->correlation);
    }
    
    // calc dynamic thresholds based on current volatility
    double vol_factor = 1.0;
    if (cb_size(tracker->volatility1_buffer) > 5) {
        double vol1 = sqrt(rolling_mean(tracker->volatility1_buffer));
        double vol2 = sqrt(rolling_mean(tracker->volatility2_buffer));
        vol_factor = (vol1 + vol2) / 0.02; // normalize around 2% daily vol
    }
    
    update_dynamic_thresholds(tracker, vol_factor);
    
    // calc z-score
    double z_score = calculate_z_score(current_spread, tracker->mean_spread, tracker->std_spread);
    
    // enhanced z-score w/ attention if enabled
    if (tracker->use_attention && tracker->temporal_attention && cb_size(tracker->spread_buffer) >= 10) {
        tracker->attention_enhanced_zscore = calculate_attention_enhanced_zscore(
            tracker->spread_buffer, tracker->temporal_attention);
        
        // blend traditional + attention z-scores
        double blend_factor = 0.7; // 70% attn, 30% trad
        z_score = blend_factor * tracker->attention_enhanced_zscore + (1 - blend_factor) * z_score;
    } else {
        tracker->attention_enhanced_zscore = z_score;
    }
    
    // generate signal using dynamic thresholds
    int trade_signal = mean_reversion_signal(z_score, tracker->dynamic_entry_threshold, tracker->dynamic_exit_threshold);
    
    // calc position size using volatility targeting if risk manager available
    double position_size = 10000.0; // default
    if (tracker->risk_manager) {
        // adjust target vol based on current regime
        if (tracker->regime_detector) {
            double regime_adjusted_target = calculate_regime_adjusted_target_vol(
                tracker->risk_manager, tracker->regime_detector->current_regime);
            tracker->risk_manager->target_volatility = regime_adjusted_target;
        }
        
        position_size = calculate_volatility_target_size(tracker->risk_manager, fabs(z_score), 1000000.0);
        
        // update volatility estimate with recent P&L
        if (cb_size(tracker->spread_buffer) > 1) {
            double recent_return = (current_spread - cb_get(tracker->spread_buffer, cb_size(tracker->spread_buffer) - 2)) / position_size;
            update_volatility_estimate(tracker->risk_manager, recent_return);
        }
    }
    
    // check transaction costs if enabled
    PnLAnalysis pnl_analysis = {0};
    if (tracker->use_transaction_costs) {
        // update transaction costs with current spreads
        tracker->transaction_costs.bid_ask_spread_asset1 = ask1 - bid1;
        tracker->transaction_costs.bid_ask_spread_asset2 = ask2 - bid2;
        
        double theoretical_pnl = fabs(z_score) * 0.3 * position_size; // rough estimate
        pnl_analysis = calculate_pnl_with_costs(theoretical_pnl, &tracker->transaction_costs, position_size);
        
        // override signal if not profitable after costs
        if (!pnl_analysis.is_profitable) {
            trade_signal = 0;
        }
    }
    
    // fill enhanced signal structure
    signal.spread = current_spread;
    signal.z_score = z_score;
    signal.correlation = tracker->correlation;
    signal.hedge_ratio = tracker->current_hedge_ratio;
    signal.dynamic_threshold_entry = tracker->dynamic_entry_threshold;
    signal.dynamic_threshold_exit = tracker->dynamic_exit_threshold;
    signal.signal = trade_signal;
    signal.position_size = position_size;
    signal.pnl_analysis = pnl_analysis;
    signal.timestamp_micro = timestamp_micro;
    
    if (tracker->regime_detector) {
        signal.regime = tracker->regime_detector->current_regime;
    }
    
    // calc cointegration tests if enough data
    if (cb_size(tracker->price_buffer1) >= 30) {
        signal.cointegration_stat = johansen_test(tracker->price_buffer1, tracker->price_buffer2);
    }
    
    tracker->last_update_micro = timestamp_micro;
    
    return signal;
}
