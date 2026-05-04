#include <iostream>
#include <fstream>
#include <vector>
#include <utility>
#include <memory>

#include "ReportGenerator.h"
#include "Metrics.h"
#include "../strategies/Strategy.h"

using namespace std;

void ReportGenerator::print(
    Strategy &strategy,
    OrderBook &ob)
{
    cout << "\n===== REPORT =====\n";

    strategy.printStats(ob);

    cout << "Trades Executed: "
         << ob.trades.size()
         << endl;

    cout << "Snapshots Stored: "
         << ob.snapshots.size()
         << endl;
}

void ReportGenerator::exportCSV(
    Strategy &strategy,
    OrderBook &ob,
    const std::string &filename)
{
    ofstream file(filename);

    if (!file.is_open())
    {
        cout << "Failed to open CSV file\n";
        return;
    }

    file << "trade_no,buyId,sellId,price,qty,timestamp\n";

    for (int i = 0; i < ob.trades.size(); i++)
    {
        auto &t = ob.trades[i];

        file << i + 1 << ","
             << t.buyId << ","
             << t.sellId << ","
             << t.price << ","
             << t.quantity << ","
             << t.timestamp
             << "\n";
    }

    file.close();

    cout << "CSV exported: "
         << filename
         << endl;
}

void ReportGenerator::exportJson(
    Strategy &strategy,
    OrderBook &ob,
    const std::string &filename)
{
    ofstream file(filename);

    if (!file.is_open())
    {
        cout << "Failed to open JSON file\n";
        return;
    }

    double mid = 0;
    double bestBid = 0;
    double bestAsk = 0;

    for (auto it = ob.snapshots.rbegin(); it != ob.snapshots.rend(); ++it)
    {
        if (!it->bidLevels.empty() && !it->askLevels.empty())
        {
            bestBid = it->bidLevels[0].first;
            bestAsk = it->askLevels[0].first;
            break;
        }
    }

    // FIX: START JSON
    file << "{\n";

    // ================================
    // FIX: ADD METRICS BLOCK
    // ================================
    file << "  \"metrics\": {\n";
    file << "    \"bestBid\": " << bestBid << ",\n";
    file << "    \"bestAsk\": " << bestAsk << ",\n";
    file << "    \"spread\": " << (bestAsk - bestBid) << ",\n";
    file << "    \"tradeCount\": " << ob.trades.size() << "\n";
    file << "  },\n";

    // ================================
    // FIX: ADD ORDER BOOK (BIDS)
    // ================================
    file << "  \"bids\": [\n";
    for (auto &p : ob.bids)
    {
        int totalQty = 0;
        for (auto &o : p.second)
            totalQty += o->quantity;

        file << "    {\"price\": " << p.first
             << ", \"qty\": " << totalQty << "},\n";
    }
    file << "  ],\n";

    // ================================
    // FIX: ADD ORDER BOOK (ASKS)
    // ================================
    file << "  \"asks\": [\n";
    for (auto &p : ob.asks)
    {
        int totalQty = 0;
        for (auto &o : p.second)
            totalQty += o->quantity;

        file << "    {\"price\": " << p.first
             << ", \"qty\": " << totalQty << "},\n";
    }
    file << "  ],\n";

    // ================================
    // EXISTING: TRADES
    // ================================
    file << "  \"trades\": [\n";

    for (int i = 0; i < ob.trades.size(); i++)
    {
        auto &t = ob.trades[i];

        file << "    {\n";
        file << "      \"buyId\": " << t.buyId << ",\n";
        file << "      \"sellId\": " << t.sellId << ",\n";
        file << "      \"price\": " << t.price << ",\n";
        file << "      \"quantity\": " << t.quantity << ",\n";
        file << "      \"timestamp\": " << t.timestamp << "\n";
        file << "    }";

        if (i != ob.trades.size() - 1)
            file << ",";

        file << "\n";
    }

    file << "  ],\n";

    // ================================
    // FIX: SNAPSHOTS (FOR HEATMAP)
    // ================================
    file << "  \"snapshots\": [\n";

    for (int i = 0; i < ob.snapshots.size(); i++)
    {
        auto &s = ob.snapshots[i];

        file << "    {\n";
        file << "      \"timestamp\": " << s.timestamp << "\n";
        file << "    }";

        if (i != ob.snapshots.size() - 1)
            file << ",";

        file << "\n";
    }

    file << "  ]\n";

    file << "}\n";

    file << ",\n  \"strategies\": {\n";

    file << "    \"Spread\": {\n";
    file << "      \"pnl\": " << strategy.currentPnL(ob) << ",\n";
    file << "      \"sharpe\": 0,\n";   // placeholder (you can compute later)
    file << "      \"drawdown\": 0,\n"; // placeholder
    file << "      \"trades\": " << ob.trades.size() << ",\n";
    file << "      \"pnlHistory\": [";

    for (size_t i = 0; i < strategy.pnlHistory.size(); i++)
    {
        file << strategy.pnlHistory[i];
        if (i != strategy.pnlHistory.size() - 1)
            file << ",";
    }

    file << "]\n";
    file << "    }\n";

    file << "  }\n";
    file.close();

    cout << "JSON exported: " << filename << endl;
}

