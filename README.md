# Sakura Signals: Production-Ready Statistical Arbitrage System

A C library for statistical arbitrage and pairs trading

## Production-Ready Features

### Core Infrastructure
- **SIMD-Optimized Operations**: AVX2 vectorized calculations for ultra-low latency
- **Memory-Efficient Circular Buffers**: O(1) rolling window updates 
- **Real-Time Processing**: Microsecond-precision timestamping and execution

### Advanced Signal Generation
- **Transformer-Inspired Attention**: Temporal attention mechanism for enhanced signal quality
- **Dynamic Hedge Ratios**: Time-varying beta calculations using rolling regression
- **Regime Detection**: Hidden Markov Model for market state identification
- **Alternative Cointegration Tests**: Johansen, threshold, and fractional cointegration

### Risk Management & Position Sizing
- **Volatility Targeting**: Modern position sizing to achieve consistent risk exposure
- **Regime-Adaptive Sizing**: Dynamic volatility targets based on market regime
- **Portfolio Heat Monitoring**: Real-time risk exposure across multiple pairs
- **Dynamic Threshold Adjustment**: Volatility-adaptive entry/exit levels
- **Correlation-Based Heat**: Position scaling based on pair correlation clustering

### Microstructure Awareness
- **Bid-Ask Spread Integration**: Real-time spread costs in signal generation
- **Transaction Cost Modeling**: Comprehensive cost analysis including market impact
- **Execution Shortfall Calculation**: Implementation shortfall measurement
- **Liquidity Validation**: Order book depth requirements for position sizing

## Core Components

### Data Structures
- `PairTracker`: Complete trading system with regime detection, risk management, and attention
- `RegimeDetector`: HMM-based market regime classification (Normal/Stress/Crisis)
- `TransactionCosts`: Comprehensive cost modeling (spreads, impact, financing, slippage)
- `RiskManager`: Kelly Criterion, portfolio heat, and Sharpe ratio calculation
- `AttentionLayer`: Transformer-inspired temporal attention mechanism
- `PairSignal`: Enhanced signal with hedge ratios, costs, and regime information

### Production Modules
- `regime_detection.c`: Hidden Markov Model implementation for market regimes
- `dynamic_hedging.c`: Time-varying hedge ratio calculation and half-life estimation
- `transaction_costs.c`: Microstructure-aware cost modeling and execution analysis
- `risk_management.c`: Kelly Criterion, volatility scaling, and portfolio risk metrics
- `simd_optimizations.c`: AVX2/NEON vectorized operations for high-frequency trading
- `advanced_cointegration.c`: Johansen, threshold, and fractional cointegration tests
- `attention.c`: Transformer attention mechanism for enhanced signal generation

## Quick Start

### Build and Run
```bash
make
make run
```

### Alternative Build Options
```bash
make debug     # Build with debug symbols
make asan      # Build with AddressSanitizer
make analyze   # Run static analysis
```

### Clean Up
```bash
make clean
```

## Usage Examples

```c
#include "sakura_signals.h"

// Create fully-featured production tracker
PairTracker *tracker = create_enhanced_pair_tracker(50, true);

// Generate signal with microstructure data
PairSignal signal = generate_enhanced_pairs_signal(tracker, 
    price1, price2, bid1, ask1, bid2, ask2, timestamp_microseconds);

// Access comprehensive signal information
printf("Signal: %d | Hedge Ratio: %.3f | Regime: %d\n", 
       signal.signal, signal.hedge_ratio, signal.regime);
printf("Position Size: $%.0f | Net PnL: $%.2f\n", 
       signal.position_size, signal.pnl_analysis.net_pnl_after_costs);
printf("Dynamic Entry: %.2f | Transaction Costs: $%.2f\n",
       signal.dynamic_threshold_entry, signal.pnl_analysis.total_cost);
```

### High-Frequency Optimized Version
```c
// SIMD-optimized calculations for HFT environments
double correlation = simd_cb_correlation(buffer1, buffer2);
double mean = simd_cb_rolling_mean(price_buffer);
double volatility = simd_cb_rolling_std(returns_buffer);

// Volatility-targeted position sizing
double position = calculate_volatility_target_size(risk_manager, signal_strength, account_size);

// Regime-aware threshold adjustment
update_dynamic_thresholds(tracker, volatility_factor);
```

## Signal Interpretation

- **signal = 1**: Long spread (long asset1, short asset2)
- **signal = -1**: Short spread (short asset1, long asset2)  
- **signal = 0**: No position or neutral

## Attention Mechanism Features

The transformer-inspired attention mechanism enhances traditional statistical arbitrage through:

- **Temporal Attention Weighting**: Recent observations receive higher attention weights
- **Magnitude-Based Scoring**: Larger price movements get increased attention
- **Momentum Integration**: Incorporates directional momentum into z-score calculations
- **Adaptive Blending**: Combines traditional and attention-enhanced signals
- **Xavier Initialization**: Proper weight initialization for stable learning

### Attention Architecture
- Query, Key, Value weight matrices for temporal feature extraction
- Softmax-normalized attention scores for numerical stability
- Context vector computation with weighted feature aggregation
- Enhanced z-score calculation using attention-weighted statistics

## Performance 

- O(1) rolling window updates using circular buffers
- Memory-efficient data structures
- Optimized for real-time processing
- Minimal dynamic memory allocation during operation
- Attention computation optimized for 1D time series

## Additional notes

- High-performance C implementation optimized for low latency
- Real-time attention weight computation for financial time series
- Memory-efficient circular buffer integration with ML mechanisms
- Hybrid statistical/ML approach suitable for production environments

## Demo Output

The demo showcases both traditional and attention-enhanced signals:
```
=== Traditional vs Attention-Enhanced Signals ===

--- Day  65 ---
Traditional:  Pair: AAPL/MSFT | Spread: 0.051234 | Z-Score: 1.423 | Corr: 0.756 | Signal: NEUTRAL
w/ Attention: Pair: AAPL/MSFT | Spread: 0.051234 | Z-Score: 1.651 | Corr: 0.756 | Signal: NEUTRAL
Att. Z-Score: 1.651 (vs 1.423 traditional)
```
