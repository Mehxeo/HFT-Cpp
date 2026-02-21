cmake_minimum_required(VERSION 3.15)
project(HighPerformanceOrderBook CXX)

# C++17 required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags for performance
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
    
    # Optimization flags
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -mtune=native -flto -DNDEBUG")
    
    # Additional performance flags
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ffast-math -funroll-loops")
    
    # Debug flags
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fsanitize=address -fsanitize=undefined")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Ob2 /Oi /Ot /GL /DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /RTC1")
endif()

# Find threads library
find_package(Threads REQUIRED)

# Header-only library for the order book
add_library(orderbook INTERFACE)
target_include_directories(orderbook INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Main executable
add_executable(orderbook_demo main.cpp)
target_link_libraries(orderbook_demo 
    PRIVATE 
        orderbook
        Threads::Threads
)

# Set optimization level
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "C++ flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "C++ Release flags: ${CMAKE_CXX_FLAGS_RELEASE}")

# Print configuration summary
message(STATUS "========================================")
message(STATUS "High-Performance Order Book Configuration")
message(STATUS "========================================")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "========================================")
