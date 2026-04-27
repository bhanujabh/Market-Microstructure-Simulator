#include <iostream>
#include <fstream>

#include "ReportGenerator.h"
#include "Metrics.h"

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

    file << "{\n";
    file << "  \"totalTrades\": "
         << ob.trades.size()
         << ",\n";

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

    file << "  ]\n";
    file << "}\n";

    file.close();

    cout << "JSON exported: "
         << filename
         << endl;
}