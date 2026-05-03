#pragma once
#include "Strategy.h"

class ImbalanceStrategy : public Strategy
{
public:
    void onEvent(OrderBook &ob) override;
    void onTrade(const Trade &t, OrderBook &ob) override;
    void printStats(OrderBook &ob) override;
};