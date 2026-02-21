#pragma once

#include "order.hpp"
#include "memory_pool.hpp"
#include <unordered_map>
#include <map>
#include <memory>
#include <algorithm>

template<size_t MaxOrders>
class LimitOrderBook {
private:
    OrderMemoryPool<MaxOrders> memory_pool_;
    std::unordered_map<uint64_t, uint32_t> order_map_;
    std::map<int64_t, std::unique_ptr<PriceLevel>, std::greater<int64_t>> buy_side_;
    std::map<int64_t, std::unique_ptr<PriceLevel>, std::less<int64_t>> sell_side_;
    uint64_t total_orders_processed_;
    uint64_t total_matches_;

public:
    LimitOrderBook() 
        : total_orders_processed_(0)
        , total_matches_(0)
    {
        order_map_.reserve(MaxOrders);
    }

    [[nodiscard]] inline bool process_order(const Order& order) {
        ++total_orders_processed_;
        switch (order.type) {
            case OrderType::ADD: return add_order(order);
            case OrderType::CANCEL: return cancel_order(order.order_id);
            default: return false;
        }
    }

    [[nodiscard]] inline bool add_order(const Order& order) {
        if (!memory_pool_.has_space()) return false;
        Order rem = order;
        if (!try_match_order(rem)) return false;
        if (rem.quantity == 0) return true;
        return add_to_book(rem);
    }

    [[nodiscard]] inline bool cancel_order(uint64_t order_id) {
        auto it = order_map_.find(order_id);
        if (it == order_map_.end()) return false;
        const uint32_t idx = it->second;
        remove_from_price_level(idx);
        order_map_.erase(it);
        memory_pool_.deallocate(idx);
        return true;
    }

    [[nodiscard]] inline int64_t get_best_bid() const { return buy_side_.empty() ? 0 : buy_side_.begin()->first; }
    [[nodiscard]] inline int64_t get_best_ask() const { return sell_side_.empty() ? INT64_MAX : sell_side_.begin()->first; }
    [[nodiscard]] inline uint64_t get_best_bid_quantity() const { return buy_side_.empty() ? 0 : buy_side_.begin()->second->total_quantity; }
    [[nodiscard]] inline uint64_t get_best_ask_quantity() const { return sell_side_.empty() ? 0 : sell_side_.begin()->second->total_quantity; }
    [[nodiscard]] uint64_t total_orders_processed() const { return total_orders_processed_; }
    [[nodiscard]] uint64_t total_matches() const { return total_matches_; }
    [[nodiscard]] size_t active_orders() const { return memory_pool_.allocated_count(); }

private:
    [[nodiscard]] inline bool try_match_order(Order& order) {
        if (order.side == Side::BUY) return match_buy_order(order);
        return match_sell_order(order);
    }

    [[nodiscard]] inline bool match_buy_order(Order& order) {
        while (order.quantity > 0 && !sell_side_.empty()) {
            auto& [price, level] = *sell_side_.begin();
            if (order.price < price) break;
            if (!match_against_price_level(order, level.get())) return false;
            if (level->order_count == 0) sell_side_.erase(sell_side_.begin());
        }
        return true;
    }

    [[nodiscard]] inline bool match_sell_order(Order& order) {
        while (order.quantity > 0 && !buy_side_.empty()) {
            auto& [price, level] = *buy_side_.begin();
            if (order.price > price) break;
            if (!match_against_price_level(order, level.get())) return false;
            if (level->order_count == 0) buy_side_.erase(buy_side_.begin());
        }
        return true;
    }

    [[nodiscard]] inline bool match_against_price_level(Order& order, PriceLevel* level) {
        uint32_t curr = level->head_order_idx;
        while (order.quantity > 0 && curr != UINT32_MAX) {
            OrderNode& node = memory_pool_.get(curr);
            const uint32_t next = node.next_order_idx;
            const uint32_t match_qty = std::min(order.quantity, node.order.quantity);
            order.quantity -= match_qty;
            node.order.quantity -= match_qty;
            level->total_quantity -= match_qty;
            ++total_matches_;
            if (node.order.quantity == 0) {
                order_map_.erase(node.order.order_id);
                remove_from_price_level(curr);
                memory_pool_.deallocate(curr);
            }
            curr = next;
        }
        return true;
    }

    [[nodiscard]] inline bool add_to_book(const Order& order) {
        const uint32_t idx = memory_pool_.allocate();
        if (idx == UINT32_MAX) return false;
        OrderNode& node = memory_pool_.get(idx);
        node.order = order;
        PriceLevel* level = get_or_create_price_level(order.price, order.side);
        if (!level) {
            memory_pool_.deallocate(idx);
            return false;
        }
        node.price_level = level;
        if (level->head_order_idx == UINT32_MAX) {
            level->head_order_idx = level->tail_order_idx = idx;
            node.prev_order_idx = node.next_order_idx = UINT32_MAX;
        } else {
            OrderNode& tail = memory_pool_.get(level->tail_order_idx);
            tail.next_order_idx = idx;
            node.prev_order_idx = level->tail_order_idx;
            node.next_order_idx = UINT32_MAX;
            level->tail_order_idx = idx;
        }
        level->total_quantity += order.quantity;
        ++level->order_count;
        order_map_[order.order_id] = idx;
        return true;
    }

    [[nodiscard]] inline PriceLevel* get_or_create_price_level(int64_t price, Side side) {
        if (side == Side::BUY) {
            auto it = buy_side_.find(price);
            if (it != buy_side_.end()) return it->second.get();
            auto level = std::make_unique<PriceLevel>(price);
            PriceLevel* ptr = level.get();
            buy_side_[price] = std::move(level);
            return ptr;
        } else {
            auto it = sell_side_.find(price);
            if (it != sell_side_.end()) return it->second.get();
            auto level = std::make_unique<PriceLevel>(price);
            PriceLevel* ptr = level.get();
            sell_side_[price] = std::move(level);
            return ptr;
        }
    }

    inline void remove_from_price_level(uint32_t idx) {
        OrderNode& node = memory_pool_.get(idx);
        PriceLevel* level = node.price_level;
        if (!level) return;
        if (node.prev_order_idx != UINT32_MAX) memory_pool_.get(node.prev_order_idx).next_order_idx = node.next_order_idx;
        else level->head_order_idx = node.next_order_idx;
        if (node.next_order_idx != UINT32_MAX) memory_pool_.get(node.next_order_idx).prev_order_idx = node.prev_order_idx;
        else level->tail_order_idx = node.prev_order_idx;
        level->total_quantity -= node.order.quantity;
        --level->order_count;
    }
};
