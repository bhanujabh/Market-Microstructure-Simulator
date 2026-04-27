#pragma once
#include "Strategy.h"

class MomentumStrategy : public Strategy
{
public:
    std::vector<int> prices;
    int nextId = 3000;

    void onEvent(OrderBook &ob) override;
    void onTrade(const Trade &t, OrderBook &ob) override;
    void printStats(OrderBook &ob) override;
};