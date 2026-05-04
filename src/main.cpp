#include <iostream>
#include <memory>

#include "strategies/Strategy.h"

#include "simulation/DataGenerator.h"
#include "simulation/Backtester.h"

#include "strategies/SpreadStrategy.h"
#include "strategies/ImbalanceStrategy.h"
#include "strategies/MomentumStrategy.h"
#include "strategies/MarketMakingStrategy.h"

#include "engine/OrderBook.h"
#include "analytics/ReportGenerator.h"

using namespace std;

int main()
{
    // ---------------------------------
    // Step 1: Generate market data CSV
    // ---------------------------------
    DataGenerator::generateRandomSession(
        "data/random_day.csv",
        100,
        100);

    // Other options:
    // generateUptrendSession(...)
    // generateDowntrendSession(...)
    // generateVolatileSession(...)

    // ---------------------------------
    // Step 2: Choose strategy
    // ---------------------------------
    SpreadStrategy spread;
    MarketMakingStrategy mm;
    MomentumStrategy momentum;
    ImbalanceStrategy imbalance;

    // try:
    // ImbalanceStrategy strategy;
    // MomentumStrategy strategy;
    // MarketMakingStrategy strategy;

    // ---------------------------------
    // Step 3: Run backtest
    // ---------------------------------
    Backtester bt;

    auto ob1 = bt.run(
        "data/random_day.csv",
        spread,
        false, // print trades
        false  // export csv/json
    );
    auto ob2 = bt.run(
        "data/random_day.csv",
        mm,
        false, // print trades
        false  // export csv/json
    );
    auto ob3 = bt.run(
        "data/random_day.csv",
        momentum,
        false, // print trades
        false  // export csv/json
    );
    auto ob4 = bt.run(
        "data/random_day.csv",
        imbalance,
        false, // print trades
        false  // export csv/json
    );

    vector<pair<string, Strategy *>> strategies = {
        {"Spread", &spread},
        {"MarketMaking", &mm},
        {"Momentum", &momentum},
        {"Imbalance", &imbalance}};

    vector<unique_ptr<OrderBook>> books;

    books.push_back(std::move(ob1));
    books.push_back(std::move(ob2));
    books.push_back(std::move(ob3));
    books.push_back(std::move(ob4));
    ReportGenerator::exportAllStrategies(strategies, books, "backtest_results.json");

    cout << "\nDone.\n";

    return 0;
}