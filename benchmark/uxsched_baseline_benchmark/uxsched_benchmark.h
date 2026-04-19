#pragma once
#include "workloads.h"
#include "stats_printer.h"
#include "../../schedular/schedular.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <functional>
#include <mutex>
#include <thread>
#include <x86intrin.h>

template<typename WorkloadType>
std::vector<uint64_t> run_uxsched_benchmark(const std::string& sched_name,
                                             const std::string& workload_name,
                                             int num_tasks,
                                             int duration_seconds) {
    std::cout << "=== " << workload_name << " (UXSched) ===" << std::endl;
    std::cout << "Tasks target: " << num_tasks << std::endl;

    using fn_t = std::function<void(int)>;
    // queue_size is per-core capacity in your current scheduler
    uxsched<fn_t> sched(0, 1 << 15);

    std::vector<uint64_t> latencies;
    latencies.reserve(2'000'000);
    std::mutex latencies_mutex;

    auto start_time = std::chrono::high_resolution_clock::now();
    auto end_deadline = start_time + std::chrono::seconds(duration_seconds);

    int task_id = 0;
    int submitted = 0;

    // Keep producer pressure realistic and comparable to OS pool.
    const int num_cores = std::max(1u, std::thread::hardware_concurrency());
    const int batch_size = num_cores * 16;

    while (std::chrono::high_resolution_clock::now() < end_deadline) {
        for (int i = 0; i < batch_size &&
                        std::chrono::high_resolution_clock::now() < end_deadline; ++i) {
            uint64_t spawn_time = __rdtsc();

            // Guaranteed submit path
            sched.spawn(
                [task_id, spawn_time, &latencies, &latencies_mutex, &sched](int runner_id) {
                    WorkloadType::execute(task_id, &sched, runner_id);

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

            ++task_id;
            ++submitted;
        }

        // Same pacing style as OS thread-pool benchmark to keep comparison fair
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }

    // Drain window
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    std::cout << "Submitted: " << submitted << "\n";
    sched.print_submit_stats();
    print_stats(sched_name, workload_name, latencies);
    return latencies;
}