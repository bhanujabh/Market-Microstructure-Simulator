#include <iostream>

#include "Backtester.h"
#include "MarketReplay.h"
#include "../analytics/ReportGenerator.h"

using namespace std;

void Backtester::run(
    const std::string &inputFile,
    Strategy &strategy,
    bool printTrades,
    bool exportReports)
{
    cout << "\n===== BACKTEST START =====\n";

    OrderBook ob;

    // attach strategy
    ob.strategy = &strategy;

    // replay market data
    MarketReplay replay;
    replay.replayCSV(inputFile, ob);

    cout << "\n===== BACKTEST COMPLETE =====\n";

    if (printTrades)
    {
        ob.replayTrades();
    }

    ReportGenerator::print(strategy, ob);

    if (exportReports)
    {
        ReportGenerator::exportCSV(
            strategy,
            ob,
            "backtest_results.csv");

        ReportGenerator::exportJson(
            strategy,
            ob,
            "backtest_results.json");
    }
}