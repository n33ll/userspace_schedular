// benchmark/uxsched_wins_benchmark/uxsched_runner.h
#pragma once
#include "workloads.h"
#include "os_threads_runner.h"   // for get_rss_kb(), CapacityResult
#include "../../schedular/schedular.h"
#include "../../schedular/scheduler_interface.h"

#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <algorithm>

// ============================================================================
// TASK TYPES
//
// Using concrete callable structs — no std::function, no shared_ptr.
// No futures/promises; just an atomic counter so the hot path is as lean
// as possible.
// ============================================================================

// Stage A: trivial task that executes the workload and bumps a counter.
template <typename WorkloadType>
struct ThroughputTask {
    int               task_id;
    std::atomic<int>* completed;

    void operator()(int /*runner_id*/) {
        WorkloadType::execute(task_id);
        completed->fetch_add(1, std::memory_order_relaxed);
    }
};

// Stage B: task that spins on a go-gate (cooperative yields), then runs
// TrivialWorkload once the gate opens.
struct CapacityTask {
    int                task_id;
    std::atomic<bool>* go;
    std::atomic<int>*  completed;
    IScheduler*        sched;

    void operator()(int runner_id) {
        while (!go->load(std::memory_order_relaxed))
            sched->safepoint_check(runner_id);
        volatile uint64_t x = static_cast<uint64_t>(task_id);
        for (int i = 0; i < 50; ++i) x = x * 1103515245ULL + 12345ULL;
        (void)x;
        completed->fetch_add(1, std::memory_order_relaxed);
    }
};

// ============================================================================
// STAGE A — Throughput Saturation (1 producer, N tasks)
// ============================================================================

template <typename WorkloadType>
double run_uxsched_throughput(int N) {
    using task_t = ThroughputTask<WorkloadType>;

    // Large per-core queue so the single producer rarely blocks.
    const int queue_size = 1 << 17;   // 131 072 per core
    std::atomic<int> completed{0};

    uxsched<task_t> sched(0, queue_size);

    auto t0 = std::chrono::steady_clock::now();

    for (int i = 0; i < N; ++i)
        sched.spawn(task_t{i, &completed}, 64 * 1024, i);

    while (completed.load(std::memory_order_relaxed) < N)
        std::this_thread::yield();

    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return static_cast<double>(N) * 1000.0 / ms;
}

// ============================================================================
// STAGE B — Concurrent Task Capacity
//
// Submit all N CapacityTask fibers.  Each fiber spins (cooperative yield) on
// a go-gate until all N are in-flight, then the gate opens and they all
// drain.  Reports RSS before submission, RSS at peak in-flight, and the
// gate-to-done time.
// ============================================================================

CapacityResult run_uxsched_capacity(int N) {
    // Enough per-core queue space to hold all N tasks in the initial burst.
    const int num_cores  = static_cast<int>(std::max(1u, std::thread::hardware_concurrency()));
    // Round up to next power of two ≥ max(128K, ceil(N / num_cores) * 2)
    int per_core_needed = (N / num_cores) * 2 + 2;
    int queue_size = 1 << 18;   // 256K — handles up to ~1M total in-flight
    while (queue_size < per_core_needed && queue_size < (1 << 22))
        queue_size <<= 1;

    std::atomic<bool> go{false};
    std::atomic<int>  completed{0};

    uxsched<CapacityTask> sched(0, queue_size);
    IScheduler* isched = &sched;

    double rss_before = static_cast<double>(get_rss_kb());

    // Submit all N tasks — each will immediately spin in its safepoint loop.
    for (int i = 0; i < N; ++i)
        sched.spawn(CapacityTask{i, &go, &completed, isched}, 64 * 1024, i);

    // Brief settle so all fibers have run at least once and are in _hot_local.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    double rss_peak = static_cast<double>(get_rss_kb());

    auto t0 = std::chrono::steady_clock::now();
    go.store(true, std::memory_order_seq_cst);

    while (completed.load(std::memory_order_relaxed) < N)
        std::this_thread::yield();

    auto t1 = std::chrono::steady_clock::now();
    double gate_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    return {N, rss_before, rss_peak, gate_ms};
}
