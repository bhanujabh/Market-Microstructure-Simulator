#include <iostream>

#include "simulation/DataGenerator.h"
#include "simulation/Backtester.h"

#include "strategies/SpreadStrategy.h"
#include "strategies/ImbalanceStrategy.h"
#include "strategies/MomentumStrategy.h"
#include "strategies/MarketMakingStrategy.h"

using namespace std;

int main()
{
    // ---------------------------------
    // Step 1: Generate market data CSV
    // ---------------------------------
    DataGenerator::generateRandomSession(
        "data/random_day.csv",
        50,
        100);

    // Other options:
    // generateUptrendSession(...)
    // generateDowntrendSession(...)
    // generateVolatileSession(...)

    // ---------------------------------
    // Step 2: Choose strategy
    // ---------------------------------
    SpreadStrategy strategy;

    // try:
    // ImbalanceStrategy strategy;
    // MomentumStrategy strategy;
    // MarketMakingStrategy strategy;

    // ---------------------------------
    // Step 3: Run backtest
    // ---------------------------------
    Backtester bt;

    bt.run(
        "data/random_day.csv",
        strategy,
        true, // print trades
        true  // export csv/json
    );

    cout << "\nDone.\n";

    return 0;
}