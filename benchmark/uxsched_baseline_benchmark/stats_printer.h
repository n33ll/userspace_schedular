// benchmark/uxsched_baseline_benchmark/stats_printer.h

#pragma once
#include <vector>
#include <cstdint>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <cmath>
#include <chrono>
#include <thread>
#include <string>
#include <x86intrin.h>

// TSC frequency detection
class TSCCalibration {
private:
    static uint64_t tsc_freq_hz;
    static bool calibrated;
    
public:
    static void calibrate() {
        if (calibrated) return;
        
        auto start = std::chrono::high_resolution_clock::now();
        uint64_t start_tsc = __rdtsc();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        uint64_t end_tsc = __rdtsc();
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        tsc_freq_hz = (end_tsc - start_tsc) * 1000000000ull / duration_ns;
        
        calibrated = true;
    }
    
    static double cycles_to_ns(uint64_t cycles) {
        return static_cast<double>(cycles) * 1000000000.0 / tsc_freq_hz;
    }
    
    static double cycles_to_us(uint64_t cycles) {
        return cycles_to_ns(cycles) / 1000.0;
    }
    
    static uint64_t get_freq_mhz() {
        return tsc_freq_hz / 1000000;
    }
};

uint64_t TSCCalibration::tsc_freq_hz = 0;
bool TSCCalibration::calibrated = false;

// ============================================================================
// STATISTICS CALCULATION
// ============================================================================

class LatencyStats {
public:
    std::vector<uint64_t> samples;  // Must be sorted
    
    uint64_t count() const { return samples.size(); }
    
    uint64_t min() const {
        return samples.empty() ? 0 : samples.front();
    }
    
    uint64_t max() const {
        return samples.empty() ? 0 : samples.back();
    }
    
    uint64_t mean() const {
        if (samples.empty()) return 0;
        uint64_t sum = 0;
        for (uint64_t s : samples) sum += s;
        return sum / samples.size();
    }
    
    uint64_t median() const {
        return percentile(0.50);
    }
    
    uint64_t p50() const {
        return percentile(0.50);
    }
    
    uint64_t p99() const {
        return percentile(0.99);
    }
    
    uint64_t p999() const {
        return percentile(0.999);
    }
    
    uint64_t p9999() const {
        return percentile(0.9999);
    }
    
    double stddev() const {
        if (samples.empty()) return 0.0;
        
        uint64_t m = mean();
        double sum_sq_diff = 0.0;
        
        for (uint64_t s : samples) {
            double diff = static_cast<double>(s) - m;
            sum_sq_diff += diff * diff;
        }
        
        return std::sqrt(sum_sq_diff / samples.size());
    }
    
    double coefficient_of_variation() const {
        uint64_t m = mean();
        if (m == 0) return 0.0;
        return stddev() / m;
    }
    
private:
    uint64_t percentile(double p) const {
        if (samples.empty()) return 0;
        size_t idx = static_cast<size_t>(samples.size() * p);
        if (idx >= samples.size()) idx = samples.size() - 1;
        return samples[idx];
    }
};

// ============================================================================
// BENCHMARK RESULT STRUCT
// ============================================================================

struct BenchmarkResult {
    std::string scheduler_name;
    std::string workload_name;
    std::vector<uint64_t> latencies;
    
    LatencyStats get_stats() const {
        LatencyStats stats;
        stats.samples = latencies;
        std::sort(stats.samples.begin(), stats.samples.end());
        return stats;
    }
};

// ============================================================================
// HISTOGRAM VISUALIZATION
// ============================================================================

void print_histogram(const LatencyStats& stats) {
    const int HISTOGRAM_BUCKETS = 20;
    const int BAR_WIDTH = 50;
    
    if (stats.count() < 10) {
        std::cout << "   (Not enough samples for histogram)\n";
        return;
    }
    
    std::vector<uint64_t> bucket_counts(HISTOGRAM_BUCKETS, 0);
    uint64_t min_val = stats.min();
    uint64_t max_val = stats.max();
    uint64_t bucket_width = (max_val - min_val) / HISTOGRAM_BUCKETS + 1;
    
    for (uint64_t sample : stats.samples) {
        size_t bucket = (sample - min_val) / bucket_width;
        if (bucket >= HISTOGRAM_BUCKETS) bucket = HISTOGRAM_BUCKETS - 1;
        bucket_counts[bucket]++;
    }
    
    uint64_t max_count = *std::max_element(bucket_counts.begin(), bucket_counts.end());
    
    for (int i = 0; i < HISTOGRAM_BUCKETS; ++i) {
        uint64_t bucket_start = min_val + (i * bucket_width);
        uint64_t bucket_end = bucket_start + bucket_width;
        
        double start_us = TSCCalibration::cycles_to_us(bucket_start);
        double end_us = TSCCalibration::cycles_to_us(bucket_end);
        
        std::cout << "   [" << std::setw(6) << std::fixed << std::setprecision(1) 
                  << start_us << "-" << std::setw(6) << end_us << " µs] ";
        
        int bar_len = (bucket_counts[i] * BAR_WIDTH) / max_count;
        for (int j = 0; j < bar_len; ++j) std::cout << "█";
        std::cout << " " << std::setw(6) << bucket_counts[i] << "\n";
    }
}

