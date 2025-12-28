#ifndef MPMC_QUEUE_H
#define MPMC_QUEUE_H

#include "queue.h"
#include <atomic>
#include <cstddef>
#include <emmintrin.h>
#include <stdatomic.h>

template <typename T>
class mpmc_queue : public queue<T>{
private:
    struct node{
    public:
        T value;
        node* next;
        node(const T& msg){
            next = nullptr;
            value = msg; // copy assignment
        }
        node(){
            next = nullptr;
        }
    };
    std::atomic<node*> head;
    std::atomic<node*> tail;
    node* dummy = new node();

    std::atomic<bool> closed;

public:
    mpmc_queue(){
        head.store(dummy);
        tail.store(dummy, std::memory_order_release);
        closed.store(false, std::memory_order_seq_cst);  
    }

    bool enqueue(const T& m) override;
    bool dequeue(T& m) override;
    void close() override;
    bool is_closed() const override;
    int size() const override;
};

template <typename T>
bool mpmc_queue<T>::enqueue(const T& m){
    node* t = new node(m);

    // this is wrong as there may be race conditions due to time between two atomic operations
    // tail.load(std::memory_order_acquire)->next = t;
    // tail.store(t, memory_order_release);

    node* prev = tail.exchange(t, std::memory_order_release);
    prev->next = t;
    return true;
}

template <typename T>
bool mpmc_queue<T>::dequeue(T& m){
    while(true){
        node* h = head.load(std::memory_order_acquire);
        node* n = h->next;

        if(n == nullptr){
            if(is_closed()){
                m = nullptr;
                return false;
            }
            _mm_pause();
            continue;
        }

        if(head.compare_exchange_strong(h, n, std::memory_order_release)){
            delete h;
            m = n->value;
            return true;
        }

        _mm_pause();
    }
}

template <typename T>
int mpmc_queue<T>::size() const {
    // For simplicity, return 0 - this is hard to implement efficiently in lockfree MPMC
    return 0;
}

template <typename T>
void mpmc_queue<T>::close(){
    closed.store(true, std::memory_order_release);
}

template <typename T>
bool mpmc_queue<T>::is_closed() const{
    return closed.load(std::memory_order_acquire);
}

#endif