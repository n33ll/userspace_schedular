#ifndef RUNNER_H
#define RUNNER_H

#include "../schedular/fiber_t.h"
#include "../schedular/queues/queue.h"
#include <emmintrin.h>
#include <iostream>

template <typename F>
class runner{
private:
    int _runner_id;
    queue<fiber_t<F>*>* _queue;

public: 
    runner(queue<fiber_t<F>*>* q, int id);
    void run();
};

template <typename F>
runner<F>::runner(queue<fiber_t<F>*>* q, int id){
    _queue = q;
    _runner_id = id;
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
            std::cout << "queue empty, pausing\n";
            _mm_pause();
            continue;
        }

        if (f == nullptr) {
            std::cerr << "Error: dequeued nullptr fiber!" << std::endl;
            continue;
        }      
        //execute the fiber
        std::cout << "dequeue fiber: " << f->fiber_id << std::endl;
        f->ctx.resume();
    }
}

#endif