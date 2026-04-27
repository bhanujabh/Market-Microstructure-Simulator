#include <iostream>
#include "ImbalanceStrategy.h"
#include "../core/Types.h"
#include "../analytics/Metrics.h"

using namespace std;

static double getMidPrice(OrderBook &ob)
{
    if (!ob.bids.empty() && !ob.asks.empty())
    {
        int bestBid = ob.bids.begin()->first;
        int bestAsk = ob.asks.begin()->first;
        return (bestBid + bestAsk) / 2.0;
    }
    return 0;
}

void ImbalanceStrategy::onEvent(OrderBook &ob)
{
    int bidQty = 0, askQty = 0;

    for (auto &p : ob.bids)
        for (auto o : p.second)
            bidQty += o->quantity;

    for (auto &p : ob.asks)
        for (auto o : p.second)
            askQty += o->quantity;

    if (bidQty + askQty == 0)
        return;

    double imbalance = (double)bidQty / (bidQty + askQty);

    // RISK MANAGEMENT FIRST
    if (position >= maxPosition)
    {
        int id = nextId++;
        myOrders.insert(id);
        execStats[id] = {0, 0, 1, 0}; // MARKET → expected price ~ mid
        ob.addOrder({id, Side::SELL, OrderType::MARKET, 0, 1, 0});
        return;
    }
    if (position <= -maxPosition)
    {
        int id = nextId++;
        myOrders.insert(id);
        execStats[id] = {0, 0, 1, 0}; // MARKET → expected price ~ mid
        ob.addOrder({id, Side::BUY, OrderType::MARKET, 0, 1, 0});
        return;
    }

    // STRATEGY
    int id = nextId++;
    if (imbalance > 0.7)
    {
        myOrders.insert(id);
        execStats[id] = {0, 0, 1, 0}; // MARKET → expected price ~ mid
        ob.addOrder({id, Side::BUY, OrderType::MARKET, 0, 1, 0});
    }
    else if (imbalance < 0.3)
    {
        myOrders.insert(id);
        execStats[id] = {0, 0, 1, 0}; // MARKET → expected price ~ mid
        ob.addOrder({id, Side::SELL, OrderType::MARKET, 0, 1, 0});
    }
}

void ImbalanceStrategy::onTrade(const Trade &t, OrderBook &ob)
{
    if (execStats.count(t.buyId))
    {
        execStats[t.buyId].totalValue += t.price * t.quantity;
        execStats[t.buyId].filledQty += t.quantity;

        double expected = execStats[t.buyId].expectedPrice;
        // count only once per order
        if (!countedOrders.count(t.buyId))
        {
            totalTrades++;
            countedOrders.insert(t.buyId);
            if (t.price < expected)
            { // BUY → lower is better
                wins++;
            }
        }
    }
    if (execStats.count(t.sellId))
    {
        execStats[t.sellId].totalValue += t.price * t.quantity;
        execStats[t.sellId].filledQty += t.quantity;

        double expected = execStats[t.sellId].expectedPrice;
        if (!countedOrders.count(t.sellId))
        {
            totalTrades++;
            countedOrders.insert(t.sellId);
            if (t.price > expected)
            { // SELL → higher is better
                wins++;
            }
        }
    }

    if (myOrders.count(t.buyId))
    {
        position += t.quantity;
        cash -= t.price * t.quantity;
    }
    if (myOrders.count(t.sellId))
    {
        position -= t.quantity;
        cash += t.price * t.quantity;
    }

    cout << "Trade: " << t.price << " | Position: " << position << " | Cash: " << cash << endl;

    double mid = getMidPrice(ob);

    double pnl = Metrics::getPnL(
        cash,
        position,
        mid,
        inventoryPenalty);

    pnlHistory.push_back(pnl);
}

void ImbalanceStrategy::printStats(OrderBook &ob)
{
    for (auto &p : execStats)
    {
        cout << "Order " << p.first
             << " | AvgPx: "
             << Metrics::getAvgExecutionPrice(execStats, p.first)
             << " | FillRate: "
             << Metrics::getFillRate(execStats, p.first)
             << " | Slippage: "
             << Metrics::getSlippage(execStats, p.first)
             << endl;
    }

    double mid = getMidPrice(ob);

    cout << "Final Position: " << position << endl;
    cout << "Cash: " << cash << endl;

    cout << "PnL: "
         << Metrics::getPnL(
                cash,
                position,
                mid,
                inventoryPenalty)
         << endl;

    cout << "Drawdown: "
         << Metrics::getMaxDrawdown(pnlHistory)
         << endl;

    cout << "Sharpe: "
         << Metrics::getSharpe(pnlHistory)
         << endl;

    cout << "Win Rate: "
         << Metrics::getWinRate(wins, totalTrades)
         << endl;
}
