#pragma once

#include <thread>

inline void cpu_relax() {
#if defined(__linux__) && (defined(__x86_64__) || defined(__i386__))
    __asm__ __volatile__("pause" ::: "memory");
#elif defined(__APPLE__) && defined(__arm64__)
    __asm__ __volatile__("isb sy" ::: "memory");
#elif defined(__aarch64__)
    __asm__ __volatile__("yield" ::: "memory");
#else
    std::this_thread::yield();
#endif
}
