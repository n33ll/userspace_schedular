// benchmark/workloads.h

#pragma once
#include <cstdint>
#include <x86intrin.h>

// ============================================================================
// WORKLOAD 1: ORDER BOOK
// ============================================================================

struct OrderBookWorkload {
    template <typename Scheduler>
    static void execute(int task_id, Scheduler* sched, int runner_id) {
        // PHASE 1: Parse & Validate (100-200 cycles)
        volatile uint64_t order_id = task_id * 1000000ull;
        volatile uint32_t quantity = 100 + (task_id * 17) % 10000;
        volatile uint64_t price = 150000 + (task_id * 31) % 50000;
        
        // PHASE 2: Order Book Lookup (500-5000 cycles)
        volatile uint64_t search_key = price * quantity;
        for (int i = 0; i < 500 + (task_id % 5000); ++i) {
            search_key = (search_key * 1103515245 + 12345) % (1ull << 31);
        }
        
        // Direct call - no function pointer needed
        //if (sched) sched->safepoint_check(runner_id);
        
        // PHASE 3: Execute Match (5000-50000 cycles)
        volatile uint64_t match_result = 0;
        for (int i = 0; i < 10000 + (task_id * 13) % 40000; ++i) {
            match_result = (match_result * 1103515245 + 12345) ^ (task_id + i);
            if (i % 10000 == 0 && sched) {
                sched->safepoint_check(runner_id);
            }
        }
        
        // PHASE 4: Account Update (1000-5000 cycles)
        volatile uint64_t new_balance = order_id + match_result;
        
        // PHASE 5: Response Generation (100-500 cycles)
        volatile uint64_t response = new_balance % 1000000;
    }
};

// ============================================================================
// WORKLOAD 2: PIPELINE
// ============================================================================

struct PipelineWorkload {
    template <typename Scheduler>
    static void execute(int task_id, Scheduler* sched, int runner_id) {
        // STAGE 1: Parse (1000-5000 cycles)
        volatile uint64_t parsed = 0;
        for (int i = 0; i < 1000 + (task_id % 4000); ++i) {
            parsed = parsed * 31 + i;
        }
        //if (sched) sched->safepoint_check(runner_id);
        
        // STAGE 2: Validate (2000-8000 cycles)
        volatile bool valid = (parsed % 100) != 0;
        for (int i = 0; i < 2000 + (task_id % 6000); ++i) {
            valid = (valid && i < 5000);
        }
        if (!valid) return;
        //if (sched) sched->safepoint_check(runner_id);
        
        // STAGE 3: Process (10000-50000 cycles)
        volatile uint64_t result = 0;
        int iterations = 10000 + (task_id * 13) % 40000;
        for (int i = 0; i < iterations; ++i) {
            result = (result * 1103515245 + 12345) ^ (task_id + i);
            if (i % 5000 == 0 && sched) {
                //sched->safepoint_check(runner_id);
            }
        }
        
        // STAGE 4: Serialize (5000-15000 cycles)
        volatile uint64_t serialized = 0;
        for (int i = 0; i < 5000 + (task_id % 10000); ++i) {
            serialized = serialized * 17 + (result + i) % 256;
        }
        //if (sched) sched->safepoint_check(runner_id);
    }
};