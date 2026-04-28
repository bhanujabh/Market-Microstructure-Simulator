#include <iostream>
#include <algorithm>
#include "MatchingEngine.h"
#include "OrderBook.h"
#include "../core/Types.h"
#include "../strategies/Strategy.h"

using namespace std;

void matchOrders(OrderBook &ob)
{
    while (!ob.bids.empty() && !ob.asks.empty())
    {
        auto bestBid = ob.bids.begin();
        auto bestAsk = ob.asks.begin();

        if (bestBid->first < bestAsk->first)
            break;

        Order *buyOrder = bestBid->second.front().get();
        Order *sellOrder = bestAsk->second.front().get();

        int tradedQty = min(buyOrder->quantity, sellOrder->quantity);

        ob.trades.push_back({buyOrder->id, sellOrder->id, bestAsk->first, tradedQty, ob.tradeTimestamp++});

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

        if (buyOrder->quantity == 0)
        {
            int id = buyOrder->id;       // save BEFORE deletion
            bestBid->second.pop_front(); // deletes the object
            ob.orderMap.erase(id);       // remove tracking
        }

        if (sellOrder->quantity == 0)
        {
            int id = sellOrder->id;
            bestAsk->second.pop_front();
            ob.orderMap.erase(id);
        }

        if (bestBid->second.empty())
            ob.bids.erase(bestBid);

        if (bestAsk->second.empty())
            ob.asks.erase(bestAsk);

        ob.onEvent(EventType::TRADE_EXEC);

        if (ob.strategy && !ob.isStrategyRunning)
        {
            ob.isStrategyRunning = true;
            ob.strategy->onTrade(ob.trades.back(), ob);
            ob.isStrategyRunning = false;
        }
    }
}

void executeMarketBuy(OrderBook &ob, Order *marketOrder)
{
    int quantity = marketOrder->quantity;
    while (quantity > 0 && !ob.asks.empty())
    {
        auto bestAsk = ob.asks.begin();

        Order *sellOrder = bestAsk->second.front().get();

        int tradedQty = min(quantity, sellOrder->quantity);

        ob.trades.push_back({marketOrder->id, sellOrder->id, bestAsk->first, tradedQty, ob.tradeTimestamp++});

        cout << "Trade: MKT BUY " << marketOrder->id
             << " vs SELL " << sellOrder->id
             << " Qty=" << tradedQty
             << " @ " << bestAsk->first << endl;

        quantity -= tradedQty;
        sellOrder->quantity -= tradedQty;

        if (sellOrder->quantity == 0)
        {
            int id = sellOrder->id;
            bestAsk->second.pop_front();

            ob.orderMap.erase(id);
        }

        if (bestAsk->second.empty())
            ob.asks.erase(bestAsk);

        ob.onEvent(EventType::TRADE_EXEC);

        if (ob.strategy && !ob.isStrategyRunning)
        {
            ob.isStrategyRunning = true;
            ob.strategy->onTrade(ob.trades.back(), ob);
            ob.isStrategyRunning = false;
        }
    }

    if (quantity > 0)
    {
        cout << "Market Buy unfilled qty: " << quantity << endl;
    }
}

void executeMarketSell(OrderBook &ob, Order *marketOrder)
{
    int quantity = marketOrder->quantity;
    while (quantity > 0 && !ob.bids.empty())
    {
        auto bestBid = ob.bids.begin();

        Order *buyOrder = bestBid->second.front().get();

        int tradedQty = min(quantity, buyOrder->quantity);

        ob.trades.push_back({buyOrder->id, marketOrder->id, bestBid->first, tradedQty, ob.tradeTimestamp++});

        cout << "Trade: MKT SELL " << marketOrder->id
             << " vs BUY " << buyOrder->id
             << " Qty=" << tradedQty
             << " @ " << bestBid->first << endl;

        quantity -= tradedQty;
        buyOrder->quantity -= tradedQty;

        if (buyOrder->quantity == 0)
        {
            int id = buyOrder->id;
            bestBid->second.pop_front();
            ob.orderMap.erase(id);
        }

        if (bestBid->second.empty())
            ob.bids.erase(bestBid);

        ob.onEvent(EventType::TRADE_EXEC);

        if (ob.strategy && !ob.isStrategyRunning)
        {
            ob.isStrategyRunning = true;
            ob.strategy->onTrade(ob.trades.back(), ob);
            ob.isStrategyRunning = false;
        }
    }

    if (quantity > 0)
    {
        cout << "Market Sell unfilled qty: " << quantity << endl;
    }
}
