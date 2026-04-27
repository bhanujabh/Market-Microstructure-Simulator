#pragma once
#include <string>
#include "../engine/OrderBook.h"

class MarketReplay
{
public:
    void replayCSV(const std::string &filename, OrderBook &ob);
};