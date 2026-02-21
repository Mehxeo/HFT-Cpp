#pragma once

#include "order_book.hpp"
#include "spsc_ring_buffer.hpp"
#include "cpu_relax.hpp"
#include <chrono>
#include <vector>
#include <algorithm>
#include <thread>
#include <pthread.h>
#include <iostream>
#include <iomanip>

inline bool pin_thread_to_core(int core_id) {
#ifdef __APPLE__
    (void)core_id;
    return false;
#elif __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
#else
    (void)core_id;
    return false;
#endif
}

inline uint64_t get_timestamp_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

struct LatencyStats {
    std::vector<uint64_t> latencies;
    uint64_t min_latency;
    uint64_t max_latency;
    uint64_t mean_latency;
    uint64_t p50_latency;
    uint64_t p95_latency;
    uint64_t p99_latency;
    size_t sample_count;

    LatencyStats() : min_latency(UINT64_MAX), max_latency(0), mean_latency(0), p50_latency(0), p95_latency(0), p99_latency(0), sample_count(0) {}

    void add_sample(uint64_t latency_ns) {
        latencies.push_back(latency_ns);
        ++sample_count;
    }

    void calculate() {
        if (latencies.empty()) return;
        std::sort(latencies.begin(), latencies.end());
        min_latency = latencies.front();
        max_latency = latencies.back();
        uint64_t sum = 0;
        for (uint64_t lat : latencies) sum += lat;
        mean_latency = sum / latencies.size();
        p50_latency = latencies[latencies.size() * 50 / 100];
        p95_latency = latencies[latencies.size() * 95 / 100];
        p99_latency = latencies[latencies.size() * 99 / 100];
    }

    void print(const std::string& label) const {
        std::cout << "\n=== " << label << " ===" << std::endl;
        std::cout << "Samples:  " << sample_count << "\nMin:      " << min_latency 
                  << " ns\nMean:     " << mean_latency << " ns\np50:      " << p50_latency 
                  << " ns\np95:      " << p95_latency << " ns\np99:      " << p99_latency 
                  << " ns\nMax:      " << max_latency << " ns" << std::endl;
    }
};

template<size_t MaxOrders, size_t RingBufferSize>
class OrderBookBenchmark {
private:
    LimitOrderBook<MaxOrders> order_book_;
    SPSCRingBuffer<Order, RingBufferSize> ring_buffer_;
    LatencyStats add_stats_;
    LatencyStats cancel_stats_;
    LatencyStats total_stats_;
    std::atomic<uint64_t> orders_processed_{0};

public:
    void run(size_t num_orders, int prod_core = -1, int cons_core = -1) {
        std::vector<Order> tests = generate_test_orders(num_orders);
        std::thread cons([this, cons_core, num_orders]() {
            if (cons_core >= 0) pin_thread_to_core(cons_core);
            this->consume_orders(num_orders);
        });
        std::thread prod([this, &tests, prod_core]() {
            if (prod_core >= 0) pin_thread_to_core(prod_core);
            for (const auto& o : tests) while (!ring_buffer_.try_push(o)) cpu_relax();
        });
        prod.join();
        cons.join();
        add_stats_.calculate();
        cancel_stats_.calculate();
        total_stats_.calculate();
        print_results();
    }

private:
    void consume_orders(size_t expected) {
        Order o;
        size_t done = 0;
        while (done < expected) {
            while (!ring_buffer_.try_pop(o)) cpu_relax();
            uint64_t s = get_timestamp_ns();
            (void)order_book_.process_order(o);
            uint64_t e = get_timestamp_ns();
            uint64_t lat = e - s;
            total_stats_.add_sample(lat);
            if (o.type == OrderType::ADD) add_stats_.add_sample(lat);
            else if (o.type == OrderType::CANCEL) cancel_stats_.add_sample(lat);
            done++;
            orders_processed_++;
        }
    }

    std::vector<Order> generate_test_orders(size_t count) {
        std::vector<Order> orders;
        orders.reserve(count);
        uint64_t id = 1;
        std::vector<uint64_t> active;
        for (size_t i = 0; i < count; ++i) {
            uint64_t ts = get_timestamp_ns();
            if (!active.empty() && (i % 5 == 0)) {
                size_t idx = i % active.size();
                orders.emplace_back(active[idx], ts, 0, 0, 0, OrderType::CANCEL, Side::BUY);
                active.erase(active.begin() + idx);
            } else {
                Side s = (i % 2 == 0) ? Side::BUY : Side::SELL;
                int64_t p = (s == Side::BUY) ? 100000 - 100 - (i % 10) * 10 : 100000 + 100 + (i % 10) * 10;
                orders.emplace_back(id, ts, p, 100 + (i % 900), 0, OrderType::ADD, s);
                active.push_back(id++);
            }
        }
        return orders;
    }

    void print_results() const {
        std::cout << "\nOrders processed: " << orders_processed_ << std::endl;
        total_stats_.print("Overall Latency");
        add_stats_.print("Add Order Latency");
        cancel_stats_.print("Cancel Order Latency");
    }
};
