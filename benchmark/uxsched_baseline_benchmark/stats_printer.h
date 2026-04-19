// benchmark/stats_printer.h

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
// PRETTY PRINT FUNCTION
// ============================================================================

void print_histogram(const LatencyStats& stats);

void print_stats(const std::vector<uint64_t>& latencies) {
    if (latencies.empty()) {
        std::cout << "❌ No samples collected!\n";
        return;
    }
    
    // Calibrate TSC frequency once
    TSCCalibration::calibrate();
    
    // Calculate statistics
    LatencyStats stats;
    stats.samples = latencies;
    std::sort(stats.samples.begin(), stats.samples.end());
    
    // Get basic info
    uint64_t total_samples = stats.count();
    uint64_t total_cycles = 0;
    for (uint64_t s : stats.samples) total_cycles += s;
    double avg_cycles = static_cast<double>(total_cycles) / total_samples;
    double throughput = static_cast<double>(total_samples * 1000000000) / 
                       (stats.samples.size() * TSCCalibration::cycles_to_ns(1));
    
    // Print header
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                  LATENCY STATISTICS                        ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    std::cout << std::fixed << std::setprecision(2);
    
    // Samples
    std::cout << "\n📊 Sample Count: " << std::setw(8) << total_samples << "\n";
    std::cout << "   TSC Frequency: " << std::setw(8) << TSCCalibration::get_freq_mhz() << " MHz\n";
    
    // Latency in cycles
    std::cout << "\n⏱️  LATENCY (CPU Cycles)\n";
    std::cout << "   ├─ Min:        " << std::setw(15) << stats.min() << " cycles\n";
    std::cout << "   ├─ Mean:       " << std::setw(15) << static_cast<uint64_t>(avg_cycles) << " cycles\n";
    std::cout << "   ├─ Median:     " << std::setw(15) << stats.median() << " cycles\n";
    std::cout << "   ├─ P99:        " << std::setw(15) << stats.p99() << " cycles";
    double ratio_p99 = static_cast<double>(stats.p99()) / stats.min();
    std::cout << "  [" << std::setw(5) << ratio_p99 << "x min]\n";
    std::cout << "   ├─ P999:       " << std::setw(15) << stats.p999() << " cycles";
    double ratio_p999 = static_cast<double>(stats.p999()) / stats.min();
    std::cout << "  [" << std::setw(5) << ratio_p999 << "x min]\n";
    std::cout << "   ├─ P9999:      " << std::setw(15) << stats.p9999() << " cycles";
    double ratio_p9999 = static_cast<double>(stats.p9999()) / stats.min();
    std::cout << "  [" << std::setw(5) << ratio_p9999 << "x min]\n";
    std::cout << "   └─ Max:        " << std::setw(15) << stats.max() << " cycles";
    double ratio_max = static_cast<double>(stats.max()) / stats.min();
    std::cout << "  [" << std::setw(5) << ratio_max << "x min]\n";
    
    // Latency in microseconds
    std::cout << "\n⏱️  LATENCY (Microseconds)\n";
    std::cout << "   ├─ Min:        " << std::setw(12) << TSCCalibration::cycles_to_us(stats.min()) << " µs\n";
    std::cout << "   ├─ Mean:       " << std::setw(12) << TSCCalibration::cycles_to_us(static_cast<uint64_t>(avg_cycles)) << " µs\n";
    std::cout << "   ├─ Median:     " << std::setw(12) << TSCCalibration::cycles_to_us(stats.median()) << " µs\n";
    std::cout << "   ├─ P99:        " << std::setw(12) << TSCCalibration::cycles_to_us(stats.p99()) << " µs\n";
    std::cout << "   ├─ P999:       " << std::setw(12) << TSCCalibration::cycles_to_us(stats.p999()) << " µs\n";
    std::cout << "   ├─ P9999:      " << std::setw(12) << TSCCalibration::cycles_to_us(stats.p9999()) << " µs\n";
    std::cout << "   └─ Max:        " << std::setw(12) << TSCCalibration::cycles_to_us(stats.max()) << " µs\n";
    
    // Jitter metrics
    std::cout << "\n📈 JITTER & CONSISTENCY\n";
    std::cout << "   ├─ Std Dev:    " << std::setw(12) << TSCCalibration::cycles_to_us(static_cast<uint64_t>(stats.stddev())) << " µs\n";
    std::cout << "   └─ Coeff. Var: " << std::setw(12) << (stats.coefficient_of_variation() * 100) << " %\n";
    
    // Throughput
    std::cout << "\n🚀 THROUGHPUT\n";
    std::cout << "   └─ Tasks/sec:  " << std::setw(12) << (total_samples / ((total_cycles / static_cast<double>(TSCCalibration::get_freq_mhz())) / 1000000)) << " tasks/sec\n";
    
    // Latency distribution histogram
    std::cout << "\n📊 LATENCY DISTRIBUTION (Histogram)\n";
    print_histogram(stats);
    
    std::cout << "\n";
}

// ============================================================================
// HISTOGRAM VISUALIZATION
// ============================================================================

