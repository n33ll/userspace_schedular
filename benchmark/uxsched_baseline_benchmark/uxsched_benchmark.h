// benchmark/uxsched_benchmark.cpp

#include "workloads.h"
#include "stats_printer.h"
#include "../../schedular/schedular.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <functional>
#include <mutex>
#include <thread>

template<typename WorkloadType>
void run_uxsched_benchmark(const std::string& name, int num_fibers, int duration_seconds) {
    std::cout << "=== " << name << " (UXSched) ===" << std::endl;
    std::cout << "Fibers: " << num_fibers << std::endl;
    
    using fn_t = std::function<void(int)>;
    uxsched<fn_t> sched(0, num_fibers * 2);
    
    std::vector<uint64_t> latencies;
    std::mutex latencies_mutex;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    auto end_deadline = start_time + std::chrono::seconds(duration_seconds);
    
    int task_id = 0;
    while (std::chrono::high_resolution_clock::now() < end_deadline) {
        uint64_t spawn_time = __rdtsc();
        
        sched.spawn(
            [task_id, &latencies, &latencies_mutex, spawn_time, &sched](int runner_id) {
                // Call workload
                WorkloadType::execute(task_id, &sched, runner_id);
                
                // Record latency
                uint64_t end_time = __rdtsc();
                {
                    std::lock_guard<std::mutex> lock(latencies_mutex);
                    latencies.push_back(end_time - spawn_time);
                }
            },
            64 * 1024,
            task_id,
            100000
        );
        task_id++;
    }
    
    // Wait for completion
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    print_stats(latencies);
}

// int main() {
//     run_uxsched_benchmark<OrderBookWorkload>("Order Book", 1000, 5);
//     run_uxsched_benchmark<PipelineWorkload>("Pipeline", 1000, 5);
//     return 0;
// }