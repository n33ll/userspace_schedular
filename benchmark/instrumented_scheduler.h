#pragma once
#include "../schedular/schedular.h"
#include "perf_counter.h"
#include <iostream>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <thread>
#include <x86intrin.h>

template <typename F>
class InstrumentedScheduler : public uxsched<F> {
private:
    PerfCounter task_runtime_;
    std::unordered_map<int, uint64_t> fiber_start_times_;
    std::mutex timing_mutex_;
    std::atomic<bool> shutting_down_{false};
    
public:
    InstrumentedScheduler(int id, int queue_size) : uxsched<F>(id, queue_size) {}
    
    ~InstrumentedScheduler() {
        shutting_down_.store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        fiber_start_times_.clear();
    }
    
    // Start timing for a task with fiber_id
    void task_start(int fiber_id) {
        if (shutting_down_.load()) return;
        uint64_t start_time = __rdtsc();
        std::lock_guard<std::mutex> lock(timing_mutex_);
        fiber_start_times_[fiber_id] = start_time;
        // Debug: uncomment to see timing calls
        // std::cout << "START fiber " << fiber_id << std::endl;
    }
    
    // End timing for a task with fiber_id
    void task_end(int fiber_id) {
        if (shutting_down_.load()) return;
        uint64_t end_time = __rdtsc();
        std::lock_guard<std::mutex> lock(timing_mutex_);
        auto it = fiber_start_times_.find(fiber_id);
        if (it != fiber_start_times_.end()) {
            uint64_t runtime = end_time - it->second;
            task_runtime_.add_sample(runtime);
            fiber_start_times_.erase(it);
            // Debug: uncomment to see timing calls  
            // std::cout << "END fiber " << fiber_id << " runtime: " << runtime << std::endl;
        }
    }
    
    void print_stats() {
        task_runtime_.finalize_samples();
        
        std::cout << "=== Performance Statistics ===" << std::endl;
        std::cout << "Task Runtime (cycles) [" << task_runtime_.count() << " samples]:" << std::endl;
        std::cout << "  P50:  " << task_runtime_.p50() << std::endl;
        std::cout << "  P99:  " << task_runtime_.p99() << std::endl;
        std::cout << "  P999: " << task_runtime_.p999() << std::endl;
        std::cout << "  Min:  " << task_runtime_.min() << std::endl;
        std::cout << "  Max:  " << task_runtime_.max() << std::endl;
    }
    
    void clear_stats() {
        task_runtime_.clear();
        fiber_start_times_.clear();
    }
};