#include "instrumented_scheduler.h"
#include "workloads.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <vector>
#include <x86intrin.h>

template<typename WorkloadType>
void run_benchmark(const std::string& name, int num_fibers, int iterations) {
    std::cout << "=== " << name << " ===" << std::endl;
    std::cout << "Fibers: " << num_fibers << ", Iterations: " << iterations << std::endl;
    
    using fn_t = std::function<void(int)>;
    InstrumentedScheduler<fn_t> sched(0, num_fibers * 2);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Spawn fibers
    for (int i = 0; i < num_fibers; ++i) {
        sched.spawn(
            [i, &sched, num_fibers](int runner_id) { 
                sched.task_start(i);
                WorkloadType::fiber_task(i, &sched, runner_id, num_fibers);
                sched.task_end(i);
            },
            64 * 1024, // 64KB stack
            i,
            100000       // 200K cycle budget to force yielding
        );
    }
    
    // Wait for completion
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    sched.print_stats();
    std::cout << "Total Time: " << duration.count() << " μs" << std::endl;
    std::cout << "Throughput: " << (num_fibers * 1000000.0 / duration.count()) << " fibers/sec" << std::endl;
    std::cout << std::endl;
}

int main() {
    // TSC frequency detection for cycle-to-time conversion
    auto start = std::chrono::high_resolution_clock::now();
    uint64_t start_tsc = __rdtsc();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    uint64_t end_tsc = __rdtsc();
    auto end = std::chrono::high_resolution_clock::now();
    
    uint64_t tsc_freq = (end_tsc - start_tsc) * 10; // cycles per second
    std::cout << "TSC Frequency: " << tsc_freq / 1000000 << " MHz" << std::endl;
    
    // Benchmark matrix - test with working numbers
    std::vector<int> fiber_counts = {10, 100, 1000};  
    
    for (int fibers : fiber_counts) {
        std::cout << "\n=== Testing " << fibers << " fibers ===" << std::endl;
        run_benchmark<CPUBoundWorkload>("CPU Bound", fibers, 1);
        run_benchmark<MemoryBoundWorkload>("Memory Bound", fibers, 1);
        run_benchmark<YieldHeavyWorkload>("Yield Heavy", fibers, 1);
    }
    
    return 0;
}
