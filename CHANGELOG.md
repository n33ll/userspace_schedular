# Change Log
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/) and this project adheres to [Semantic Versioning](http://semver.org/).

## [4.0.0] - 2026-05-03
### Added
 - Benchmark: Add Flamegraph and Various results
 - safepoint_check: remove the cycle-budget logic
 - Benchmark: update the latency measuments to use promises.
 - Add exponential backoff rather than using _mM_pause()
 - Make bounded MPMC queue with array reporposed as queue
 - Re-Vamp the scheduler
 - Add baseline vs uxsched benchmarking over two types of workloads

## [3.0.0] - 2025-12-28
### Added
 - Update dequeue to handle concurrent calls
 - runner.h: add close() and is_closed() member functions.
 - Update destructor for gracefull exit.
 - Correct the argument of get() member function to pass as reference.
 - Add MPMC queue
 - Incorporate MPMC queues in schedular and general fixes
 - Add benchmarking

## [2.0.0] - 2025-12-18
### Added
 - Add non-numa (naive) stealer 
 - Add size() member function inside the queue 
 - Add load() to specific queue member function for both loaders 
 - Integrate naive stealer into the shedular 
 - Add NUMA aware stealing 
 - Add NUMA support and get_numa_node() member function 
 - Delete c_hash_loader as it is similar to rr_loader 
 - Integrate NUMA compatible queues inside schedular

## [1.1.0] - 2025-12-10
### Added
 - Add and incorporate affinity hashing schedular

## [1.0.0] - 2025-12-02
### Added
 - Move the runner DIR inside the schedular
 - Add Multi runner logic with round-robin Task Loader
 - Update task_fn inside main and Add results

## [0.1.0] - 2025-12-01
### Added
 - safepoint_check: Add yeild functionality
 - main.cpp : incorporate safepoint_check()
 - Add results DIR for the executable

## [0.0.0] - 2025-11-30
### Added
 - Initial commit: Genisis of userspace schedular

[4.0.0]: https://github.com/n33ll/userspace_schedular/commits/4.0.0
[3.0.0]: https://github.com/n33ll/userspace_schedular/commits/3.0.0
[2.0.0]: https://github.com/n33ll/userspace_schedular/commits/2.0.0
[1.1.0]: https://github.com/n33ll/userspace_schedular/commits/1.1.0
[1.0.0]: https://github.com/n33ll/userspace_schedular/commits/1.0.0
[0.1.0]: https://github.com/n33ll/userspace_schedular/commits/0.1.0
[0.0.0]: https://github.com/n33ll/userspace_schedular/commits/0.0.0
