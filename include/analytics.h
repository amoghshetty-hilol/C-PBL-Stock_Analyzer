#ifndef ANALYTICS_H
#define ANALYTICS_H

#include "common.h"
#include <stddef.h>

double calculate_sma(const Stock *stock, int target_index, int window);
void generate_trading_signal(const Stock *stock, char *output_signal, size_t buf_size);
double calculate_rsi(const Stock *stock, int target_index, int period);
void generate_enhanced_signal(const Stock *stock, char *output_signal, size_t buf_size);
void rank_market_performers(Market *market);
void export_analytical_report(const Stock *stock);
void render_native_windows_graph(const Stock *stock, int datapoints);

#endif /* ANALYTICS_H */
