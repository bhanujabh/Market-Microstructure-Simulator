#include <iostream>
#include <algorithm>
#include <vector>
#include "MatchingEngine.h"
#include "OrderBook.h"
#include "../core/Types.h"
#include "../strategies/Strategy.h"

using namespace std;

void matchOrders(OrderBook &ob)
{
    auto validateOrderPtr = [](const Order *ptr, const char *context)
    {
        if (!ptr)
        {
            std::cerr << "❌ [" << context << "] nullptr Order pointer\n";
            std::abort();
        }
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        // Reject pointers in first 4KB (null page) and suspiciously low values
        if (addr < 0x1000 || addr > 0x7FFFFFFFFFFF)
        {
            std::cerr << "❌ [" << context << "] CORRUPTED pointer: 0x"
                      << std::hex << addr << std::dec
                      << " (Order ID: " << ptr->id << ")\n";
            std::abort(); // Stop immediately to preserve debug state
        }
#ifdef DEBUG
// Optional: add a magic value check if you add one to Order struct
#endif
    };
    vector<Trade> localTrades;

    while (!ob.bids.empty() && !ob.asks.empty())
    {
        auto bestBid = ob.bids.begin();
        auto bestAsk = ob.asks.begin();

        if (bestBid->second.empty())
        {
            ob.bids.erase(bestBid);
            continue;
        }
        if (bestAsk->second.empty())
        {
            ob.asks.erase(bestAsk);
            continue;
        }

        if (bestBid->first < bestAsk->first)
            break;

        auto &buyList = bestBid->second;
        auto &sellList = bestAsk->second;

        if (!buyList.front())
        {
            buyList.pop_front();
            if (buyList.empty())
                ob.bids.erase(bestBid);
            continue;
        }
        if (!sellList.front())
        {
            sellList.pop_front();
            if (sellList.empty())
                ob.asks.erase(bestAsk);
            continue;
        }

        // Snapshot all fields before any mutation
        Order *buyOrder = buyList.front().get();
        Order *sellOrder = sellList.front().get();

        const int buyOrderId = buyOrder->id;
        const int buyOwnerId = buyOrder->ownerId;
        const int sellOrderId = sellOrder->id;
        const int sellOwnerId = sellOrder->ownerId;
        const double tradePrice = bestAsk->first;
        const int tradedQty = min(buyOrder->quantity, sellOrder->quantity);

        // Self-trade prevention — snapshot already taken, safe to mutate
        if (buyOwnerId == sellOwnerId)
        {
            buyList.pop_front(); // buyOrder dangling after this
            ob.orderMap.erase(buyOrderId);
            if (buyList.empty())
                ob.bids.erase(bestBid);
            continue;
        }

        localTrades.push_back({buyOrderId, sellOrderId, tradePrice,
                               tradedQty, ob.tradeTimestamp++,
                               buyOwnerId, sellOwnerId});

        cout << "Trade executed: " << tradedQty << " @ " << tradePrice << "\n";

        // Mutate quantities — raw pointers still valid here
        buyOrder->quantity -= tradedQty;
        sellOrder->quantity -= tradedQty;

        if (buyOrder->quantity == 0)
        {
            buyList.pop_front(); // buyOrder dangling after this
            ob.orderMap.erase(buyOrderId);
            if (buyList.empty())
                ob.bids.erase(bestBid);
            // ← buyOrder not touched again
        }

        if (sellOrder->quantity == 0)
        {
            sellList.pop_front(); // sellOrder dangling after this
            ob.orderMap.erase(sellOrderId);
            if (sellList.empty())
                ob.asks.erase(bestAsk);
            // ← sellOrder not touched again
        }
    }

    for (auto &t : localTrades)
    {
        ob.trades.push_back(t);
        if (ob.strategy)
        {
            ob.strategy->onTrade(t, ob); // re-entrant addOrder calls are safely
        } // caught by isProcessing guard in addOrder
    }
}

