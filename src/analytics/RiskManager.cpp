#include "RiskManager.h"
#include <algorithm>

RiskManager::RiskManager(
    int maxPos,
    int maxQty,
    double lossLimit)
    : maxPosition(maxPos),
      maxOrderQty(maxQty),
      maxLoss(lossLimit)
{
}

bool RiskManager::allowBuy(int position, int qty) const
{
    return (position + qty <= maxPosition &&
            qty <= maxOrderQty);
}

bool RiskManager::allowSell(int position, int qty) const
{
    return (position - qty >= -maxPosition &&
            qty <= maxOrderQty);
}

bool RiskManager::killSwitch(double pnl) const
{
    return pnl <= maxLoss;
}

int RiskManager::reduceLongQty(int position) const
{
    if (position <= maxPosition)
        return 0;

    return position - maxPosition;
}

int RiskManager::reduceShortQty(int position) const
{
    if (position >= -maxPosition)
        return 0;

    return (-maxPosition - position);
}