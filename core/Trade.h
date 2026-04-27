#pragma once

struct Trade
{
    int buyId{};
    int sellId{};
    int price{};
    int quantity{};
    long long timestamp{};
};