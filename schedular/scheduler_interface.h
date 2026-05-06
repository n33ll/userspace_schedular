#ifndef SCHEDULER_INTERFACE_H
#define SCHEDULER_INTERFACE_H

// Minimal non-template base class for cooperative schedulers.
// Stored as IScheduler* in benchmark tasks to break the circular dependency
// between uxsched<F> and the task type F.
struct IScheduler {
    virtual void safepoint_check(int runner_id) = 0;
    virtual ~IScheduler() = default;
};

#endif
