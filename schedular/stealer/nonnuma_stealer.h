#ifndef NONNUMA_STEALER_H
#define NONNUMA_STEALER_H
#include <iostream>
#include <thread>
#include <vector>
#include "../fiber_t.h"
#include "../queues/queue.h"
#include <atomic>
#include "thread"

template <typename F>
class nonnuma_stealer{
private:
    int _stealer_id;
    std::vector<queue<fiber_t<F>*>*>* _queues;
    
public:
    nonnuma_stealer(int id, std::vector<queue<fiber_t<F>*>*>*& queues);
    bool get(fiber_t<F>*& m);
};

template <typename F>
nonnuma_stealer<F>::nonnuma_stealer(int id, std::vector<queue<fiber_t<F>*>*>*& queues){
    _stealer_id = id;
    _queues = queues;
}

template <typename F>
bool nonnuma_stealer<F>::get(fiber_t<F>*& m){
    int num_queues = _queues->size();
    for (int i{0}; i < num_queues; ++i) {
        queue<fiber_t<F>*>* q = _queues->at(i);
        
        if (q->dequeue(m)) {
            std::cout << "stealer " << _stealer_id << " stole from queue " << i << std::endl;
            return true;;
        }
    }
    m = nullptr;
    return false;
}

#endif