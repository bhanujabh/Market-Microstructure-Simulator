#pragma once
#include<vector>
#include <unordered_map>
#include <unordered_set>
#include "Strategy.h"

class MomentumStrategy : public Strategy
{
    public:
    std::vector<int> prices;

    int position = 0;
    int maxPosition = 5;
    int nextId = 3000;

    std::unordered_set<int> myOrders;

    double cash = 0;     // money earned/spent

    std::unordered_map<int, ExecutionStats> execStats;

    double inventoryPenalty = 0.1;

    int wins = 0;
    int totalTrades = 0;

    std::unordered_set<int> countedOrders;

    void onTrade(const Trade& t, OrderBook &ob) override;
    void onEvent(OrderBook &ob) override;
    void printStats(OrderBook &ob) override;

    double getPnL(OrderBook &ob);
    double getAvgExecutionPrice(int orderId);
    double getFillRate(int orderId);
    double getSlippage(int orderId);
    double getSharpe();
    double getWinRate();
};
