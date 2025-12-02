#ifndef RR_LOADER_H
#define RR_LOADER_H

#include "../fiber_t.h"
#include "../queues/queue.h"

template <typename F>
class rr_loader{
private:
    int _loader_id;
    int q_id;
    int num_q;
    std::vector<queue<fiber_t<F>*>*>* _queues;

public:
    rr_loader(int id, std::vector<queue<fiber_t<F>*>*>*& queues);
    void load(fiber_t<F>* f);
    void load(fiber_t<F>* f, int q_id);
    void multi_load();
};

template <typename F>
rr_loader<F>::rr_loader(int id, std::vector<queue<fiber_t<F>*>*>*& queues){
    _loader_id = id;
    _queues = queues;
    q_id = 0;
    num_q = queues->size();
}

template <typename F>
void rr_loader<F>::load(fiber_t<F>* f){
    while(true){
        queue<fiber_t<F>*>* q = _queues->at(q_id);
        if(!q->is_closed()){
            q->enqueue(f);
            q_id = (q_id + 1) % num_q;
            return;
        }
        q_id = (q_id + 1) % num_q; // round-robin it up!!!!!
    }
}

#endif