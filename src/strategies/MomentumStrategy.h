#pragma once
#include "../strategies/Strategy.h"

class OrderBook;
class Trade;

class MomentumStrategy : public Strategy
{
public:
    std::vector<int> prices;
    int myId = 2;

    void onEvent(OrderBook &ob) override;
    void onTrade(const Trade &t, OrderBook &ob) override;
    void printStats(OrderBook &ob) override;
};