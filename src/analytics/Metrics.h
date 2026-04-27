#pragma once
#include <vector>
#include <unordered_map>
#include "../analytics/ExecutionStats.h"

class Metrics
{
public:
    static double getPnL(
        double cash,
        int position,
        double mid,
        double inventoryPenalty);

    static double getAvgExecutionPrice(
        const std::unordered_map<int, ExecutionStats> &execStats,
        int orderId);

    static double getFillRate(
        const std::unordered_map<int, ExecutionStats> &execStats,
        int orderId);

    static double getSlippage(
        const std::unordered_map<int, ExecutionStats> &execStats,
        int orderId);

    static double getSharpe(
        const std::vector<double> &pnlHistory);

    static double getWinRate(
        int wins,
        int totalTrades);

    static double getMaxDrawdown(
        const std::vector<double> &pnlHistory);
};