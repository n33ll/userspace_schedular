// benchmark/uxsched_wins_benchmark/workloads.h
#pragma once
#include <cstdint>

// TrivialWorkload: 50 multiply-xor iterations — ~150-200 CPU cycles of pure
// register work.  So light that per-task scheduler overhead (queue ops,
// context-switch cost) dominates.  This is deliberately chosen to maximise
// the signal of the lock-free queue advantage in Stages A and B.
struct TrivialWorkload {
    static void execute(int task_id) {
        volatile uint64_t x = static_cast<uint64_t>(task_id) * 6364136223846793005ULL
                              + 1442695040888963407ULL;
        for (int i = 0; i < 50; ++i)
            x = x * 1103515245ULL + 12345ULL;
        (void)x;
    }
};
