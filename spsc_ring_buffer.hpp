#pragma once

#include "order.hpp"
#include <atomic>

template<typename T, size_t Size>
class SPSCRingBuffer {
    static_assert((Size & (Size - 1)) == 0, "Size must be a power of 2");
private:
    T buffer_[Size];
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
    static constexpr size_t MASK = Size - 1;

public:
    SPSCRingBuffer() = default;

    [[nodiscard]] bool try_push(const T& item) {
        const size_t h = head_.load(std::memory_order_relaxed);
        const size_t next_h = (h + 1) & MASK;
        if (next_h == tail_.load(std::memory_order_acquire)) return false;
        buffer_[h] = item;
        head_.store(next_h, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool try_pop(T& item) {
        const size_t t = tail_.load(std::memory_order_relaxed);
        if (t == head_.load(std::memory_order_acquire)) return false;
        item = buffer_[t];
        tail_.store((t + 1) & MASK, std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool empty() const { return tail_.load() == head_.load(); }
    [[nodiscard]] size_t size() const { return (head_.load() - tail_.load()) & MASK; }
    [[nodiscard]] static constexpr size_t capacity() { return Size - 1; }
};
