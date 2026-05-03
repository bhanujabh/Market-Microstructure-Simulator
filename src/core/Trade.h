#pragma once
struct Trade
{
    int buyId{};
    int sellId{};
    double price{};
    int quantity{};
    long long timestamp{};
    // brackets represent default initialisation
};