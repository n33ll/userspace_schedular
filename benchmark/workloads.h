#pragma once
#include "../schedular/schedular.h"
#include <vector>
#include <functional>

// CPU-bound: measure context switch overhead
struct CPUBoundWorkload {
    static void fiber_task(int fiber_id, uxsched<std::function<void(int)>>* sched, int runner_id, int num_fibers = 0) {
        volatile uint64_t sum = 0;
        for (int i = 0; i < 1000; ++i) {
            sum += i * i;
            if (i % 100 == 0) {
                sched->safepoint_check(runner_id);
            }
        }
    }
};

// Memory-bound: test cache locality
struct MemoryBoundWorkload {
    static constexpr size_t BASE_ARRAY_SIZE = 64 * 1024; // 64KB base size
    
    static void fiber_task(int fiber_id, uxsched<std::function<void(int)>>* sched, int runner_id, int num_fibers = 0) {
        // Scale down memory allocation based on actual fiber count
        size_t array_size = BASE_ARRAY_SIZE;
        if (num_fibers >= 1000) {
            array_size = 2 * 1024;  // 2KB for 1000+ fibers (2MB total)
        } else if (num_fibers >= 100) {
            array_size = 8 * 1024;  // 8KB for 100+ fibers (800KB total)
        }
        
        // Use local allocation to avoid thread_local issues with work stealing
        std::vector<uint64_t> data(array_size, 0);
        
        for (size_t i = 0; i < array_size; i += 64/sizeof(uint64_t)) { // Cache line stride
            data[i] = data[i] * 2 + 1;
            if (i % 500 == 0) { // More frequent safepoint checks for smaller arrays
                sched->safepoint_check(runner_id);
            }
        }
    }
};

// Yield-heavy: stress context switching
struct YieldHeavyWorkload {
    static void fiber_task(int fiber_id, uxsched<std::function<void(int)>>* sched, int runner_id, int num_fibers = 0) {
        for (int i = 0; i < 100; ++i) {
            sched->safepoint_check(runner_id); // Force yield every iteration
        }
    }
};

