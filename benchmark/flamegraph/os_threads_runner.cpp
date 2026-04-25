#include "../uxsched_baseline_benchmark/os_threads_benchmark.h"
#include "../uxsched_baseline_benchmark/workloads.h"
#include <iostream>
#include <cstdlib>

int main() {
    const int num_tasks = 10000;
    const int duration_seconds = 5;

    std::cout << "[runner] os_threads_runner\n";
    run_os_threads_benchmark<PipelineWorkload>(
        "OS Threads",
        "Pipeline",
        num_tasks,
        duration_seconds
    );
    return 0;
}