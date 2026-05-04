#include "Strategy.h"
#include "../engine/OrderBook.h"

double Strategy::getMidPrice(OrderBook &ob)
{
    if (!ob.bids.empty() && !ob.asks.empty())
    {
        double bestBid = ob.bids.begin()->first;
        double bestAsk = ob.asks.begin()->first;
        return (bestBid + bestAsk) / 2.0;
    }
    return 0.0;
}

double Strategy::currentPnL(OrderBook &ob)
{
    return Metrics::getPnL(
        cash,
        position,
        getMidPrice(ob),
        inventoryPenalty);
}