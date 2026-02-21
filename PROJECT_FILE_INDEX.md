# Project File Index

## Core Implementation Files (C++ Headers)

### 1. `order.hpp` (3.3 KB)
**Purpose**: Core data structures  
**Contents**:
- `Order` struct - 64-byte cache-aligned order representation
- `PriceLevel` struct - Price level with order linked list
- `OrderNode` struct - Order with linkage information
- `Side` enum - BUY/SELL
- `OrderType` enum - ADD/CANCEL/MATCH

**Key Features**:
- Cache-line aligned (64 bytes)
- Fixed-point price representation
- Minimal memory footprint

---

### 2. `spsc_ring_buffer.hpp` (5.4 KB)
**Purpose**: Lock-free single-producer single-consumer queue  
**Contents**:
- `SPSCRingBuffer` template class
- Atomic head/tail indices
- Cache-line separated for false-sharing prevention
- Optimized memory ordering

**Key Features**:
- ~5-10ns per operation
- Power-of-2 size requirement
- Prefetching support
- Cached atomic values

---

### 3. `memory_pool.hpp` (3.6 KB)
**Purpose**: Preallocated memory management  
**Contents**:
- `OrderMemoryPool` template class
- Free list implementation
- Index-based allocation

**Key Features**:
- O(1) allocation/deallocation
- Zero dynamic allocation in hot path
- Contiguous memory layout
- ~3-5ns operations

---

### 4. `order_book.hpp` (13 KB)
**Purpose**: Main limit order book implementation  
**Contents**:
- `LimitOrderBook` template class
- Price-time priority matching
- Separate buy/sell sides
- Hash map for order lookup

**Key Features**:
- O(1) top-of-book access
- O(log n) price level operations
- O(1) order cancellation
- Inline hot path functions

---

### 5. `benchmark.hpp` (11 KB)
**Purpose**: Performance measurement and benchmarking  
**Contents**:
- `OrderBookBenchmark` template class
- `LatencyStats` struct
- Thread pinning utilities
- Nanosecond-precision timing

**Key Features**:
- Percentile calculations (p50, p95, p99)
- Multi-threaded producer-consumer
- Busy-polling simulation
- Zero I/O in measurement path

---

## Example and Test Files

### 6. `main.cpp` (8.9 KB)
**Purpose**: Example usage and demonstration  
**Contents**:
- Simple order book example
- SPSC ring buffer demo
- Market feed simulation (1,000 orders)
- Performance benchmark (50,000 orders)

**Features**:
- Multiple usage scenarios
- Real-time statistics
- Configurable parameters

---

## Build and Configuration Files

### 7. `build.sh` (2.0 KB)
**Purpose**: Automated build script  
**Supports**:
- Release build (optimized)
- Debug build
- Clean command
- Compiler detection

**Usage**:
```bash
./build.sh        # Release build
./build.sh debug  # Debug build
./build.sh clean  # Clean artifacts
```

---

### 8. `CMakeLists.txt` (1.9 KB)
**Purpose**: CMake build configuration  
**Features**:
- C++17 standard enforcement
- Aggressive optimization flags
- Cross-platform support
- Thread library detection

**Usage**:
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

---

### 9. `.gitignore` (236 bytes)
**Purpose**: Git version control exclusions  
**Excludes**:
- Build artifacts
- IDE files
- Platform-specific files
- Debug symbols

---

## Documentation Files

### 10. `README.md` (7.6 KB)
**Purpose**: Main project documentation  
**Contents**:
- Feature overview
- Architecture description
- Usage examples
- Design decisions
- Performance characteristics
- References

**Audience**: Developers, researchers, students

---

### 11. `QUICKSTART.md` (4.9 KB)
**Purpose**: Getting started guide  
**Contents**:
- Build instructions (3 methods)
- Running examples
- Customization guide
- Troubleshooting
- Performance tips

**Audience**: First-time users

---

### 12. `PERFORMANCE.md` (11 KB)
**Purpose**: In-depth performance analysis  
**Contents**:
- Memory layout diagrams
- Latency breakdown by CPU
- Optimization techniques explained
- Profiling results
- Bottleneck analysis
- Future enhancement ideas

**Audience**: Performance engineers, researchers

---

