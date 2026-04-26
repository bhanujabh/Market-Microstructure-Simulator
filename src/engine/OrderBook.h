#pragma once
#include <list>
#include <unordered_map>
#include <map>
#include <vector>
#include "../core/Order.h"
#include "../core/Trade.h"
#include "../core/Snapshot.h"
#include "../core/Types.h"

class Strategy; // forward declaration,Avoids circular include

struct OrderNode
{
    Order *order;
    std::list<Order *>::iterator it;

    OrderNode(Order *o, std::list<Order *>::iterator iter)
        : order(o), it(iter) {}
};

class OrderBook
{
public:
    std::unordered_map<int, OrderNode> orderMap;

    std::map<int, std::list<Order *>, std::greater<int>> bids;
    std::map<int, std::list<Order *>> asks;

    std::vector<Trade> trades;

    std::vector<Snapshot> snapshots;

    Strategy *strategy = nullptr;
    long long orderTimestamp = 0;
    long long tradeTimestamp = 0;

    bool isStrategyRunning = false;

    void addOrder(const Order &order);
    void cancelOrder(int orderId);
    void modifyOrder(int orderId, int newPrice, int newQty);

    void takeSnapshot(long long timestamp);
    void printSnapshots();

    void onEvent(EventType event);
    void replayTrades();

    ~OrderBook();
};