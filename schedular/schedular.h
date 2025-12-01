#ifndef SCHEDULAR_H
#define SCHEDULAR_H

#include <boost/context/continuation.hpp>
#include <cstddef>
#include <cstdint>
#include <x86intrin.h>
#include <iostream>
#include <thread>
#include "queues/queue.h"
#include "queues/spsc_queue.h"
#include "../runner/runner.h"
#include "fiber_t.h"

template <typename F>
class uxsched{
private:
    int _schedular_id;
    // queue
    queue<fiber_t<F>*>* _queue;

    // runner
    runner<F>* _worker;
    std::thread _worker_thread;

public:
    // prepare the schedular
    uxsched(int id);
    ~uxsched();
    // spawn a fiber and add it to runqueue
    fiber_t<F>* spawn(F func, int stack_size, int fiber_id, int cycle_budget = 10000);

    /*  safepoint check that gets added by the user inside the fiber.
        this is used to add a safepoint where the schedular can check if 
        the fiber exceeds the execution time designated.
    */ 
    void safepoint_check();
};

template <typename F>
uxsched<F>::uxsched(int id){
    //initialize the queue
    _schedular_id = id;
    _queue = new spsc_queue<fiber_t<F>*>(10);

    //start the runner
    _worker = new runner<F>(_queue , _schedular_id);
    _worker_thread = std::thread(&runner<F>::run, _worker);

    std::cout<<"schedular initializing done \n";
}

template <typename F>
uxsched<F>::~uxsched(){
    _queue->close();
    if (_worker_thread.joinable()){_worker_thread.join();}
}

template <typename F>
fiber_t<F>* uxsched<F>::spawn(F func, int stack_size, int fiber_id, int cycle_budget){

    //add it to the runqueue
    fiber_t<F>* f = new fiber_t<F>();

    f->func = std::move(func);
    f->stack_size = stack_size;
    f->fiber_id = fiber_id;
    f->cycle_budget = cycle_budget;

    // enqueue this f
    std::cout << "enqueue fiber: " << f->fiber_id << std::endl;
    _queue->enqueue(f);
    return f;
}

template <typename F>
void uxsched<F>::safepoint_check(){
    fiber_t<F>* f = nullptr;
    _worker->get_current_fiber(f);

    if(!f){return;}
    
    // yeild only when the budget is exceeded;
    uint64_t now = __rdtsc();
    if(now - f->start_tsc < f->cycle_budget){
        return;
    }

    f->yeild = true;
    f->runner_ctx = std::move(f->runner_ctx).resume();
    
}

#endif