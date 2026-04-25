#pragma once
#include "workloads.h"
#include "stats_printer.h"
#include "../../schedular/schedular.h"

#include <iostream>
#include <chrono>
#include <vector>
#include <functional>
#include <thread>
#include <algorithm>
#include <cstdlib>
#include <future>
#include <memory>
#include <x86intrin.h>

inline int read_env_int(const char* key, int fallback) {
    const char* v = std::getenv(key);
    if (!v) return fallback;
    try {
        int x = std::stoi(v);
        return (x > 0) ? x : fallback;
    } catch (...) {
        return fallback;
    }
}

template<typename WorkloadType>
std::vector<uint64_t> run_uxsched_benchmark(const std::string& sched_name,
                                            const std::string& workload_name,
                                            int num_tasks,
                                            int duration_seconds) {
    std::cout << "=== " << workload_name << " (UXSched) ===\n";
    std::cout << "Tasks target: " << num_tasks << "\n";

    const int queue_size = read_env_int("UXSCHED_QUEUE_SIZE", 1 << 15);
    std::cout << "UXSCHED_QUEUE_SIZE(per-core): " << queue_size << "\n";

    using fn_t = std::function<void(int)>;
    uxsched<fn_t> sched(0, queue_size);

    std::vector<uint64_t> latencies;
    latencies.reserve(2'000'000);

    // Keep futures so submitter can collect completion latencies safely
    std::vector<std::future<uint64_t>> pending;
    pending.reserve(2'000'000);

    auto start_time = std::chrono::high_resolution_clock::now();
    auto end_deadline = start_time + std::chrono::seconds(duration_seconds);

    int task_id = 0;
    int submitted = 0;

    const int num_cores = std::max(1u, std::thread::hardware_concurrency());
    const int batch_size = num_cores * 16;

    while (std::chrono::high_resolution_clock::now() < end_deadline) {
        for (int i = 0; i < batch_size &&
                        std::chrono::high_resolution_clock::now() < end_deadline; ++i) {
            const int this_task_id = task_id++;
            const uint64_t spawn_time = __rdtsc();

            auto p = std::make_shared<std::promise<uint64_t>>();
            pending.emplace_back(p->get_future());

            sched.spawn(
                [this_task_id, spawn_time, p, &sched](int runner_id) {
                    WorkloadType::execute(this_task_id, &sched, runner_id);
                    uint64_t end_time = __rdtsc();
                    p->set_value(end_time - spawn_time);
                },
                64 * 1024,
                this_task_id,
                100000
            );

            ++submitted;
        }

        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }

    // Collect all submitted results (acts as exact drain)
    latencies.reserve(pending.size());
    for (auto& f : pending) {
        latencies.push_back(f.get());
    }

    std::cout << "Submitted: " << submitted << "\n";
    sched.print_submit_stats();
    print_stats(sched_name, workload_name, latencies);
    return latencies;
}