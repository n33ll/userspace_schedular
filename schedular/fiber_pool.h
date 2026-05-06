#ifndef FIBER_POOL_H
#define FIBER_POOL_H

#include "fiber_t.h"
#include <boost/context/stack_context.hpp>
#include <boost/context/preallocated.hpp>
#include <atomic>
#include <sys/mman.h>

// No-op stack allocator. Required by the boost::context::callcc overload that
// accepts a preallocated stack — boost still calls allocator.deallocate() when
// the context completes, so we make it a no-op (the fiber_pool owns the stack
// memory and reuses it).
struct pool_stack_allocator {
    boost::context::stack_context allocate() {
        // Should never be called when using the preallocated overload.
        return {};
    }
    void deallocate(boost::context::stack_context&) noexcept {
        // Intentionally empty: the fiber_pool owns and recycles this stack.
    }
};

// Lock-free pool of fiber_t objects + pre-allocated stacks.
//
// Each fiber in the pool owns a pre-mmap'd stack. When a fiber completes,
// it is returned here so the next spawn reuses both the fiber_t object and
// its stack without any malloc/mmap on the hot submission or execution path.
//
// Stack layout (grows down):
//   [ guard page (PROT_NONE) | ... usable stack ... | sp (top) ]
//   ^mem                                              ^sctx.sp
//   |<-----------  sctx.size  ---------------------->|

template <typename F>
class fiber_pool {
    static constexpr size_t PAGE_SIZE  = 4096;
    static constexpr size_t STACK_SIZE = 64 * 1024;   // one guard + 64 KB usable
    static constexpr int    PREWARM    = 1024;         // stacks mapped at construction

    std::atomic<fiber_t<F>*> _head{nullptr};

    // -----------------------------------------------------------------------
    // Stack helpers
    // -----------------------------------------------------------------------
    static boost::context::stack_context alloc_stack() {
        const size_t total = STACK_SIZE + PAGE_SIZE;
        void* mem = ::mmap(nullptr, total,
                           PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
                           -1, 0);
        ::mprotect(mem, PAGE_SIZE, PROT_NONE);   // guard page at bottom

        boost::context::stack_context sctx;
        sctx.size = total;
        sctx.sp   = static_cast<char*>(mem) + total;  // top (stack grows down)
        return sctx;
    }

    static void free_stack(boost::context::stack_context sctx) {
        void* mem = static_cast<char*>(sctx.sp) - sctx.size;
        ::munmap(mem, sctx.size);
    }

    // -----------------------------------------------------------------------
    // Intrusive free-list push (lock-free CAS)
    // -----------------------------------------------------------------------
    void push(fiber_t<F>* f) noexcept {
        f->pool_next = _head.load(std::memory_order_relaxed);
        while (!_head.compare_exchange_weak(
                   f->pool_next, f,
                   std::memory_order_release,
                   std::memory_order_relaxed)) { /* retry */ }
    }

public:
    explicit fiber_pool(int prewarm = PREWARM) {
        // Single-threaded init — relaxed stores are fine here.
        for (int i = 0; i < prewarm; ++i) {
            auto* f      = new fiber_t<F>();
            f->sctx      = alloc_stack();
            f->pool_next = _head.load(std::memory_order_relaxed);
            _head.store(f, std::memory_order_relaxed);
        }
        // Publish all pre-warmed nodes to other threads.
        std::atomic_thread_fence(std::memory_order_release);
    }

    ~fiber_pool() {
        fiber_t<F>* f = _head.exchange(nullptr, std::memory_order_acquire);
        while (f) {
            fiber_t<F>* next = f->pool_next;
            free_stack(f->sctx);
            delete f;
            f = next;
        }
    }

    // Returns a ready-to-use fiber_t with a valid pre-allocated stack.
    // If the pool is empty a new fiber + stack is allocated (slow path).
    fiber_t<F>* acquire() {
        fiber_t<F>* f = _head.load(std::memory_order_acquire);
        while (f) {
            fiber_t<F>* next = f->pool_next;
            if (_head.compare_exchange_weak(
                    f, next,
                    std::memory_order_acquire,
                    std::memory_order_relaxed)) {
                // Reset all mutable state for reuse.
                f->yeild               = false;
                f->context_initialized = false;
                f->pool_next           = nullptr;
                return f;
            }
            // CAS failed: f was updated to the current head by compare_exchange_weak.
        }

        // Pool exhausted — allocate a fresh fiber + stack.
        auto* f2  = new fiber_t<F>();
        f2->sctx  = alloc_stack();
        return f2;
    }

    // Returns a completed (or aborted) fiber to the pool for reuse.
    // The caller must ensure the fiber is no longer executing.
    void release(fiber_t<F>* f) noexcept {
        // Destroy the boost continuations — with preallocated stacks boost does
        // NOT call munmap, so our stack memory in sctx stays intact.
        f->f_ctx      = {};
        f->runner_ctx = {};

        f->context_initialized = false;
        f->yeild               = false;

        push(f);
    }
};

#endif