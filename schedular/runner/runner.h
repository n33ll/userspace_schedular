#ifndef RUNNER_H
#define RUNNER_H

#include "../fiber_t.h"
#include "../queues/queue.h"
#include <atomic>
#include <deque>
#include <emmintrin.h>
#include "../stealer/nonnuma_stealer.h"

// forward declare scheduler
template <typename F>
class uxsched;

template <typename F>
class runner{
private:
    int _runner_id;
    queue<fiber_t<F>*>* _queue;
    nonnuma_stealer<F>* _stealer;
    uxsched<F>* _sched;
    fiber_t<F>* _current_fiber;
    std::atomic<bool> _running;

    // hot queue for yielded fibers (latency-first)
    std::deque<fiber_t<F>*> _hot_local;

    // fairness: don't starve cold queues
    int _hot_budget = 8;
    int _hot_used = 0;

public:
    runner(int id, queue<fiber_t<F>*>* queue, nonnuma_stealer<F>* stealer, uxsched<F>* sched);
    void run();
    void close();
    bool is_closed();
    void get_current_fiber(fiber_t<F>* &current_fiber);
};

template <typename F>
runner<F>::runner(int id, queue<fiber_t<F>*>* queue, nonnuma_stealer<F>* stealer, uxsched<F>* sched){
    _queue = queue;
    _stealer = stealer;
    _sched = sched;
    _runner_id = id;
    _current_fiber = nullptr;
    _running.store(true, std::memory_order_release);
}

template <typename F>
void runner<F>::run(){
    while(_running.load(std::memory_order_acquire)){
        fiber_t<F>* f = nullptr;

        // 1) hot yielded fibers first (with fairness cap)
        if (!_hot_local.empty() && _hot_used < _hot_budget) {
            f = _hot_local.back();
            _hot_local.pop_back();
            _hot_used++;
        } else {
            _hot_used = 0;

            // 2) local queue
            if(!_queue->dequeue(f)){
                if(_queue->is_closed()){
                    return;
                }

                // 3) global overflow queue
                if(!_sched->try_pop_overflow(f)){
                    // 4) work stealing
                    if(!_stealer->get(f)){
                        _mm_pause();
                        continue;
                    }
                }
            }
        }

        if (f == nullptr) {
            _mm_pause();
            continue;
        }

        _current_fiber = f;
        f->start_tsc = __rdtsc();

        if(!f->context_initialized){
            f->context_initialized = true;
            f->f_ctx = boost::context::callcc([f, this](boost::context::continuation&& runner_ctx){
                f->runner_ctx = std::move(runner_ctx);
                f->func(_runner_id);
                return std::move(f->runner_ctx);
            });
        } else {
            f->f_ctx = std::move(f->f_ctx).resume();
        }

        _current_fiber = nullptr;

        // Yielded fibers go to hot queue, not cold tail queue
        if(f->yeild){
            f->yeild = false;
            _hot_local.push_back(f);
        } else {
            // TODO Day2: reclaim/pool fiber here to avoid leaks
            delete f;  // only if safe with your lifecycle model
        }
    }
}

template <typename F>
void runner<F>::get_current_fiber(fiber_t<F>* &current_fiber){
    current_fiber = _current_fiber;
}

template <typename F>
void runner<F>::close(){
    _running.store(false, std::memory_order_release);
}

template <typename F>
bool runner<F>::is_closed(){
    return !_running.load(std::memory_order_acquire);
}

#endif