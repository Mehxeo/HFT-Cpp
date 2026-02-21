# Quick Start Guide

## Building the Project

### Option 1: Using the Build Script (Recommended)

```bash
# Release build (optimized for performance)
./build.sh

# Debug build (with symbols, no optimization)
./build.sh debug

# Clean build artifacts
./build.sh clean
```

### Option 2: Manual Compilation

```bash
# Release build (maximum performance)
clang++ -std=c++17 -O3 -march=native -mtune=native -Wall -Wextra -pthread main.cpp -o orderbook_demo

# Debug build
clang++ -std=c++17 -O0 -g -Wall -Wextra -pthread main.cpp -o orderbook_demo
```

### Option 3: Using CMake (if available)

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
./orderbook_demo
```

## Running the Demo

```bash
./orderbook_demo
```

The demo includes:
1. **Simple Order Book Example**: Basic add/cancel/match operations
2. **SPSC Ring Buffer Example**: Lock-free queue demonstration
3. **Market Feed Simulation**: Producer-consumer model with 1,000 orders
4. **Performance Benchmark**: Latency measurements with 50,000 orders

## Expected Output

You should see latency measurements similar to:

```
=== Overall Latency ===
Samples:  50000
Min:      0 ns
Mean:     50-100 ns
p50:      40-60 ns
p95:      80-150 ns
p99:      120-250 ns
Max:      varies (depends on system)
```

**Note**: Actual latencies vary based on:
- CPU architecture and clock speed
- System load
- Cache state
- Compiler optimizations
- OS scheduler behavior

## Customizing the Benchmark

Edit the constants in `main.cpp`:

```cpp
constexpr size_t MAX_ORDERS = 100000;          // Max concurrent orders
constexpr size_t RING_BUFFER_SIZE = 8192;      // Power of 2
constexpr size_t NUM_BENCHMARK_ORDERS = 50000; // Test iterations
```

## Optimizing for Your System

### CPU Architecture
The code auto-detects your CPU and uses appropriate instructions:
- **ARM (Apple Silicon)**: Uses `yield` instruction
- **x86-64 (Intel/AMD)**: Uses `pause` instruction
- **Other**: Falls back to `std::this_thread::yield()`

### Thread Pinning
Thread pinning is disabled by default on macOS (not well supported).
On Linux, you can enable it by modifying the core IDs in `main.cpp`:

```cpp
const int producer_core = 0;  // Pin to core 0
const int consumer_core = 1;  // Pin to core 1
```

## Performance Tips

1. **Close other applications**: Background processes affect latency
2. **Disable Turbo Boost**: For more consistent measurements
3. **Run multiple times**: First run includes cold cache effects
4. **Monitor CPU frequency**: Use `turbostat` (Linux) or `powermetrics` (macOS)
5. **Check thermal throttling**: Extended runs may cause CPU to throttle

## Troubleshooting

### Compilation Errors

**Problem**: `clang++: command not found`
```bash
# macOS
xcode-select --install

# Linux (Ubuntu/Debian)
sudo apt-get install clang

# Linux (Fedora/RHEL)
sudo dnf install clang
```

**Problem**: C++17 not supported
- Update your compiler to GCC 7+ or Clang 5+

### Runtime Issues

**Problem**: Latencies are much higher than expected
- Close background applications
- Ensure Release build (`-O3` optimization)
- Check CPU is not throttled due to thermals
- Try running with real-time priority (Linux): `sudo chrt -f 99 ./orderbook_demo`

**Problem**: Program hangs
- Increase ring buffer size if orders aren't processed fast enough
- Check that both producer and consumer threads are running

## Understanding the Code

### File Structure
- `order.hpp`: Core data structures (Order, PriceLevel, OrderNode)
- `spsc_ring_buffer.hpp`: Lock-free SPSC queue implementation
- `memory_pool.hpp`: Preallocated memory management
- `order_book.hpp`: Main order book logic with price-time priority
- `benchmark.hpp`: Latency measurement and thread management
- `main.cpp`: Example usage and test scenarios

### Key Design Patterns
- **Zero-copy**: Orders stored contiguously, passed by value when small
- **Index-based linking**: Use indices instead of pointers for cache efficiency
- **Cache-line alignment**: Prevent false sharing between threads
- **Lock-free synchronization**: Atomic operations with optimized memory ordering
- **Busy polling**: Spin-wait for lowest latency (at cost of CPU usage)

## Next Steps

1. **Experiment with parameters**: Adjust order counts, buffer sizes
2. **Add instrumentation**: Insert more detailed timing measurements
3. **Profile the code**: Use `perf` (Linux) or `Instruments` (macOS)
4. **Extend functionality**: Add IOC orders, iceberg orders, market orders
5. **Integrate with real data**: Connect to market data feeds

## Performance Comparison

Typical financial industry order book latencies:
- **Traditional exchange systems**: 10-100 microseconds
- **Modern low-latency systems**: 1-10 microseconds  
- **Ultra-low-latency HFT**: 100-500 nanoseconds
- **This implementation**: 50-200 nanoseconds (p99)

This implementation achieves ultra-low-latency performance suitable for:
- High-frequency trading simulation
- Market making strategies
- Order book research
- Trading algorithm backtesting
- Educational purposes
