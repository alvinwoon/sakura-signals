#ifndef SAKURA_SIGNALS_H
#define SAKURA_SIGNALS_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>

#define MAX_SYMBOLS 1000
#define MAX_WINDOW_SIZE 252
#define MAX_PAIRS 500
#define MAX_REGIMES 3
#define SIMD_ALIGNMENT 32

typedef struct {
    double price;
    long timestamp;
} PricePoint;

typedef struct {
    double *data;
    int size;
    int capacity;
    int head;
    bool is_full;
} CircularBuffer;

typedef struct {
    double **matrix;
    int size;
} CorrelationMatrix;

typedef struct {
    int current_regime;  // 0: normal, 1: stress, 2: crisis
    double regime_confidence;
    double regime_probabilities[MAX_REGIMES];
    double transition_matrix[MAX_REGIMES][MAX_REGIMES];
    CircularBuffer *volatility_buffer;
    CircularBuffer *correlation_buffer;
    int last_regime_change;
} RegimeDetector;

typedef struct {
    double bid_ask_spread_asset1;
    double bid_ask_spread_asset2;
    double market_impact_asset1;
    double market_impact_asset2;
    double financing_cost;
    double commission_rate;
    double position_size;
    double slippage_factor;
} TransactionCosts;

typedef struct {
    double theoretical_pnl;
    double net_pnl_after_costs;
    double total_cost;
    double market_impact_cost;
    double spread_cost;
    double financing_cost;
    bool is_profitable;
} PnLAnalysis;

typedef struct {
    double target_volatility;
    double current_volatility;
    double volatility_scalar;
    double position_size;
    double max_position_limit;
    double portfolio_heat;
    double risk_per_trade;
    double sharpe_ratio;
    double max_drawdown;
    CircularBuffer *returns_buffer;
    CircularBuffer *volatility_buffer;
    int volatility_window;
} RiskManager;

typedef struct {
    char symbol1[16];
    char symbol2[16];
    double spread;
    double z_score;
    double correlation;
    double cointegration_stat;
    double hedge_ratio;
    double dynamic_threshold_entry;
    double dynamic_threshold_exit;
    int signal; // -1: short, 0: neutral, 1: long
    int regime;
    PnLAnalysis pnl_analysis;
    double position_size;
    long timestamp_micro;
} PairSignal;

typedef struct {
    double **query_weights;
    double **key_weights;
    double **value_weights;
    int input_dim;
    int attention_dim;
    int sequence_length;
} AttentionLayer;

typedef struct {
    double *attention_scores;
    double *context_vector;
    double *weighted_features;
    int sequence_length;
    int feature_dim;
} AttentionOutput;

typedef struct {
    CircularBuffer *price_buffer1;
    CircularBuffer *price_buffer2;
    CircularBuffer *spread_buffer;
    CircularBuffer *hedge_ratio_buffer;
    CircularBuffer *volatility1_buffer;
    CircularBuffer *volatility2_buffer;
    AttentionLayer *temporal_attention;
    AttentionOutput *attention_cache;
    RegimeDetector *regime_detector;
    TransactionCosts transaction_costs;
    RiskManager *risk_manager;
    double mean_spread;
    double std_spread;
    double correlation;
    double attention_enhanced_zscore;
    double current_hedge_ratio;
    double dynamic_entry_threshold;
    double dynamic_exit_threshold;
    int window_size;
    bool use_attention;
    bool use_regime_detection;
    bool use_dynamic_hedging;
    bool use_transaction_costs;
    long last_update_micro;
} PairTracker;

// Circular buffer functions
CircularBuffer* create_circular_buffer(int capacity);
void destroy_circular_buffer(CircularBuffer *cb);
void cb_push(CircularBuffer *cb, double value);
double cb_get(CircularBuffer *cb, int index);
int cb_size(CircularBuffer *cb);

// Statistical functions
double rolling_mean(CircularBuffer *cb);
double rolling_std(CircularBuffer *cb);
double calculate_z_score(double value, double mean, double std);
double calculate_correlation(CircularBuffer *cb1, CircularBuffer *cb2);

