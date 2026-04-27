#include <iostream>
#include "MarketMakingStrategy.h"
#include "../core/Types.h"

using namespace std;

void MarketMakingStrategy::onEvent(OrderBook &ob) {
        if (ob.bids.empty() || ob.asks.empty()) return;

        int bestBid = ob.bids.begin()->first;
        int bestAsk = ob.asks.begin()->first;

        int mid = (bestBid + bestAsk) / 2;

        int buyPrice = mid - spread;
        int sellPrice = mid + spread;

        // RISK FIRST
        if (position >= maxPosition) {
            int id = nextId++;
            myOrders.insert(id);

            // for research level analysis
            // double mid = (bestBid + bestAsk) / 2.0;
            // neutral baseline
            // double expectedPrice = mid;

            // bestBid / bestAsk => execution slippage
            // mid => market impact slippage
            double expectedPrice = bestBid;  // SELL → hits bid
            execStats[id] = {0, 0, 1, bestBid};
            ob.addOrder({id, Side::SELL, OrderType::MARKET, 0, 1});
            return;
        }
        if (position <= -maxPosition) {
            int id = nextId++;
            execStats[id] = {0, 0, 1, bestAsk};
            ob.addOrder({id, Side::BUY, OrderType::MARKET, 0, 1});
            return;
        }

        // cancel old orders
        if (buyOrderId != -1) ob.cancelOrder(buyOrderId);
        if (sellOrderId != -1) ob.cancelOrder(sellOrderId);

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

    void MarketMakingStrategy::onTrade(const Trade& t, OrderBook &ob) { 
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

        if (myOrders.count(t.buyId)) {
            position += t.quantity;
            cash -= t.price * t.quantity;
        }
        if (myOrders.count(t.sellId)) {
            position -= t.quantity;
            cash += t.price * t.quantity;
        } 
        
        cout << "Trade: " << t.price << " | Position: " << position << " | Cash: " << cash << endl;

        double pnl = getPnL(ob);
        pnlHistory.push_back(pnl);
        if (pnl > maxPnL) maxPnL = pnl;
        maxDrawdown = max(maxDrawdown, maxPnL - pnl);
    }

    double MarketMakingStrategy::getPnL(OrderBook &ob) {
        double mid = 0;

        if (!ob.bids.empty() && !ob.asks.empty()) {
            int bestBid = ob.bids.begin()->first;
            int bestAsk = ob.asks.begin()->first;
            mid = (bestBid + bestAsk) / 2.0;
        }
        return cash + position * mid - inventoryPenalty * abs(position);
    }

    double MarketMakingStrategy::getAvgExecutionPrice(int orderId) {
        auto &s = execStats[orderId];
        if (s.filledQty == 0) return 0;
        return s.totalValue / s.filledQty;
    }
    
    double MarketMakingStrategy::getFillRate(int orderId) {
        auto &s = execStats[orderId];
        return (double)s.filledQty / s.intendedQty;
    }
    
    double MarketMakingStrategy::getSlippage(int orderId) {
        return getAvgExecutionPrice(orderId) - execStats[orderId].expectedPrice;
    }

    double MarketMakingStrategy::getSharpe() {
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

    double MarketMakingStrategy::getWinRate() {
        if (totalTrades == 0) return 0;
        return (double)wins / totalTrades;
    }

    void MarketMakingStrategy::printStats(OrderBook &ob) {
        for (auto &p : execStats) {
            cout << "Order " << p.first
             << " | AvgPx: " << getAvgExecutionPrice(p.first)
             << " | FillRate: " << getFillRate(p.first)
             << " | Slippage: " << getSlippage(p.first)
             << " | Sharpe ratio: " << getSharpe()
             << endl;
        }
        cout << "Final Position: " << position << endl;
        cout << "Cash: " << cash << endl;
        cout << "PnL: " << getPnL(ob) << endl;
        cout << "Drawdown: " << maxDrawdown << endl;
        cout << "Sharpe: " << getSharpe() << endl;
        cout << "Win Rate: " << getWinRate() << endl;
    }