#pragma once
#include "../strategies/Strategy.h"

class OrderBook;
class Trade;

class MarketMakingStrategy : public Strategy
{
public:
    int buyOrderId = -1;
    int sellOrderId = -1;

    double lastBuyPrice = -1;
    double lastSellPrice = -1;

    int spread = 2;
    int myId = 3;

    void onEvent(OrderBook &ob) override;
    void onTrade(const Trade &t, OrderBook &ob) override;
    void printStats(OrderBook &ob) override;
};
