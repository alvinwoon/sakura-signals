#include "sakura_signals.h"

RiskManager* create_risk_manager(int returns_window, double target_vol) {
    RiskManager *manager = malloc(sizeof(RiskManager));
    if (!manager) return NULL;
    
    manager->returns_buffer = create_circular_buffer(returns_window);
    manager->volatility_buffer = create_circular_buffer(returns_window);
    
    if (!manager->returns_buffer || !manager->volatility_buffer) {
        destroy_circular_buffer(manager->returns_buffer);
        destroy_circular_buffer(manager->volatility_buffer);
        free(manager);
        return NULL;
    }
    
    // init volatility targeting params
    manager->target_volatility = target_vol; // target annual vol (e.g., 0.15 = 15%)
    manager->current_volatility = target_vol; // init to target
    manager->volatility_scalar = 1.0;
    manager->position_size = 0.0;
    manager->max_position_limit = 1000000.0; // $1M default
    manager->portfolio_heat = 0.0;
    manager->risk_per_trade = 0.02; // 2% of capital per trade
    manager->sharpe_ratio = 0.0;
    manager->max_drawdown = 0.0;
    manager->volatility_window = returns_window;
    
    return manager;
}

void destroy_risk_manager(RiskManager *manager) {
    if (manager) {
        destroy_circular_buffer(manager->returns_buffer);
        destroy_circular_buffer(manager->volatility_buffer);
        free(manager);
    }
}

double calculate_volatility_target_size(RiskManager *manager, double signal_strength, double account_size) {
    if (!manager || account_size <= 0) return 0.0;
    
    // volatility targeting: target_vol / current_vol * base_position
    double base_position = account_size * manager->risk_per_trade;
    
    // calc volatility scalar
    if (manager->current_volatility > 0) {
        manager->volatility_scalar = manager->target_volatility / manager->current_volatility;
    } else {
        manager->volatility_scalar = 1.0;
    }
    
    // apply volatility targeting
    double vol_targeted_size = base_position * manager->volatility_scalar;
    
    // scale by signal strength (z-score confidence)
    double strength_factor = fabs(signal_strength) / 3.0; // normalize around 3-sigma
    if (strength_factor > 1.0) strength_factor = 1.0;
    if (strength_factor < 0.1) strength_factor = 0.1;
    
    double sized_position = vol_targeted_size * strength_factor;
    
    // apply position limits
    if (sized_position > manager->max_position_limit) {
        sized_position = manager->max_position_limit;
    }
    
    // apply portfolio heat limit (total exposure across all pairs)
    if (manager->portfolio_heat > 0.5) { // if > 50% heat, reduce size
        sized_position *= (1.0 - manager->portfolio_heat);
    }
    
    // clamp volatility scalar to reasonable bounds (0.1x to 5x)
    if (manager->volatility_scalar > 5.0) {
        sized_position = base_position * 5.0 * strength_factor;
    } else if (manager->volatility_scalar < 0.1) {
        sized_position = base_position * 0.1 * strength_factor;
    }
    
    manager->position_size = sized_position;
    return sized_position;
}

void update_volatility_estimate(RiskManager *manager, double trade_return) {
    if (!manager) return;
    
    cb_push(manager->returns_buffer, trade_return);
    
    // calc squared return for volatility estimation
    double squared_return = trade_return * trade_return;
    cb_push(manager->volatility_buffer, squared_return);
    
    int size = cb_size(manager->volatility_buffer);
    if (size < 5) return;
    
    // calc realized volatility using squared returns
    double variance = rolling_mean(manager->volatility_buffer);
    manager->current_volatility = sqrt(variance * 252); // annualized vol
    
    // apply exponential decay for more responsive estimates
    static double prev_vol = 0.0;
    if (prev_vol > 0) {
        double decay_factor = 0.94; // daily decay
        manager->current_volatility = decay_factor * prev_vol + (1 - decay_factor) * manager->current_volatility;
    }
    prev_vol = manager->current_volatility;
}

double calculate_regime_adjusted_target_vol(RiskManager *manager, int regime) {
    if (!manager) return 0.15; // default 15% target vol
    
    double base_target = manager->target_volatility;
    
    switch (regime) {
        case 0: // normal regime
            return base_target;
        case 1: // stress regime - reduce target vol
            return base_target * 0.75; // 25% reduction
        case 2: // crisis regime - significantly reduce target vol
            return base_target * 0.5;  // 50% reduction
        default:
            return base_target;
    }
}

void update_portfolio_risk(RiskManager *manager, double trade_return) {
    if (!manager) return;
    
    cb_push(manager->returns_buffer, trade_return);
    
    int size = cb_size(manager->returns_buffer);
    if (size < 10) return;
    
    // calc sharpe ratio
    double mean_return = rolling_mean(manager->returns_buffer);
    double std_return = rolling_std(manager->returns_buffer);
    
    if (std_return > 0) {
        manager->sharpe_ratio = mean_return / std_return * sqrt(252); // annualized
    }
    
    // calc max drawdown
    double peak = -99999.0;
    double max_dd = 0.0;
    double cumulative = 0.0;
    
    for (int i = 0; i < size; i++) {
        cumulative += cb_get(manager->returns_buffer, i);
        if (cumulative > peak) {
            peak = cumulative;
        }
        double drawdown = peak - cumulative;
        if (drawdown > max_dd) {
            max_dd = drawdown;
        }
    }
    manager->max_drawdown = max_dd;
    
    // calc portfolio heat (current risk exposure)
    double recent_vol = rolling_std(manager->returns_buffer);
    manager->portfolio_heat = recent_vol * 10.0; // scale factor
    if (manager->portfolio_heat > 1.0) manager->portfolio_heat = 1.0;
}

// volatility-based position scaling
double calculate_volatility_adjusted_size(RiskManager *manager, double base_size, double current_vol, double target_vol) {
    if (current_vol <= 0 || target_vol <= 0) return base_size;
    
    double vol_ratio = target_vol / current_vol;
    double adjusted_size = base_size * vol_ratio;
    
    // clamp adjustment to prevent extreme sizes
    if (vol_ratio > 3.0) adjusted_size = base_size * 3.0;
    if (vol_ratio < 0.3) adjusted_size = base_size * 0.3;
    
    return adjusted_size;
}

// correlation-based heat adjustment
double calculate_correlation_heat(CorrelationMatrix *cm, int n_active_pairs) {
    if (!cm || n_active_pairs <= 1) return 0.0;
    
    double total_correlation = 0.0;
    int pair_count = 0;
    
    // sum all off-diagonal correlations
    for (int i = 0; i < n_active_pairs; i++) {
        for (int j = i + 1; j < n_active_pairs; j++) {
            total_correlation += fabs(cm->matrix[i][j]);
            pair_count++;
        }
    }
    
    if (pair_count == 0) return 0.0;
    
    double avg_correlation = total_correlation / pair_count;
    
    // higher correlation = higher heat
    return avg_correlation;
}