void executeMarketBuy(OrderBook &ob, Order *marketOrder)
{
    auto validateOrderPtr = [](const Order *ptr, const char *context)
    {
        if (!ptr)
        {
            std::cerr << "❌ [" << context << "] nullptr Order pointer\n";
            std::abort();
        }
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        // Reject pointers in first 4KB (null page) and suspiciously low values
        if (addr < 0x1000 || addr > 0x7FFFFFFFFFFF)
        {
            std::cerr << "❌ [" << context << "] CORRUPTED pointer: 0x"
                      << std::hex << addr << std::dec
                      << " (Order ID: " << ptr->id << ")\n";
            std::abort(); // Stop immediately to preserve debug state
        }
#ifdef DEBUG
// Optional: add a magic value check if you add one to Order struct
#endif
    };
    vector<Trade> localTrades;
    int quantity = marketOrder->quantity;
    while (quantity > 0 && !ob.asks.empty())
    {
        auto bestAsk = ob.asks.begin();
        if (bestAsk->second.empty())
        {
            ob.asks.erase(bestAsk);
            continue;
        }

        // auto &sellList = bestAsk->second;
        // Helper to detect suspicious pointers
        auto isValidOrderPtr = [](const Order *ptr)
        {
            return ptr && (reinterpret_cast<uintptr_t>(ptr) >= 0x1000);
        };

        // Peek and validate BEFORE extracting raw pointer
        if (bestAsk->second.empty() || !bestAsk->second.front())
        {
            if (!bestAsk->second.empty())
                bestAsk->second.pop_front();
            if (bestAsk->second.empty())
                ob.asks.erase(bestAsk);
            continue;
        }
        if (bestAsk->second.front()->ownerId == marketOrder->ownerId)
        {
            auto sellOrderId = bestAsk->second.front()->id;
            bestAsk->second.pop_front(); // unique_ptr destroyed, Order deleted
            ob.orderMap.erase(sellOrderId);
            if (bestAsk->second.empty())
                ob.asks.erase(bestAsk);
            continue;
        }

        // Snapshot all fields we need from the order BEFORE any mutation
        Order *sellOrder = bestAsk->second.front().get();
        const int sellOrderId = sellOrder->id;
        const int sellOwnerId = sellOrder->ownerId;
        const int tradedQty = min(quantity, sellOrder->quantity);
        const double tradePrice = bestAsk->first;
        const int marketOrderId = marketOrder->id;
        const int marketOwnerId = marketOrder->ownerId;

        // Record trade using only snapshots — no raw pointer access after this
        localTrades.push_back({marketOrderId, sellOrderId, tradePrice,
                               tradedQty, ob.tradeTimestamp++,
                               marketOwnerId, sellOwnerId});

        cout << "Trade: MKT BUY " << marketOrderId
             << " vs SELL " << sellOrderId
             << " Qty=" << tradedQty
             << " @ " << tradePrice << endl;

        quantity -= tradedQty;
        // Mutate remaining qty via raw pointer — still valid here
        sellOrder->quantity -= tradedQty;

        if (sellOrder->quantity == 0)
        {
            bestAsk->second.pop_front();
            ob.orderMap.erase(sellOrderId);

            if (bestAsk->second.empty())
            {
                ob.asks.erase(bestAsk);
            }
            // continue;
        }

        // if (sellList.empty())
        //     ob.asks.erase(bestAsk);
    }
    for (auto &t : localTrades)
    {
        ob.trades.push_back(t);
        if (ob.strategy)
        {
            ob.strategy->onTrade(t, ob); // re-entrant addOrder calls are safely
        } // caught by isProcessing guard in addOrder
    }
    if (quantity > 0)
    {
        cout << "Market Buy unfilled qty: " << quantity << endl;
    }
}

