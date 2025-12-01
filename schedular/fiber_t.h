#ifndef FIBER_T_H
#define FIBER_T_H

#include <boost/context/continuation_fcontext.hpp>
#include <cstdint>

template <typename F>
struct fiber_t{
    int fiber_id;

    //state markers
    bool yeild = false;
    bool context_initialized = false;

    //fiber memory
    int stack_size;
    int stack_base;

    //rdtsc helpers
    uint64_t start_tsc;
    uint64_t cycle_budget;

    F func;

    boost::context::continuation f_ctx;
    boost::context::continuation runner_ctx;
    
};

#endif