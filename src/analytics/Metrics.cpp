#include <vector>
#include <cmath>
#include <unordered_map>
#include "Metrics.h"

using namespace std;

double Metrics::getPnL(double cash,
                       int position,
                       double mid,
                       double inventoryPenalty)
{

    // if (!ob.bids.empty() && !ob.asks.empty())
    // {
    //     int bestBid = ob.bids.begin()->first;
    //     int bestAsk = ob.asks.begin()->first;
    //     mid = (bestBid + bestAsk) / 2.0;
    // }
    return cash + position * mid - inventoryPenalty * abs(position);
}

double Metrics::getAvgExecutionPrice(const std::unordered_map<int, ExecutionStats> &execStats,
                                     int orderId)
{
    auto it = execStats.find(orderId);

    if (it == execStats.end())
        return 0;

    const auto &s = it->second;

    if (s.filledQty == 0)
        return 0;

    return s.totalValue / s.filledQty;
}

double Metrics::getFillRate(const std::unordered_map<int, ExecutionStats> &execStats,
                            int orderId)
{
    auto it = execStats.find(orderId);

    if (it == execStats.end())
        return 0;

    const auto &s = it->second;

    if (s.intendedQty == 0)
        return 0;

    return (double)s.filledQty / s.intendedQty;
}

double Metrics::getSlippage(const std::unordered_map<int, ExecutionStats> &execStats,
                            int orderId)
{
    return getAvgExecutionPrice(execStats, orderId) - execStats.at(orderId).expectedPrice;
}

double Metrics::getSharpe(const std::vector<double> &pnlHistory)
{
    if (pnlHistory.size() < 2)
        return 0;

    vector<double> returns;

    for (int i = 1; i < pnlHistory.size(); i++)
        returns.push_back(
            pnlHistory[i] - pnlHistory[i - 1]);

    double mean = 0;

    for (double r : returns)
        mean += r;

    mean /= returns.size();

    double var = 0;

    for (double r : returns)
        var += (r - mean) * (r - mean);

    var /= returns.size();

    double stddev = sqrt(var);

    if (stddev == 0)
        return 0;

    return mean / stddev;
}

double Metrics::getWinRate(int wins,
                           int totalTrades)
{
    if (totalTrades == 0)
        return 0;

    return (double)wins / totalTrades;
}

double Metrics::getMaxDrawdown(
    const std::vector<double> &pnlHistory)
{
    if (pnlHistory.empty())
        return 0;

    double peak = pnlHistory[0];
    double maxDrawdown = 0;

    for (double pnl : pnlHistory)
    {
        if (pnl > peak)
            peak = pnl;

        double drawdown = peak - pnl;

        if (drawdown > maxDrawdown)
            maxDrawdown = drawdown;
    }

    return maxDrawdown;
}