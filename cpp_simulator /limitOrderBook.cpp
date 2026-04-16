#include <iostream>
#include <map>
#include <queue>
#include <list>
#include <unordered_map>
#include <unordered_set>

using namespace std;

enum OrderType { MARKET, LIMIT };
enum Side { BUY, SELL };
enum EventType {
    ORDER_ADD,
    ORDER_CANCEL,
    ORDER_MODIFY,
    TRADE_EXEC
};

long long orderTimestamp = 0;
long long tradeTimestamp = 0;

struct Order {
    int id;
    Side side;
    OrderType type;
    int price;
    int quantity;
    long long timestamp;
};

struct OrderNode {
    Order* order;
    list<Order*>::iterator it;

    OrderNode(Order* o, list<Order*>::iterator iter)
        : order(o), it(iter) {}
};

struct Trade {
    int buyId;
    int sellId;
    int price;
    int quantity;
    long long timestamp;
};

struct Snapshot {
    long long timestamp;

    vector<pair<int, int>> bidLevels; // price, total qty
    vector<pair<int, int>> askLevels;
};

class OrderBook; // forward declaration

void matchOrders(OrderBook &ob);
void executeMarketBuy(OrderBook &ob, Order* order);
void executeMarketSell(OrderBook &ob, Order* order);

class Strategy {
public:
    virtual void onEvent(OrderBook &ob) = 0;
    virtual void onTrade(const Trade& t, OrderBook &ob) = 0;
    vector<double> pnlHistory;
    double maxPnL = -1e9;
    double maxDrawdown = 0;
};

class OrderBook {
public:
    unordered_map<int, OrderNode> orderMap;

    map<int, list<Order*>, greater<int>> bids;
    map<int, list<Order*>> asks;

    vector<Trade> trades;

    vector<Snapshot> snapshots;

    bool isStrategyRunning = false;

    void addOrder(Order order) {
        Order* newOrder = new Order(order); // allocate 
        newOrder->timestamp = orderTimestamp++; 

        if (order.type == MARKET) {
            cout << "Market Order Received: ID=" << newOrder->id << " Time=" << newOrder->timestamp << endl;
            if (order.side == BUY)
                executeMarketBuy(*this, newOrder);
            else
                executeMarketSell(*this, newOrder);

            delete newOrder;
            onEvent(ORDER_ADD);
            return;
        }

        if (order.side == BUY) {
            bids[order.price].push_back(newOrder);
            auto it = prev(bids[order.price].end());
            orderMap.emplace(order.id, OrderNode(newOrder, it));
        } else {
            asks[order.price].push_back(newOrder);
            auto it = prev(asks[order.price].end());
            orderMap.emplace(order.id, OrderNode(newOrder, it));
        }

        cout << "Order Added: ID=" << order.id << " Time=" << order.timestamp << endl;

        matchOrders(*this);
        onEvent(ORDER_ADD);
    }

    void cancelOrder(int orderId) {
        if (this->orderMap.find(orderId) == this->orderMap.end()) {
            cout << "Order not found\n";
            return;
        }

        OrderNode &node = orderMap.at(orderId);
        Order* order = node.order;
        
        if (order->side == BUY) {
            auto &orderList = this->bids[order->price];
            orderList.erase(node.it);   

            if (orderList.empty())
                bids.erase(order->price);
        } else {
            auto &orderList = this->asks[order->price];
            orderList.erase(node.it);  

            if (orderList.empty())
                asks.erase(order->price);
        }
        delete order;
        orderMap.erase(orderId);
        
        cout << "Order " << orderId << " cancelled\n";

        onEvent(ORDER_CANCEL);
    }

