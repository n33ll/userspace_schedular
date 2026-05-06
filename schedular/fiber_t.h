#ifndef FIBER_T_H
#define FIBER_T_H

#include <boost/context/continuation_fcontext.hpp>
#include <boost/context/stack_context.hpp>
#include <cstdint>

template <typename F>
struct fiber_t{
    int fiber_id;

    // state markers
    bool yeild = false;
    bool context_initialized = false;

    // pre-allocated stack owned by the fiber pool
    boost::context::stack_context sctx{};

    // intrusive free-list pointer for fiber_pool
    fiber_t<F>* pool_next = nullptr;

    // rdtsc helpers
    uint64_t start_tsc;
    uint64_t cycle_budget;

    F func;

    boost::context::continuation f_ctx;
    boost::context::continuation runner_ctx;
};

#endif