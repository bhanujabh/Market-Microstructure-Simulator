#include <iostream>
#include "OrderBook.h"
#include "MatchingEngine.h"
#include "../strategies/Strategy.h"
#include <memory>

using namespace std;

void OrderBook::addOrder(const Order &order)
{
    // [FIX] Check duplicate before queuing, not just before inserting
    if (orderMap.find(order.id) != orderMap.end())
    {
        cout << "ERROR: Duplicate Order ID " << order.id << endl;
        return;
    }
    // [CHANGE 1] Guard against re-entrant calls from strategy callbacks.
    // Instead of crashing or double-matching, queue the order and return.
    // The outer call's drain loop will process it after the book settles.
    if (isProcessing)
    {
        pendingOrders.push_back(order);
        return;
    }
    auto newOrder = std::make_unique<Order>(order); // allocate
    newOrder->timestamp = orderTimestamp++;
    Order *rawPtr = newOrder.get();

    if (orderMap.find(order.id) != orderMap.end())
    {
        cout << "ERROR: Duplicate Order ID " << order.id << endl;
        return;
    }

    // [CHANGE 2] Set isProcessing before any matching begins so any
    // re-entrant addOrder call (from strategy->onTrade) hits the guard above.
    isProcessing = true;

    _processOrder(order);

    // if (order.type == OrderType::MARKET)
    // {
    //     cout << "Market Order Received: ID=" << newOrder->id << " Time=" << newOrder->timestamp << endl;
    //     if (order.side == Side::BUY)
    //         executeMarketBuy(*this, rawPtr);
    //     else
    //         executeMarketSell(*this, rawPtr);

    //     onEvent(EventType::ORDER_ADD);
    //     return;
    // }

    // if (order.side == Side::BUY)
    // {
    //     auto &q = bids[order.price];
    //     q.push_back(std::move(newOrder));
    //     auto it = prev(q.end());
    //     orderMap.emplace(order.id, OrderNode(rawPtr, it));
    // }
    // else
    // {
    //     auto &q = asks[order.price];
    //     q.push_back(std::move(newOrder));
    //     auto it = prev(q.end());
    //     orderMap.emplace(order.id, OrderNode(rawPtr, it));
    // }

    // cout << "Order Added: ID=" << order.id << " Time=" << rawPtr->timestamp << endl;

    // matchOrders(*this);
    // onEvent(EventType::ORDER_ADD);

    // [CHANGE 3] Drain loop — process any orders the strategy queued during
    // onTrade callbacks. Each iteration may produce more trades → more queued
    // orders, so we loop until the queue is fully empty.
    // while (!pendingOrders.empty())
    // {
    //     // Snapshot and clear atomically so new entries during this loop
    //     // go into pendingOrders and are caught by the next iteration.
    //     vector<Order> batch = std::move(pendingOrders);
    //     pendingOrders.clear();

    //     for (auto &pendingOrder : batch)
    //     {
    //         auto pendingNew = std::make_unique<Order>(pendingOrder);
    //         pendingNew->timestamp = orderTimestamp++;
    //         Order *pendingRaw = pendingNew.get();

    //         if (orderMap.find(pendingOrder.id) != orderMap.end())
    //         {
    //             cout << "ERROR: Duplicate Order ID " << pendingOrder.id << endl;
    //             continue;
    //         }

    //         if (pendingOrder.type == OrderType::MARKET)
    //         {
    //             cout << "Market Order Received: ID=" << pendingNew->id << " Time=" << pendingNew->timestamp << endl;
    //             if (pendingOrder.side == Side::BUY)
    //                 executeMarketBuy(*this, pendingRaw);
    //             else
    //                 executeMarketSell(*this, pendingRaw);
    //         }
    //         else
    //         {
    //             if (pendingOrder.side == Side::BUY)
    //             {
    //                 auto &q = bids[pendingOrder.price];
    //                 q.push_back(std::move(pendingNew));
    //                 auto it = prev(q.end());
    //                 orderMap.emplace(pendingOrder.id, OrderNode(pendingRaw, it));
    //             }
    //             else
    //             {
    //                 auto &q = asks[pendingOrder.price];
    //                 q.push_back(std::move(pendingNew));
    //                 auto it = prev(q.end());
    //                 orderMap.emplace(pendingOrder.id, OrderNode(pendingRaw, it));
    //             }

    //             cout << "Order Added: ID=" << pendingOrder.id << " Time=" << pendingRaw->timestamp << endl;
    //             matchOrders(*this);
    //         }
    //     }
    // }

    while (!pendingOrders.empty())
    {
        vector<Order> batch = std::move(pendingOrders);
        pendingOrders.clear();
        for (auto &pending : batch)
            _processOrder(pending);
    }

    onEvent(EventType::ORDER_ADD);
    // [CHANGE 4] Only release the lock after the queue is fully drained,
    // so no re-entrant call can sneak in during the drain loop itself.
    isProcessing = false;
}

