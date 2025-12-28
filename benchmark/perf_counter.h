#ifndef PERF_COUNTER_H
#define PERF_COUNTER_H

#include <x86intrin.h>
#include <chrono>
#include <vector>
#include <algorithm>
#include <mutex>

class PerfCounter {
private:
    std::vector<uint64_t> samples_;
    uint64_t start_tsc_;
    std::mutex mutex_;
    
public:
    void start() { start_tsc_ = __rdtsc(); }
    void end() { 
        std::lock_guard<std::mutex> lock(mutex_);
        samples_.push_back(__rdtsc() - start_tsc_); 
    }
    void add_sample(uint64_t sample) { 
        std::lock_guard<std::mutex> lock(mutex_);
        samples_.push_back(sample); 
    }
    
    // Critical: don't sort in hot path
    void finalize_samples() { 
        std::lock_guard<std::mutex> lock(mutex_);
        std::sort(samples_.begin(), samples_.end()); 
    }
    
    uint64_t p50() const { return samples_.empty() ? 0 : samples_[samples_.size() * 0.5]; }
    uint64_t p99() const { return samples_.empty() ? 0 : samples_[samples_.size() * 0.99]; }
    uint64_t p999() const { return samples_.empty() ? 0 : samples_[samples_.size() * 0.999]; }
    uint64_t min() const { return samples_.empty() ? 0 : samples_.front(); }
    uint64_t max() const { return samples_.empty() ? 0 : samples_.back(); }
    
    void clear() { 
        std::lock_guard<std::mutex> lock(mutex_);
        samples_.clear(); 
    }
    size_t count() const { return samples_.size(); }
};

#endif