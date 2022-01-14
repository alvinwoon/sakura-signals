#include "sakura_signals.h"
#include <time.h>

// Helper function to generate correlated price series
void generate_sample_data(double *prices1, double *prices2, int n, double correlation) {
    srand((unsigned int)time(NULL));
    
    double price1 = 100.0;
    double price2 = 95.0;
    
    for (int i = 0; i < n; i++) {
        // Generate correlated random walks
        double random1 = ((double)rand() / RAND_MAX - 0.5) * 0.02;
        double random2 = correlation * random1 + sqrt(1 - correlation * correlation) * 
                        ((double)rand() / RAND_MAX - 0.5) * 0.02;
        
        price1 *= (1 + random1);
        price2 *= (1 + random2);
        
        prices1[i] = price1;
        prices2[i] = price2;
    }
}

PairTracker* create_pair_tracker(int window_size) {
    PairTracker *tracker = malloc(sizeof(PairTracker));
    if (!tracker) return NULL;
    
    tracker->price_buffer1 = create_circular_buffer(window_size);
    tracker->price_buffer2 = create_circular_buffer(window_size);
    tracker->spread_buffer = create_circular_buffer(window_size);
    tracker->window_size = window_size;
    tracker->mean_spread = 0.0;
    tracker->std_spread = 0.0;
    tracker->correlation = 0.0;
    tracker->attention_enhanced_zscore = 0.0;
    tracker->use_attention = false;
    tracker->temporal_attention = NULL;
    tracker->attention_cache = NULL;
    
    if (!tracker->price_buffer1 || !tracker->price_buffer2 || !tracker->spread_buffer) {
        destroy_circular_buffer(tracker->price_buffer1);
        destroy_circular_buffer(tracker->price_buffer2);
        destroy_circular_buffer(tracker->spread_buffer);
        free(tracker);
        return NULL;
    }
    
    return tracker;
}

PairTracker* create_pair_tracker_with_attention(int window_size) {
    PairTracker *tracker = create_pair_tracker(window_size);
    if (!tracker) return NULL;
    
    // Initialize attention mechanism
    tracker->temporal_attention = create_attention_layer(1, 2, window_size); // 1D input, 2D attention
    tracker->attention_cache = create_attention_output(window_size, 2);
    tracker->use_attention = true;
    
    if (!tracker->temporal_attention || !tracker->attention_cache) {
        destroy_attention_layer(tracker->temporal_attention);
        destroy_attention_output(tracker->attention_cache);
        destroy_pair_tracker(tracker);
        return NULL;
    }
    
    return tracker;
}

PairTracker* create_enhanced_pair_tracker(int window_size, bool use_all_features) {
    PairTracker *tracker = create_pair_tracker(window_size);
    if (!tracker) return NULL;
    
    // init additional buffers
    tracker->hedge_ratio_buffer = create_circular_buffer(window_size);
    tracker->volatility1_buffer = create_circular_buffer(window_size);
    tracker->volatility2_buffer = create_circular_buffer(window_size);
    
    if (!tracker->hedge_ratio_buffer || !tracker->volatility1_buffer || !tracker->volatility2_buffer) {
        destroy_pair_tracker(tracker);
        return NULL;
    }
    
    if (use_all_features) {
        // enable attention
        tracker->temporal_attention = create_attention_layer(1, 2, window_size);
        tracker->attention_cache = create_attention_output(window_size, 2);
        tracker->use_attention = true;
        
        // enable regime detection
        tracker->regime_detector = create_regime_detector(window_size / 2);
        tracker->use_regime_detection = true;
        
        // enable risk management with volatility targeting
        tracker->risk_manager = create_risk_manager(window_size, 0.15); // 15% target vol
        
        // enable dynamic hedging
        tracker->use_dynamic_hedging = true;
        
        // enable transaction costs
        tracker->use_transaction_costs = true;
        tracker->transaction_costs = create_transaction_costs(0.001, 0.001, 0.0005, 0.0005);
        
        // init dynamic thresholds
        tracker->dynamic_entry_threshold = 2.0;
        tracker->dynamic_exit_threshold = 0.5;
        tracker->current_hedge_ratio = 1.0;
    }
    
    return tracker;
}

void destroy_pair_tracker(PairTracker *tracker) {
    if (tracker) {
        destroy_circular_buffer(tracker->price_buffer1);
        destroy_circular_buffer(tracker->price_buffer2);
        destroy_circular_buffer(tracker->spread_buffer);
        destroy_circular_buffer(tracker->hedge_ratio_buffer);
        destroy_circular_buffer(tracker->volatility1_buffer);
        destroy_circular_buffer(tracker->volatility2_buffer);
        destroy_attention_layer(tracker->temporal_attention);
        destroy_attention_output(tracker->attention_cache);
        destroy_regime_detector(tracker->regime_detector);
        destroy_risk_manager(tracker->risk_manager);
        free(tracker);
    }
}

