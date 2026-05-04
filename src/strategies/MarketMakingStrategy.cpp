#include <iostream>
#include "MarketMakingStrategy.h"
#include "../core/Types.h"
#include "../analytics/Metrics.h"
#include "../engine/OrderBook.h"
#include "../core/Trade.h"

using namespace std;

void MarketMakingStrategy::onEvent(OrderBook &ob)
{
    if (ob.bids.empty() || ob.asks.empty())
        return;

    double bestBid = ob.bids.begin()->first;
    double bestAsk = ob.asks.begin()->first;

    double mid = getMidPrice(ob);

    double skew = position * 0.5; // tune this

    // dynamic spread
    double dynamicSpread = spread * (1 + abs(position) * 0.1);

    double buyPrice = mid - dynamicSpread - skew;
    double sellPrice = mid + dynamicSpread - skew;

    if (risk.killSwitch(currentPnL(ob)))
    {
        cout << "Risk Kill Switch Triggered\n";
        return;
    }

    int qty = max(1, min(3, 3 - abs(position)));

    // RISK FIRST
    // ===== CHANGED: use RiskManager instead of maxPosition =====
    if (!risk.allowBuy(position, qty))
    {
        int id = ob.generateOrderId();
        // no need to add to my orders bcz market orders are implemented immediately
        // myOrders.insert(id);

        execStats[id] = {0.0, 0, qty, mid, Side::SELL};

        // ===== CHANGED: added 6th timestamp field =====
        ob.addOrder({id, Side::SELL, OrderType::MARKET, 0, qty, 0, myId});
        return;
    }

    // ===== CHANGED: use RiskManager instead of maxPosition =====
    if (!risk.allowSell(position, qty))
    {
        int id = ob.generateOrderId();
        // myOrders.insert(id); // ===== CHANGED: inserted missing myOrders =====

        execStats[id] = {0.0, 0, qty, mid, Side::BUY};

        // ===== CHANGED: added 6th timestamp field =====
        ob.addOrder({id, Side::BUY, OrderType::MARKET, 0, qty, 0, myId});
        return;
    }

    // INVENTORY CONTROL EXIT
    if (position > 5)
    {
        int id = ob.generateOrderId();
        execStats[id] = {0.0, 0, position, mid, Side::SELL};

        ob.addOrder({id, Side::SELL, OrderType::MARKET, 0, position, 0, myId});
        return;
    }

    if (position < -5)
    {
        int id = ob.generateOrderId();
        execStats[id] = {0.0, 0, abs(position), mid, Side::BUY};

        ob.addOrder({id, Side::BUY, OrderType::MARKET, 0, abs(position), 0, myId});
        return;
    }

    // If exchange removes your order (not via trade), you won’t detect it.

    if (buyOrderId != -1 && ob.orderMap.find(buyOrderId) == ob.orderMap.end())
        buyOrderId = -1;

    if (sellOrderId != -1 && ob.orderMap.find(sellOrderId) == ob.orderMap.end())
        sellOrderId = -1;

    // ONLY cancel if price changed significantly

    // BUY side
    if (buyOrderId != -1 &&
        ob.orderMap.find(buyOrderId) != ob.orderMap.end())
    {
        if (lastBuyPrice < 0 || abs(buyPrice - lastBuyPrice) > 0.5)
        {
            ob.cancelOrder(buyOrderId);
            buyOrderId = -1; // mark for replacement
        }
    }

    // SELL side
    if (sellOrderId != -1 &&
        ob.orderMap.find(sellOrderId) != ob.orderMap.end())
    {
        if (lastSellPrice < 0 || abs(sellPrice - lastSellPrice) > 0.5)
        {
            ob.cancelOrder(sellOrderId);
            sellOrderId = -1;
        }
    }

    // place new ones
    // Place BUY only if cancelled or doesn't exist
    if (buyOrderId == -1)
    {
        buyOrderId = ob.generateOrderId();
        myOrders.insert(buyOrderId);

        execStats[buyOrderId] = {0.0, 0, qty, buyPrice, Side::BUY};

        ob.addOrder({buyOrderId, Side::BUY, OrderType::LIMIT, buyPrice, qty, 0, myId});
        lastBuyPrice = buyPrice; // track
    }

    // Place SELL only if cancelled or doesn't exist
    if (sellOrderId == -1)
    {
        sellOrderId = ob.generateOrderId();
        myOrders.insert(sellOrderId);

        execStats[sellOrderId] = {0.0, 0, qty, sellPrice, Side::SELL};

        ob.addOrder({sellOrderId, Side::SELL, OrderType::LIMIT, sellPrice, qty, 0, myId});
        lastSellPrice = sellPrice; // track
    }

    cout << "Market Making...\n";
}

void MarketMakingStrategy::onTrade(const Trade &t, OrderBook &ob)
{
    if (t.buyId == buyOrderId)
        buyOrderId = -1;

    if (t.sellId == sellOrderId)
        sellOrderId = -1;
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

    pnlHistory.push_back(currentPnL(ob));
    cout << "Checking execStats for buyId: " << t.buyId << endl;
    cout << "Checking execStats for sellId: " << t.sellId << endl;
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