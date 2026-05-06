#ifndef BOUNDED_MPMC_QUEUE_H
#define BOUNDED_MPMC_QUEUE_H

#include "queue.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <type_traits>
#include <new>

// Bounded lock-free MPMC queue (Vyukov algorithm)
// Correct under contention, no per-op heap allocation.
template <typename T>
class bounded_mpmc_queue : public queue<T> {
private:
    struct cell_t {
        std::atomic<size_t> sequence;
        T data;
    };

    size_t _capacity;
    size_t _mask;
    cell_t* _buffer;

    alignas(64) std::atomic<size_t> _enqueue_pos;
    alignas(64) std::atomic<size_t> _dequeue_pos;

    alignas(64) std::atomic<bool> _closed;

    static size_t next_pow2(size_t x) {
        if (x < 2) return 2;
        --x;
        x |= x >> 1;
        x |= x >> 2;
        x |= x >> 4;
        x |= x >> 8;
        x |= x >> 16;
        if constexpr (sizeof(size_t) == 8) x |= x >> 32;
        return x + 1;
    }

public:
    // Keep default constructor for existing call sites.
    // You should pass a capacity explicitly from scheduler later.
    explicit bounded_mpmc_queue(size_t capacity = 1 << 16)
        : _capacity(next_pow2(capacity)),
          _mask(_capacity - 1),
          _buffer(nullptr),
          _enqueue_pos(0),
          _dequeue_pos(0),
          _closed(false) {
        _buffer = static_cast<cell_t*>(::operator new[](sizeof(cell_t) * _capacity));
        for (size_t i = 0; i < _capacity; ++i) {
            new (&_buffer[i]) cell_t{};
            _buffer[i].sequence.store(i, std::memory_order_relaxed);
        }
    }

    ~bounded_mpmc_queue() {
        // Drain is caller-managed. We just destroy storage.
        for (size_t i = 0; i < _capacity; ++i) {
            _buffer[i].~cell_t();
        }
        ::operator delete[](_buffer);
    }

    bool enqueue(const T& m) override {
        if (_closed.load(std::memory_order_acquire)) return false;

        cell_t* cell;
        size_t pos = _enqueue_pos.load(std::memory_order_relaxed);

        for (;;) {
            cell = &_buffer[pos & _mask];
            size_t seq = cell->sequence.load(std::memory_order_acquire);
            intptr_t dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);

            if (dif == 0) {
                if (_enqueue_pos.compare_exchange_weak(
                        pos, pos + 1,
                        std::memory_order_relaxed,
                        std::memory_order_relaxed)) {
                    break; // slot claimed
                }
            } else if (dif < 0) {
                // Queue full
                return false;
            } else {
                pos = _enqueue_pos.load(std::memory_order_relaxed);
            }
        }

        cell->data = m;
        cell->sequence.store(pos + 1, std::memory_order_release);
        return true;
    }

    bool dequeue(T& m) override {
        cell_t* cell;
        size_t pos = _dequeue_pos.load(std::memory_order_relaxed);

        for (;;) {
            cell = &_buffer[pos & _mask];
            size_t seq = cell->sequence.load(std::memory_order_acquire);
            intptr_t dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);

            if (dif == 0) {
                if (_dequeue_pos.compare_exchange_weak(
                        pos, pos + 1,
                        std::memory_order_relaxed,
                        std::memory_order_relaxed)) {
                    break; // slot claimed
                }
            } else if (dif < 0) {
                // Queue empty
                // NOTE: no m=nullptr assignment; works for any T
                return false;
            } else {
                pos = _dequeue_pos.load(std::memory_order_relaxed);
            }
        }

        m = cell->data;
        cell->sequence.store(pos + _mask + 1, std::memory_order_release);
        return true;
    }

    void close() override {
        _closed.store(true, std::memory_order_release);
    }

    bool is_closed() const override {
        return _closed.load(std::memory_order_acquire);
    }

    int size() const override {
        return 0;
    }
};

#endif