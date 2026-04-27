#include <iostream>
#include "MarketMakingStrategy.h"
#include "../core/Types.h"
#include "../analytics/Metrics.h"

using namespace std;

void MarketMakingStrategy::onEvent(OrderBook &ob)
{
    if (ob.bids.empty() || ob.asks.empty())
        return;

    int bestBid = ob.bids.begin()->first;
    int bestAsk = ob.asks.begin()->first;

    int mid = (bestBid + bestAsk) / 2;

    int buyPrice = mid - spread;
    int sellPrice = mid + spread;

    if (risk.killSwitch(currentPnL(ob)))
    {
        cout << "Risk Kill Switch Triggered\n";
        return;
    }

    // RISK FIRST
    // ===== CHANGED: use RiskManager instead of maxPosition =====
    if (!risk.allowBuy(position))
    {
        int id = nextId++;
        myOrders.insert(id);

        execStats[id] = {0, 0, 1, bestBid};

        // ===== CHANGED: added 6th timestamp field =====
        ob.addOrder({id, Side::SELL, OrderType::MARKET, 0, 1, 0});
        return;
    }

    // ===== CHANGED: use RiskManager instead of maxPosition =====
    if (!risk.allowSell(position))
    {
        int id = nextId++;
        myOrders.insert(id); // ===== CHANGED: inserted missing myOrders =====

        execStats[id] = {0, 0, 1, bestAsk};

        // ===== CHANGED: added 6th timestamp field =====
        ob.addOrder({id, Side::BUY, OrderType::MARKET, 0, 1, 0});
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
    ob.addOrder({buyOrderId, Side::BUY, OrderType::LIMIT, buyPrice, 1, 0});
    execStats[sellOrderId] = {0, 0, 1, sellPrice};
    ob.addOrder({sellOrderId, Side::SELL, OrderType::LIMIT, sellPrice, 1, 0});
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

    pnlHistory.push_back(currentPnL(ob));
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