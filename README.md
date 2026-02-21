# High-Performance Limit Order Book

A lock-free, low-latency limit order book implementation in modern C++ (C++17) designed for high-frequency trading simulation and research.

## Features

### Core Architecture
- **Lock-free SPSC Ring Buffer**: Single-producer single-consumer queue for order ingestion with minimal synchronization overhead
- **Preallocated Memory Pool**: Zero dynamic allocation in the matching path - all memory allocated upfront
- **Cache-line Aligned Data Structures**: 64-byte alignment to prevent false sharing between threads
- **Producer-Consumer Model**: Separate threads for market data ingestion and order matching

### Order Book Features
- **Price-Time Priority**: FIFO matching at each price level
- **Separate Buy/Sell Sides**: Independent bid and ask sides for optimal performance
- **O(1) Top-of-Book Access**: Instant access to best bid and ask prices
- **O(log n) Price Level Operations**: Efficient price level management using sorted maps
- **Order Operations**: Add, cancel, and match with full price-time priority

### Performance Optimizations
1. **Memory Efficiency**
   - Single cache-line order structure (64 bytes)
   - Index-based linking instead of pointers for cache locality
   - Contiguous memory allocation for better prefetching

2. **Lock-Free Synchronization**
   - Atomic operations with optimized memory ordering
   - Separate cache lines for producer/consumer indices
   - Cached atomic values to reduce memory access

3. **CPU Optimization**
   - Thread pinning support for reduced context switches
   - Busy polling loops to simulate kernel bypass
   - Prefetching of next elements in queues
   - `__builtin_ia32_pause()` for efficient spinning

4. **Benchmarking**
   - Nanosecond-precision latency measurements
   - Percentile statistics (p50, p95, p99)
   - Separate metrics for add/cancel operations
   - Zero I/O in measurement path

## Project Structure

```
.
├── order.hpp              # Order data structures and types
├── spsc_ring_buffer.hpp   # Lock-free SPSC ring buffer
├── memory_pool.hpp        # Preallocated memory pool
├── order_book.hpp         # Main order book implementation
├── benchmark.hpp          # Benchmarking and latency measurement
├── main.cpp              # Example usage and tests
└── CMakeLists.txt        # Build configuration
```

## Building

### Requirements
- C++17 compatible compiler (GCC 7+, Clang 5+, or MSVC 2017+)
- CMake 3.15 or later
- POSIX threads library

### Compilation

```bash
# Create build directory
mkdir build && cd build

# Configure (Release build for maximum performance)
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build
cmake --build .

# Run
./orderbook_demo
```

### Compiler Optimizations

The CMake configuration enables aggressive optimizations for Release builds:
- `-O3`: Maximum optimization level
- `-march=native`: Tune for the current CPU architecture
- `-mtune=native`: Optimize instruction scheduling for current CPU
- `-flto`: Link-time optimization
- `-ffast-math`: Fast floating-point operations
- `-funroll-loops`: Unroll loops for better performance

## Usage

### Simple Example

```cpp
#include "order_book.hpp"

// Create order book with capacity for 1000 concurrent orders
LimitOrderBook<1000> book;

// Add buy order: ID=1, timestamp=1000, price=$999.00, qty=100
book.process_order(Order{1, 1000, 99900, 100, OrderType::ADD, Side::BUY});

// Add sell order
book.process_order(Order{2, 1001, 100100, 100, OrderType::ADD, Side::SELL});

// Query best prices
int64_t best_bid = book.get_best_bid();
int64_t best_ask = book.get_best_ask();

// Cancel order
book.cancel_order(1);
```

### Producer-Consumer with Ring Buffer

```cpp
#include "spsc_ring_buffer.hpp"
#include "order_book.hpp"

SPSCRingBuffer<Order, 1024> buffer;
LimitOrderBook<10000> book;

// Producer thread: feed orders
std::thread producer([&]() {
    Order order{/* ... */};
    while (!buffer.try_push(order)) {
        __builtin_ia32_pause(); // Busy wait
    }
});

// Consumer thread: process orders
std::thread consumer([&]() {
    Order order;
    while (buffer.try_pop(order)) {
        book.process_order(order);
    }
});
```

### Running Benchmarks

The `main.cpp` includes several examples:
- Simple order book operations
- SPSC ring buffer demonstration
- Market data feed simulation
- Full performance benchmark with latency statistics

## Performance Characteristics

### Typical Latencies (on modern x86_64 CPU)
- **Add Order**: ~100-500 ns (p99)
- **Cancel Order**: ~50-200 ns (p99)
- **Order Matching**: ~200-800 ns (p99)

*Note: Actual latencies depend on CPU architecture, clock speed, cache state, and system load*

### Scalability
- **Max Concurrent Orders**: Configurable via template parameter (default: 100,000)
- **Ring Buffer Size**: Must be power of 2 (default: 8,192)
- **Memory Usage**: Approximately `MaxOrders * sizeof(OrderNode)` (~120 bytes per order slot)

## Design Decisions and Optimizations

### Why SPSC Ring Buffer?
- Single writer and single reader eliminate contention
- Lock-free design with minimal atomic operations
- Predictable performance without OS scheduler involvement
- Simulates kernel bypass typical in HFT systems (e.g., Solarflare, Mellanox)

### Why Preallocated Memory Pool?
- Dynamic allocation takes 100s-1000s of nanoseconds
- Predictable latency - no GC pauses or memory fragmentation
- Better cache performance with contiguous memory
- Standard practice in latency-critical financial systems

### Why Cache-line Alignment?
- Prevents false sharing between producer and consumer
- Orders fit in single cache line (64 bytes) for atomic access
- Reduces memory bus traffic and L1/L2 cache misses

### Why Busy Polling?
- Context switches take 1-10 microseconds
- System calls (epoll, select) add latency
- Dedicated cores can spin-wait for sub-microsecond response
- Common in HFT and real-time systems

### Why Thread Pinning?
- Prevents OS from moving threads between cores
- Maintains hot cache state
- Reduces context switch overhead
- More predictable latency profile

## Limitations and Trade-offs

1. **CPU Usage**: Busy polling consumes 100% CPU even when idle
2. **Single Asset**: This implementation handles one instrument; multi-asset books need separate instances
3. **Market Orders**: Not explicitly supported; use limit orders at extreme prices
4. **Order Book Depth**: No built-in depth visualization (can be added)
5. **Persistence**: No on-disk storage or recovery mechanism
6. **Network**: No network I/O; assumes orders arrive via other means

## Future Enhancements

- MPSC ring buffer for multiple market data sources
- Order book depth snapshots and incremental updates
- More sophisticated order types (IOC, FOK, iceberg)
- FIX protocol integration
- Market data replay from historical feeds
- SIMD optimizations for bulk order processing

## References

### Lock-free Programming
- Dmitry Vyukov's SPSC Queue
- Michael-Scott Queue Algorithm
- Lamport's Bakery Algorithm

### High-Frequency Trading
- "Trading and Exchanges" by Larry Harris
- "Flash Boys" by Michael Lewis (for context)
- Academic papers on microstructure

### Performance Engineering
- "Systems Performance" by Brendan Gregg
- "C++ Move Semantics" by Nicolai Josuttis
- Intel Architecture Optimization Reference Manual

## License

This code is provided for educational and research purposes.

## Contributing

Contributions welcome! Areas of interest:
- Platform-specific optimizations (ARM, RISC-V)
- Additional performance benchmarks
- Memory access pattern analysis
- Comparison with other order book implementations

---

**Disclaimer**: This is a simulation/educational tool. Real trading systems require extensive testing, risk management, regulatory compliance, and professional expertise.
