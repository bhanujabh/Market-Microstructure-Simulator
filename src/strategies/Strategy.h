#pragma once
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "../engine/OrderBook.h"
#include "../analytics/ExecutionStats.h"
#include "../analytics/RiskManager.h"
#include "../analytics/Metrics.h"

class Strategy
{
public:
    virtual ~Strategy() = default;

    virtual void onEvent(OrderBook &ob) = 0;
    virtual void onTrade(const Trade &t, OrderBook &ob) = 0;
    virtual void printStats(OrderBook &ob) = 0;

protected:
    // Shared trading state
    int position = 0; // current inventory
    double cash = 0.0;

    std::unordered_map<int, ExecutionStats> execStats;
    std::unordered_set<int> countedOrders;
    std::unordered_set<int> myOrders;

    int wins = 0;
    int totalTrades = 0;

    double inventoryPenalty = 0.1;

    std::vector<double> pnlHistory;

    RiskManager risk;

    // Shared helpers
    double getMidPrice(OrderBook &ob)
    {
        if (!ob.bids.empty() && !ob.asks.empty())
        {
            int bestBid = ob.bids.begin()->first;
            int bestAsk = ob.asks.begin()->first;
            return (bestBid + bestAsk) / 2.0;
        }
        return 0.0;
    }

    double currentPnL(OrderBook &ob)
    {
        return Metrics::getPnL(
            cash,
            position,
            getMidPrice(ob),
            inventoryPenalty);
    }
};