// [CHANGE] Extract the core insert+match logic into a helper
// so both the main path and drain loop share it without duplicating onEvent
void OrderBook::_processOrder(const Order &order)
{
    auto newOrder = std::make_unique<Order>(order);
    newOrder->timestamp = orderTimestamp++;
    Order *rawPtr = newOrder.get();

    if (orderMap.find(order.id) != orderMap.end())
    {
        cout << "ERROR: Duplicate Order ID " << order.id << endl;
        return;
    }

    if (order.type == OrderType::MARKET)
    {
        cout << "Market Order Received: ID=" << newOrder->id
             << " Time=" << newOrder->timestamp << endl;

        // [FIX] Keep unique_ptr alive for the ENTIRE duration of market
        // execution, including any re-entrant callbacks fired inside.
        // Previously rawPtr could dangle if a callback triggered _processOrder
        // again and the allocator reused the same memory.
        Order *raw = newOrder.get();
        activeMarketOrders.push_back(std::move(newOrder)); // [FIX] transfer ownership to stable storage

        if (order.side == Side::BUY)
            executeMarketBuy(*this, raw);
        else
            executeMarketSell(*this, raw);

        // [FIX] Remove from stable storage only after execution fully completes
        activeMarketOrders.erase(
            std::remove_if(activeMarketOrders.begin(), activeMarketOrders.end(),
                           [raw](const std::unique_ptr<Order> &p)
                           { return p.get() == raw; }),
            activeMarketOrders.end());
        return;
    }
    if (order.side == Side::BUY)
    {
        auto &q = bids[order.price];
        q.push_back(std::move(newOrder));
        orderMap.emplace(order.id, OrderNode(rawPtr, prev(q.end())));
    }
    else
    {
        auto &q = asks[order.price];
        q.push_back(std::move(newOrder));
        orderMap.emplace(order.id, OrderNode(rawPtr, prev(q.end())));
    }

    cout << "Order Added: ID=" << order.id
         << " Time=" << rawPtr->timestamp << endl;
    matchOrders(*this);
}

void OrderBook::cancelOrder(int orderId)
{
    // in the below code strategy can trigger cancels while matching is in progress
    // print "Order not found" (noisy)
    // fetch the node twice (find + at)
    // risk using a stale iterator if the order was already removed elsewhere
    // call onEvent unconditionally
    // if (this->orderMap.find(orderId) == this->orderMap.end())
    // {
    //     cout << "Order not found\n";
    //     return;
    // }

    auto itMap = orderMap.find(orderId);
    if (itMap == orderMap.end())
    {
        return; // silently ignore duplicate/stale cancels
    }
    OrderNode &node = itMap->second;

    Order *order = node.order;

    Side side = order->side;  // save BEFORE erase
    int price = order->price; // save BEFORE erase

    if (side == Side::BUY)
    {
        auto it = bids.find(price);
        if (it != bids.end())
        {
            it->second.erase(node.it); // frees order

            if (it->second.empty())
                bids.erase(it); // erase by iterator
        }
    }
    else
    {
        auto it = asks.find(price);
        if (it != asks.end())
        {
            it->second.erase(node.it);

            if (it->second.empty())
                asks.erase(it);
        }
    }
    orderMap.erase(itMap);

    cout << "Order " << orderId << " cancelled\n";

    if (!isProcessing)
        onEvent(EventType::ORDER_CANCEL);
}

void OrderBook::modifyOrder(int orderId, double newPrice, int newQty)
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

        // move ownership out of current node
        auto movedOrder = std::move(*node.it);

        // remove from current position
        // erase empty node
        level.erase(node.it);

        // update timestamp
        movedOrder->timestamp = orderTimestamp++;

        // move to back of queue
        level.push_back(std::move(movedOrder));

        // save new iterator
        node.it = prev(level.end());

        // refresh raw pointer
        node.order = node.it->get();
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
        for (auto &order : priceLevel.second)
        {
            totalQty += order->quantity;
        }
        snap.bidLevels.push_back({priceLevel.first, totalQty});
    }
    // Aggregate asks
    for (auto &priceLevel : asks)
    {
        int totalQty = 0;
        for (auto &order : priceLevel.second)
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

    // [CHANGE] Use isProcessing instead of isStrategyRunning
    // onEvent is only called after isProcessing = false, so this
    // guard is just safety for cancelOrder/modifyOrder paths
    if (strategy && !isProcessing)
    {
        isProcessing = true; // ← Set BEFORE calling strategy
        strategy->onEvent(*this);

        // Drain any orders the strategy queued
        while (!pendingOrders.empty())
        {
            vector<Order> batch = std::move(pendingOrders);
            pendingOrders.clear();
            for (auto &pending : batch)
                _processOrder(pending);
        }

        isProcessing = false; // ← Release AFTER draining
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