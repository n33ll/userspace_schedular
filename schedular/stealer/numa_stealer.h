#ifndef NUMA_STEALER_H
#define NUMA_STEALER_H
#include <iostream>
#include <thread>
#include <vector>
#include "../fiber_t.h"
#include "../queues/queue.h"
#include <atomic>
#include "thread"
#include <numa.h>
#include <sched.h>

template <typename F>
class numa_stealer{
private:
    int _stealer_id;
    std::vector<queue<fiber_t<F>*>*>* _queues;
    
public:
    numa_stealer(int id, std::vector<queue<fiber_t<F>*>*>*& queues);
    bool get(fiber_t<F>*& m);
};

template <typename F>
numa_stealer<F>::numa_stealer(int id, std::vector<queue<fiber_t<F>*>*>*& queues){
    _stealer_id = id;
    _queues = queues;
}

template <typename F>
bool numa_stealer<F>::get(fiber_t<F>*& m){
    int num_queues = _queues->size();
    
    // Check if NUMA is available
    if (numa_available() >= 0) {
        int my_numa_node = numa_node_of_cpu(sched_getcpu());
        
        // numa aware stealing
        for (int i = 0; i < num_queues; ++i) {
            int queue_numa_node = _queues->at(i)->get_numa_node();
            if (queue_numa_node == my_numa_node) {
                queue<fiber_t<F>*>* q = _queues->at(i);
                if (q->dequeue(m)) {
                    return true;
                }
            }
        }
    } 

    for (int i = 0; i < num_queues; ++i) {
        queue<fiber_t<F>*>* q = _queues->at(i);
        if (q->dequeue(m)) {
            return true;
        }
    }
    return false;
}

#endif