// ============================================================================
// SINGLE BENCHMARK STATS PRINTER
// ============================================================================

void print_stats(const std::string& scheduler_name, const std::string& workload_name, 
                 const std::vector<uint64_t>& latencies) {
    if (latencies.empty()) {
        std::cout << "❌ No samples collected!\n";
        return;
    }
    
    TSCCalibration::calibrate();
    
    LatencyStats stats;
    stats.samples = latencies;
    std::sort(stats.samples.begin(), stats.samples.end());
    
    uint64_t total_samples = stats.count();
    uint64_t total_cycles = 0;
    for (uint64_t s : stats.samples) total_cycles += s;
    double avg_cycles = static_cast<double>(total_cycles) / total_samples;
    
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  " << scheduler_name << " | " << workload_name << std::string(56 - scheduler_name.length() - workload_name.length() - 3, ' ') << "║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    std::cout << std::fixed << std::setprecision(2);
    
    // Samples
    std::cout << "\n�� SAMPLES & FREQUENCY\n";
    std::cout << "   ├─ Total Samples:  " << std::setw(12) << total_samples << "\n";
    std::cout << "   └─ TSC Frequency:  " << std::setw(12) << TSCCalibration::get_freq_mhz() << " MHz\n";
    
    // Latency in cycles
    std::cout << "\n⏱️  LATENCY (CPU Cycles)\n";
    std::cout << "   ├─ Min:            " << std::setw(15) << stats.min() << " cycles\n";
    std::cout << "   ├─ Mean:           " << std::setw(15) << static_cast<uint64_t>(avg_cycles) << " cycles\n";
    std::cout << "   ├─ Median (P50):   " << std::setw(15) << stats.median() << " cycles\n";
    std::cout << "   ├─ P99:            " << std::setw(15) << stats.p99() << " cycles";
    double ratio_p99 = static_cast<double>(stats.p99()) / stats.min();
    std::cout << "  [" << std::setw(6) << ratio_p99 << "x min]\n";
    std::cout << "   ├─ P999:           " << std::setw(15) << stats.p999() << " cycles";
    double ratio_p999 = static_cast<double>(stats.p999()) / stats.min();
    std::cout << "  [" << std::setw(6) << ratio_p999 << "x min]\n";
    std::cout << "   ├─ P9999:          " << std::setw(15) << stats.p9999() << " cycles";
    double ratio_p9999 = static_cast<double>(stats.p9999()) / stats.min();
    std::cout << "  [" << std::setw(6) << ratio_p9999 << "x min]\n";
    std::cout << "   └─ Max:            " << std::setw(15) << stats.max() << " cycles";
    double ratio_max = static_cast<double>(stats.max()) / stats.min();
    std::cout << "  [" << std::setw(6) << ratio_max << "x min]\n";
    
    // Latency in microseconds
    std::cout << "\n⏱️  LATENCY (Microseconds)\n";
    std::cout << "   ├─ Min:            " << std::setw(12) << TSCCalibration::cycles_to_us(stats.min()) << " µs\n";
    std::cout << "   ├─ Mean:           " << std::setw(12) << TSCCalibration::cycles_to_us(static_cast<uint64_t>(avg_cycles)) << " µs\n";
    std::cout << "   ├─ Median (P50):   " << std::setw(12) << TSCCalibration::cycles_to_us(stats.median()) << " µs\n";
    std::cout << "   ├─ P99:            " << std::setw(12) << TSCCalibration::cycles_to_us(stats.p99()) << " µs\n";
    std::cout << "   ├─ P999:           " << std::setw(12) << TSCCalibration::cycles_to_us(stats.p999()) << " µs\n";
    std::cout << "   ├─ P9999:          " << std::setw(12) << TSCCalibration::cycles_to_us(stats.p9999()) << " µs\n";
    std::cout << "   └─ Max:            " << std::setw(12) << TSCCalibration::cycles_to_us(stats.max()) << " µs\n";
    
    // Jitter metrics
    std::cout << "\n📈 JITTER & CONSISTENCY\n";
    std::cout << "   ├─ Std Dev:        " << std::setw(12) << TSCCalibration::cycles_to_us(static_cast<uint64_t>(stats.stddev())) << " µs\n";
    std::cout << "   └─ Coeff. Var:     " << std::setw(12) << (stats.coefficient_of_variation() * 100) << " %\n";
    
    // Throughput
    std::cout << "\n🚀 THROUGHPUT\n";
    double tasks_per_sec = total_samples / ((total_cycles / static_cast<double>(TSCCalibration::get_freq_mhz())) / 1000000);
    std::cout << "   └─ Tasks/sec:      " << std::setw(12) << tasks_per_sec << " tasks/sec\n";
    
    // Latency distribution histogram
    std::cout << "\n📊 LATENCY DISTRIBUTION\n";
    print_histogram(stats);
    
    std::cout << "\n";
}

