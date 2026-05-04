#pragma once
#include "../strategies/Strategy.h"

class OrderBook;
class Trade;

class ImbalanceStrategy : public Strategy
{
public:
    int myId = 4;
    void onEvent(OrderBook &ob) override;
    void onTrade(const Trade &t, OrderBook &ob) override;
    void printStats(OrderBook &ob) override;
};