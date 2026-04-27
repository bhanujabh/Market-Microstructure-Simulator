#include "DataGenerator.h"

#include <fstream>
#include <iostream>
#include <random>
#include <algorithm>

using namespace std;

namespace
{
    mt19937 rng(random_device{}());

    int clampPrice(int price)
    {
        return max(1, price);
    }

    string randomSide()
    {
        uniform_int_distribution<int> dist(0, 1);
        return dist(rng) == 0 ? "BUY" : "SELL";
    }

    string randomType(int marketOrderChancePercent = 20)
    {
        uniform_int_distribution<int> dist(1, 100);
        return dist(rng) <= marketOrderChancePercent
                   ? "MARKET"
                   : "LIMIT";
    }

    int randomQty()
    {
        uniform_int_distribution<int> dist(1, 10);
        return dist(rng);
    }

    int randomStep(int low = -2, int high = 2)
    {
        uniform_int_distribution<int> dist(low, high);
        return dist(rng);
    }

    void writeHeader(ofstream &file)
    {
        file << "timestamp,id,type,side,price,qty\n";
    }

    void writeRow(ofstream &file,
                  int timestamp,
                  int id,
                  const string &type,
                  const string &side,
                  int price,
                  int qty)
    {
        file << timestamp << ","
             << id << ","
             << type << ","
             << side << ","
             << price << ","
             << qty << "\n";
    }
}

// --------------------------------------
// Random mixed market
// --------------------------------------
void DataGenerator::generateRandomSession(
    const string &filename,
    int numOrders,
    int startPrice)
{
    ofstream file(filename);

    if (!file.is_open())
    {
        cout << "Failed to create file: "
             << filename << endl;
        return;
    }

    writeHeader(file);

    int price = startPrice;

    for (int i = 1; i <= numOrders; i++)
    {
        price = clampPrice(price + randomStep());

        string type = randomType();
        string side = randomSide();

        int rowPrice =
            (type == "MARKET") ? 0 : price;

        writeRow(
            file,
            i,
            1000 + i,
            type,
            side,
            rowPrice,
            randomQty());
    }

    file.close();

    cout << "Generated random session: "
         << filename << endl;
}

// --------------------------------------
// Uptrend market
// --------------------------------------
void DataGenerator::generateUptrendSession(
    const string &filename,
    int numOrders,
    int startPrice)
{
    ofstream file(filename);

    if (!file.is_open())
    {
        cout << "Failed to create file: "
             << filename << endl;
        return;
    }

    writeHeader(file);

    int price = startPrice;

    for (int i = 1; i <= numOrders; i++)
    {
        price += max(0, randomStep(-1, 3));
        price = clampPrice(price);

        string type = randomType();
        string side = randomSide();

        int rowPrice =
            (type == "MARKET") ? 0 : price;

        writeRow(
            file,
            i,
            2000 + i,
            type,
            side,
            rowPrice,
            randomQty());
    }

    file.close();

    cout << "Generated uptrend session: "
         << filename << endl;
}

// --------------------------------------
// Downtrend market
// --------------------------------------
void DataGenerator::generateDowntrendSession(
    const string &filename,
    int numOrders,
    int startPrice)
{
    ofstream file(filename);

    if (!file.is_open())
    {
        cout << "Failed to create file: "
             << filename << endl;
        return;
    }

    writeHeader(file);

    int price = startPrice;

    for (int i = 1; i <= numOrders; i++)
    {
        price -= max(0, randomStep(-1, 3));
        price = clampPrice(price);

        string type = randomType();
        string side = randomSide();

        int rowPrice =
            (type == "MARKET") ? 0 : price;

        writeRow(
            file,
            i,
            3000 + i,
            type,
            side,
            rowPrice,
            randomQty());
    }

    file.close();

    cout << "Generated downtrend session: "
         << filename << endl;
}

// --------------------------------------
// Volatile market
// --------------------------------------
void DataGenerator::generateVolatileSession(
    const string &filename,
    int numOrders,
    int startPrice)
{
    ofstream file(filename);

    if (!file.is_open())
    {
        cout << "Failed to create file: "
             << filename << endl;
        return;
    }

    writeHeader(file);

    int price = startPrice;

    for (int i = 1; i <= numOrders; i++)
    {
        price += randomStep(-8, 8);
        price = clampPrice(price);

        string type = randomType(35);
        string side = randomSide();

        int rowPrice =
            (type == "MARKET") ? 0 : price;

        writeRow(
            file,
            i,
            4000 + i,
            type,
            side,
            rowPrice,
            randomQty());
    }

    file.close();

    cout << "Generated volatile session: "
         << filename << endl;
}