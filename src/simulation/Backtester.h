#pragma once

#include <string>
#include "../engine/OrderBook.h"
#include "../strategies/Strategy.h"

class Backtester
{
public:
    void run(
        const std::string &inputFile,
        Strategy &strategy,
        bool printTrades = true,
        bool exportReports = true);
};