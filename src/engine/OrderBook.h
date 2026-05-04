#pragma once
#include <list>
#include <unordered_map>
#include <map>
#include <vector>
#include <memory>

#include "../core/Order.h"
#include "../core/Trade.h"
#include "../core/Snapshot.h"
#include "../core/Types.h"

class Strategy; // forward declaration,Avoids circular include

struct OrderNode
{
    Order *order;
    std::list<std::unique_ptr<Order>>::iterator it;

    OrderNode(Order *o,
              std::list<std::unique_ptr<Order>>::iterator iter)
        : order(o), it(iter) {}
};

class OrderBook
{
private:
    void _processOrder(const Order &order);

public:
    OrderBook() = default;
    // disable copy
    OrderBook(const OrderBook &) = delete;
    OrderBook &operator=(const OrderBook &) = delete;

    // enable move
    OrderBook(OrderBook &&) noexcept = default;
    OrderBook &operator=(OrderBook &&) noexcept = default;
    std::unordered_map<int, OrderNode> orderMap;

    std::map<double,
             std::list<std::unique_ptr<Order>>,
             std::greater<double>>
        bids;
    std::map<double,
             std::list<std::unique_ptr<Order>>>
        asks;

    std::vector<Trade> trades;
    std::vector<Order> pendingOrders;
    bool isProcessing = false;

    std::vector<Snapshot> snapshots;

    Strategy *strategy = nullptr;
    long long orderTimestamp = 0;
    long long tradeTimestamp = 0;

    // [FIX] Stable storage for in-flight market orders so their raw pointers
    // remain valid across re-entrant callbacks during execution
    std::vector<std::unique_ptr<Order>> activeMarketOrders;

    void addOrder(const Order &order);
    void cancelOrder(int orderId);
    void modifyOrder(int orderId, double newPrice, int newQty);

    void takeSnapshot(long long timestamp);
    void printSnapshots();

    void onEvent(EventType event);
    void replayTrades();

    int nextOrderId = 1000;

    int generateOrderId()
    {
        return nextOrderId++;
    }

    ~OrderBook() = default;
};