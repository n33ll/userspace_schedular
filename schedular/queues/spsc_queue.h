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

    int _cpu_id;
    int _numa_node;

    int tail;
    std::atomic<int> head;

public:
    spsc_queue(int capacity, int cpu_id = 0, int numa_node = 0): 
        buffer_capacity(capacity), _cpu_id(cpu_id), _numa_node(numa_node) {
        ring_buffer.resize(capacity);
        tail = 0;
        head.store(0, std::memory_order_relaxed);
        packets.store(0, std::memory_order_seq_cst);
        closed.store(false, std::memory_order_release);
    };

    bool enqueue(const T& m) override;
    bool dequeue(T& m) override;
    void close() override;
    bool is_closed() const override;
    int size() const override;
    int get_numa_node() const;
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

    int current_packets = packets.load(std::memory_order_acquire);
    if(current_packets == 0) {
        m = nullptr;
        return false;
    }

    if(packets.compare_exchange_strong(current_packets, current_packets - 1, std::memory_order_seq_cst)) {
        int my_head = head.fetch_add(1, std::memory_order_seq_cst) % buffer_capacity;
        m = ring_buffer[my_head];
        if (m == nullptr) {
            std::cout << "Warning: dequeued nullptr from ring_buffer at position " << my_head << std::endl;
        }
        return true;
    }

    m = nullptr;
    return false;
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

template<typename T>
int spsc_queue<T>::get_numa_node() const{
    return this->_numa_node;
}

#endif