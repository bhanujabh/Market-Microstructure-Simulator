#include <iostream>
#include "ImbalanceStrategy.h"
#include "../core/Types.h"
#include "../analytics/Metrics.h"
#include "../engine/OrderBook.h"
#include "../core/Trade.h"

using namespace std;

void ImbalanceStrategy::onEvent(OrderBook &ob)
{
    int bidQty = 0, askQty = 0;
    int levels = 0;
    for (auto &p : ob.bids)
    {
        for (const auto &o : p.second)
            bidQty += o->quantity;
        if (++levels == 3)
            break;
    }
    levels = 0;
    for (auto &p : ob.asks)
    {
        for (const auto &o : p.second)
            askQty += o->quantity;
        if (++levels == 3)
            break;
    }
    if (bidQty + askQty == 0)
        return;

    double imbalance = (double)bidQty / (bidQty + askQty);
    double strength = abs(imbalance - 0.5);
    int qty = max(1, (int)(strength * 10));

    if (risk.killSwitch(currentPnL(ob)))
    {
        cout << "Risk Kill Switch Triggered\n";
        return;
    }

    // RISK MANAGEMENT FIRST
    // ===== CHANGED: use RiskManager instead of maxPosition =====
    if (!risk.allowBuy(position, qty))
    {
        if (position > 3)
            qty = 1;
        int id = ob.generateOrderId();
        myOrders.insert(id);
        double mid = getMidPrice(ob);

        execStats[id] = {0.0, 0, qty, mid, Side::SELL};

        ob.addOrder({id,
                     Side::SELL,
                     OrderType::MARKET,
                     0,
                     qty,
                     0, myId});

        return;
    }

    // ===== CHANGED: use RiskManager instead of maxPosition =====
    if (!risk.allowSell(position, qty))
    {
        if (position < -3)
            qty = 1;
        int id = ob.generateOrderId();
        myOrders.insert(id);
        double mid = getMidPrice(ob);

        execStats[id] = {0.0, 0, qty, mid, Side::BUY};

        ob.addOrder({id,
                     Side::BUY,
                     OrderType::MARKET,
                     0,
                     qty,
                     0, myId});

        return;
    }

    if (position > 0 && imbalance < 0.5)
    {
        int id = ob.generateOrderId();
        myOrders.insert(id);

        double mid = getMidPrice(ob);

        execStats[id] = {0.0, 0, abs(position), mid, Side::SELL};

        ob.addOrder({id, Side::SELL, OrderType::MARKET, 0, abs(position), 0, myId});
    }

    double spread = askQty - bidQty;
    // normalising the spread
    double volumeMid = (askQty + bidQty) / 2.0;
    double spreadPct = spread / volumeMid;
    double upperThreshold = 0.5 + 0.2 * spreadPct;
    double lowerThreshold = 0.5 - 0.2 * spreadPct;

    // STRATEGY

    if (imbalance > upperThreshold)
    {
        // MARKET → expected price ~ mid
        if (risk.allowBuy(position))
        {
            int id = ob.generateOrderId();
            double mid = getMidPrice(ob);
            double price = mid - (1 + strength * 2); // passive buy
            double strength = abs(imbalance - 0.5);
            int qty = max(1, (int)(strength * 10));
            if (position < -3)
                qty = 1;
            myOrders.insert(id);

            execStats[id] = {0.0, 0, qty, price, Side::BUY};

            ob.addOrder({id,
                         Side::BUY,
                         OrderType::LIMIT,
                         price,
                         qty,
                         0, myId});
        }
    }
    else if (imbalance < lowerThreshold)
    {
        // MARKET → expected price ~ mid
        if (risk.allowSell(position))
        {
            int id = ob.generateOrderId();
            double mid = getMidPrice(ob);
            double price = mid + (1 + strength * 2);
            double strength = abs(imbalance - 0.5);
            int qty = max(1, (int)(strength * 10));
            if (position > 3)
                qty = 1;
            myOrders.insert(id);

            execStats[id] = {0.0, 0, qty, price, Side::SELL};

            ob.addOrder({id,
                         Side::SELL,
                         OrderType::LIMIT,
                         price,
                         qty,
                         0, myId});
        }
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
