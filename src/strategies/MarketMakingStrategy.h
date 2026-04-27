#pragma once
#include "Strategy.h"

class MarketMakingStrategy : public Strategy
{
public:
    int buyOrderId = -1;
    int sellOrderId = -1;

    int nextId = 1000;
    int spread = 2;

    void onEvent(OrderBook &ob) override;
    void onTrade(const Trade &t, OrderBook &ob) override;
    void printStats(OrderBook &ob) override;
};
