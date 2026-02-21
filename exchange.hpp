#pragma once

#include "order_book.hpp"
#include <unordered_map>
#include <memory>
#include <string>

template<size_t MaxOrdersPerBook>
class Exchange {
private:
    std::unordered_map<uint32_t, std::unique_ptr<LimitOrderBook<MaxOrdersPerBook>>> books_;

public:
    Exchange() = default;

    LimitOrderBook<MaxOrdersPerBook>& get_book(uint32_t symbol_id) {
        auto it = books_.find(symbol_id);
        if (it == books_.end()) {
            auto book = std::make_unique<LimitOrderBook<MaxOrdersPerBook>>();
            auto* ptr = book.get();
            books_[symbol_id] = std::move(book);
            return *ptr;
        }
        return *(it->second);
    }

    bool process_order(const Order& order) {
        return get_book(order.symbol_id).process_order(order);
    }

    bool cancel_order(uint32_t symbol_id, uint64_t order_id) {
        return get_book(symbol_id).cancel_order(order_id);
    }

    size_t num_stocks() const { return books_.size(); }
};
