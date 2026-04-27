#include <iostream>
#include "MomentumStrategy.h"
#include "../core/Types.h"
#include "../analytics/Metrics.h"

using namespace std;

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
    {
        pnlHistory.push_back(currentPnL(ob));
        return;
    }
    int n = prices.size();

    // RISK FIRST
    if (!risk.allowBuy(position))
    {
        cout << "Reducing long\n";
        int id = nextId++;
        myOrders.insert(id);
        execStats[id] = {0, 0, 1, t.price}; // expected price
        ob.addOrder({id, Side::SELL, OrderType::MARKET, 0, 1, 0});
        pnlHistory.push_back(currentPnL(ob));
        return;
    }
    if (!risk.allowSell(position))
    {
        cout << "Reducing short\n";

        int id = nextId++;
        myOrders.insert(id);
        execStats[id] = {0, 0, 1, t.price};

        ob.addOrder({id,
                     Side::BUY,
                     OrderType::MARKET,
                     0,
                     1,
                     0});

        pnlHistory.push_back(currentPnL(ob));
        return;
    }

    // upward momentum
    if (prices[n - 1] > prices[n - 2] && prices[n - 2] > prices[n - 3])
    {
        cout << "Momentum BUY signal\n";
        if (risk.allowBuy(position))
        {
            int id = nextId++;
            myOrders.insert(id);
            execStats[id] = {0, 0, 1, t.price}; // expected price
            ob.addOrder({id, Side::BUY, OrderType::MARKET, 0, 1, 0});
        }
    }

    // downward momentum
    else if (prices[n - 1] < prices[n - 2] && prices[n - 2] < prices[n - 3])
    {
        cout << "Momentum SELL signal\n";
        if (risk.allowSell(position))
        {
            int id = nextId++;
            myOrders.insert(id);
            ob.addOrder({id, Side::SELL, OrderType::MARKET, 0, 1});
        }
    }
    pnlHistory.push_back(currentPnL(ob));
}

void MomentumStrategy::onEvent(OrderBook &ob)
{
    // Kill switch on every event
    if (risk.killSwitch(currentPnL(ob)))
    {
        cout << "Risk Kill Switch Triggered\n";
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
         << currentPnL(ob)
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
