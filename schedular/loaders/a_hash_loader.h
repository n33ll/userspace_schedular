#ifndef A_HASH_LOADER_H
#define A_HASH_LOADER_H

#include "../fiber_t.h"
#include "../queues/queue.h"
#include <emmintrin.h>
#include <vector>
#include <functional>
#include "../exp_backoff.h"

template <typename F>
class ah_loader{
private:
    int _loader_id;
    int num_q;
    std::vector<queue<fiber_t<F>*>*>* _queues;

public:
    ah_loader(int id, std::vector<queue<fiber_t<F>*>*>*& queues);

    // Blocking load (guarantees enqueue before returning)
    void load(fiber_t<F>* f);

    // Best-effort load to selected queue
    bool try_load(fiber_t<F>* f, int q_id);

    // Best-effort hashed load
    bool try_load(fiber_t<F>* f);

    // Blocking load to selected queue
    void load(fiber_t<F>* f, int q_id);

    void multi_load();
};

template <typename F>
ah_loader<F>::ah_loader(int id, std::vector<queue<fiber_t<F>*>*>*& queues){
    _loader_id = id;
    _queues = queues;
    num_q = static_cast<int>(queues->size());
}

template <typename F>
bool ah_loader<F>::try_load(fiber_t<F>* f, int q_id){
    queue<fiber_t<F>*>* q = _queues->at(q_id);
    return q->enqueue(f); // false if bounded queue is full
}

template <typename F>
bool ah_loader<F>::try_load(fiber_t<F>* f){
    int q_id = std::hash<int>()(f->fiber_id) % num_q;
    return try_load(f, q_id);
}

template <typename F>
void ah_loader<F>::load(fiber_t<F>* f){
    exp_backoff _b1;
    int start_q = std::hash<int>()(f->fiber_id) % num_q;
    int q_id = start_q;

    // Blocking + fallback probing:
    // Try target queue first, then probe others before pausing.
    while (true) {
        if (try_load(f, q_id)){
            _b1.reset();
            return;
        }

        q_id = (q_id + 1) % num_q;
        if (q_id == start_q) {
            _b1.wait(); // all queues looked full momentarily
        }
    }
}

template <typename F>
void ah_loader<F>::load(fiber_t<F>* f, int q_id){
    exp_backoff _b2;

    while (!try_load(f, q_id)) {
        _b2.wait(); 
    }
    _b2.reset();
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