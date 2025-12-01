#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

#include "schedular/schedular.h"

std::atomic<int> counter{0};
using fn_t = std::function<void()>;

void task_fn(int id, uxsched<fn_t>* sched) {
    int i = 0;
    while (i < 5) {
        int c = ++counter;
        std::cout << "fiber " << id << " tick " << i << " | counter = " << c << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        ++i;
        sched->safepoint_check();
    }
}

int main() {
    std::cout << "booting schedular...\n";

    uxsched<fn_t> sched(0);

    // spawn some fibers
    for (int i = 0; i < 5; ++i) {
        sched.spawn(
            [i, &sched]() { task_fn(i, &sched); },  // function
            64 * 1024,               // stack size (ignored by boost, but keep for later)
            i                        // fiber id
        );
    }

    std::cout << "done.\n";
    return 0;
}