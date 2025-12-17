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
#include "runner/runner.h"
#include "loaders/rr_loader.h"
#include "loaders/a_hash_loader.h"
#include "fiber_t.h"
#include "stealer/nonnuma_stealer.h"

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

    void pin_thread_to_core(std::thread& thread, int core_id);

public:
    // prepare the schedular
    uxsched(int id, int queue_size);
    ~uxsched();
    // spawn a fiber and add it to runqueue
    fiber_t<F>* spawn(F func, int stack_size, int fiber_id, int cycle_budget = 10000);

    /*  safepoint check that gets added by the user inside the fiber.
        this is used to add a safepoint where the schedular can check if 
        the fiber exceeds the execution time designated.
    */ 
    void safepoint_check(int runner_id);
};

template <typename F>
uxsched<F>::uxsched(int id, int queue_size){
    _schedular_id = id;
    _queue_size = queue_size;

    _num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    _queues.resize(_num_cores);
    _workers.resize(_num_cores);
    _threads.resize(_num_cores);

    for(int i = 0; i < _num_cores; ++i){
        _queues[i] = new spsc_queue<fiber_t<F>*>(_queue_size);
    }

    std::vector<queue<fiber_t<F>*>*>* _qs_ptr = &_queues;
    _loader = new ah_loader<F>(_schedular_id, _qs_ptr);
    _stealer = new nonnuma_stealer<F>(_schedular_id, _qs_ptr);

    for(int i{0};i< _num_cores; ++i){
        _workers[i] = new runner<F>(i, _queues[i], _stealer);
        _threads[i] = std::thread(&runner<F>::run, _workers[i]);
        pin_thread_to_core(_threads[i], i); // Pin each thread to its core
    }

    std::cout<<"schedular initializing done \n";
}

template <typename F>
uxsched<F>::~uxsched(){
    /* this is being commented out since after schedular is deleted the queue 
    should not close without checks as there may still be tasks */
    //for(auto q : _queues){q->close();}
    
    //weird why for(auto t : _threads) doesnt work
    for(auto t = _threads.begin();t!=_threads.end();t++){
        if(t->joinable()){t->join();}
    }
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
    _loader->load(f);
    return f;
}

template <typename F>
void uxsched<F>::safepoint_check(int runner_id){
    fiber_t<F>* f = nullptr;

    // crux of the issue :
    // we donot know from what worker safepoint_check is being called.
    _workers[runner_id]->get_current_fiber(f);

    if(!f){return;}
    
    // yeild only when the budget is exceeded;
    uint64_t now = __rdtsc();
    if(now - f->start_tsc < f->cycle_budget){
        return;
    }

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