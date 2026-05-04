#include <iostream>
#include "MomentumStrategy.h"
#include "../core/Types.h"
#include "../analytics/Metrics.h"
#include "../engine/OrderBook.h"
#include "../core/Trade.h"

using namespace std;

void MomentumStrategy::onTrade(const Trade &t, OrderBook &ob)
{
    // prevent unbounded growth
    if (prices.size() > 100)
        prices.erase(prices.begin());
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

    // position + cash update
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

    // Need at least 3 points
    if (prices.size() < 3)
    {
        pnlHistory.push_back(currentPnL(ob));
        return;
    }
    int n = prices.size();
    double momentum = prices[n - 1] - prices[n - 3];
    int qty = min(3, max(1, (int)abs(momentum)));

    // RISK FIRST
    if (!risk.allowBuy(position, qty))
    {
        cout << "Reducing long\n";
        int id = ob.generateOrderId();
        myOrders.insert(id);
        int reduceQty = abs(position);
        if (position > 0)
        { // ===== FIX: correct side =====
            execStats[id] = {0.0, 0, reduceQty, t.price, Side::SELL};
            ob.addOrder({id, Side::SELL, OrderType::MARKET, 0, reduceQty, 0, myId});
        }
        else
        {
            execStats[id] = {0.0, 0, reduceQty, t.price, Side::BUY};
            ob.addOrder({id, Side::BUY, OrderType::MARKET, 0, reduceQty, 0, myId});
        }
        pnlHistory.push_back(currentPnL(ob));
        return;
    }
    if (!risk.allowSell(position, qty))
    {
        cout << "Reducing position (SELL blocked)\n";
        int id = ob.generateOrderId();
        myOrders.insert(id);
        int reduceQty = abs(position);
        if (position > 0)
        {
            execStats[id] = {0.0, 0, reduceQty, t.price, Side::SELL};
            ob.addOrder({id, Side::SELL, OrderType::MARKET, 0, reduceQty, 0, myId});
        }
        else
        {
            execStats[id] = {0.0, 0, reduceQty, t.price, Side::BUY};
            ob.addOrder({id, Side::BUY, OrderType::MARKET, 0, reduceQty, 0, myId});
        }
        pnlHistory.push_back(currentPnL(ob));
        return;
    }
    // EXIT LOGIC

    if (position > 0 && prices[n - 1] < prices[n - 2])
    {
        int id = ob.generateOrderId();
        myOrders.insert(id);

        execStats[id] = {0.0, 0, position, t.price, Side::SELL};

        ob.addOrder({id, Side::SELL, OrderType::MARKET, 0, position, 0, myId});
        pnlHistory.push_back(currentPnL(ob));
        return;
    }

    if (position < 0 && prices[n - 1] > prices[n - 2])
    {
        int id = ob.generateOrderId();
        myOrders.insert(id);

        execStats[id] = {0.0, 0, abs(position), t.price, Side::BUY};

        ob.addOrder({id, Side::BUY, OrderType::MARKET, 0, abs(position), 0, myId});
        pnlHistory.push_back(currentPnL(ob));
        return;
    }

    // upward momentum -> BUY momentum
    if (momentum > 0.5 && risk.allowBuy(position, qty))
    {
        cout << "Momentum BUY signal\n";

        int id = ob.generateOrderId();
        myOrders.insert(id);

        // ===== FIX: correct ExecutionStats order =====
        execStats[id] = {0.0, 0, qty, t.price, Side::BUY};

        // ===== FIX: MARKET order (true momentum) =====
        ob.addOrder({id, Side::BUY, OrderType::MARKET, 0, qty, 0, myId});
    }
    // downward momentum
    else if (momentum < -0.5 && risk.allowSell(position, qty))
    {
        cout << "Momentum SELL signal\n";

        int id = ob.generateOrderId();
        myOrders.insert(id);

        execStats[id] = {0.0, 0, qty, t.price, Side::SELL};

        ob.addOrder({id, Side::SELL, OrderType::MARKET, 0, qty, 0, myId});
    }

    pnlHistory.push_back(currentPnL(ob));
    cout << "Checking execStats for buyId: " << t.buyId << endl;
    cout << "Checking execStats for sellId: " << t.sellId << endl;
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
