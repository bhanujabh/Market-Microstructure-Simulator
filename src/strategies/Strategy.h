#pragma once
#include <vector>
#include "../engine/OrderBook.h"

class Strategy
{
public:
    virtual void onEvent(OrderBook &ob) = 0;
    virtual void onTrade(const Trade &t, OrderBook &ob) = 0;
    virtual void printStats(OrderBook &ob) = 0;
    std::vector<double> pnlHistory;
    double maxPnL = -1e9;
    double maxDrawdown = 0;
    virtual ~Strategy() = default;
};
