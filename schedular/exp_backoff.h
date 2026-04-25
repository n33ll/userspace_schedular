#ifndef EXP_BACKOFF_H
#define EXP_BACKOFF_H

#include <cstdint>
#include <thread>
#include <chrono>
#include <emmintrin.h>   // _mm_pause

// Exponential backoff for hot spin paths:
// phase 1: short CPU pause spins
// phase 2: std::this_thread::yield()
// phase 3: tiny sleep (ns/us), exponentially increasing up to cap
class exp_backoff {
private:
    uint32_t _attempts = 0;

    // Tunables (safe defaults for low-latency schedulers)
    static constexpr uint32_t kPauseItersBase   = 4;      // initial pause loop count
    static constexpr uint32_t kPauseItersMax    = 1u << 10; // max pause loop count
    static constexpr uint32_t kYieldThreshold   = 8;      // attempts before yield starts
    static constexpr uint32_t kSleepThreshold   = 16;     // attempts before sleep starts
    static constexpr uint64_t kSleepNsBase      = 200;    // 0.2 us
    static constexpr uint64_t kSleepNsMax       = 50'000; // 50 us

public:
    exp_backoff() = default;

    inline void reset() noexcept {
        _attempts = 0;
    }

    inline uint32_t attempts() const noexcept {
        return _attempts;
    }

    inline void wait() noexcept {
        // attempt count starts at 1 for simpler thresholds
        ++_attempts;

        // Phase 1: pause spinning with exponential growth
        if (_attempts < kYieldThreshold) {
            uint32_t shift = (_attempts > 10) ? 10 : _attempts; // clamp
            uint32_t spins = kPauseItersBase << shift;
            if (spins > kPauseItersMax) spins = kPauseItersMax;

            for (uint32_t i = 0; i < spins; ++i) {
                _mm_pause();
            }
            return;
        }

        // Phase 2: tiny sleep with exponential cap
        uint32_t sleep_shift = _attempts - kSleepThreshold;
        if (sleep_shift > 20) sleep_shift = 20; // avoid overflow
        uint64_t ns = kSleepNsBase << sleep_shift;
        if (ns > kSleepNsMax) ns = kSleepNsMax;

        std::this_thread::sleep_for(std::chrono::nanoseconds(ns));
    }
};

#endif