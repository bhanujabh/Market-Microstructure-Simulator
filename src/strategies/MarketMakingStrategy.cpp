#include <iostream>
#include "MarketMakingStrategy.h"
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

void MarketMakingStrategy::onEvent(OrderBook &ob)
{
    if (ob.bids.empty() || ob.asks.empty())
        return;

    int bestBid = ob.bids.begin()->first;
    int bestAsk = ob.asks.begin()->first;

    int mid = (bestBid + bestAsk) / 2;

    int buyPrice = mid - spread;
    int sellPrice = mid + spread;

    // RISK FIRST
    if (position >= maxPosition)
    {
        int id = nextId++;
        myOrders.insert(id);

        // for research level analysis
        // double mid = (bestBid + bestAsk) / 2.0;
        // neutral baseline
        // double expectedPrice = mid;

        // bestBid / bestAsk => execution slippage
        // mid => market impact slippage
        double expectedPrice = bestBid; // SELL → hits bid
        execStats[id] = {0, 0, 1, bestBid};
        ob.addOrder({id, Side::SELL, OrderType::MARKET, 0, 1});
        return;
    }
    if (position <= -maxPosition)
    {
        int id = nextId++;
        execStats[id] = {0, 0, 1, bestAsk};
        ob.addOrder({id, Side::BUY, OrderType::MARKET, 0, 1});
        return;
    }

    // cancel old orders
    if (buyOrderId != -1)
        ob.cancelOrder(buyOrderId);
    if (sellOrderId != -1)
        ob.cancelOrder(sellOrderId);

    // place new ones
    buyOrderId = nextId++;
    sellOrderId = nextId++;

    myOrders.insert(buyOrderId);
    myOrders.insert(sellOrderId);

    cout << "Market Making...\n";

    execStats[buyOrderId] = {0, 0, 1, buyPrice};
    ob.addOrder({buyOrderId, Side::BUY, OrderType::LIMIT, buyPrice, 1});
    execStats[sellOrderId] = {0, 0, 1, sellPrice};
    ob.addOrder({sellOrderId, Side::SELL, OrderType::LIMIT, sellPrice, 1});
}

void MarketMakingStrategy::onTrade(const Trade &t, OrderBook &ob)
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

void MarketMakingStrategy::printStats(OrderBook &ob)
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