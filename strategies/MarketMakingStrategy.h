#pragma once
#include <unordered_map>
#include <unordered_set>
#include "Strategy.h"

class MarketMakingStrategy : public Strategy {
public:
    int buyOrderId = -1;
    int sellOrderId = -1;

    int nextId = 1000;
    int spread = 2;

    // tracking inventory
    int position = 0;
    int maxPosition = 5;

    std::unordered_set<int> myOrders;

    double cash = 0;     // money earned/spent

    std::unordered_map<int, ExecutionStats> execStats;

    double inventoryPenalty = 0.1;

    int wins = 0;
    int totalTrades = 0;

    std::unordered_set<int> countedOrders;
    
    void onEvent(OrderBook &ob) override;
    void onTrade(const Trade &t, OrderBook &ob) override;
    void printStats(OrderBook &ob) override;

    double getPnL(OrderBook &ob);
    double getAvgExecutionPrice(int orderId);
    double getFillRate(int orderId);
    double getSlippage(int orderId);
    double getSharpe();
    double getWinRate();

};
