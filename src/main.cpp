#include <iostream>
#include <fstream>
#include<string>
#include "engine/OrderBook.h"
#include "core/Types.h"
#include "strategies/SpreadStrategy.h"
#include "strategies/ImbalanceStrategy.h"
#include "strategies/MomentumStrategy.h"
#include "strategies/MarketMakingStrategy.h"

int main()
{

    auto runScenario = [](OrderBook &ob)
    {
        ob.addOrder({1, Side::BUY, OrderType::LIMIT, 100, 10});
        ob.addOrder({2, Side::BUY, OrderType::LIMIT, 99, 8});
        ob.addOrder({3, Side::BUY, OrderType::LIMIT, 98, 7});

        ob.addOrder({4, Side::SELL, OrderType::LIMIT, 103, 9});
        ob.addOrder({5, Side::SELL, OrderType::LIMIT, 104, 6});
        ob.addOrder({6, Side::SELL, OrderType::LIMIT, 105, 12});

        ob.addOrder({7, Side::BUY, OrderType::MARKET, 0, 3});
        ob.addOrder({8, Side::SELL, OrderType::MARKET, 0, 2});

        ob.modifyOrder(2, 99, 5);
        ob.cancelOrder(3);

        ob.takeSnapshot(100);

        ob.addOrder({9, Side::BUY, OrderType::LIMIT, 101, 4});
        ob.addOrder({10, Side::SELL, OrderType::LIMIT, 102, 3});

        ob.takeSnapshot(200);

        ob.addOrder({11, Side::BUY, OrderType::MARKET, 0, 5});
        ob.addOrder({12, Side::SELL, OrderType::MARKET, 0, 4});

        ob.takeSnapshot(300);
    };

    // ---------- MASTER BOOK ----------
    OrderBook base;
    runScenario(base);

    // ---------- STRATEGIES ----------
    SpreadStrategy spread;
    ImbalanceStrategy imbalance;
    MomentumStrategy momentum;
    MarketMakingStrategy mm;

    OrderBook ob1;
    ob1.strategy = &spread;
    runScenario(ob1);
    OrderBook ob2;
    ob2.strategy = &imbalance;
    runScenario(ob2);
    OrderBook ob3;
    ob3.strategy = &momentum;
    runScenario(ob3);
    OrderBook ob4;
    ob4.strategy = &mm;
    runScenario(ob4);

    // ---------- EXPORT ----------
    std::ofstream f("output.json");

    f << "{";

    // METRICS
    int bestBid = base.bids.empty() ? 0 : base.bids.begin()->first;
    int bestAsk = base.asks.empty() ? 0 : base.asks.begin()->first;
    int spreadVal = (bestBid && bestAsk) ? bestAsk - bestBid : 0;

    f << "\"metrics\":{";
    f << "\"bestBid\":" << bestBid << ",";
    f << "\"bestAsk\":" << bestAsk << ",";
    f << "\"spread\":" << spreadVal << ",";
    f << "\"tradeCount\":" << base.trades.size();
    f << "},";

    // BIDS
    f << "\"bids\":[";
    bool first = true;
    for (auto &[price, lvl] : base.bids)
    {
        int qty = 0;
        for (auto o : lvl)
            qty += o->quantity;

        if (!first)
            f << ",";
        f << "{\"price\":" << price << ",\"qty\":" << qty << "}";
        first = false;
    }
    f << "],";

    // ASKS
    f << "\"asks\":[";
    first = true;
    for (auto &[price, lvl] : base.asks)
    {
        int qty = 0;
        for (auto o : lvl)
            qty += o->quantity;

        if (!first)
            f << ",";
        f << "{\"price\":" << price << ",\"qty\":" << qty << "}";
        first = false;
    }
    f << "],";

    // TRADES
    f << "\"trades\":[";
    first = true;
    for (auto &t : base.trades)
    {
        if (!first)
            f << ",";
        f << "{";
        f << "\"price\":" << t.price << ",";
        f << "\"qty\":" << t.quantity << ",";
        f << "\"buyId\":" << t.buyId << ",";
        f << "\"sellId\":" << t.sellId << ",";
        f << "\"timestamp\":" << t.timestamp;
        f << "}";
        first = false;
    }
    f << "],";

    // SNAPSHOTS
    f << "\"snapshots\":[";
    first = true;
    for (auto &s : base.snapshots)
    {
        if (!first)
            f << ",";
        f << "{";
        f << "\"timestamp\":" << s.timestamp << ",";
        f << "\"bidDepth\":" << s.bidLevels.size() << ",";
        f << "\"askDepth\":" << s.askLevels.size();
        f << "}";
        first = false;
    }
    f << "],";

    // STRATEGIES
    f << "\"strategies\":{";

    auto writeStrat = [&](std::string name, Strategy &st, OrderBook &ob, bool comma)
    {
        f << "\"" << name << "\":{";
        f << "\"pnl\":" << ((SpreadStrategy &)st).getPnL(ob) << ",";
        f << "\"sharpe\":" << ((SpreadStrategy &)st).getSharpe() << ",";
        f << "\"drawdown\":" << st.maxDrawdown << ",";
        f << "\"trades\":" << st.pnlHistory.size() << ",";
        f << "\"pnlHistory\":[";
        for (int i = 0; i < st.pnlHistory.size(); i++)
        {
            if (i)
                f << ",";
            f << st.pnlHistory[i];
        }
        f << "]";
        f << "}";
        if (comma)
            f << ",";
    };

    writeStrat("Spread", spread, ob1, true);
    writeStrat("Imbalance", imbalance, ob2, true);
    writeStrat("Momentum", momentum, ob3, true);
    writeStrat("MarketMaking", mm, ob4, false);

    f << "}";

    f << "}";

    f.close();

    std::cout << "output.json generated.\n";

    return 0;
}