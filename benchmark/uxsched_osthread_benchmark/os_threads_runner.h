// benchmark/uxsched_wins_benchmark/os_threads_runner.h
#pragma once
#include "workloads.h"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <atomic>
#include <chrono>
#include <iostream>
#include <fstream>
#include <string>
#include <cstdio>
#include <algorithm>
#include <stdexcept>
#include <system_error>
#include <pthread.h>

// ============================================================================
// RSS helper (Linux /proc/self/status)
// ============================================================================

inline long get_rss_kb() {
    std::ifstream f("/proc/self/status");
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            long rss = 0;
            std::sscanf(line.c_str(), "VmRSS: %ld kB", &rss);
            return rss;
        }
    }
    return 0;
}

// ============================================================================
// MutexTaskQueue — single shared mutex + condition_variable queue.
// Enqueues ints (task_ids).  This is the bottleneck we are measuring against
// UXSched's per-core lock-free queues.
// ============================================================================

class MutexTaskQueue {
    std::queue<int>         q_;
    std::mutex              m_;
    std::condition_variable cv_;
    bool                    closed_{false};

public:
    void enqueue(int v) {
        {
            std::lock_guard<std::mutex> lk(m_);
            q_.push(v);
        }
        cv_.notify_one();
    }

    // Returns -1 on close-and-empty.
    int dequeue() {
        std::unique_lock<std::mutex> lk(m_);
        cv_.wait(lk, [this]{ return !q_.empty() || closed_; });
        if (q_.empty()) return -1;
        int v = q_.front();
        q_.pop();
        return v;
    }

    void close() {
        { std::lock_guard<std::mutex> lk(m_); closed_ = true; }
        cv_.notify_all();
    }
};

// ============================================================================
// STAGE A — Throughput Saturation (1 producer, N tasks)
//
// Measures wall-clock time for N trivial tasks to complete through a mutex
// thread pool.  One producer thread (main) enqueues all N tasks; num_workers
// worker threads dequeue and execute.
// Returns tasks/sec.
// ============================================================================

template <typename WorkloadType>
double run_os_throughput(int N, int num_workers) {
    MutexTaskQueue queue;
    std::atomic<int> completed{0};

    auto worker = [&]() {
        while (true) {
            int id = queue.dequeue();
            if (id < 0) break;
            WorkloadType::execute(id);
            completed.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(num_workers);
    for (int i = 0; i < num_workers; ++i)
        threads.emplace_back(worker);

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < N; ++i) queue.enqueue(i);

    // Wait for all tasks to complete, then signal workers to stop.
    while (completed.load(std::memory_order_relaxed) < N)
        std::this_thread::yield();

    auto t1 = std::chrono::steady_clock::now();
    queue.close();
    for (auto& t : threads) t.join();

    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return static_cast<double>(N) * 1000.0 / ms;
}

// ============================================================================
// STAGE B — Concurrent Task Capacity
//
// Tries to create `target_n` OS threads simultaneously, each spinning until
// an atomic go flag is set.  Reports how many threads were actually created
// (thread creation fails with EAGAIN beyond the OS limit), peak RSS, and
// wall-clock time to complete once the gate opens.
//
// Uses pthread_create directly with a 64 KB stack — matching UXSched's fiber
// stack size — so the comparison is stack-neutral and any failure reflects
// true kernel per-thread resource exhaustion (task_struct, kernel stack,
// scheduler structures) rather than virtual address space from 8 MB defaults.
// ============================================================================

struct CapacityResult {
    int    created;
    double rss_before_kb;
    double rss_peak_kb;
    double gate_to_done_ms;
};

struct OsTaskArg {
    std::atomic<bool>* go;
    std::atomic<int>*  completed;
    int                id;
};

static void* os_task_fn(void* arg_ptr) {
    auto* a = static_cast<OsTaskArg*>(arg_ptr);
    while (!a->go->load(std::memory_order_relaxed))
        sched_yield();
    volatile uint64_t x = static_cast<uint64_t>(a->id);
    for (int i = 0; i < 50; ++i) x = x * 1103515245ULL + 12345ULL;
    (void)x;
    a->completed->fetch_add(1, std::memory_order_relaxed);
    delete a;
    return nullptr;
}

CapacityResult run_os_capacity(int target_n) {
    std::atomic<bool> go{false};
    std::atomic<int>  completed{0};

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    // Match UXSched fiber stack size for a stack-neutral comparison.
    pthread_attr_setstacksize(&attr, 64 * 1024);

    double rss_before = static_cast<double>(get_rss_kb());

    std::vector<pthread_t> tids;
    tids.reserve(target_n);
    int created = 0;
    for (int i = 0; i < target_n; ++i) {
        auto* arg = new OsTaskArg{&go, &completed, i};
        pthread_t tid;
        int rc = pthread_create(&tid, &attr, os_task_fn, arg);
        if (rc != 0) {
            delete arg;
            break;
        }
        tids.push_back(tid);
        ++created;
    }
    pthread_attr_destroy(&attr);

    // Brief settle — let all spawned threads reach their spin loop.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    double rss_peak = static_cast<double>(get_rss_kb());

    auto t0 = std::chrono::steady_clock::now();
    go.store(true, std::memory_order_seq_cst);

    while (completed.load(std::memory_order_relaxed) < created)
        std::this_thread::yield();

    auto t1 = std::chrono::steady_clock::now();
    double gate_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    for (pthread_t tid : tids) pthread_join(tid, nullptr);

    return {created, rss_before, rss_peak, gate_ms};
}
