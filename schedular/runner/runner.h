#ifndef RUNNER_H
#define RUNNER_H

#include "../fiber_t.h"
#include "../queues/queue.h"
#include <atomic>
#include <emmintrin.h>
#include <iostream>
#include "../stealer/nonnuma_stealer.h"

template <typename F>
class runner{
private:
    int _runner_id;
    queue<fiber_t<F>*>* _queue;
    nonnuma_stealer<F>* _stealer;
    fiber_t<F>* _current_fiber;
    std::atomic<bool> _running;

public: 
    runner(int id, queue<fiber_t<F>*>* queue, nonnuma_stealer<F>* stealer);
    void run();
    void close();
    bool is_closed();
    void get_current_fiber(fiber_t<F>* &current_fiber);
};

template <typename F>
runner<F>::runner(int id, queue<fiber_t<F>*>* queue, nonnuma_stealer<F>* stealer){
    _queue = queue;
    _stealer = stealer;
    _runner_id = id;
    _current_fiber = nullptr;
    _running.store(true,std::memory_order_release);
    std::cout << "RUNNERS initialized \n";
}

template <typename F>
void runner<F>::run(){
    //std::cout << "runner " << _runner_id << " starting \n";
    while(_running.load(std::memory_order_acquire)){
        fiber_t<F>* f = nullptr;
        
        if(!_queue->dequeue(f)){
            if(_queue->is_closed()){
                //std::cout << "runner " << _runner_id << " stops as queue is closed \n";
                return;
            }

            if(!_stealer->get(f)){
                _mm_pause();
                continue;
            }
        }

        if (f == nullptr) {
            _mm_pause();
            continue;
        }      
        //execute the fiber
        //std::cout << "dequeue fiber: " << f->fiber_id << std::endl;
        _current_fiber = f;
        f->start_tsc = __rdtsc();

        // if not initialized we shoud create the context fo the fiber and then re-use that 
        // context on future dequeues of the same fiber. 
        if(!f->context_initialized){
            //std::cout << "Creating context for fiber " << f->fiber_id << std::endl;
            f->context_initialized = true;
            f->f_ctx = boost::context::callcc([f, this](boost::context::continuation&& runner){
                f->runner_ctx = std::move(runner);

                f->func(_runner_id);
                
                // When fiber completes, return control to runner
                return std::move(f->runner_ctx);
            });
        }else{
            // Resume the fiber context and get back the updated runner context
            f->f_ctx = std::move(f->f_ctx).resume();
        }

        _current_fiber = nullptr;
        
        // only re-enqueue if fiber yielded
        if(f->yeild){
            f->yeild = false;
            _queue->enqueue(f);
            //std::cout << "FIBER: " << f->fiber_id << " yeilded\n";
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