#pragma once

enum class OrderType
{
    MARKET,
    LIMIT
};
enum class Side
{
    BUY,
    SELL
};
enum class EventType
{
    ORDER_ADD,
    ORDER_CANCEL,
    ORDER_MODIFY,
    TRADE_EXEC
};