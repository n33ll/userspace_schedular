#ifndef SCHEDULAR_H
#define SCHEDULAR_H

#include <boost/context/continuation.hpp>
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
    fiber_t<F>* spawn(F func, int stack_size, int fiber_id);

    // halt current fiber and run the next one.
    void yeild();
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
fiber_t<F>* uxsched<F>::spawn(F func, int stack_size, int fiber_id){

    //add it to the runqueue
    fiber_t<F>* f = new fiber_t<F>();

    f->func = std::move(func);
    f->stack_size = stack_size;
    f->fiber_id = fiber_id;

    f->ctx = boost::context::callcc([f](boost::context::continuation&& parent){
        std::cout << "making fiber: " << f->fiber_id << std::endl;
        parent=parent.resume();

        std::cout << "running fiber: " << f->fiber_id << std::endl;
        f->func();
        return std::move(parent);
    });

    // enqueue this f
    std::cout << "enqueue fiber: " << f->fiber_id << std::endl;
    _queue->enqueue(f);
    return f;
}

#endif