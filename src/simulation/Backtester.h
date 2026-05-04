#pragma once

#include <string>
#include <memory>
#include "../engine/OrderBook.h"
#include "../strategies/Strategy.h"

class Backtester
{
public:
    std::unique_ptr<OrderBook> run(
        const std::string &inputFile,
        Strategy &strategy,
        bool printTrades = true,
        bool exportReports = true);
};