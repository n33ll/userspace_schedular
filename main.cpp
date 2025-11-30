#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

#include "schedular/schedular.h"

std::atomic<int> counter{0};

void task_fn(int id) {
    for (int i = 0; i < 5; ++i) {
        int c = ++counter;
        std::cout << "fiber " << id << " tick " << i << " | counter = " << c << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

int main() {
    std::cout << "booting schedular...\n";

    using fn_t = std::function<void()>;

    uxsched<fn_t> sched(0);

    // spawn some fibers
    for (int i = 0; i < 5; ++i) {
        sched.spawn(
            [i]() { task_fn(i); },  // function
            64 * 1024,               // stack size (ignored by boost, but keep for later)
            i                        // fiber id
        );
    }

    std::cout << "done.\n";
    return 0;
}