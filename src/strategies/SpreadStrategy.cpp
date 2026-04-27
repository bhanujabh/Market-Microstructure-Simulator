#include <iostream>
#include "SpreadStrategy.h"
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

    double mid = getMidPrice(ob);

    double pnl = Metrics::getPnL(
        cash,
        position,
        mid,
        inventoryPenalty);

    pnlHistory.push_back(pnl);
}

void SpreadStrategy::printStats(OrderBook &ob)
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