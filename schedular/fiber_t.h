#ifndef FIBER_T_H
#define FIBER_T_H

#include <boost/context/continuation_fcontext.hpp>

template <typename F>
struct fiber_t{
    int fiber_id;

    bool DONE;
    bool WAITING;
    bool RUNNING;


    int stack_size;
    int stack_base;
    F func;

    boost::context::continuation ctx;
};

#endif