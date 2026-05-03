#pragma once

struct ExecutionStats
{
    double totalValue = 0;
    int filledQty = 0;
    int intendedQty = 0;      // original quantity
    double expectedPrice = 0; // this should be double
};