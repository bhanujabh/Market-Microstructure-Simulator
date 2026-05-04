#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "MarketReplay.h"
#include "../core/Types.h"
#include "../core/Order.h"

using namespace std;

void MarketReplay::replayCSV(const std::string &filename, OrderBook &ob)
{
    ifstream file(filename);

    if (!file.is_open())
    {
        cout << "Failed to open file: " << filename << endl;
        return;
    }

    string line;

    // Skip header row
    getline(file, line);
    int replayOwnerId = 10000000;
    while (getline(file, line))
    {
        if (line.empty())
            continue;

        stringstream ss(line);

        string timestampStr;
        string idStr;
        string typeStr;
        string sideStr;
        string priceStr;
        string qtyStr;

        getline(ss, timestampStr, ',');
        getline(ss, idStr, ',');
        getline(ss, typeStr, ',');
        getline(ss, sideStr, ',');
        getline(ss, priceStr, ',');
        getline(ss, qtyStr, ',');

        int id = stoi(idStr);
        int price = stoi(priceStr);
        int qty = stoi(qtyStr);

        OrderType type;
        Side side;

        // Parse order type
        if (typeStr == "MARKET")
            type = OrderType::MARKET;
        else
            type = OrderType::LIMIT;

        // Parse side
        if (sideStr == "BUY")
            side = Side::BUY;
        else
            side = Side::SELL;

        Order order{}; // Zero-initialize ALL fields first
        order.id = id;
        order.side = side;
        order.type = type;
        order.price = static_cast<double>(price);
        order.quantity = qty;
        order.timestamp = 0;             // Explicitly set
        order.ownerId = replayOwnerId++; // Explicitly set - no ambiguity

        ob.addOrder(order);
    }

    file.close();

    cout << "Replay complete.\n";
}