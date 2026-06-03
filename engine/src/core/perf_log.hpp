#pragma once

#include <chrono>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

namespace smn {

enum class SolverMode {
    CPU,
    GPU_Single,
    GPU_Dual,
    Random,
};

inline const char* solver_mode_name(SolverMode mode) {
    switch (mode) {
        case SolverMode::CPU:        return "cpu";
        case SolverMode::GPU_Single: return "gpu";
        case SolverMode::GPU_Dual:   return "dual-gpu";
        case SolverMode::Random:     return "random";
    }
    return "unknown";
}

inline SolverMode solver_mode_from_string(const std::string& value) {
    if (value == "gpu")      return SolverMode::GPU_Single;
    if (value == "dual-gpu") return SolverMode::GPU_Dual;
    if (value == "random")   return SolverMode::Random;
    return SolverMode::CPU;
}

inline bool solver_mode_uses_gpu(SolverMode mode) {
    return mode == SolverMode::GPU_Single || mode == SolverMode::GPU_Dual;
}

struct PerfEntry {
    int iteration = 0;
    double total_ms = 0.0;
    double placement_ms = 0.0;
    int positions_evaluated = 0;
    int collision_tests = 0;
    int parts_placed = 0;
    SolverMode solver = SolverMode::CPU;
    std::string gpu_name;
};

class PerfLog {
public:
    void set_output(const std::string& csv_path);
    void add(const PerfEntry& entry);
    void flush();
    bool enabled() const { return enabled_; }
    const std::vector<PerfEntry>& entries() const { return entries_; }

private:
    bool enabled_ = false;
    std::string path_;
    std::vector<PerfEntry> entries_;
    std::mutex mutex_;
};

class ScopedTimer {
public:
    ScopedTimer() : start_(std::chrono::high_resolution_clock::now()) {}

    double elapsed_ms() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(now - start_).count();
    }

    void reset() { start_ = std::chrono::high_resolution_clock::now(); }

private:
    std::chrono::high_resolution_clock::time_point start_;
};

}  // namespace smn
