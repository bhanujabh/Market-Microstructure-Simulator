#pragma once

#include <string>
#include <utility>
#include <vector>
#include <memory>
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

    static void exportAllStrategies(
        std::vector<std::pair<std::string, Strategy *>> strategies,
        std::vector<std::unique_ptr<OrderBook>> &books,
        const std::string &filename);
};