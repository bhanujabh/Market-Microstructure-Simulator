#pragma once
struct Trade
{
    int buyId{};
    int sellId{};
    double price{};
    int quantity{};
    long long timestamp{};
    int buyOwnerId;
    int sellOwnerId;
    // brackets represent default initialisation
};