#include <iostream>
#include <map>
#include <queue>
#include <list>
#include <unordered_map>

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
    virtual void onTrade(const Trade& t) = 0;
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
};

class SpreadStrategy : public Strategy {
public:
    int threshold = 2;

    void onEvent(OrderBook &ob) override {
        if (ob.bids.empty() || ob.asks.empty()) return;

        int bestBid = ob.bids.begin()->first;
        int bestAsk = ob.asks.begin()->first;

        if (bestAsk - bestBid >= threshold) {
            cout << "Strategy triggered\n";

            ob.addOrder({1000, BUY, LIMIT, bestBid, 1});
            ob.addOrder({1001, SELL, LIMIT, bestAsk, 1});
        }
    }
    void onTrade(const Trade& t) override {
        cout << "Trade: " << t.price << endl;
    }
};

class ImbalanceStrategy : public Strategy {
public:
    void onEvent(OrderBook &ob) override {
        int bidQty = 0, askQty = 0;

        for (auto &p : ob.bids)
            for (auto o : p.second) bidQty += o->quantity;

        for (auto &p : ob.asks)
            for (auto o : p.second) askQty += o->quantity;

        double imbalance = (double)bidQty / (bidQty + askQty);

        if (imbalance > 0.7) {
            ob.addOrder({2000, BUY, MARKET, 0, 1});
        } else if (imbalance < 0.3) {
            ob.addOrder({2001, SELL, MARKET, 0, 1});
        }
    }
    void onTrade(const Trade& t) override {
        cout << "Trade: " << t.price << endl;
    }
};

class MomentumStrategy : public Strategy {
public:
    vector<int> prices;

    void onTrade(const Trade& t) override {
        prices.push_back(t.price);

        if (prices.size() < 3) return;

        int n = prices.size();

        // upward momentum
        if (prices[n-1] > prices[n-2] && prices[n-2] > prices[n-3]) {
            cout << "Momentum BUY signal\n";
        }

        // downward momentum
        else if (prices[n-1] < prices[n-2] && prices[n-2] < prices[n-3]) {
            cout << "Momentum SELL signal\n";
        }
    }

    void onEvent(OrderBook &ob) override {}
};

class MarketMakingStrategy : public Strategy {
public:
    int buyOrderId = -1;
    int sellOrderId = -1;

    int nextId = 1000;
    int spread = 2;

    void onEvent(OrderBook &ob) override {
        if (ob.bids.empty() || ob.asks.empty()) return;

        int bestBid = ob.bids.begin()->first;
        int bestAsk = ob.asks.begin()->first;

        int mid = (bestBid + bestAsk) / 2;

        int buyPrice = mid - spread;
        int sellPrice = mid + spread;

        // cancel old orders
        if (buyOrderId != -1) ob.cancelOrder(buyOrderId);
        if (sellOrderId != -1) ob.cancelOrder(sellOrderId);

        // place new ones 
        buyOrderId = nextId++;
        sellOrderId = nextId++;

        cout << "Market Making...\n";

        ob.addOrder({4000, BUY, LIMIT, buyPrice, 1});
        ob.addOrder({4001, SELL, LIMIT, sellPrice, 1});
    }

    // tracking inventory 
    int position = 0;

    void onTrade(const Trade& t) override { 
        if (t.buyId == buyOrderId) {
            position += t.quantity;
        }
        if (t.sellId == sellOrderId) {
            position -= t.quantity;
        }
        
        cout << "Position: " << position << endl;
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

        ob.onEvent(TRADE_EXEC);

        if (ob.strategy) {
            ob.strategy->onTrade(ob.trades.back());
        }

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

        ob.onEvent(TRADE_EXEC);

        if (ob.strategy) {
            ob.strategy->onTrade(ob.trades.back());
        }

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

        ob.onEvent(TRADE_EXEC);

        if (ob.strategy) {
            ob.strategy->onTrade(ob.trades.back());
        }

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