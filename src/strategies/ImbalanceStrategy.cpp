#include <iostream>
#include "ImbalanceStrategy.h"
#include "../core/Types.h"

using namespace std;

void ImbalanceStrategy::onEvent(OrderBook &ob)
{
    int bidQty = 0, askQty = 0;

    for (auto &p : ob.bids)
        for (auto o : p.second)
            bidQty += o->quantity;

    for (auto &p : ob.asks)
        for (auto o : p.second)
            askQty += o->quantity;

    double imbalance = (double)bidQty / (bidQty + askQty);

    // RISK MANAGEMENT FIRST
    if (position >= maxPosition)
    {
        int id = nextId++;
        myOrders.insert(id);
        execStats[id] = {0, 0, 1, 0}; // MARKET → expected price ~ mid
        ob.addOrder({id, Side::SELL, OrderType::MARKET, 0, 1});
        return;
    }
    if (position <= -maxPosition)
    {
        int id = nextId++;
        myOrders.insert(id);
        execStats[id] = {0, 0, 1, 0}; // MARKET → expected price ~ mid
        ob.addOrder({id, Side::BUY, OrderType::MARKET, 0, 1});
        return;
    }

    // STRATEGY
    int id = nextId++;
    if (imbalance > 0.7)
    {
        myOrders.insert(id);
        execStats[id] = {0, 0, 1, 0}; // MARKET → expected price ~ mid
        ob.addOrder({id, Side::BUY, OrderType::MARKET, 0, 1});
    }
    else if (imbalance < 0.3)
    {
        myOrders.insert(id);
        execStats[id] = {0, 0, 1, 0}; // MARKET → expected price ~ mid
        ob.addOrder({id, Side::SELL, OrderType::MARKET, 0, 1});
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

    double pnl = getPnL(ob);
    pnlHistory.push_back(pnl);
    if (pnl > maxPnL)
        maxPnL = pnl;
    maxDrawdown = max(maxDrawdown, maxPnL - pnl);
}

double ImbalanceStrategy::getPnL(OrderBook &ob)
{
    double mid = 0;

    if (!ob.bids.empty() && !ob.asks.empty())
    {
        int bestBid = ob.bids.begin()->first;
        int bestAsk = ob.asks.begin()->first;
        mid = (bestBid + bestAsk) / 2.0;
    }
    return cash + position * mid - inventoryPenalty * abs(position);
}

double ImbalanceStrategy::getAvgExecutionPrice(int orderId)
{
    auto &s = execStats[orderId];
    if (s.filledQty == 0)
        return 0;
    return s.totalValue / s.filledQty;
}

double ImbalanceStrategy::getFillRate(int orderId)
{
    auto &s = execStats[orderId];
    return (double)s.filledQty / s.intendedQty;
}

double ImbalanceStrategy::getSlippage(int orderId)
{
    return getAvgExecutionPrice(orderId) - execStats[orderId].expectedPrice;
}

double ImbalanceStrategy::getSharpe()
{
    if (pnlHistory.size() < 2)
        return 0;
    vector<double> returns;
    for (int i = 1; i < pnlHistory.size(); i++)
    {
        returns.push_back(pnlHistory[i] - pnlHistory[i - 1]);
    }
    double mean = 0;
    for (double r : returns)
        mean += r;
    mean /= returns.size();
    double var = 0;
    for (double r : returns)
        var += (r - mean) * (r - mean);
    var /= returns.size();
    double stddev = sqrt(var);
    if (stddev == 0)
        return 0;
    return mean / stddev;
}

double ImbalanceStrategy::getWinRate()
{
    if (totalTrades == 0)
        return 0;
    return (double)wins / totalTrades;
}

void ImbalanceStrategy::printStats(OrderBook &ob)
{
    for (auto &p : execStats)
    {
        cout << "Order " << p.first
             << " | AvgPx: " << getAvgExecutionPrice(p.first)
             << " | FillRate: " << getFillRate(p.first)
             << " | Slippage: " << getSlippage(p.first)
             << endl;
    }
    cout << "Final Position: " << position << endl;
    cout << "Cash: " << cash << endl;
    cout << "PnL: " << getPnL(ob) << endl;
    cout << "Drawdown: " << maxDrawdown << endl;
    cout << "Sharpe: " << getSharpe() << endl;
    cout << "Win Rate: " << getWinRate() << endl;
}
