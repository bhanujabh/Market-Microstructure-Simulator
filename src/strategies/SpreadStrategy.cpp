#include <iostream>
#include "SpreadStrategy.h"
#include "../core/Types.h"

using namespace std;

void SpreadStrategy::onEvent(OrderBook &ob)
{
    if (ob.bids.empty() || ob.asks.empty())
        return;

    int bestBid = ob.bids.begin()->first;
    int bestAsk = ob.asks.begin()->first;

    // too much inventory → sell immediately to reduce risk
    // Why MARKET order? SELL, MARKET -> Because: want to exit immediately
    // limit order → might not fill but market order → guaranteed execution
    if (position >= maxPosition)
    {
        cout << "Reducing long position\n";
        int id = nextId++;
        myOrders.insert(id);
        execStats[id] = {0, 0, 1, bestBid}; // or mid
        ob.addOrder({id, Side::SELL, OrderType::MARKET, 0, 1});
        return; // stop further trading
    }
    if (position <= -maxPosition)
    {
        cout << "Reducing short position\n";
        int id = nextId++;
        myOrders.insert(id);
        execStats[id] = {0, 0, 1, bestAsk}; // or mid
        ob.addOrder({id, Side::BUY, OrderType::MARKET, 0, 1});
        return;
    }

    if (bestAsk - bestBid < threshold)
        return;
    cout << "Strategy triggered\n";

    // RISK CHECKS
    bool allowBuy = (position < maxPosition);
    bool allowSell = (position > -maxPosition);

    // cancel previous orders to avoid spam
    if (buyOrderId != -1)
        ob.cancelOrder(buyOrderId);
    if (sellOrderId != -1)
        ob.cancelOrder(sellOrderId);

    // place only if allowed
    if (allowBuy)
    {
        buyOrderId = nextId++;
        myOrders.insert(buyOrderId);
        execStats[buyOrderId] = {0, 0, 1, bestBid}; // expected price
        ob.addOrder({buyOrderId, Side::BUY, OrderType::LIMIT, bestBid, 1});
    }

    if (allowSell)
    {
        sellOrderId = nextId++;
        myOrders.insert(sellOrderId);
        execStats[sellOrderId] = {0, 0, 1, bestAsk}; // expected price
        ob.addOrder({sellOrderId, Side::SELL, OrderType::LIMIT, bestAsk, 1});
    }
}

void SpreadStrategy::onTrade(const Trade &t, OrderBook &ob)
{
    // Execution stats
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

    // Position + cash (YOU MISSED THIS)
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

double SpreadStrategy::getPnL(OrderBook &ob)
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
double SpreadStrategy::getAvgExecutionPrice(int orderId)
{
    auto &s = execStats[orderId];
    if (s.filledQty == 0)
        return 0;
    return s.totalValue / s.filledQty;
}
double SpreadStrategy::getFillRate(int orderId)
{
    auto &s = execStats[orderId];
    return (double)s.filledQty / s.intendedQty;
}
double SpreadStrategy::getSlippage(int orderId)
{
    return getAvgExecutionPrice(orderId) - execStats[orderId].expectedPrice;
}
double SpreadStrategy::getSharpe()
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
double SpreadStrategy::getWinRate()
{
    if (totalTrades == 0)
        return 0;
    return (double)wins / totalTrades;
}
void SpreadStrategy::printStats(OrderBook &ob)
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