    void modifyOrder(int orderId, int newPrice, int newQty) {
        if (orderMap.find(orderId) == orderMap.end()) {
            cout << "Order not found\n";
            return;
        }

        if (newQty == 0) {
            cancelOrder(orderId);
            return;
        }
        
        OrderNode &node = orderMap.at(orderId);
        Order* order = node.order;

        Side side = order->side;
        OrderType type = order->type;
        
        // Case 1: PRICE CHANGE → cancel + re-add
        if (order->price != newPrice) {
            // remove old order
            cancelOrder(orderId);

            // create new order with SAME ID (or new ID depending design)
            Order newOrder = {orderId, side, type, newPrice, newQty};
            addOrder(newOrder);
            
            cout << "Order " << orderId << " modified (price change)\n";
            return;
        }
        
        // Case 2: ONLY QUANTITY CHANGE
        if (newQty > order->quantity) {
            order->timestamp = orderTimestamp++; // lose priority
        }
        order->quantity = newQty;
        cout << "Order " << orderId << " modified (qty change)\n";

        matchOrders(*this);
        onEvent(ORDER_MODIFY);
    }

    void takeSnapshot(long long timestamp) {
        Snapshot snap;
        snap.timestamp = timestamp;
        // Aggregate bids
        for (auto &priceLevel : bids) {
            int totalQty = 0;
            for (auto order : priceLevel.second) {
                totalQty += order->quantity;
            }
            snap.bidLevels.push_back({priceLevel.first, totalQty});
        }
        // Aggregate asks
        for (auto &priceLevel : asks) {
            int totalQty = 0;
            for (auto order : priceLevel.second) {
                totalQty += order->quantity;
            }
            snap.askLevels.push_back({priceLevel.first, totalQty});
        }
        snapshots.push_back(snap);
    }

    void printSnapshots() {
        for (auto &snap : snapshots) {
            cout << "\nSnapshot @ " << snap.timestamp << endl;

            cout << "BIDS:\n";
            for (auto &b : snap.bidLevels)
                cout << b.first << " -> " << b.second << endl;

            cout << "ASKS:\n";
            for (auto &a : snap.askLevels)
                cout << a.first << " -> " << a.second << endl;
        }
    }

    Strategy* strategy = nullptr;
    // strategy → points to strat
    // Why pointer? Because: polymorphism (base class pointer → derived class object)
    // and flexibility (swap strategies easily)

    void onEvent(EventType event) {
        long long ts = orderTimestamp + tradeTimestamp; // unified timeline
        takeSnapshot(ts);

        // Without this, infinite recursion:
        if (strategy && !isStrategyRunning) {
            isStrategyRunning = true; // block re-entry
            strategy->onEvent(*this); // run strategy
            isStrategyRunning = false; // allow future calls
        }
    }

    void replayTrades() {
        cout << "\n--- Replay ---\n";
        for (auto &t : trades) {
            cout << "Trade: "
             << t.quantity << " @ " << t.price
             << " | BuyID: " << t.buyId
             << " | SellID: " << t.sellId << endl;
        }
    }
};

struct ExecutionStats {
    double totalValue = 0;
    int filledQty = 0;
    int intendedQty = 0;
    int expectedPrice = 0; // this should be double 
};

class SpreadStrategy : public Strategy {
public:
    int threshold = 2;

    int position = 0;        // current inventory
    int maxPosition = 5;     // risk limit

    int buyOrderId = -1;
    int sellOrderId = -1;

    int nextId = 1000;

    unordered_set<int> myOrders;

    double cash = 0;     // money earned/spent

    unordered_map<int, ExecutionStats> execStats;

    double inventoryPenalty = 0.1;

