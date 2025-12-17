#ifndef RUNNER_H
#define RUNNER_H

#include "../fiber_t.h"
#include "../queues/queue.h"
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

public: 
    runner(int id, queue<fiber_t<F>*>* queue, nonnuma_stealer<F>* stealer);
    void run();
    void get_current_fiber(fiber_t<F>* &current_fiber);
};

template <typename F>
runner<F>::runner(int id, queue<fiber_t<F>*>* queue, nonnuma_stealer<F>* stealer){
    _queue = queue;
    _stealer = stealer;
    _runner_id = id;
    _current_fiber = nullptr;
    std::cout << "runner initialized \n";
}

template <typename F>
void runner<F>::run(){
    std::cout << "runner running \n";
    while(true){
        fiber_t<F>* f = nullptr;
        
        if(!_queue->dequeue(f)){
            if(_queue->is_closed()){
                std::cout << "runner stops as queue is closed \n";
                return;
            }
            std::cout << "queue empty, stealing\n";

            // get from stealer, if unsuccesfull then pause and retry, otherwise execute f.
            if(!_stealer->get(f)){
                _mm_pause();
                continue;
            }
        }

        if (f == nullptr) {
            std::cerr << "Error: dequeued nullptr fiber!" << std::endl;
            continue;
        }      
        //execute the fiber
        std::cout << "dequeue fiber: " << f->fiber_id << std::endl;
        _current_fiber = f;
        f->start_tsc = __rdtsc();

        // if not initialized we shoud create the context fo the fiber and then re-use that 
        // context on future dequeues of the same fiber. 
        if(!f->context_initialized){
            f->context_initialized = true;
            f->f_ctx = boost::context::callcc([f, this](boost::context::continuation&& runner){
                f->runner_ctx = std::move(runner);

                //std::cout << "running fiber: " << f->fiber_id << std::endl;
                f->func(_runner_id);
                return std::move(runner);
            });
        }else{
            f->f_ctx = std::move(f->f_ctx).resume();
        }

        _current_fiber = nullptr;
        
        if(f->yeild){
            f->yeild = false;
            _queue->enqueue(f);
        }
    }
}

template <typename F>
void runner<F>::get_current_fiber(fiber_t<F>* &current_fiber){
    current_fiber = _current_fiber;
}

#endif