int main(void) {
    printf("=== Sakura Signals: Statistical Arbitrage ===\n\n");
    
    const int n_points = 200;
    const int window_size = 50;
    double correlation = 0.75;
    
    // Generate sample price data
    double *prices1 = malloc(n_points * sizeof(double));
    double *prices2 = malloc(n_points * sizeof(double));
    
    if (!prices1 || !prices2) {
        printf("Memory allocation failed\n");
        return 1;
    }
    
    generate_sample_data(prices1, prices2, n_points, correlation);
    
    // Create traditional pair tracker
    PairTracker *tracker = create_pair_tracker(window_size);
    if (!tracker) {
        printf("Failed to create pair tracker\n");
        free(prices1);
        free(prices2);
        return 1;
    }
    
    // Create fully enhanced pair tracker
    PairTracker *enhanced_tracker = create_enhanced_pair_tracker(window_size, true);
    if (!enhanced_tracker) {
        printf("Failed to create enhanced pair tracker\n");
        destroy_pair_tracker(tracker);
        free(prices1);
        free(prices2);
        return 1;
    }
    
    printf("Analyzing %d price points with rolling window of %d...\n", n_points, window_size);
    printf("Target correlation: %.3f\n\n", correlation);
    
    printf("=== Traditional vs Production-Ready Enhanced Signals ===\n");
    
    // process price data w/ enhanced features
    int signal_count = 0;
    int enhanced_signal_count = 0;
    long timestamp = 1640995200000000L; // start timestamp (microseconds)
    
    for (int i = 0; i < n_points; i++) {
        PairSignal signal = generate_pairs_signal(tracker, prices1[i], prices2[i]);
        
        // generate realistic bid/ask spreads
        double spread1 = prices1[i] * 0.0002; // 2 bps spread
        double spread2 = prices2[i] * 0.0002;
        double bid1 = prices1[i] - spread1/2;
        double ask1 = prices1[i] + spread1/2;
        double bid2 = prices2[i] - spread2/2;
        double ask2 = prices2[i] + spread2/2;
        
        PairSignal enhanced_signal = generate_enhanced_pairs_signal(enhanced_tracker, 
            prices1[i], prices2[i], bid1, ask1, bid2, ask2, timestamp + i * 1000000);
        
        // copy symbols for display
        strcpy(signal.symbol1, "AAPL");
        strcpy(signal.symbol2, "MSFT");
        strcpy(enhanced_signal.symbol1, "AAPL");
        strcpy(enhanced_signal.symbol2, "MSFT");
        
        // print signals every 20 points after sufficient data
        if (i >= window_size && i % 20 == 0) {
            printf("\n--- Day %3d ---\n", i);
            printf("Traditional:     ");
            print_pair_signal(&signal);
            printf("Production:      ");
            print_pair_signal(&enhanced_signal);
            
            const char* regime_str = "NORMAL";
            if (enhanced_signal.regime == 1) regime_str = "STRESS";
            else if (enhanced_signal.regime == 2) regime_str = "CRISIS";
            
            printf("  Hedge Ratio: %.3f | Regime: %s | Pos Size: $%.0f\n", 
                   enhanced_signal.hedge_ratio, regime_str, enhanced_signal.position_size);
            printf("  Dynamic Entry: %.2f | Exit: %.2f | Net PnL: $%.2f\n",
                   enhanced_signal.dynamic_threshold_entry, enhanced_signal.dynamic_threshold_exit,
                   enhanced_signal.pnl_analysis.net_pnl_after_costs);
        }
        
        if (signal.signal != 0) {
            signal_count++;
        }
        if (enhanced_signal.signal != 0) {
            enhanced_signal_count++;
        }
    }
    
    printf("\n=== Performance Summary ===\n");
    printf("Traditional signals generated: %d\n", signal_count);
    printf("Production signals generated: %d\n", enhanced_signal_count);
    printf("Signal improvement: %+d (%.1f%%)\n", 
           enhanced_signal_count - signal_count,
           signal_count > 0 ? 100.0 * (enhanced_signal_count - signal_count) / signal_count : 0.0);
    printf("Final correlation: %.3f\n", tracker->correlation);
    printf("Final spread mean: %.6f\n", tracker->mean_spread);
    printf("Final spread std: %.6f\n", tracker->std_spread);
    printf("Final hedge ratio: %.3f\n", enhanced_tracker->current_hedge_ratio);
    printf("Final regime: %d\n", enhanced_tracker->regime_detector ? enhanced_tracker->regime_detector->current_regime : 0);
    if (enhanced_tracker->risk_manager) {
        printf("Final Sharpe ratio: %.3f\n", enhanced_tracker->risk_manager->sharpe_ratio);
        printf("Target volatility: %.1f%% | Current volatility: %.1f%%\n", 
               enhanced_tracker->risk_manager->target_volatility * 100,
               enhanced_tracker->risk_manager->current_volatility * 100);
        printf("Volatility scalar: %.2fx | Portfolio heat: %.3f\n",
               enhanced_tracker->risk_manager->volatility_scalar,
               enhanced_tracker->risk_manager->portfolio_heat);
    }
    
    // Demo correlation matrix with multiple assets
    printf("\n=== Correlation Matrix Demo ===\n");
    const int n_assets = 3;
    CircularBuffer **buffers = malloc(n_assets * sizeof(CircularBuffer*));
    
    for (int i = 0; i < n_assets; i++) {
        buffers[i] = create_circular_buffer(window_size);
        // Fill with sample data (reuse generated prices)
        for (int j = 0; j < window_size && j < n_points; j++) {
            double price = prices1[j] * (1 + i * 0.1); // Slightly different assets
            cb_push(buffers[i], price);
        }
    }
    
    CorrelationMatrix *cm = create_correlation_matrix(n_assets);
    update_correlation_matrix(cm, buffers, n_assets);
    
    printf("Correlation Matrix:\n");
    for (int i = 0; i < n_assets; i++) {
        for (int j = 0; j < n_assets; j++) {
            printf("%8.3f ", cm->matrix[i][j]);
        }
        printf("\n");
    }
    
    // cleanup
    destroy_pair_tracker(tracker);
    destroy_pair_tracker(enhanced_tracker);
    destroy_correlation_matrix(cm);
    for (int i = 0; i < n_assets; i++) {
        destroy_circular_buffer(buffers[i]);
    }
    free(buffers);
    free(prices1);
    free(prices2);
    
    printf("\nDemo completed successfully!\n");
    return 0;
}
