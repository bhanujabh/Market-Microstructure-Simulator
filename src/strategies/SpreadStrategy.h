#pragma once
#include "../strategies/Strategy.h"

class OrderBook;
class Trade;

class SpreadStrategy : public Strategy
{
public:
    int threshold = 2;

    int buyOrderId = -1;
    int sellOrderId = -1;

    int myId = 1;

    void onEvent(OrderBook &ob) override;
    void onTrade(const Trade &t, OrderBook &ob) override;
    void printStats(OrderBook &ob) override;
};