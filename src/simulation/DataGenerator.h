#pragma once
#include <string>

class DataGenerator
{
public:
    // Random mixed market session
    static void generateRandomSession(
        const std::string &filename,
        int numOrders = 1000,
        int startPrice = 100);

    // Trending upward market
    static void generateUptrendSession(
        const std::string &filename,
        int numOrders = 1000,
        int startPrice = 100);

    // Trending downward market
    static void generateDowntrendSession(
        const std::string &filename,
        int numOrders = 1000,
        int startPrice = 100);

    // High volatility market
    static void generateVolatileSession(
        const std::string &filename,
        int numOrders = 1000,
        int startPrice = 100);
};