#pragma once
#include "Strategy.h"

class ImbalanceStrategy : public Strategy
{
public:
    int nextId = 2000;

    void onEvent(OrderBook &ob) override;
    void onTrade(const Trade &t, OrderBook &ob) override;
    void printStats(OrderBook &ob) override;
};