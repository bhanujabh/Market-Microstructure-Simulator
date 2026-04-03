#include <iostream>
#include <map>
#include <queue>
#include <list>
#include <unordered_map>

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

struct OrderNode {
    Order* order;
    list<Order*>::iterator it;

    OrderNode(Order* o, list<Order*>::iterator iter)
        : order(o), it(iter) {}
};

class OrderBook; // forward declaration

void matchOrders(OrderBook &ob);
void executeMarketBuy(OrderBook &ob, int quantity);
void executeMarketSell(OrderBook &ob, int quantity);

class OrderBook {
public:
    unordered_map<int, OrderNode> orderMap;

    map<int, list<Order*>, greater<int>> bids;
    map<int, list<Order*>> asks;
    bool isCancelled = false;

    void addOrder(Order order) {
        if (order.type == MARKET) {
            if (order.side == BUY)
                executeMarketBuy(*this, order.quantity);
            else
                executeMarketSell(*this, order.quantity);
            return;
        }

        Order* newOrder = new Order(order); // allocate 

        if (order.side == BUY) {
            bids[order.price].push_back(newOrder);
            auto it = prev(bids[order.price].end());
            orderMap[order.id] = OrderNode(newOrder, it);
        } else {
            asks[order.price].push_back(newOrder);
            auto it = prev(asks[order.price].end());
            orderMap[order.id] = OrderNode(newOrder, it);
        }

        matchOrders(*this);
    }

    void cancelOrder(int orderId) {
        if (this->orderMap.find(orderId) == this->orderMap.end()) {
            cout << "Order not found\n";
            return;
        }

        OrderNode node = orderMap[orderId];
        Order* order = node.order;
        
        if (order->side == BUY) {
            auto &orderList = this->bids[order->price];
            orderList.erase(node.it);   

            if (orderList.empty())
                bids.erase(order->price);
        } else {
            auto &orderList = this->asks[order->price];
            orderList.erase(node.it);  

            if (orderList.empty())
                asks.erase(order->price);
        }
        delete order;
        orderMap.erase(orderId);
        
        cout << "Order " << orderId << " cancelled\n";
    }
};

void matchOrders(OrderBook &ob) {
    while (!ob.bids.empty() && !ob.asks.empty()) {
        auto bestBid = ob.bids.begin();
        auto bestAsk = ob.asks.begin();

        if (bestBid->first < bestAsk->first) break;

        Order* buyOrder = bestBid->second.front();
        Order* sellOrder = bestAsk->second.front();

        int tradedQty = min(buyOrder->quantity, sellOrder->quantity);

        cout << "Trade executed: "
             << tradedQty << " @ " << bestAsk->first << endl;

        buyOrder->quantity -= tradedQty;
        sellOrder->quantity -= tradedQty;

        if (buyOrder->quantity == 0)
            cout << "Buy Order FULLY filled\n";
        else
            cout << "Buy Order PARTIALLY filled\n";

        if (sellOrder->quantity == 0)
            cout << "Sell Order FULLY filled\n";
        else
            cout << "Sell Order PARTIALLY filled\n";

        if (buyOrder->quantity == 0){
            int id = buyOrder->id;        // save BEFORE deletion
            bestBid->second.pop_front(); // deletes the object
            delete buyOrder; // free memory
            ob.orderMap.erase(id); // remove tracking 
        }
        
        if (sellOrder->quantity == 0){
            int id = sellOrder->id;
            bestAsk->second.pop_front();
            delete sellOrder;
            ob.orderMap.erase(id);
        }

        if (bestBid->second.empty())
            ob.bids.erase(bestBid);

        if (bestAsk->second.empty())
            ob.asks.erase(bestAsk);
    }
}

void executeMarketBuy(OrderBook &ob, int quantity) {
    while (quantity > 0 && !ob.asks.empty()) {
        auto bestAsk = ob.asks.begin();

        Order* sellOrder = bestAsk->second.front();

        int tradedQty = min(quantity, sellOrder->quantity);

        cout << "Market Buy executed: "
             << tradedQty << " @ " << bestAsk->first << endl;

        quantity -= tradedQty;
        sellOrder->quantity -= tradedQty;

        if (sellOrder->quantity == 0){
            int id = sellOrder->id;
            bestAsk->second.pop_front();
            delete sellOrder;
            ob.orderMap.erase(id);
        }

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

        Order* buyOrder = bestBid->second.front();

        int tradedQty = min(quantity, buyOrder->quantity);

        cout << "Market Sell executed: "
             << tradedQty << " @ " << bestBid->first << endl;

        quantity -= tradedQty;
        buyOrder->quantity -= tradedQty;

        if (buyOrder->quantity == 0){
            int id = buyOrder->id;        
            bestBid->second.pop_front(); 
            delete buyOrder;
            ob.orderMap.erase(id);
        }
            
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
    ob.addOrder({2, SELL, LIMIT, 95, 5});
    ob.addOrder({3, SELL, LIMIT, 98, 5});
    ob.cancelOrder(1);
    ob.addOrder({4, BUY, MARKET, 0, 5});

    return 0;
}