// Correlation matrix functions
CorrelationMatrix* create_correlation_matrix(int size);
void destroy_correlation_matrix(CorrelationMatrix *cm);
void update_correlation_matrix(CorrelationMatrix *cm, CircularBuffer **buffers, int n_series);

// Cointegration functions
double engle_granger_test(CircularBuffer *y, CircularBuffer *x);
bool test_cointegration(double test_stat, double critical_value);

// Attention mechanism functions
AttentionLayer* create_attention_layer(int input_dim, int attention_dim, int sequence_length);
void destroy_attention_layer(AttentionLayer *layer);
AttentionOutput* create_attention_output(int sequence_length, int feature_dim);
void destroy_attention_output(AttentionOutput *output);
double* softmax(double *scores, int length);
double* matrix_multiply(double **matrix, double *vector, int rows, int cols);
AttentionOutput* apply_temporal_attention(AttentionLayer *layer, CircularBuffer *sequence);
double calculate_attention_enhanced_zscore(CircularBuffer *spread_buffer, AttentionLayer *attention);

// Signal generation functions
PairSignal generate_pairs_signal(PairTracker *tracker, double current_price1, double current_price2);
PairSignal generate_pairs_signal_with_attention(PairTracker *tracker, double current_price1, double current_price2);
int mean_reversion_signal(double z_score, double entry_threshold, double exit_threshold);

// Utility functions
void print_pair_signal(PairSignal *signal);
double linear_regression_slope(CircularBuffer *y, CircularBuffer *x);

// Regime detection functions
RegimeDetector* create_regime_detector(int volatility_window);
void destroy_regime_detector(RegimeDetector *detector);
void update_regime(RegimeDetector *detector, double price1, double price2, double correlation);
int detect_regime_change(RegimeDetector *detector, double threshold);

// Dynamic hedging functions
double calculate_dynamic_hedge_ratio(CircularBuffer *price1, CircularBuffer *price2, int lookback);
double calculate_half_life(CircularBuffer *spread_buffer);
void update_dynamic_thresholds(PairTracker *tracker, double volatility_factor);

// Transaction cost functions
TransactionCosts create_transaction_costs(double ba_spread1, double ba_spread2, double impact1, double impact2);
PnLAnalysis calculate_pnl_with_costs(double theoretical_pnl, TransactionCosts *costs, double position_size);
bool is_trade_profitable_after_costs(PairSignal *signal, TransactionCosts *costs);

// Risk management functions
RiskManager* create_risk_manager(int returns_window, double target_vol);
void destroy_risk_manager(RiskManager *manager);
double calculate_volatility_target_size(RiskManager *manager, double signal_strength, double account_size);
void update_volatility_estimate(RiskManager *manager, double trade_return);
double calculate_regime_adjusted_target_vol(RiskManager *manager, int regime);
void update_portfolio_risk(RiskManager *manager, double trade_return);

// SIMD optimized functions
double simd_rolling_mean(double *data, int size);
double simd_rolling_std(double *data, int size);
double simd_correlation(double *data1, double *data2, int size);
double simd_cb_rolling_mean(CircularBuffer *cb);
double simd_cb_rolling_std(CircularBuffer *cb);
double simd_cb_correlation(CircularBuffer *cb1, CircularBuffer *cb2);

// Alternative cointegration tests
double johansen_test(CircularBuffer *price1, CircularBuffer *price2);
double threshold_cointegration_test(CircularBuffer *spread, double threshold);

// Enhanced signal generation
PairSignal generate_enhanced_pairs_signal(PairTracker *tracker, double price1, double price2, 
                                        double bid1, double ask1, double bid2, double ask2, long timestamp_micro);

// Pair tracker management functions  
PairTracker* create_pair_tracker_with_attention(int window_size);
PairTracker* create_enhanced_pair_tracker(int window_size, bool use_all_features);
void destroy_pair_tracker(PairTracker *tracker);

#endif
