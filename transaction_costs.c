#include "sakura_signals.h"
#include <sys/time.h>

TransactionCosts create_transaction_costs(double ba_spread1, double ba_spread2, double impact1, double impact2) {
    TransactionCosts costs = {0};
    
    costs.bid_ask_spread_asset1 = ba_spread1;
    costs.bid_ask_spread_asset2 = ba_spread2;
    costs.market_impact_asset1 = impact1;
    costs.market_impact_asset2 = impact2;
    costs.financing_cost = 0.001; // 10 bps daily financing
    costs.commission_rate = 0.0005; // 5 bps commission
    costs.slippage_factor = 0.0002; // 2 bps slippage
    
    return costs;
}

PnLAnalysis calculate_pnl_with_costs(double theoretical_pnl, TransactionCosts *costs, double position_size) {
    PnLAnalysis analysis = {0};
    
    analysis.theoretical_pnl = theoretical_pnl;
    
    // calc bid-ask spread cost
    double spread_cost_asset1 = costs->bid_ask_spread_asset1 * position_size;
    double spread_cost_asset2 = costs->bid_ask_spread_asset2 * position_size;
    analysis.spread_cost = spread_cost_asset1 + spread_cost_asset2;
    
    // calc market impact cost (square root law)
    double impact_cost_asset1 = costs->market_impact_asset1 * sqrt(position_size);
    double impact_cost_asset2 = costs->market_impact_asset2 * sqrt(position_size);
    analysis.market_impact_cost = impact_cost_asset1 + impact_cost_asset2;
    
    // calc financing cost (for holding overnight)
    analysis.financing_cost = costs->financing_cost * position_size;
    
    // calc commission and slippage
    double commission = costs->commission_rate * position_size * 2; // round trip
    double slippage = costs->slippage_factor * position_size * 2;   // round trip
    
    // total cost
    analysis.total_cost = analysis.spread_cost + analysis.market_impact_cost + 
                         analysis.financing_cost + commission + slippage;
    
    // net pnl after costs
    analysis.net_pnl_after_costs = theoretical_pnl - analysis.total_cost;
    analysis.is_profitable = analysis.net_pnl_after_costs > 0;
    
    return analysis;
}

bool is_trade_profitable_after_costs(PairSignal *signal, TransactionCosts *costs) {
    if (!signal || !costs) return false;
    
    // estimate theoretical pnl based on z-score reversion
    double expected_reversion = fabs(signal->z_score) * 0.5; // conservative estimate
    double theoretical_pnl = expected_reversion * signal->position_size;
    
    PnLAnalysis analysis = calculate_pnl_with_costs(theoretical_pnl, costs, signal->position_size);
    
    return analysis.is_profitable;
}

// microstructure-aware functions
double get_effective_spread(double bid, double ask, double size) {
    double quoted_spread = ask - bid;
    
    // adjust for size (larger orders get worse prices)
    double size_impact = sqrt(size) * 0.0001; // empirical factor
    
    return quoted_spread + size_impact;
}

bool is_liquidity_sufficient(double bid_size, double ask_size, double required_size) {
    // require at least 2x the position size in top of book
    return (bid_size >= required_size * 2.0) && (ask_size >= required_size * 2.0);
}

long get_microsecond_timestamp() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000L + tv.tv_usec;
}

double calculate_execution_shortfall(double arrival_price, double execution_price, double size) {
    return fabs(execution_price - arrival_price) * size;
}