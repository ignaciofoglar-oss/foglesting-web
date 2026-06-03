#pragma once

#include "core/geometry.hpp"
#include "core/perf_log.hpp"

#include <functional>
#include <vector>

namespace smn {

enum class OptimizationType {
    BoundingBox,
    CompactArea,
};

struct NestingOptions {
    double spacing = 5.0;
    std::vector<double> rotations = {0.0, 90.0, 180.0, 270.0};
    int max_sheets = 50;
    bool use_nfp_candidates = true;
    bool use_compaction = true;
    OptimizationType optimization_type = OptimizationType::BoundingBox;
    double optimization_ratio = 0.5;
    int placement_pool = 1;
    int placement_variant = 0;
    SolverMode solver_mode = SolverMode::CPU;
};

struct IterativeNestingOptions {
    NestingOptions base;
    int iterations = 250;
    unsigned int seed = 42;
    int cpu_cores = 4;
    int ga_population = 1;
    double ga_mutation_rate = 10.0;
    double ga_random_shuffle_prob = 0.0;
    double ga_random_shuffle_intensity = 25.0;
    PerfLog* perf_log = nullptr;
};

#include <vector>

struct MutationInfo {
    double area = 0.0;
    bool was_shuffle = false;
};

struct IterationStats {
    int iteration = 0;
    double occupied_area = 0.0;
    double best_occupied_area = 0.0;
    double saved_area = 0.0;
    double utilization = 0.0;
    double time_to_find = 0.0;
    std::vector<MutationInfo> discarded_mutations;
    bool was_shuffle = false;
};

struct IterativeNestingResult {
    NestingResult best;
    std::vector<IterationStats> history;
};

struct NestingViolation {
    size_t first_index = 0;
    size_t second_index = 0;
    double distance = 0.0;
    bool intersects = false;
};

using IterationCallback = std::function<bool(const IterationStats&, const NestingResult&, const NestingResult&)>;

NestingResult nest_parts(const Sheet& sheet, const std::vector<Part>& parts, const NestingOptions& options);
IterativeNestingResult nest_parts_iterative(
    const Sheet& sheet,
    const std::vector<Part>& parts,
    const IterativeNestingOptions& options,
    const IterationCallback& callback = {}
);
double occupied_layout_area(const NestingResult& result);
std::vector<NestingViolation> find_nesting_violations(const NestingResult& result, double spacing);

}  // namespace smn
