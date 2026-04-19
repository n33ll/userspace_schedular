#pragma once
#include "workloads.h"
#include "stats_printer.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <x86intrin.h>

struct os_task {
    int task_id;
    uint64_t spawn_time;
};

struct NullScheduler {
    void safepoint_check(int) {}
};

class TaskQueue {
private:
    std::queue<os_task*> q_;
    std::mutex m_;
    std::condition_variable cv_;
    bool closed_ = false;

public:
    void enqueue(os_task* t) {
        {
            std::lock_guard<std::mutex> lock(m_);
            q_.push(t);
        }
        cv_.notify_one();
    }

    os_task* dequeue() {
        std::unique_lock<std::mutex> lock(m_);
        cv_.wait(lock, [this]{ return !q_.empty() || closed_; });
        if (q_.empty()) return nullptr;
        os_task* t = q_.front();
        q_.pop();
        return t;
    }

    void close() {
        {
            std::lock_guard<std::mutex> lock(m_);
            closed_ = true;
        }
        cv_.notify_all();
    }
};

template<typename WorkloadType>
std::vector<uint64_t> run_os_threads_benchmark(const std::string& sched_name,
                                                const std::string& workload_name,
                                                int num_tasks,
                                                int duration_seconds) {
    std::cout << "=== " << workload_name << " (OS Thread Pool) ===" << std::endl;
    std::cout << "Tasks target: " << num_tasks << std::endl;

    int num_cores = std::max(1u, std::thread::hardware_concurrency());
    TaskQueue shared_queue;

    std::vector<std::thread> threads;
    std::vector<uint64_t> latencies;
    latencies.reserve(2'000'000);
    std::mutex latencies_mutex;

    auto start_time = std::chrono::high_resolution_clock::now();
    auto end_deadline = start_time + std::chrono::seconds(duration_seconds);

    auto worker = [&](int thread_id) {
        NullScheduler null_sched;
        while (true) {
            os_task* t = shared_queue.dequeue();
            if (t == nullptr) return;

            WorkloadType::execute(t->task_id, &null_sched, thread_id);

            uint64_t end_time = __rdtsc();
            {
                std::lock_guard<std::mutex> lock(latencies_mutex);
                latencies.push_back(end_time - t->spawn_time);
            }
            delete t;
        }
    };

    for (int i = 0; i < num_cores; ++i) {
        threads.emplace_back(worker, i);
    }

    int task_id = 0;
    int submitted = 0;
    int batch_size = num_cores * 16;

    while (std::chrono::high_resolution_clock::now() < end_deadline) {
        for (int i = 0; i < batch_size &&
                        std::chrono::high_resolution_clock::now() < end_deadline; ++i) {
            uint64_t spawn_time = __rdtsc();
            shared_queue.enqueue(new os_task{task_id, spawn_time});
            ++task_id;
            ++submitted;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    shared_queue.close();

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    std::cout << "Submitted: " << submitted << "\n";
    print_stats(sched_name, workload_name, latencies);
    return latencies;
}