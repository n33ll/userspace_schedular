#ifndef SCHEDULAR_H
#define SCHEDULAR_H

#include <boost/context/continuation.hpp>
#include <cstddef>
#include <cstdint>
#include <x86intrin.h>
#include <iostream>
#include <thread>
#include <unistd.h>
#include <atomic>
#include "queues/queue.h"
#include "queues/mpmc_queue.h"
#include "runner/runner.h"
#include "loaders/a_hash_loader.h"
#include "fiber_t.h"
#include "stealer/nonnuma_stealer.h"
#include <sched.h>

template <typename F>
class uxsched{
private:
    int _schedular_id;
    int _queue_size;
    int _num_cores;

    std::vector<queue<fiber_t<F>*>*> _queues;
    std::vector<runner<F>*> _workers;
    std::vector<std::thread> _threads;

    nonnuma_stealer<F>* _stealer;
    ah_loader<F>* _loader;

    // Global overflow queue for burst absorption
    mpmc_queue<fiber_t<F>*>* _overflow_q;

    // Telemetry
    std::atomic<uint64_t> _submit_fast_ok{0};
    std::atomic<uint64_t> _submit_overflow_ok{0};
    std::atomic<uint64_t> _submit_retry_count{0};
    std::atomic<uint64_t> _submit_fail{0};

    void pin_thread_to_core(std::thread& thread, int core_id);

public:
    uxsched(int id, int queue_size);
    ~uxsched();

    // Guaranteed delivery
    fiber_t<F>* spawn(F func, int stack_size, int fiber_id, int cycle_budget = 10000);

    // Non-blocking best effort
    bool try_spawn(F func, int stack_size, int fiber_id, int cycle_budget = 10000);

    void safepoint_check(int runner_id);

    // worker helper
    bool try_pop_overflow(fiber_t<F>*& f) {
        return _overflow_q->dequeue(f);
    }

    void print_submit_stats() const {
        std::cout << "[submit] fast_ok=" << _submit_fast_ok.load()
                  << " overflow_ok=" << _submit_overflow_ok.load()
                  << " retries=" << _submit_retry_count.load()
                  << " fail=" << _submit_fail.load()
                  << "\n";
    }
};

template <typename F>
uxsched<F>::uxsched(int id, int queue_size){
    _schedular_id = id;
    _queue_size = queue_size;
    _num_cores = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));

    _queues.resize(_num_cores);
    _workers.resize(_num_cores);
    _threads.resize(_num_cores);

    size_t per_queue_capacity = (_queue_size > 0) ? static_cast<size_t>(_queue_size) : (1 << 16);

    for(int i = 0; i < _num_cores; ++i){
        _queues[i] = new mpmc_queue<fiber_t<F>*>(per_queue_capacity);
    }

    // overflow can be bigger than per-core queue
    _overflow_q = new mpmc_queue<fiber_t<F>*>(per_queue_capacity * 4);

    std::vector<queue<fiber_t<F>*>*>* _qs_ptr = &_queues;
    _loader = new ah_loader<F>(_schedular_id, _qs_ptr);
    _stealer = new nonnuma_stealer<F>(_schedular_id, _qs_ptr);

    for(int i = 0; i < _num_cores; ++i){
        _workers[i] = new runner<F>(i, _queues[i], _stealer, this);
        _threads[i] = std::thread(&runner<F>::run, _workers[i]);
        pin_thread_to_core(_threads[i], i);
    }
}

template <typename F>
uxsched<F>::~uxsched(){
    for(auto q : _queues) q->close();
    _overflow_q->close();

    for(auto& t : _threads){
        if(t.joinable()) t.join();
    }

    for (auto* w : _workers) delete w;
    for (auto* q : _queues) delete q;
    delete _overflow_q;
    delete _stealer;
    delete _loader;
}

template <typename F>
bool uxsched<F>::try_spawn(F func, int stack_size, int fiber_id, int cycle_budget){
    fiber_t<F>* f = new fiber_t<F>();
    f->func = std::move(func);
    f->stack_size = stack_size;
    f->fiber_id = fiber_id;
    f->cycle_budget = cycle_budget;

    // Fast path: hashed queue
    if (_loader->try_load(f)) {
        _submit_fast_ok.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    // Fallback: overflow queue (still non-blocking)
    if (_overflow_q->enqueue(f)) {
        _submit_overflow_ok.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    // could not submit
    delete f;
    _submit_fail.fetch_add(1, std::memory_order_relaxed);
    return false;
}

template <typename F>
fiber_t<F>* uxsched<F>::spawn(F func, int stack_size, int fiber_id, int cycle_budget){
    fiber_t<F>* f = new fiber_t<F>();
    f->func = std::move(func);
    f->stack_size = stack_size;
    f->fiber_id = fiber_id;
    f->cycle_budget = cycle_budget;

    // 1) fast path
    if (_loader->try_load(f)) {
        _submit_fast_ok.fetch_add(1, std::memory_order_relaxed);
        return f;
    }

    // 2) overflow path
    if (_overflow_q->enqueue(f)) {
        _submit_overflow_ok.fetch_add(1, std::memory_order_relaxed);
        return f;
    }

    // 3) bounded retry loop (guaranteed semantics)
    while (true) {
        _submit_retry_count.fetch_add(1, std::memory_order_relaxed);

        if (_loader->try_load(f)) {
            _submit_fast_ok.fetch_add(1, std::memory_order_relaxed);
            return f;
        }
        if (_overflow_q->enqueue(f)) {
            _submit_overflow_ok.fetch_add(1, std::memory_order_relaxed);
            return f;
        }

        _mm_pause();
    }
}

template <typename F>
void uxsched<F>::safepoint_check(int runner_id){
    fiber_t<F>* f = nullptr;
    _workers[runner_id]->get_current_fiber(f);
    if(!f) return;

    uint64_t now = __rdtsc();
    if(now - f->start_tsc < f->cycle_budget) return;

    f->yeild = true;
    f->runner_ctx = std::move(f->runner_ctx).resume();
}

template <typename F>
void uxsched<F>::pin_thread_to_core(std::thread& thread, int core_id){
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    int rc = pthread_setaffinity_np(thread.native_handle(), sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
        std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
    }
}

#endif