void executeMarketSell(OrderBook &ob, Order *marketOrder)
{
    auto validateOrderPtr = [](const Order *ptr, const char *context)
    {
        if (!ptr)
        {
            std::cerr << "❌ [" << context << "] nullptr Order pointer\n";
            std::abort();
        }
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        // Reject pointers in first 4KB (null page) and suspiciously low values
        if (addr < 0x1000 || addr > 0x7FFFFFFFFFFF)
        {
            std::cerr << "❌ [" << context << "] CORRUPTED pointer: 0x"
                      << std::hex << addr << std::dec
                      << " (Order ID: " << ptr->id << ")\n";
            std::abort(); // Stop immediately to preserve debug state
        }
#ifdef DEBUG
// Optional: add a magic value check if you add one to Order struct
#endif
    };
    vector<Trade> localTrades;
    int quantity = marketOrder->quantity;
    const int marketOrderId = marketOrder->id;
    const int marketOwnerId = marketOrder->ownerId;

    // Helper: reject suspicious low-value pointers (catches 0x1, 0x2, etc.)
    auto isValidOrderPtr = [](const Order *ptr)
    {
        return ptr && (reinterpret_cast<uintptr_t>(ptr) >= 0x1000);
    };

    while (quantity > 0 && !ob.bids.empty())
    {
        auto bestBid = ob.bids.begin();

        if (bestBid->second.empty())
        {
            ob.bids.erase(bestBid);
            continue;
        }

        // auto &buyList = bestBid->second;
        // Robust validation BEFORE extracting raw pointer
        if (bestBid->second.empty() || !isValidOrderPtr(bestBid->second.front().get()))
        {
            if (!bestBid->second.empty())
                bestBid->second.pop_front();
            if (bestBid->second.empty())
                ob.bids.erase(bestBid);
            continue;
        }

        // Self-trade check BEFORE extracting raw pointer (prevents dangling)
        if (bestBid->second.front()->ownerId == marketOwnerId)
        {
            auto buyOrderId = bestBid->second.front()->id;
            bestBid->second.pop_front();
            ob.orderMap.erase(buyOrderId);
            if (bestBid->second.empty())
                ob.bids.erase(bestBid);
            continue;
        }

        // Snapshot all fields before any mutation
        Order *buyOrder = bestBid->second.front().get();

        const int buyOrderId = buyOrder->id;
        const int buyOwnerId = buyOrder->ownerId;
        const double tradePrice = bestBid->first;
        const int tradedQty = min(quantity, buyOrder->quantity);

        if (buyOwnerId == marketOwnerId)
        {
            bestBid->second.pop_front(); // buyOrder dangling after this
            ob.orderMap.erase(buyOrderId);
            if (bestBid->second.empty())
                ob.bids.erase(bestBid);
            continue;
        }

        localTrades.push_back({buyOrderId, marketOrderId, tradePrice,
                               tradedQty, ob.tradeTimestamp++,
                               buyOwnerId, marketOrder->ownerId});

        cout << "Trade: MKT SELL " << marketOrderId
             << " vs BUY " << buyOrderId
             << " Qty=" << tradedQty
             << " @ " << tradePrice << "\n";

        quantity -= tradedQty;

        // Mutate remaining qty — raw pointer still valid here
        buyOrder->quantity -= tradedQty;

        if (buyOrder->quantity == 0)
        {
            bestBid->second.pop_front(); // buyOrder dangling after this
            ob.orderMap.erase(buyOrderId);
            if (bestBid->second.empty())
                ob.bids.erase(bestBid);
            // ← buyOrder not touched again
        }
    }

    for (auto &t : localTrades)
    {
        ob.trades.push_back(t);
        if (ob.strategy)
        {
            ob.strategy->onTrade(t, ob); // re-entrant addOrder calls are safely
        } // caught by isProcessing guard in addOrder
    }
    if (quantity > 0)
        cout << "Market Sell unfilled qty: " << quantity << "\n";
}