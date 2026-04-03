#include <iostream>
#include <map>
#include <queue>
using namespace std;

enum OrderType { MARKET, LIMIT };
enum Side { BUY, SELL };

struct Order {
    int id;
    Side side;
    OrderType type;
    int price;
    int quantity;
};

class OrderBook; // forward declaration

void matchOrders(OrderBook &ob);
void executeMarketBuy(OrderBook &ob, int quantity);
void executeMarketSell(OrderBook &ob, int quantity);

class OrderBook {
public:
    map<int, queue<Order>, greater<int>> bids;
    map<int, queue<Order>> asks;

    void addOrder(Order order) {
        if (order.type == MARKET) {
            if (order.side == BUY)
                executeMarketBuy(*this, order.quantity);
            else
                executeMarketSell(*this, order.quantity);
            return;
        }
        if (order.side == BUY) {
            bids[order.price].push(order);
        } else {
            asks[order.price].push(order);
        }
        matchOrders(*this);
    }
};

void matchOrders(OrderBook &ob) {
    while (!ob.bids.empty() && !ob.asks.empty()) {
        auto bestBid = ob.bids.begin();
        auto bestAsk = ob.asks.begin();

        if (bestBid->first < bestAsk->first) break;

        Order &buyOrder = bestBid->second.front();
        Order &sellOrder = bestAsk->second.front();

        int tradedQty = min(buyOrder.quantity, sellOrder.quantity);

        cout << "Trade executed: "
             << tradedQty << " @ " << bestAsk->first << endl;

        buyOrder.quantity -= tradedQty;
        sellOrder.quantity -= tradedQty;

        if (buyOrder.quantity == 0)
            cout << "Buy Order FULLY filled\n";
        else
            cout << "Buy Order PARTIALLY filled\n";

        if (sellOrder.quantity == 0)
            cout << "Sell Order FULLY filled\n";
        else
            cout << "Sell Order PARTIALLY filled\n";

        if (buyOrder.quantity == 0)
            bestBid->second.pop();

        if (sellOrder.quantity == 0)
            bestAsk->second.pop();

        if (bestBid->second.empty())
            ob.bids.erase(bestBid);

        if (bestAsk->second.empty())
            ob.asks.erase(bestAsk);
    }
}

void executeMarketBuy(OrderBook &ob, int quantity) {
    while (quantity > 0 && !ob.asks.empty()) {
        auto bestAsk = ob.asks.begin();

        Order &sellOrder = bestAsk->second.front();

        int tradedQty = min(quantity, sellOrder.quantity);

        cout << "Market Buy executed: "
             << tradedQty << " @ " << bestAsk->first << endl;

        quantity -= tradedQty;
        sellOrder.quantity -= tradedQty;

        if (sellOrder.quantity == 0)
            bestAsk->second.pop();

        if (bestAsk->second.empty())
            ob.asks.erase(bestAsk);
    }

    if (quantity > 0) {
        cout << "Market Buy unfilled qty: " << quantity << endl;
    }
}

void executeMarketSell(OrderBook &ob, int quantity) {
    while (quantity > 0 && !ob.bids.empty()) {
        auto bestBid = ob.bids.begin();

        Order &buyOrder = bestBid->second.front();

        int tradedQty = min(quantity, buyOrder.quantity);

        cout << "Market Sell executed: "
             << tradedQty << " @ " << bestBid->first << endl;

        quantity -= tradedQty;
        buyOrder.quantity -= tradedQty;

        if (buyOrder.quantity == 0)
            bestBid->second.pop();

        if (bestBid->second.empty())
            ob.bids.erase(bestBid);
    }

    if (quantity > 0) {
        cout << "Market Sell unfilled qty: " << quantity << endl;
    }
}

int main() {
    OrderBook ob;

    ob.addOrder({1, BUY, LIMIT, 100, 10});
    ob.addOrder({2, SELL, LIMIT, 99, 5});
    ob.addOrder({2, BUY, MARKET, 99, 5});

    return 0;
}