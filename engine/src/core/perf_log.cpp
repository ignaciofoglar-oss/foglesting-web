#include "core/perf_log.hpp"

#include <iomanip>
#include <sstream>

namespace smn {

void PerfLog::set_output(const std::string& csv_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    path_ = csv_path;
    enabled_ = !csv_path.empty();
    entries_.clear();
}

void PerfLog::add(const PerfEntry& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.push_back(entry);
}

void PerfLog::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!enabled_ || path_.empty()) {
        return;
    }

    std::ofstream file(path_, std::ios::trunc);
    if (!file.is_open()) {
        return;
    }

    file << "iteration,solver,gpu_name,total_ms,placement_ms,positions_evaluated,collision_tests,parts_placed\n";
    for (const auto& entry : entries_) {
        file << entry.iteration << ","
             << solver_mode_name(entry.solver) << ","
             << entry.gpu_name << ","
             << std::fixed << std::setprecision(2) << entry.total_ms << ","
             << std::fixed << std::setprecision(2) << entry.placement_ms << ","
             << entry.positions_evaluated << ","
             << entry.collision_tests << ","
             << entry.parts_placed << "\n";
    }
    file.flush();
}

}  // namespace smn
