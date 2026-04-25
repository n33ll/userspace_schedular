#include <iostream>
#include "workloads.h"
#include "stats_printer.h"
#include "uxsched_benchmark.h"
#include "os_threads_benchmark.h"

int main() {
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║           UXSCHED vs OS THREADS BENCHMARK                  ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
    
    TSCCalibration::calibrate();
    
    // ────────────────────────────────────────────────────────────────
    // ORDER BOOK WORKLOAD
    // ────────────────────────────────────────────────────────────────
    std::cout << "\n🔥 ORDER BOOK WORKLOAD\n";
    std::cout << "════════════════════════════════════════════════════════════\n";
    
    auto ob_uxsched_latencies = run_uxsched_benchmark<OrderBookWorkload>("UXSched", "OrderBook", 10000, 50);
    std::cout << "\n";
    auto ob_osthread_latencies = run_os_threads_benchmark<OrderBookWorkload>("OS Threads", "OrderBook", 10000, 50);
    
    BenchmarkResult ob_uxsched = {"UXSched", "OrderBook", ob_uxsched_latencies};
    BenchmarkResult ob_osthread = {"OS Threads", "OrderBook", ob_osthread_latencies};
    print_comparison("Order Book", ob_uxsched, ob_osthread);
    
    // ────────────────────────────────────────────────────────────────
    // PIPELINE WORKLOAD
    // ────────────────────────────────────────────────────────────────
    std::cout << "\n🔥 PIPELINE WORKLOAD\n";
    std::cout << "════════════════════════════════════════════════════════════\n";
    
    auto pl_uxsched_latencies = run_uxsched_benchmark<PipelineWorkload>("UXSched", "Pipeline", 10000, 50);
    std::cout << "\n";
    auto pl_osthread_latencies = run_os_threads_benchmark<PipelineWorkload>("OS Threads", "Pipeline", 10000, 50);
    
    BenchmarkResult pl_uxsched = {"UXSched", "Pipeline", pl_uxsched_latencies};
    BenchmarkResult pl_osthread = {"OS Threads", "Pipeline", pl_osthread_latencies};
    print_comparison("Pipeline", pl_uxsched, pl_osthread);
    
    // ────────────────────────────────────────────────────────────────
    // FINAL SUMMARY
    // ────────────────────────────────────────────────────────────────
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                    BENCHMARK COMPLETE ✓                    ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
    
    return 0;
}