    void onEvent(OrderBook &ob) override {
        if (ob.bids.empty() || ob.asks.empty()) return;

        int bestBid = ob.bids.begin()->first;
        int bestAsk = ob.asks.begin()->first;

        // too much inventory → sell immediately to reduce risk
        // Why MARKET order? SELL, MARKET -> Because: want to exit immediately
        // limit order → might not fill but market order → guaranteed execution 
        if (position >= maxPosition) {
            cout << "Reducing long position\n";
            int id = nextId++;
            myOrders.insert(id);
            execStats[id] = {0, 0, 1, bestBid}; // or mid
            ob.addOrder({id, SELL, MARKET, 0, 1});
            return; // stop further trading
        }
        if (position <= -maxPosition) {
            cout << "Reducing short position\n";
            int id = nextId++;
            myOrders.insert(id);
            execStats[id] = {0, 0, 1, bestAsk}; // or mid
            ob.addOrder({id, BUY, MARKET, 0, 1});
            return;
        }

        if (bestAsk - bestBid < threshold) return;
        cout << "Strategy triggered\n";

        // RISK CHECKS
        bool allowBuy = (position < maxPosition);
        bool allowSell = (position > -maxPosition);

        // cancel previous orders to avoid spam
        if (buyOrderId != -1) ob.cancelOrder(buyOrderId);
        if (sellOrderId != -1) ob.cancelOrder(sellOrderId);

        // place only if allowed
        if (allowBuy) {
            buyOrderId = nextId++;
            myOrders.insert(buyOrderId);
            execStats[buyOrderId] = {0, 0, 1, bestBid};  // expected price
            ob.addOrder({buyOrderId, BUY, LIMIT, bestBid, 1});
        }

        if (allowSell) {
            sellOrderId = nextId++;
            myOrders.insert(sellOrderId);
            execStats[sellOrderId] = {0, 0, 1, bestAsk};  // expected price
            ob.addOrder({sellOrderId, SELL, LIMIT, bestAsk, 1});
        }
    }
    void onTrade(const Trade& t, OrderBook &ob) override {
        // Execution stats
        if (execStats.count(t.buyId)) {
            execStats[t.buyId].totalValue += t.price * t.quantity;
            execStats[t.buyId].filledQty += t.quantity;
        }
        if (execStats.count(t.sellId)) {
            execStats[t.sellId].totalValue += t.price * t.quantity;
            execStats[t.sellId].filledQty += t.quantity;
        } 

        // Position + cash (YOU MISSED THIS)
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

    double getPnL(OrderBook &ob) {
        double mid = 0;

        if (!ob.bids.empty() && !ob.asks.empty()) {
            int bestBid = ob.bids.begin()->first;
            int bestAsk = ob.asks.begin()->first;
            mid = (bestBid + bestAsk) / 2.0;
        }
        return cash + position * mid - inventoryPenalty * abs(position);
    }

    double getAvgExecutionPrice(int orderId) {
        auto &s = execStats[orderId];
        if (s.filledQty == 0) return 0;
        return s.totalValue / s.filledQty;
    }
    
    double getFillRate(int orderId) {
        auto &s = execStats[orderId];
        return (double)s.filledQty / s.intendedQty;
    }
    
    double getSlippage(int orderId) {
        return getAvgExecutionPrice(orderId) - execStats[orderId].expectedPrice;
    }

    double getSharpe() {
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

    void printStats(OrderBook &ob) {
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
    }
};

class ImbalanceStrategy : public Strategy {
public:
    int position = 0;
    int maxPosition = 5;
    int nextId = 2000;

    unordered_set<int> myOrders;

    double cash = 0;     // money earned/spent

    unordered_map<int, ExecutionStats> execStats;

    double inventoryPenalty = 0.1;

    void onEvent(OrderBook &ob) override {
        int bidQty = 0, askQty = 0;

        for (auto &p : ob.bids)
            for (auto o : p.second) bidQty += o->quantity;

        for (auto &p : ob.asks)
            for (auto o : p.second) askQty += o->quantity;

        double imbalance = (double)bidQty / (bidQty + askQty);

        // RISK MANAGEMENT FIRST
        if (position >= maxPosition) {
            int id = nextId++;
            myOrders.insert(id);
            execStats[id] = {0, 0, 1, 0};  // MARKET → expected price ~ mid
            ob.addOrder({id, SELL, MARKET, 0, 1});
            return;
        }
        if (position <= -maxPosition) {
            int id = nextId++;
            myOrders.insert(id);
            execStats[id] = {0, 0, 1, 0};  // MARKET → expected price ~ mid
            ob.addOrder({id, BUY, MARKET, 0, 1});
            return;
        }
        
        // STRATEGY 
        int id = nextId++;
        if (imbalance > 0.7) {
            myOrders.insert(id);
            execStats[id] = {0, 0, 1, 0};  // MARKET → expected price ~ mid
            ob.addOrder({id, BUY, MARKET, 0, 1});
        } else if (imbalance < 0.3) {
            myOrders.insert(id);
            execStats[id] = {0, 0, 1, 0};  // MARKET → expected price ~ mid
            ob.addOrder({id, SELL, MARKET, 0, 1});
        }
    }
    void onTrade(const Trade& t, OrderBook &ob) override {
        if (execStats.count(t.buyId)) {
            execStats[t.buyId].totalValue += t.price * t.quantity;
            execStats[t.buyId].filledQty += t.quantity;
        }
        if (execStats.count(t.sellId)) {
            execStats[t.sellId].totalValue += t.price * t.quantity;
            execStats[t.sellId].filledQty += t.quantity;
        }

        if (myOrders.count(t.buyId)){
            position += t.quantity;
            cash -= t.price * t.quantity;
        } 
        if (myOrders.count(t.sellId)){
            position -= t.quantity;
            cash += t.price * t.quantity;
        } 

        cout << "Trade: " << t.price << " | Position: " << position << " | Cash: " << cash << endl;

        double pnl = getPnL(ob);
        pnlHistory.push_back(pnl);
        if (pnl > maxPnL) maxPnL = pnl;
        maxDrawdown = max(maxDrawdown, maxPnL - pnl);
    }

    double getPnL(OrderBook &ob) {
        double mid = 0;

        if (!ob.bids.empty() && !ob.asks.empty()) {
            int bestBid = ob.bids.begin()->first;
            int bestAsk = ob.asks.begin()->first;
            mid = (bestBid + bestAsk) / 2.0;
        }
        return cash + position * mid - inventoryPenalty * abs(position);
    }

    double getAvgExecutionPrice(int orderId) {
        auto &s = execStats[orderId];
        if (s.filledQty == 0) return 0;
        return s.totalValue / s.filledQty;
    }
    
    double getFillRate(int orderId) {
        auto &s = execStats[orderId];
        return (double)s.filledQty / s.intendedQty;
    }
    
    double getSlippage(int orderId) {
        return getAvgExecutionPrice(orderId) - execStats[orderId].expectedPrice;
    }

    double getSharpe() {
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

    void printStats(OrderBook &ob) {
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
    }
};

class MomentumStrategy : public Strategy {
public:
    vector<int> prices;

    int position = 0;
    int maxPosition = 5;
    int nextId = 3000;

    unordered_set<int> myOrders;

    double cash = 0;     // money earned/spent

    unordered_map<int, ExecutionStats> execStats;

    double inventoryPenalty = 0.1;

    void onTrade(const Trade& t, OrderBook &ob) override {
        prices.push_back(t.price);

        if (execStats.count(t.buyId)) {
            execStats[t.buyId].totalValue += t.price * t.quantity;
            execStats[t.buyId].filledQty += t.quantity;
        }
        if (execStats.count(t.sellId)) {
            execStats[t.sellId].totalValue += t.price * t.quantity;
            execStats[t.sellId].filledQty += t.quantity;
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
            ob.addOrder({id, SELL, MARKET, 0, 1});
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
            ob.addOrder({id, BUY, MARKET, 0, 1});
        }

        // downward momentum
        else if (prices[n-1] < prices[n-2] && prices[n-2] < prices[n-3]) {
            cout << "Momentum SELL signal\n";
            int id = nextId++;
            myOrders.insert(id);
            ob.addOrder({id, SELL, MARKET, 0, 1});
        }

        double pnl = getPnL(ob);
        pnlHistory.push_back(pnl);
        if (pnl > maxPnL) maxPnL = pnl;
        maxDrawdown = max(maxDrawdown, maxPnL - pnl);
    }

    void onEvent(OrderBook &ob) override {
        if (position > maxPosition) {
            int reduceQty = position - maxPosition;
            int id = nextId++;
            myOrders.insert(id);
            ob.addOrder({id, SELL, MARKET, 0, reduceQty});
            return;
        }
        
        if (position < -maxPosition) {
            int reduceQty = -maxPosition - position;
            int id = nextId++;
            myOrders.insert(id);
            ob.addOrder({id, BUY, MARKET, 0, reduceQty});
            return;
        }
    }

    double getPnL(OrderBook &ob) {
        double mid = 0;

        if (!ob.bids.empty() && !ob.asks.empty()) {
            int bestBid = ob.bids.begin()->first;
            int bestAsk = ob.asks.begin()->first;
            mid = (bestBid + bestAsk) / 2.0;
        }
        return cash + position * mid - inventoryPenalty * abs(position);
    }

    double getAvgExecutionPrice(int orderId) {
        auto &s = execStats[orderId];
        if (s.filledQty == 0) return 0;
        return s.totalValue / s.filledQty;
    }
    
    double getFillRate(int orderId) {
        auto &s = execStats[orderId];
        return (double)s.filledQty / s.intendedQty;
    }
    
    double getSlippage(int orderId) {
        return getAvgExecutionPrice(orderId) - execStats[orderId].expectedPrice;
    }

    double getSharpe() {
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

    void printStats(OrderBook &ob) {
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
    }
};

class MarketMakingStrategy : public Strategy {
public:
    int buyOrderId = -1;
    int sellOrderId = -1;

    int nextId = 1000;
    int spread = 2;

    // tracking inventory
    int position = 0;
    int maxPosition = 5;

    unordered_set<int> myOrders;

    double cash = 0;     // money earned/spent

    unordered_map<int, ExecutionStats> execStats;

    double inventoryPenalty = 0.1;
    
    void onEvent(OrderBook &ob) override {
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
            ob.addOrder({id, SELL, MARKET, 0, 1});
            return;
        }
        if (position <= -maxPosition) {
            int id = nextId++;
            execStats[id] = {0, 0, 1, bestAsk};
            ob.addOrder({id, BUY, MARKET, 0, 1});
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
        ob.addOrder({buyOrderId, BUY, LIMIT, buyPrice, 1});
        execStats[sellOrderId] = {0, 0, 1, sellPrice};
        ob.addOrder({sellOrderId, SELL, LIMIT, sellPrice, 1});
    } 

    void onTrade(const Trade& t, OrderBook &ob) override { 
        if (execStats.count(t.buyId)) {
            execStats[t.buyId].totalValue += t.price * t.quantity;
            execStats[t.buyId].filledQty += t.quantity;
        }
        if (execStats.count(t.sellId)) {
            execStats[t.sellId].totalValue += t.price * t.quantity;
            execStats[t.sellId].filledQty += t.quantity;
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

    double getPnL(OrderBook &ob) {
        double mid = 0;

        if (!ob.bids.empty() && !ob.asks.empty()) {
            int bestBid = ob.bids.begin()->first;
            int bestAsk = ob.asks.begin()->first;
            mid = (bestBid + bestAsk) / 2.0;
        }
        return cash + position * mid - inventoryPenalty * abs(position);
    }

    double getAvgExecutionPrice(int orderId) {
        auto &s = execStats[orderId];
        if (s.filledQty == 0) return 0;
        return s.totalValue / s.filledQty;
    }
    
    double getFillRate(int orderId) {
        auto &s = execStats[orderId];
        return (double)s.filledQty / s.intendedQty;
    }
    
    double getSlippage(int orderId) {
        return getAvgExecutionPrice(orderId) - execStats[orderId].expectedPrice;
    }

    double getSharpe() {
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

    void printStats(OrderBook &ob) {
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
    }
};

void matchOrders(OrderBook &ob) {
    while (!ob.bids.empty() && !ob.asks.empty()) {
        auto bestBid = ob.bids.begin();
        auto bestAsk = ob.asks.begin();

        if (bestBid->first < bestAsk->first) break;

        Order* buyOrder = bestBid->second.front();
        Order* sellOrder = bestAsk->second.front();

        int tradedQty = min(buyOrder->quantity, sellOrder->quantity);

        ob.trades.push_back({buyOrder->id, sellOrder->id, bestAsk->first, tradedQty, tradeTimestamp++});

        if (ob.strategy && !ob.isStrategyRunning) {
            ob.isStrategyRunning = true;
            ob.strategy->onTrade(ob.trades.back(), ob);
            ob.isStrategyRunning = false;
        }

        ob.onEvent(TRADE_EXEC);

        cout << "Trade executed: "
             << tradedQty << " @ " << bestAsk->first << endl;

        buyOrder->quantity -= tradedQty;
        sellOrder->quantity -= tradedQty;

        if (buyOrder->quantity == 0)
            cout << "Buy Order FULLY filled\n";
        else
            cout << "Buy Order PARTIALLY filled\n";

        if (sellOrder->quantity == 0)
            cout << "Sell Order FULLY filled\n";
        else
            cout << "Sell Order PARTIALLY filled\n";

        if (buyOrder->quantity == 0){
            int id = buyOrder->id;        // save BEFORE deletion
            bestBid->second.pop_front(); // deletes the object
            delete buyOrder; // free memory
            ob.orderMap.erase(id); // remove tracking 
        }
        
        if (sellOrder->quantity == 0){
            int id = sellOrder->id;
            bestAsk->second.pop_front();
            delete sellOrder;
            ob.orderMap.erase(id);
        }

        if (bestBid->second.empty())
            ob.bids.erase(bestBid);

        if (bestAsk->second.empty())
            ob.asks.erase(bestAsk);
    }
}

void executeMarketBuy(OrderBook &ob, Order* marketOrder) {
    int quantity = marketOrder->quantity;
    while (quantity > 0 && !ob.asks.empty()) {
        auto bestAsk = ob.asks.begin();

        Order* sellOrder = bestAsk->second.front();

        int tradedQty = min(quantity, sellOrder->quantity);

        ob.trades.push_back({marketOrder->id, sellOrder->id, bestAsk->first, tradedQty, tradeTimestamp++});

        if (ob.strategy && !ob.isStrategyRunning) {
            ob.isStrategyRunning = true;
            ob.strategy->onTrade(ob.trades.back(), ob);
            ob.isStrategyRunning = false;
        }

        ob.onEvent(TRADE_EXEC);

        cout << "Trade: MKT BUY " << marketOrder->id
             << " vs SELL " << sellOrder->id
             << " Qty=" << tradedQty
             << " @ " << bestAsk->first << endl;

        quantity -= tradedQty;
        sellOrder->quantity -= tradedQty;

        if (sellOrder->quantity == 0){
            int id = sellOrder->id;
            bestAsk->second.pop_front();
            delete sellOrder;
            ob.orderMap.erase(id);
        }

        if (bestAsk->second.empty())
            ob.asks.erase(bestAsk);
    }

    if (quantity > 0) {
        cout << "Market Buy unfilled qty: " << quantity << endl;
    }
}

void executeMarketSell(OrderBook &ob, Order* marketOrder) {
    int quantity = marketOrder->quantity;
    while (quantity > 0 && !ob.bids.empty()) {
        auto bestBid = ob.bids.begin();

        Order* buyOrder = bestBid->second.front();

        int tradedQty = min(quantity, buyOrder->quantity);

        ob.trades.push_back({buyOrder->id, marketOrder->id, bestBid->first, tradedQty, tradeTimestamp++});

        if (ob.strategy && !ob.isStrategyRunning) {
            ob.isStrategyRunning = true;
            ob.strategy->onTrade(ob.trades.back(), ob);
            ob.isStrategyRunning = false;
        }

        ob.onEvent(TRADE_EXEC);

        cout << "Trade: MKT SELL " << marketOrder->id
             << " vs BUY " << buyOrder->id
             << " Qty=" << tradedQty
             << " @ " << bestBid->first << endl;

        quantity -= tradedQty;
        buyOrder->quantity -= tradedQty;

        if (buyOrder->quantity == 0){
            int id = buyOrder->id;        
            bestBid->second.pop_front(); 
            delete buyOrder;
            ob.orderMap.erase(id);
        }
            
        if (bestBid->second.empty())
            ob.bids.erase(bestBid);
    }

    if (quantity > 0) {
        cout << "Market Sell unfilled qty: " << quantity << endl;
    }
}

int main() {
    OrderBook ob;

    SpreadStrategy strat;
    ob.strategy = &strat;

    ob.addOrder({1, BUY, LIMIT, 100, 10});
    ob.addOrder({2, SELL, LIMIT, 105, 5});

    return 0;
}