void print_histogram(const LatencyStats& stats) {
    const int HISTOGRAM_BUCKETS = 20;
    const int BAR_WIDTH = 60;
    
    if (stats.count() < 10) {
        std::cout << "   (Not enough samples for histogram)\n";
        return;
    }
    
    // Create buckets
    std::vector<uint64_t> bucket_counts(HISTOGRAM_BUCKETS, 0);
    uint64_t min_val = stats.min();
    uint64_t max_val = stats.max();
    uint64_t bucket_width = (max_val - min_val) / HISTOGRAM_BUCKETS + 1;
    
    for (uint64_t sample : stats.samples) {
        size_t bucket = (sample - min_val) / bucket_width;
        if (bucket >= HISTOGRAM_BUCKETS) bucket = HISTOGRAM_BUCKETS - 1;
        bucket_counts[bucket]++;
    }
    
    // Find max count for scaling
    uint64_t max_count = *std::max_element(bucket_counts.begin(), bucket_counts.end());
    
    // Print bars
    for (int i = 0; i < HISTOGRAM_BUCKETS; ++i) {
        uint64_t bucket_start = min_val + (i * bucket_width);
        uint64_t bucket_end = bucket_start + bucket_width;
        
        double start_us = TSCCalibration::cycles_to_us(bucket_start);
        double end_us = TSCCalibration::cycles_to_us(bucket_end);
        
        std::cout << "   [" << std::setw(6) << std::fixed << std::setprecision(1) 
                  << start_us << "-" << std::setw(6) << end_us << " µs] ";
        
        // Draw bar
        int bar_len = (bucket_counts[i] * BAR_WIDTH) / max_count;
        for (int j = 0; j < bar_len; ++j) std::cout << "█";
        std::cout << " " << bucket_counts[i] << "\n";
    }
}

// ============================================================================
// COMPARISON PRINTER
// ============================================================================

struct BenchmarkResult {
    std::string name;
    std::vector<uint64_t> latencies;
    
    LatencyStats get_stats() const {
        LatencyStats stats;
        stats.samples = latencies;
        std::sort(stats.samples.begin(), stats.samples.end());
        return stats;
    }
};

void print_comparison(const std::vector<BenchmarkResult>& results) {
    TSCCalibration::calibrate();
    
    if (results.size() < 2) {
        std::cout << "Need at least 2 results to compare\n";
        return;
    }
    
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                        COMPARISON SUMMARY                                      ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════════════════════╝\n";
    std::cout << std::fixed << std::setprecision(2);
    
    std::cout << "\n";
    std::cout << "╔═══════════════════╦════════════════╦════════════════╦══════════════╗\n";
    std::cout << "║ Metric            ║ " << std::setw(12) << results[0].name << " ║ " << std::setw(12) << results[1].name << " ║ Improvement  ║\n";
    std::cout << "╠═══════════════════╬════════════════╬════════════════╬══════════════╣\n";
    
    auto stats0 = results[0].get_stats();
    auto stats1 = results[1].get_stats();
    
    // P50
    double p50_0 = TSCCalibration::cycles_to_us(stats0.p50());
    double p50_1 = TSCCalibration::cycles_to_us(stats1.p50());
    double p50_ratio = p50_0 / p50_1;
    std::cout << "║ P50 Latency (µs)  ║ " << std::setw(12) << p50_0 << " ║ " << std::setw(12) << p50_1 << " ║ ";
    if (p50_ratio > 1) std::cout << std::setw(8) << p50_ratio << "x faster ║\n";
    else std::cout << std::setw(8) << (1.0/p50_ratio) << "x slower  ║\n";
    
    // P99
    double p99_0 = TSCCalibration::cycles_to_us(stats0.p99());
    double p99_1 = TSCCalibration::cycles_to_us(stats1.p99());
    double p99_ratio = p99_0 / p99_1;
    std::cout << "║ P99 Latency (µs)  ║ " << std::setw(12) << p99_0 << " ║ " << std::setw(12) << p99_1 << " ║ ";
    if (p99_ratio > 1) std::cout << std::setw(8) << p99_ratio << "x faster ║\n";
    else std::cout << std::setw(8) << (1.0/p99_ratio) << "x slower  ║\n";
    
    // P999
    double p999_0 = TSCCalibration::cycles_to_us(stats0.p999());
    double p999_1 = TSCCalibration::cycles_to_us(stats1.p999());
    double p999_ratio = p999_0 / p999_1;
    std::cout << "║ P999 Latency (µs) ║ " << std::setw(12) << p999_0 << " ║ " << std::setw(12) << p999_1 << " ║ ";
    if (p999_ratio > 1) std::cout << std::setw(8) << p999_ratio << "x faster ║\n";
    else std::cout << std::setw(8) << (1.0/p999_ratio) << "x slower  ║\n";
    
    // Coefficient of variation
    double cv0 = stats0.coefficient_of_variation() * 100;
    double cv1 = stats1.coefficient_of_variation() * 100;
    std::cout << "║ Jitter (CV %)     ║ " << std::setw(12) << cv0 << " ║ " << std::setw(12) << cv1 << " ║ ";
    if (cv0 > cv1) std::cout << std::setw(8) << (cv0/cv1) << "x better  ║\n";
    else std::cout << std::setw(8) << (cv1/cv0) << "x worse   ║\n";
    
    // Samples
    std::cout << "║ Sample Count      ║ " << std::setw(12) << stats0.count() << " ║ " << std::setw(12) << stats1.count() << " ║              ║\n";
    
    std::cout << "╚═══════════════════╩════════════════╩════════════════╩══════════════╝\n";
}