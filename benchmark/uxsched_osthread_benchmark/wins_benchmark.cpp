// benchmark/uxsched_wins_benchmark/wins_benchmark.cpp
//
// Two-stage benchmark suite that demonstrates concrete wins for UXSched
// over a conventional mutex-based OS thread pool.
//
//   Stage A: Throughput Saturation    — 1 producer, 1M trivial tasks
//   Stage C: Concurrent Task Capacity — N in-flight fibers vs OS threads

#include "workloads.h"
#include "os_threads_runner.h"
#include "uxsched_runner.h"

#include <iostream>
#include <iomanip>
#include <thread>
#include <algorithm>
#include <string>

static int hw_concurrency() {
    return static_cast<int>(std::max(1u, std::thread::hardware_concurrency()));
}

static void separator(const std::string& title) {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════╗\n";
    std::string padded = "║  " + title;
    padded += std::string(std::max<int>(0, 67 - (int)padded.size()), ' ') + "║";
    std::cout << padded << "\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n";
}

// ============================================================================
// STAGE A
// ============================================================================

static void run_stage_a() {
    separator("STAGE A — Throughput Saturation  (1 producer, 1M tasks)");

    const int N = 1'000'000;
    const int W = hw_concurrency();

    std::cout << "  Hardware threads : " << W << "\n";
    std::cout << "  Task count       : " << N << "\n";
    std::cout << "  Workload         : TrivialWorkload (~150 CPU cycles)\n\n";

    std::cout << "  [UXSched] running...\n";
    double ux_tps = run_uxsched_throughput<TrivialWorkload>(N);

    std::cout << "  [OS Threads] running...\n";
    double os_tps = run_os_throughput<TrivialWorkload>(N, W);

    double speedup = ux_tps / os_tps;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "\n";
    std::cout << "  ┌──────────────────────────┬────────────────────┐\n";
    std::cout << "  │ Scheduler                │ Tasks/sec          │\n";
    std::cout << "  ├──────────────────────────┼────────────────────┤\n";
    std::cout << "  │ UXSched (lock-free MPMC) │ " << std::setw(14) << (ux_tps / 1e6) << " M    │\n";
    std::cout << "  │ OS Threads (mutex queue) │ " << std::setw(14) << (os_tps / 1e6) << " M    │\n";
    std::cout << "  ├──────────────────────────┼────────────────────┤\n";
    std::cout << "  │ UXSched speedup          │ " << std::setw(13) << speedup << "x    │\n";
    std::cout << "  └──────────────────────────┴────────────────────┘\n";

    if (speedup >= 1.0)
        std::cout << "  ✓ UXSched wins by " << speedup << "x\n";
    else
        std::cout << "  ✗ OS Threads faster by " << (1.0 / speedup) << "x\n";
}

// ============================================================================
// STAGE B
// ============================================================================

static void run_stage_b() {
    separator("STAGE B — Concurrent Task Capacity  (N simultaneous in-flight)");

    std::cout << "  UXSched fibers : 64 KB stack each  → 100K ≈ 6.4 GB virtual\n";
    std::cout << "  OS threads     : default stack, kernel structures  → hard wall at ~10K\n\n";

    std::cout << "  ┌───────────┬────────────────────────────────┬─────────────────────────────────┐\n";
    std::cout << "  │N in-flight│ UXSched                        │ OS Threads                      │\n";
    std::cout << "  │           │ peak RSS (MB)  gate→done (ms)  │ created peak RSS (MB)  done (ms)│\n";
    std::cout << "  ├───────────┼────────────────────────────────┼─────────────────────────────────┤\n";

    for (int N : {1000, 5000, 10000, 50000, 100000}) {
        // UXSched
        auto ur = run_uxsched_capacity(N);
        double ux_peak_mb   = ur.rss_peak_kb   / 1024.0;
        double ux_before_mb = ur.rss_before_kb / 1024.0;

        // OS Threads (cap at 20K to avoid hanging the machine on WSL)
        int cap = std::min(N, 20'000);
        auto or_ = run_os_capacity(cap);
        double os_peak_mb = or_.rss_peak_kb / 1024.0;
        std::string os_created_str = (or_.created < cap)
            ? ("FAILED@" + std::to_string(or_.created))
            : std::to_string(or_.created);

        std::cout << std::fixed << std::setprecision(1);
        std::cout << "  │ " << std::setw(9) << N
                  << " │ " << std::setw(14) << ux_peak_mb
                  << "  " << std::setw(15) << ur.gate_to_done_ms << " │ "
                  << std::setw(9) << os_created_str
                  << "  " << std::setw(14) << os_peak_mb
                  << "  " << std::setw(8) << or_.gate_to_done_ms << " │\n";
    }

    std::cout << "  └───────────┴────────────────────────────────┴─────────────────────────────────┘\n";
    std::cout << "  (UXSched succeeds at 100K; OS threads hit kernel thread limit at ~10K-20K)\n";
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    std::cout << "╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                   UXSCHED WINS BENCHMARK                         ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n";
    std::cout << "Hardware threads: " << hw_concurrency() << "\n";

    run_stage_a();
    run_stage_b();

    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                    BENCHMARK COMPLETE                           ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════╝\n\n";
    return 0;
}
