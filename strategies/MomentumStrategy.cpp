#include <iostream>
#include "MomentumStrategy.h"
#include "../core/Types.h"

using namespace std;

void MomentumStrategy::onTrade(const Trade& t, OrderBook &ob) {
        prices.push_back(t.price);

        if (execStats.count(t.buyId)) {
            execStats[t.buyId].totalValue += t.price * t.quantity;
            execStats[t.buyId].filledQty += t.quantity;

            double expected = execStats[t.buyId].expectedPrice;
            // count only once per order
            if (!countedOrders.count(t.buyId)) {
                totalTrades++;
                countedOrders.insert(t.buyId);
                if (t.price < expected) {  // BUY → lower is better
                    wins++;
                }
            }
        }
        if (execStats.count(t.sellId)) {
            execStats[t.sellId].totalValue += t.price * t.quantity;
            execStats[t.sellId].filledQty += t.quantity;

            double expected = execStats[t.sellId].expectedPrice;
            if (!countedOrders.count(t.sellId)) {
                totalTrades++;
                countedOrders.insert(t.sellId);
                if (t.price > expected) {  // SELL → higher is better
                    wins++;
                }
            }
        }

        if (myOrders.count(t.buyId)){
            position += t.quantity;
            cash -= t.price * t.quantity; // you paid 
        } 
        if (myOrders.count(t.sellId)){
            position -= t.quantity;
            cash += t.price * t.quantity; // you earned
        } 

        if (prices.size() < 3) return;

        int n = prices.size();

        // RISK FIRST
        if (position >= maxPosition) {
            cout << "Reducing long\n";
            int id = nextId++;
            myOrders.insert(id);
            execStats[id] = {0, 0, 1, t.price};  // expected price
            ob.addOrder({id, Side::SELL, OrderType::MARKET, 0, 1});
            return;
        }
        if (position <= -maxPosition) {
            cout << "Reducing short\n";
            return;
        }

        // upward momentum
        if (prices[n-1] > prices[n-2] && prices[n-2] > prices[n-3]) {
            cout << "Momentum BUY signal\n";
            int id = nextId++;
            myOrders.insert(id);
            execStats[id] = {0, 0, 1, t.price};  // expected price
            ob.addOrder({id, Side::BUY, OrderType::MARKET, 0, 1});
        }

        // downward momentum
        else if (prices[n-1] < prices[n-2] && prices[n-2] < prices[n-3]) {
            cout << "Momentum SELL signal\n";
            int id = nextId++;
            myOrders.insert(id);
            ob.addOrder({id, Side::SELL, OrderType::MARKET, 0, 1});
        }

        double pnl = getPnL(ob);
        pnlHistory.push_back(pnl);
        if (pnl > maxPnL) maxPnL = pnl;
        maxDrawdown = max(maxDrawdown, maxPnL - pnl);
    }

    void MomentumStrategy::onEvent(OrderBook &ob) {
        if (position > maxPosition) {
            int reduceQty = position - maxPosition;
            int id = nextId++;
            myOrders.insert(id);
            ob.addOrder({id, Side::SELL, OrderType::MARKET, 0, reduceQty});
            return;
        }
        
        if (position < -maxPosition) {
            int reduceQty = -maxPosition - position;
            int id = nextId++;
            myOrders.insert(id);
            ob.addOrder({id, Side::BUY, OrderType::MARKET, 0, reduceQty});
            return;
        }
    }

    double MomentumStrategy::getPnL(OrderBook &ob) {
        double mid = 0;

        if (!ob.bids.empty() && !ob.asks.empty()) {
            int bestBid = ob.bids.begin()->first;
            int bestAsk = ob.asks.begin()->first;
            mid = (bestBid + bestAsk) / 2.0;
        }
        return cash + position * mid - inventoryPenalty * abs(position);
    }

    double MomentumStrategy::getAvgExecutionPrice(int orderId) {
        auto &s = execStats[orderId];
        if (s.filledQty == 0) return 0;
        return s.totalValue / s.filledQty;
    }
    
    double MomentumStrategy::getFillRate(int orderId) {
        auto &s = execStats[orderId];
        return (double)s.filledQty / s.intendedQty;
    }
    
    double MomentumStrategy::getSlippage(int orderId) {
        return getAvgExecutionPrice(orderId) - execStats[orderId].expectedPrice;
    }

    double MomentumStrategy::getSharpe() {
        if (pnlHistory.size() < 2) return 0;
        vector<double> returns;
        for (int i = 1; i < pnlHistory.size(); i++) {
            returns.push_back(pnlHistory[i] - pnlHistory[i-1]);
        }
        double mean = 0;
        for (double r : returns) mean += r;
        mean /= returns.size();
        double var = 0;
        for (double r : returns) var += (r - mean)*(r - mean);
        var /= returns.size();
        double stddev = sqrt(var);
        if (stddev == 0) return 0;
        return mean / stddev;
    }

    double MomentumStrategy::getWinRate() {
        if (totalTrades == 0) return 0;
        return (double)wins / totalTrades;
    }

    void MomentumStrategy::printStats(OrderBook &ob) {
        for (auto &p : execStats) {
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