### 13. `ARCHITECTURE.md` (10 KB)
**Purpose**: Visual system architecture  
**Contents**:
- ASCII diagrams of system components
- Memory layout visualizations
- Data flow diagrams
- Matching algorithm flowchart
- Cache access patterns
- Thread communication

**Audience**: Visual learners, architects

---

### 14. `PROJECT_SUMMARY.md` (8 KB)
**Purpose**: High-level project overview  
**Contents**:
- Feature checklist
- Quick start summary
- Performance comparison
- Use cases
- Production readiness assessment
- Contribution guidelines

**Audience**: Managers, decision-makers, contributors

---

### 15. `PROJECT_FILE_INDEX.md` (This File)
**Purpose**: Complete file reference  
**Contents**:
- Detailed description of every file
- Purpose and key features
- File sizes and relationships
- Quick reference guide

**Audience**: Developers navigating the codebase

---

## File Dependency Graph

```
main.cpp
  ├── benchmark.hpp
  │     ├── order_book.hpp
  │     │     ├── memory_pool.hpp
  │     │     │     └── order.hpp
  │     │     └── spsc_ring_buffer.hpp
  │     │           └── order.hpp
  │     └── spsc_ring_buffer.hpp
  │           └── order.hpp
  └── (no other dependencies)
```

## Build Artifacts (Generated)

### `orderbook_demo` (60-80 KB)
- Optimized executable
- Stripped symbols in release mode
- Platform-specific binary

### `build/` Directory
- CMake build artifacts (if using CMake)
- Can be safely deleted

---

## File Statistics

| Type          | Count | Total Size | Purpose                    |
|---------------|-------|------------|----------------------------|
| Headers (.hpp)| 5     | ~37 KB     | Core implementation        |
| Source (.cpp) | 1     | ~9 KB      | Examples and tests         |
| Documentation | 6     | ~52 KB     | Guides and references      |
| Build Scripts | 2     | ~4 KB      | Compilation automation     |
| Config Files  | 1     | <1 KB      | Version control            |
| **Total**     | **15**| **~102 KB**| Complete project           |

---

## Recommended Reading Order

### For First-Time Users:
1. `README.md` - Understand what the project does
2. `QUICKSTART.md` - Build and run examples
3. `main.cpp` - See usage examples
4. `ARCHITECTURE.md` - Understand system design

### For Developers:
1. `PROJECT_SUMMARY.md` - Quick overview
2. `order.hpp` - Understand data structures
3. `spsc_ring_buffer.hpp` - Lock-free queue
4. `memory_pool.hpp` - Memory management
5. `order_book.hpp` - Core logic
6. `benchmark.hpp` - Benchmarking framework
7. `PERFORMANCE.md` - Optimization techniques

### For Performance Engineers:
1. `PERFORMANCE.md` - Detailed analysis
2. `ARCHITECTURE.md` - Memory layouts
3. `benchmark.hpp` - Measurement code
4. Profile with `perf` or `Instruments`
5. Modify and experiment

---

## Quick Reference

### Compile Commands
```bash
# Using build script
./build.sh

# Direct compilation (macOS/Linux)
clang++ -std=c++17 -O3 -march=native -pthread main.cpp -o orderbook_demo
g++ -std=c++17 -O3 -march=native -pthread main.cpp -o orderbook_demo

# Using CMake
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

### Run Commands
```bash
# Run demo
./orderbook_demo

# Run with real-time priority (Linux only)
sudo chrt -f 99 ./orderbook_demo
```

### Key Classes
- `Order` - Basic order structure
- `SPSCRingBuffer<T, Size>` - Lock-free queue
- `OrderMemoryPool<MaxOrders>` - Memory pool
- `LimitOrderBook<MaxOrders>` - Order book
- `OrderBookBenchmark<MaxOrders, RingBufferSize>` - Benchmark harness

### Configuration Constants (in main.cpp)
```cpp
constexpr size_t MAX_ORDERS = 100000;          // Order capacity
constexpr size_t RING_BUFFER_SIZE = 8192;      // Queue size
constexpr size_t NUM_BENCHMARK_ORDERS = 50000; // Test size
```

---

## License and Usage

All files are provided for **educational and research purposes**.

See individual files and README.md for detailed licensing information.

---

**Last Updated**: February 20, 2026  
**Project Version**: 1.0  
**Total Lines of Code**: ~2,500 (excluding comments and documentation)
