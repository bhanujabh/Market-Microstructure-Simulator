#include <iostream>
#include "OrderBook.h"
#include "MatchingEngine.h"
#include "../strategies/Strategy.h"

using namespace std;

void OrderBook::addOrder(const Order &order)
{
    Order *newOrder = new Order(order); // allocate
    newOrder->timestamp = orderTimestamp++;

    if (order.type == OrderType::MARKET)
    {
        cout << "Market Order Received: ID=" << newOrder->id << " Time=" << newOrder->timestamp << endl;
        if (order.side == Side::BUY)
            executeMarketBuy(*this, newOrder);
        else
            executeMarketSell(*this, newOrder);

        delete newOrder;
        onEvent(EventType::ORDER_ADD);
        return;
    }

    if (order.side == Side::BUY)
    {
        bids[order.price].push_back(newOrder);
        auto it = prev(bids[order.price].end());
        orderMap.emplace(order.id, OrderNode(newOrder, it));
    }
    else
    {
        asks[order.price].push_back(newOrder);
        auto it = prev(asks[order.price].end());
        orderMap.emplace(order.id, OrderNode(newOrder, it));
    }

    cout << "Order Added: ID=" << order.id << " Time=" << newOrder->timestamp << endl;

    matchOrders(*this);
    onEvent(EventType::ORDER_ADD);
}

void OrderBook::cancelOrder(int orderId)
{
    if (this->orderMap.find(orderId) == this->orderMap.end())
    {
        cout << "Order not found\n";
        return;
    }

    OrderNode &node = orderMap.at(orderId);
    Order *order = node.order;

    if (order->side == Side::BUY)
    {
        auto &orderList = this->bids[order->price];
        orderList.erase(node.it);

        if (orderList.empty())
            bids.erase(order->price);
    }
    else
    {
        auto &orderList = this->asks[order->price];
        orderList.erase(node.it);

        if (orderList.empty())
            asks.erase(order->price);
    }
    delete order;
    orderMap.erase(orderId);

    cout << "Order " << orderId << " cancelled\n";

    onEvent(EventType::ORDER_CANCEL);
}

void OrderBook::modifyOrder(int orderId, int newPrice, int newQty)
{
    if (orderMap.find(orderId) == orderMap.end())
    {
        cout << "Order not found\n";
        return;
    }

    if (newQty == 0)
    {
        cancelOrder(orderId);
        return;
    }

    OrderNode &node = orderMap.at(orderId);
    Order *order = node.order;

    Side side = order->side;
    OrderType type = order->type;

    // Case 1: PRICE CHANGE → cancel + re-add
    if (order->price != newPrice)
    {
        // remove old order
        cancelOrder(orderId);

        // create new order with SAME ID (or new ID depending design)
        Order newOrder = {orderId, side, type, newPrice, newQty};
        addOrder(newOrder);

        cout << "Order " << orderId << " modified (price change)\n";
        return;
    }

    // Case 2: ONLY QUANTITY CHANGE
    if (newQty > order->quantity)
    {
        auto &level = (order->side == Side::BUY)
                          ? bids[order->price]
                          : asks[order->price];

        // remove from current position
        level.erase(node.it);

        // update timestamp
        order->timestamp = orderTimestamp++;

        // move to back of queue
        level.push_back(order);

        // save new iterator
        node.it = prev(level.end());
    }
    order->quantity = newQty;
    cout << "Order " << orderId << " modified (qty change)\n";

    matchOrders(*this);
    onEvent(EventType::ORDER_MODIFY);
}

void OrderBook::takeSnapshot(long long timestamp)
{
    Snapshot snap;
    snap.timestamp = timestamp;
    // Aggregate bids
    for (auto &priceLevel : bids)
    {
        int totalQty = 0;
        for (auto order : priceLevel.second)
        {
            totalQty += order->quantity;
        }
        snap.bidLevels.push_back({priceLevel.first, totalQty});
    }
    // Aggregate asks
    for (auto &priceLevel : asks)
    {
        int totalQty = 0;
        for (auto order : priceLevel.second)
        {
            totalQty += order->quantity;
        }
        snap.askLevels.push_back({priceLevel.first, totalQty});
    }
    snapshots.push_back(snap);
}

void OrderBook::printSnapshots()
{
    for (auto &snap : snapshots)
    {
        cout << "\nSnapshot @ " << snap.timestamp << endl;

        cout << "BIDS:\n";
        for (auto &b : snap.bidLevels)
            cout << b.first << " -> " << b.second << endl;

        cout << "ASKS:\n";
        for (auto &a : snap.askLevels)
            cout << a.first << " -> " << a.second << endl;
    }
}

// Strategy *strategy = nullptr;
// strategy → points to strat
// Why pointer? Because: polymorphism (base class pointer → derived class object)
// and flexibility (swap strategies easily)

void OrderBook::onEvent(EventType event)
{
    long long ts = orderTimestamp + tradeTimestamp; // unified timeline
    takeSnapshot(ts);

    // Without this, infinite recursion:
    if (strategy && !isStrategyRunning)
    {
        isStrategyRunning = true;  // block re-entry
        strategy->onEvent(*this);  // run strategy
        isStrategyRunning = false; // allow future calls
    }
}

void OrderBook::replayTrades()
{
    cout << "\n--- Replay ---\n";
    for (auto &t : trades)
    {
        cout << "Trade: "
             << t.quantity << " @ " << t.price
             << " | BuyID: " << t.buyId
             << " | SellID: " << t.sellId << endl;
    }
}

OrderBook::~OrderBook()
{
    for (auto &[id, node] : orderMap)
        delete node.order;
}