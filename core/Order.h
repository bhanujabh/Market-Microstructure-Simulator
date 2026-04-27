#pragma once
#include "Types.h"

struct Order
{
    int id{};
    Side side{};
    OrderType type{};
    int price{};
    int quantity{};
    long long timestamp{};
    // default initialization {} -> prevents garbage values
};