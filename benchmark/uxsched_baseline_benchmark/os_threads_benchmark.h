// benchmark/os_threads_benchmark.cpp

#include "workloads.h"
#include "stats_printer.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <mutex>

// Dummy scheduler that does nothing (for OS threads)
struct NullScheduler {
    void safepoint_check(int runner_id) {
        std::this_thread::yield();  // OS yield instead
    }
};

template<typename WorkloadType>
void run_os_threads_benchmark(const std::string& name, int num_tasks, int duration_seconds) {
    std::cout << "=== " << name << " (OS Threads) ===" << std::endl;
    std::cout << "Tasks: " << num_tasks << std::endl;
    
    int num_cores = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;
    std::vector<uint64_t> latencies;
    std::mutex latencies_mutex;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    auto end_deadline = start_time + std::chrono::seconds(duration_seconds);
    
    // Worker function that runs on each OS thread
    auto worker = [&](int thread_id) {
        NullScheduler dummy_sched;

        int task_id = thread_id;
        while (std::chrono::high_resolution_clock::now() < end_deadline) {
            uint64_t spawn_time = __rdtsc();
            
            // Call workload (no-op yield for threads)
            WorkloadType::execute(task_id, &dummy_sched, thread_id);
            
            // Record latency
            uint64_t end_time = __rdtsc();
            {
                std::lock_guard<std::mutex> lock(latencies_mutex);
                latencies.push_back(end_time - spawn_time);
            }
            
            task_id += num_cores;  // Next task for this thread
        }
    };
    
    // Create worker threads
    for (int i = 0; i < num_cores; ++i) {
        threads.emplace_back(worker, i);
    }
    
    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }
    
    print_stats(latencies);
}

// int main() {
//     run_os_threads_benchmark<OrderBookWorkload>("Order Book", 1000, 5);
//     run_os_threads_benchmark<PipelineWorkload>("Pipeline", 1000, 5);
//     return 0;
// }