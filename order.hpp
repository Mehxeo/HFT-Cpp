#pragma once

#include <cstdint>
#include <atomic>

constexpr size_t CACHE_LINE_SIZE = 64;

enum class Side : uint8_t {
    BUY = 0,
    SELL = 1
};

enum class OrderType : uint8_t {
    ADD = 0,
    CANCEL = 1,
    MATCH = 2
};

struct alignas(CACHE_LINE_SIZE) Order {
    uint64_t order_id;
    uint64_t timestamp;
    int64_t price;
    uint32_t quantity;
    uint32_t symbol_id;
    OrderType type;
    Side side;
    uint16_t padding;

    Order() = default;

    Order(uint64_t id, uint64_t ts, int64_t p, uint32_t qty, uint32_t s_id, OrderType t, Side s)
        : order_id(id)
        , timestamp(ts)
        , price(p)
        , quantity(qty)
        , symbol_id(s_id)
        , type(t)
        , side(s)
        , padding(0)
    {}
};

static_assert(sizeof(Order) == CACHE_LINE_SIZE, "Order must be exactly one cache line");

struct PriceLevel {
    int64_t price;
    uint64_t total_quantity;
    uint32_t order_count;
    uint32_t head_order_idx;
    uint32_t tail_order_idx;
    PriceLevel* next;
    PriceLevel* prev;

    PriceLevel(int64_t p = 0)
        : price(p)
        , total_quantity(0)
        , order_count(0)
        , head_order_idx(UINT32_MAX)
        , tail_order_idx(UINT32_MAX)
        , next(nullptr)
        , prev(nullptr)
    {}
};

struct OrderNode {
    Order order;
    uint32_t next_order_idx;
    uint32_t prev_order_idx;
    PriceLevel* price_level;
    bool is_active;

    OrderNode()
        : order()
        , next_order_idx(UINT32_MAX)
        , prev_order_idx(UINT32_MAX)
        , price_level(nullptr)
        , is_active(false)
    {}
};
