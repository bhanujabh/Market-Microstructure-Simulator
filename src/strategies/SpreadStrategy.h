#pragma once
#include "Strategy.h"

class SpreadStrategy : public Strategy
{
public:
    int threshold = 2;

    int buyOrderId = -1;
    int sellOrderId = -1;

    int nextId = 1000;

    void onEvent(OrderBook &ob) override;
    void onTrade(const Trade &t, OrderBook &ob) override;
    void printStats(OrderBook &ob) override;
};