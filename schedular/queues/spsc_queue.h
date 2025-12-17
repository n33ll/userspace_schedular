#ifndef  SPSC_QUEUE_H
#define SPSC_QUEUE_H

#include "queue.h"
#include <atomic>
#include <iostream>
#include <vector>
#include <emmintrin.h>

template <typename T>
class spsc_queue : public queue<T>{
//implementation using ring buffer
private:
    int buffer_capacity;
    std::atomic<int> packets;
    std::vector<T> ring_buffer;
    std::atomic<bool> closed;

    int tail;
    int head;

public:
    spsc_queue(int capacity): buffer_capacity(capacity){
        ring_buffer.resize(capacity);
        tail = 0;
        head = 0;
        packets.store(0,std::memory_order_seq_cst);
        closed.store(false, std::memory_order_release);
    };

    bool enqueue(const T& m) override;
    bool dequeue(T& m) override;
    void close() override;
    bool is_closed() const override;
    int size() const override;
};

template<typename T>
bool spsc_queue<T>::enqueue(const T& m){
    if(is_closed()){return false;}

    // check if buffer is full then wait for dequeue
    if(packets == buffer_capacity){
        while(head == tail){
            _mm_pause();
        }
    }

    tail = (tail + 1)%buffer_capacity;
    ring_buffer[tail] = m;
    packets.fetch_add(1, std::memory_order_seq_cst);

    return true;
}

template<typename T>
bool spsc_queue<T>::dequeue(T& m){
    if(is_closed()){return false;}

    // check if buffer is empty and wait for enqueue
    if(packets == 0){
        while (head == tail) {
            _mm_pause();
        }
    }

    head = (head + 1)%buffer_capacity;
    m = ring_buffer[head];
    packets.fetch_sub(1,std::memory_order_seq_cst);
    return true;
}

template<typename T>
void spsc_queue<T>::close(){
    while(packets.load()){
        _mm_pause();
    }
    closed.store(true, std::memory_order_seq_cst);
}


template<typename T>
bool spsc_queue<T>::is_closed() const{
    return closed.load(std::memory_order_seq_cst);
}

template<typename T>
int spsc_queue<T>::size() const{
    return packets.load(std::memory_order_acquire);
}

#endif