// ============================================================================
// COMPARISON PRINTER - SIDE BY SIDE
// ============================================================================

void print_comparison(const std::string& workload_name,
                      const BenchmarkResult& uxsched_result,
                      const BenchmarkResult& osthread_result) {
    TSCCalibration::calibrate();
    
    auto uxsched_stats = uxsched_result.get_stats();
    auto osthread_stats = osthread_result.get_stats();
    
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                     COMPARISON: " << workload_name << std::string(52 - workload_name.length(), ' ') << "║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════════════════════╝\n";
    std::cout << std::fixed << std::setprecision(2);
    
    std::cout << "\n╔═══════════════════════════╦═══════════════════╦═══════════════════╦═════════════════╗\n";
    std::cout << "║ Metric                    ║ UXSched           ║ OS Threads        ║ Speedup (✓=wins)║\n";
    std::cout << "╠═══════════════════════════╬═══════════════════╬═══════════════════╬═════════════════╣\n";
    
    // P50 Latency
    double p50_uxsched = TSCCalibration::cycles_to_us(uxsched_stats.p50());
    double p50_osthread = TSCCalibration::cycles_to_us(osthread_stats.p50());
    double p50_speedup = p50_osthread / p50_uxsched;
    std::cout << "║ P50 Latency (µs)          ║ " << std::setw(15) << p50_uxsched << " ║ " << std::setw(15) << p50_osthread << " ║ ";
    if (p50_speedup > 1) std::cout << std::setw(13) << p50_speedup << "x ✓ ║\n";
    else std::cout << std::setw(13) << (1.0/p50_speedup) << "x   ║\n";
    
    // P99 Latency
    double p99_uxsched = TSCCalibration::cycles_to_us(uxsched_stats.p99());
    double p99_osthread = TSCCalibration::cycles_to_us(osthread_stats.p99());
    double p99_speedup = p99_osthread / p99_uxsched;
    std::cout << "║ P99 Latency (µs)          ║ " << std::setw(15) << p99_uxsched << " ║ " << std::setw(15) << p99_osthread << " ║ ";
    if (p99_speedup > 1) std::cout << std::setw(13) << p99_speedup << "x ✓ ║\n";
    else std::cout << std::setw(13) << (1.0/p99_speedup) << "x   ║\n";
    
    // P999 Latency
    double p999_uxsched = TSCCalibration::cycles_to_us(uxsched_stats.p999());
    double p999_osthread = TSCCalibration::cycles_to_us(osthread_stats.p999());
    double p999_speedup = p999_osthread / p999_uxsched;
    std::cout << "║ P999 Latency (µs)         ║ " << std::setw(15) << p999_uxsched << " ║ " << std::setw(15) << p999_osthread << " ║ ";
    if (p999_speedup > 1) std::cout << std::setw(13) << p999_speedup << "x ✓ ║\n";
    else std::cout << std::setw(13) << (1.0/p999_speedup) << "x   ║\n";
    
    // P9999 Latency
    double p9999_uxsched = TSCCalibration::cycles_to_us(uxsched_stats.p9999());
    double p9999_osthread = TSCCalibration::cycles_to_us(osthread_stats.p9999());
    double p9999_speedup = p9999_osthread / p9999_uxsched;
    std::cout << "║ P9999 Latency (µs)        ║ " << std::setw(15) << p9999_uxsched << " ║ " << std::setw(15) << p9999_osthread << " ║ ";
    if (p9999_speedup > 1) std::cout << std::setw(13) << p9999_speedup << "x ✓ ║\n";
    else std::cout << std::setw(13) << (1.0/p9999_speedup) << "x   ║\n";
    
    // Jitter (CV)
    double cv_uxsched = uxsched_stats.coefficient_of_variation() * 100;
    double cv_osthread = osthread_stats.coefficient_of_variation() * 100;
    double cv_improvement = cv_osthread / cv_uxsched;  // Lower is better
    std::cout << "║ Jitter (Coeff. Var %)     ║ " << std::setw(15) << cv_uxsched << " ║ " << std::setw(15) << cv_osthread << " ║ ";
    if (cv_uxsched < cv_osthread) std::cout << std::setw(13) << cv_improvement << "x ✓ ║\n";
    else std::cout << std::setw(13) << (1.0/cv_improvement) << "x   ║\n";
    
    // Sample Count
    std::cout << "║ Sample Count              ║ " << std::setw(15) << uxsched_stats.count() << " ║ " << std::setw(15) << osthread_stats.count() << " ║                 ║\n";
    
    std::cout << "╚═══════════════════════════╩═══════════════════╩═══════════════════╩═════════════════╝\n";
    
    // Summary
    std::cout << "\n📋 SUMMARY\n";
    std::cout << "   Average Speedup (P50/P99/P999): " << (p50_speedup + p99_speedup + p999_speedup) / 3.0 << "x\n";
    std::cout << "   Best Case (P9999):              " << p9999_speedup << "x\n";
    std::cout << "   Jitter Improvement:             " << cv_improvement << "x better\n";
    std::cout << "\n";
}