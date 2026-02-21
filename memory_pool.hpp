#pragma once

#include "order.hpp"
#include <vector>

template<size_t MaxOrders>
class OrderMemoryPool {
private:
    std::vector<OrderNode> pool_;
    uint32_t free_list_head_;
    size_t allocated_count_;

public:
    OrderMemoryPool() : pool_(MaxOrders), free_list_head_(0), allocated_count_(0) {
        for (size_t i = 0; i < MaxOrders - 1; ++i) {
            pool_[i].next_order_idx = (uint32_t)(i + 1);
        }
        pool_[MaxOrders - 1].next_order_idx = UINT32_MAX;
    }

    [[nodiscard]] uint32_t allocate() {
        if (free_list_head_ == UINT32_MAX) return UINT32_MAX;
        uint32_t idx = free_list_head_;
        free_list_head_ = pool_[idx].next_order_idx;
        pool_[idx].is_active = true;
        ++allocated_count_;
        return idx;
    }

    void deallocate(uint32_t idx) {
        pool_[idx].is_active = false;
        pool_[idx].next_order_idx = free_list_head_;
        free_list_head_ = idx;
        --allocated_count_;
    }

    [[nodiscard]] OrderNode& get(uint32_t idx) { return pool_[idx]; }
    [[nodiscard]] const OrderNode& get(uint32_t idx) const { return pool_[idx]; }
    [[nodiscard]] size_t allocated_count() const { return allocated_count_; }
    [[nodiscard]] bool has_space() const { return free_list_head_ != UINT32_MAX; }
};
