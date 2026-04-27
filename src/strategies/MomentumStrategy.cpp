#include <iostream>
#include "MomentumStrategy.h"
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

void MomentumStrategy::onTrade(const Trade &t, OrderBook &ob)
{
    prices.push_back(t.price);

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
        cash -= t.price * t.quantity; // you paid
    }
    if (myOrders.count(t.sellId))
    {
        position -= t.quantity;
        cash += t.price * t.quantity; // you earned
    }

    if (prices.size() < 3)
        return;

    int n = prices.size();

    // RISK FIRST
    if (position >= maxPosition)
    {
        cout << "Reducing long\n";
        int id = nextId++;
        myOrders.insert(id);
        execStats[id] = {0, 0, 1, t.price}; // expected price
        ob.addOrder({id, Side::SELL, OrderType::MARKET, 0, 1});
        return;
    }
    if (position <= -maxPosition)
    {
        cout << "Reducing short\n";
        return;
    }

    // upward momentum
    if (prices[n - 1] > prices[n - 2] && prices[n - 2] > prices[n - 3])
    {
        cout << "Momentum BUY signal\n";
        int id = nextId++;
        myOrders.insert(id);
        execStats[id] = {0, 0, 1, t.price}; // expected price
        ob.addOrder({id, Side::BUY, OrderType::MARKET, 0, 1});
    }

    // downward momentum
    else if (prices[n - 1] < prices[n - 2] && prices[n - 2] < prices[n - 3])
    {
        cout << "Momentum SELL signal\n";
        int id = nextId++;
        myOrders.insert(id);
        ob.addOrder({id, Side::SELL, OrderType::MARKET, 0, 1});
    }

    double mid = getMidPrice(ob);

    double pnl = Metrics::getPnL(
        cash,
        position,
        mid,
        inventoryPenalty);

    pnlHistory.push_back(pnl);
}

void MomentumStrategy::onEvent(OrderBook &ob)
{
    if (position > maxPosition)
    {
        int reduceQty = position - maxPosition;
        int id = nextId++;
        myOrders.insert(id);
        ob.addOrder({id, Side::SELL, OrderType::MARKET, 0, reduceQty});
        return;
    }

    if (position < -maxPosition)
    {
        int reduceQty = -maxPosition - position;
        int id = nextId++;
        myOrders.insert(id);
        ob.addOrder({id, Side::BUY, OrderType::MARKET, 0, reduceQty});
        return;
    }
}

void MomentumStrategy::printStats(OrderBook &ob)
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