void ReportGenerator::exportAllStrategies(
    vector<pair<string, Strategy *>> strategies,
    vector<unique_ptr<OrderBook>> &books,
    const string &filename)
{
    ofstream file(filename);

    if (!file.is_open())
    {
        cout << "Failed to open JSON file\n";
        return;
    }

    OrderBook &ob = *books[0]; // use first for market data

    double bestBid = 0, bestAsk = 0;

    for (auto it = ob.snapshots.rbegin(); it != ob.snapshots.rend(); ++it)
    {
        if (!it->bidLevels.empty() && !it->askLevels.empty())
        {
            bestBid = it->bidLevels[0].first;
            bestAsk = it->askLevels[0].first;
            break;
        }
    }

    file << "{\n";

    // ================= METRICS =================
    file << "  \"metrics\": {\n";
    file << "    \"bestBid\": " << bestBid << ",\n";
    file << "    \"bestAsk\": " << bestAsk << ",\n";
    file << "    \"spread\": " << (bestAsk - bestBid) << ",\n";
    file << "    \"tradeCount\": " << ob.trades.size() << "\n";
    file << "  },\n";

    // ================= ORDER BOOK =================
    file << "  \"bids\": [\n";
    for (auto &p : ob.bids)
    {
        int qty = 0;
        for (auto &o : p.second)
            qty += o->quantity;

        file << "    {\"price\": " << p.first
             << ", \"qty\": " << qty << "},\n";
    }
    file << "  ],\n";

    file << "  \"asks\": [\n";
    for (auto &p : ob.asks)
    {
        int qty = 0;
        for (auto &o : p.second)
            qty += o->quantity;

        file << "    {\"price\": " << p.first
             << ", \"qty\": " << qty << "},\n";
    }
    file << "  ],\n";

    // ================= TRADES =================
    file << "  \"trades\": [\n";
    for (int i = 0; i < ob.trades.size(); i++)
    {
        auto &t = ob.trades[i];

        file << "    {\n";
        file << "      \"price\": " << t.price << ",\n";
        file << "      \"quantity\": " << t.quantity << ",\n";
        file << "      \"buyId\": " << t.buyId << ",\n";
        file << "      \"sellId\": " << t.sellId << "\n";
        file << "    }";

        if (i != ob.trades.size() - 1)
            file << ",";
        file << "\n";
    }
    file << "  ],\n";

    // ================= SNAPSHOTS =================
    file << "  \"snapshots\": [\n";
    for (int i = 0; i < ob.snapshots.size(); i++)
    {
        file << "    {\"timestamp\": " << ob.snapshots[i].timestamp << "}";
        if (i != ob.snapshots.size() - 1)
            file << ",";
        file << "\n";
    }
    file << "  ],\n";

    // ================= STRATEGIES =================
    file << "  \"strategies\": {\n";

    for (int i = 0; i < strategies.size(); i++)
    {
        const string &name = strategies[i].first;
        Strategy *s = strategies[i].second;
        OrderBook &sob = *books[i];

        file << "    \"" << name << "\": {\n";
        file << "      \"pnl\": " << s->currentPnL(sob) << ",\n";
        file << "      \"sharpe\": 0,\n";
        file << "      \"drawdown\": 0,\n";
        file << "      \"trades\": " << sob.trades.size() << ",\n";

        const auto &pnl = s->pnlHistory;

        file << "      \"pnlHistory\": [";
        for (int j = 0; j < pnl.size(); j++)
        {
            file << pnl[j];
            if (j != pnl.size() - 1)
                file << ",";
        }
        file << "]\n";

        file << "    }";

        if (i != strategies.size() - 1)
            file << ",";
        file << "\n";
    }

    file << "  }\n";

    file << "}\n";

    file.close();

    cout << "Multi-strategy JSON exported: " << filename << endl;
}