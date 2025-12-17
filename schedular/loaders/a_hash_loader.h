#ifndef A_HASH_LOADER_H
#define A_HASH_LOADER_H

#include "../fiber_t.h"
#include "../queues/queue.h"

template <typename F>
class ah_loader{
private:
    int _loader_id;
    int num_q;
    std::vector<queue<fiber_t<F>*>*>* _queues;

public:
    ah_loader(int id, std::vector<queue<fiber_t<F>*>*>*& queues);
    void load(fiber_t<F>* f);
    void load(fiber_t<F>* f, int q_id);
    void multi_load();
};

template <typename F>
ah_loader<F>::ah_loader(int id, std::vector<queue<fiber_t<F>*>*>*& queues){
    _loader_id = id;
    _queues = queues;
    num_q = queues->size();
}

template <typename F>
void ah_loader<F>::load(fiber_t<F>* f){
    int q_id = std::hash<int>()(f->fiber_id) % num_q;
    queue<fiber_t<F>*>* q = _queues->at(q_id);
    q->enqueue(f);
}

template <typename F>
void ah_loader<F>::load(fiber_t<F>* f, int q_id){
    queue<fiber_t<F>*>* q = _queues->at(q_id);
    q->enqueue(f);
}

static inline uint32_t fast_hash(uint64_t x) {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return (uint32_t)x;
}

#endif