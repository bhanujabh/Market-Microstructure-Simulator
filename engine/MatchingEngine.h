#pragma once
class OrderBook;
struct Order;

void matchOrders(OrderBook &ob);
void executeMarketBuy(OrderBook &ob, Order *marketOrder);
void executeMarketSell(OrderBook &ob, Order *marketOrder);
