#include "../../schedular/schedular.h"
#include "../uxsched_baseline_benchmark/workloads.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>


using fn_t = std::function<void(int)>;

template <typename WorkloadType>
void benchmark_runner(size_t queue_size, int duration_seconds) {
    using clock = std::chrono::steady_clock;

    // Keep this exactly as your scheduler expects.
    uxsched<fn_t> sched(0, queue_size);

    std::atomic<uint64_t> submitted{0};

    const auto end_deadline = clock::now() + std::chrono::seconds(duration_seconds);

    // No sleep_for, no inner now() checks.
    while (clock::now() < end_deadline) {
        const int this_task_id = static_cast<int>(submitted.fetch_add(1, std::memory_order_relaxed));
        sched.spawn(
            [this_task_id, &sched](int runner_id) {
                WorkloadType::execute(this_task_id, &sched, runner_id);
            },
            64 * 1024,
            this_task_id
        );
    }

    // Optional short settle period so enqueued tasks run before process exits.
    // Keep tiny to avoid timer-heavy profile pollution.
    const auto settle_until = clock::now() + std::chrono::milliseconds(200);
    while (clock::now() < settle_until) {
        asm volatile("pause" ::: "memory");
    }

    std::cout << "submitted=" << submitted.load(std::memory_order_relaxed) << "\n";
}

int main() {
    constexpr size_t queue_size = 1 << 12;
    constexpr int duration_seconds = 10; // shorter, hotter capture
    std::cout << "Uxsched is running orderbook workload for flamegraph" << std::endl;
    benchmark_runner<OrderBookWorkload>(queue_size, duration_seconds);
    std::cout << "done running!!!!!!!!!!!" << std::endl;
    return 0;
}