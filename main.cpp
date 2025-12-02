#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

#include "schedular/schedular.h"

std::atomic<int> counter{0};
using fn_t = std::function<void(int)>;

void task_fn(int id, uxsched<fn_t>* sched, int runner_id) {
    int i = 0;
    while (i < 5) {
        int c = ++counter;
        std::cout << "fiber " << id << " tick " << i << " | counter = " << c << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        ++i;
        sched->safepoint_check(runner_id);
    }
}

int main() {
    std::cout << "booting schedular...\n";
    int queue_size = 5;
    uxsched<fn_t> sched(0, queue_size);

    // spawn some fibers
    for (int i = 0; i < 5; ++i) {
        sched.spawn(
            [i, &sched](int runner_id) { task_fn(i, &sched, runner_id); },  // function
            64 * 1024,             // stack size (ignored by boost, but keep for later)
            i                        // fiber id
        );
    }

    std::cout << "done.\n";
    return 0;
}