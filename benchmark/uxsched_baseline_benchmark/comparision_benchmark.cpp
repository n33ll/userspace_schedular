#include <iostream>
#include "workloads.h"
#include "uxsched_benchmark.h"
#include "os_threads_benchmark.h"

int main() {
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║           UXSCHED vs OS THREADS BENCHMARK                  ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
    
    // Order Book
    std::cout << "\n📊 ORDER BOOK WORKLOAD\n";
    std::cout << "──────────────────────────────────────────────────\n";
    run_uxsched_benchmark<OrderBookWorkload>("Order Book", 1000, 5);
    std::cout << "\n";
    run_os_threads_benchmark<OrderBookWorkload>("Order Book", 1000, 5);
    
    // Pipeline
    std::cout << "\n📊 PIPELINE WORKLOAD\n";
    std::cout << "──────────────────────────────────────────────────\n";
    run_uxsched_benchmark<PipelineWorkload>("Pipeline", 1000, 5);
    std::cout << "\n";
    run_os_threads_benchmark<PipelineWorkload>("Pipeline", 1000, 5);
    
    return 0;
}