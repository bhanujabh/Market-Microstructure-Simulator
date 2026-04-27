#pragma once

#include <string>
#include "../strategies/Strategy.h"
#include "../engine/OrderBook.h"

class ReportGenerator
{
public:
    static void print(Strategy &strategy, OrderBook &ob);

    static void exportCSV(
        Strategy &strategy,
        OrderBook &ob,
        const std::string &filename);

    static void exportJson(
        Strategy &strategy,
        OrderBook &ob,
        const std::string &filename);
};