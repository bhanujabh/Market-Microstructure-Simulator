#pragma once

class RiskManager
{
private:
    int maxPosition;
    int maxOrderQty;
    double maxLoss;

public:
    RiskManager(
        int maxPos = 5, // risk limit
        int maxQty = 10,
        double lossLimit = -1000.0);

    bool allowBuy(int position, int qty = 1) const;

    bool allowSell(int position, int qty = 1) const;

    bool killSwitch(double pnl) const;

    int reduceLongQty(int position) const;

    int reduceShortQty(int position) const;
};