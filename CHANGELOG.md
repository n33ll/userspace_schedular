# Change Log
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/) and this project adheres to [Semantic Versioning](http://semver.org/).

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

[2.0.0]: https://github.com/n33ll/userspace_schedular/commits/2.0.0
[1.1.0]: https://github.com/n33ll/userspace_schedular/commits/1.1.0
[1.0.0]: https://github.com/n33ll/userspace_schedular/commits/1.0.0
[0.1.0]: https://github.com/n33ll/userspace_schedular/commits/0.1.0
[0.0.0]: https://github.com/n33ll/userspace_schedular/commits/0.0.0
