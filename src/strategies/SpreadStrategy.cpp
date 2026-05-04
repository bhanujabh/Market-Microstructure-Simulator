#include <iostream>
#include "SpreadStrategy.h"
#include "../core/Types.h"
#include "../analytics/Metrics.h"
#include "../engine/OrderBook.h"
#include "../core/Trade.h"

using namespace std;

void SpreadStrategy::onEvent(OrderBook &ob)
{
    if (ob.bids.empty() || ob.asks.empty())
        return;

    double bestBid = ob.bids.begin()->first;
    double bestAsk = ob.asks.begin()->first;
    double mid = getMidPrice(ob);

    // Kill switch
    if (risk.killSwitch(currentPnL(ob)))
    {
        cout << "Risk Kill Switch Triggered\n";
        return;
    }

    int baseQty = 1;

    // too much inventory → sell immediately to reduce risk
    // Why MARKET order? SELL, MARKET -> Because: want to exit immediately
    // limit order → might not fill but market order → guaranteed execution
    if (!risk.allowBuy(position, baseQty))
    {
        cout << "Reducing long position\n";
        int id = ob.generateOrderId();
        int reduceQty = abs(position);
        execStats[id] = {0.0, 0, baseQty, mid, Side::SELL}; // or mid
        ob.addOrder({id, Side::SELL, OrderType::MARKET, 0, reduceQty, 0, myId});
        return; // stop further trading
    }
    // Reduce short inventory
    if (!risk.allowSell(position, baseQty))
    {
        cout << "Reducing short position\n";

        int id = ob.generateOrderId();
        int reduceQty = abs(position);
        execStats[id] = {0.0, 0, baseQty, mid, Side::BUY};
        ob.addOrder({id, Side::BUY, OrderType::MARKET, 0, reduceQty, 0, myId});
        return;
    }

    if (bestAsk - bestBid < threshold)
        return;
    cout << "Strategy triggered\n";

    // RISK CHECKS
    bool allowBuy = risk.allowBuy(position, baseQty);
    bool allowSell = risk.allowSell(position, baseQty);

    // cancel previous orders to avoid spam
    if (buyOrderId != -1)
    {
        if (ob.orderMap.find(buyOrderId) != ob.orderMap.end())
        {
            ob.cancelOrder(buyOrderId);
            buyOrderId = -1; // FIX: reset ID
        }
    }
    if (sellOrderId != -1)
    {
        if (ob.orderMap.find(sellOrderId) != ob.orderMap.end())
        {
            ob.cancelOrder(sellOrderId);
            sellOrderId = -1;
        }
    }

    // place only if allowed
    if (allowBuy)
    {
        buyOrderId = ob.generateOrderId();
        myOrders.insert(buyOrderId);
        execStats[buyOrderId] = {0.0, 0, baseQty, bestBid, Side::BUY}; // expected price
        ob.addOrder({buyOrderId, Side::BUY, OrderType::LIMIT, bestBid, 1, 0, myId});
    }

    if (allowSell)
    {
        sellOrderId = ob.generateOrderId();
        myOrders.insert(sellOrderId);
        execStats[sellOrderId] = {0.0, 0, baseQty, bestAsk, Side::SELL}; // expected price
        ob.addOrder({sellOrderId, Side::SELL, OrderType::LIMIT, bestAsk, 1, 0, myId});
    }
}

void SpreadStrategy::onTrade(const Trade &t, OrderBook &ob)
{
    if (t.buyId == buyOrderId)
        buyOrderId = -1;

    if (t.sellId == sellOrderId)
        sellOrderId = -1;
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

    // Position + cash
    if (t.buyOwnerId == myId)
    {
        position += t.quantity;
        cash -= t.price * t.quantity;
    }
    else if (t.sellOwnerId == myId)
    {
        position -= t.quantity;
        cash += t.price * t.quantity;
    }

    cout << "Trade: " << t.price << " | Position: " << position << " | Cash: " << cash << endl;

    double pnl = currentPnL(ob);
    pnlHistory.push_back(pnl);
    cout << "Checking execStats for buyId: " << t.buyId << endl;
    cout << "Checking execStats for sellId: " << t.sellId << endl;
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