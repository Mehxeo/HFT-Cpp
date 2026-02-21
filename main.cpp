#include "benchmark.hpp"
#include "cpu_relax.hpp"
#include "exchange.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>
#include <atomic>

constexpr size_t MAX_ORDERS = 100000;
constexpr size_t RING_BUFFER_SIZE = 8192;
constexpr size_t NUM_BENCHMARK_ORDERS = 50000;

void simple_example() {
    std::cout << "\n=== Simple Order Book Example ===" << std::endl;
    LimitOrderBook<1000> book;
    (void)book.process_order(Order{1, 1000, 99900, 100, 0, OrderType::ADD, Side::BUY});
    (void)book.process_order(Order{2, 1001, 99800, 200, 0, OrderType::ADD, Side::BUY});
    (void)book.process_order(Order{3, 1002, 99700, 150, 0, OrderType::ADD, Side::BUY});
    (void)book.process_order(Order{4, 1003, 100100, 100, 0, OrderType::ADD, Side::SELL});
    (void)book.process_order(Order{5, 1004, 100200, 250, 0, OrderType::ADD, Side::SELL});
    (void)book.process_order(Order{6, 1005, 100300, 175, 0, OrderType::ADD, Side::SELL});
    std::cout << "Best Bid: " << book.get_best_bid() / 100.0 << std::endl;
    std::cout << "Best Ask: " << book.get_best_ask() / 100.0 << std::endl;
    (void)book.process_order(Order{7, 1006, 100100, 150, 0, OrderType::ADD, Side::BUY});
}

void multi_stock_example() {
    std::cout << "\n=== Multi-Stock Exchange Example ===" << std::endl;
    Exchange<1000> exchange;
    const uint32_t AAPL = 1;
    const uint32_t TSLA = 2;
    (void)exchange.process_order(Order{1, 1000, 15000, 100, AAPL, OrderType::ADD, Side::BUY});
    (void)exchange.process_order(Order{2, 1001, 70000, 50, TSLA, OrderType::ADD, Side::SELL});
    std::cout << "AAPL Best Bid: " << exchange.get_book(AAPL).get_best_bid() / 100.0 << std::endl;
    std::cout << "TSLA Best Ask: " << exchange.get_book(TSLA).get_best_ask() / 100.0 << std::endl;
}

void run_benchmark() {
    std::cout << "\n=== Performance Benchmark ===" << std::endl;
    OrderBookBenchmark<MAX_ORDERS, RING_BUFFER_SIZE> benchmark;
    benchmark.run(NUM_BENCHMARK_ORDERS);
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    simple_example();
    multi_stock_example();
    run_benchmark();
    return 0;
}
