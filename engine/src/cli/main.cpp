#include "core/dxf.hpp"
#include "core/gpu_context.hpp"
#include "core/nesting.hpp"
#include "core/perf_log.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct CliOptions {
    std::vector<std::filesystem::path> inputs;
    std::filesystem::path output = "nesting_result_cpp.dxf";
    smn::Sheet sheet;
    smn::DxfLoadOptions load;
    smn::NestingOptions nesting;
    int iterations = 1;
    int cpu_cores = 1;
    int ga_population = 1;
    double ga_mutation_rate = 10.0;
    double ga_random_shuffle_prob = 0.0;
    double ga_random_shuffle_intensity = 25.0;
    smn::SolverMode solver_mode = smn::SolverMode::CPU;
    bool merge_common_lines = false;
    std::string perf_log_path;
    std::string metrics_csv_path;
};

std::vector<double> parse_rotations(const std::string& raw) {
    std::vector<double> values;
    std::stringstream stream(raw);
    std::string item;
    while (std::getline(stream, item, ',')) {
        if (!item.empty()) {
            values.push_back(std::stod(item));
        }
    }
    if (values.empty()) {
        throw std::runtime_error("No rotation angles were provided.");
    }
    return values;
}

void print_usage() {
    std::cout
        << "Sheet Metal Nesting C++\n"
        << "Usage:\n"
        << "  sheet_nest_cli --input DXF_FOLDER --sheet-width 3000 --sheet-height 1500 --output result.dxf\n\n"
        << "Options:\n"
        << "  --input PATH              DXF file or folder. Can be repeated.\n"
        << "  --output PATH             Output DXF path.\n"
        << "  --sheet-width MM          Sheet width.\n"
        << "  --sheet-height MM         Sheet height.\n"
        << "  --spacing MM              Clearance between parts.\n"
        << "  --rotations LIST          Example: 0,90,180,270.\n"
        << "  --quantity N              Quantity per DXF profile.\n"
        << "  --flattening-distance MM  Curve approximation precision.\n"
        << "  --endpoint-tolerance MM   Endpoint joining tolerance.\n"
        << "  --min-part-area MM2       Ignore tiny closed profiles.\n"
        << "  --iterations N            Run iterative meta-heuristic search.\n"
        << "  --cpu-cores N             Parallel workers for each iteration.\n"
        << "  --ga-population N         Candidate layouts per iteration.\n"
        << "  --ga-mutation-rate PCT    Random order mutations per candidate.\n"
        << "  --optimization-type NAME  bounding-box or compact-area.\n"
        << "  --optimization-ratio N    0..1 width/height bias.\n"
        << "  --solver MODE             cpu, gpu, dual-gpu, or random.\n"
        << "  --merge-common-lines      Export shared collinear cut lines only once.\n"
        << "  --perf-log PATH           Write performance CSV log to PATH.\n"
        << "  --metrics-csv PATH        Write per-iteration nesting metrics to CSV.\n";
}

CliOptions parse_args(int argc, char** argv) {
    CliOptions options;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto require_value = [&](const std::string& name) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing value for " + name);
            }
            return argv[++i];
        };

        if (arg == "--help" || arg == "-h") {
            print_usage();
            std::exit(0);
        } else if (arg == "--input" || arg == "-i") {
            options.inputs.emplace_back(require_value(arg));
        } else if (arg == "--output" || arg == "-o") {
            options.output = require_value(arg);
        } else if (arg == "--sheet-width") {
            options.sheet.width = std::stod(require_value(arg));
        } else if (arg == "--sheet-height") {
            options.sheet.height = std::stod(require_value(arg));
        } else if (arg == "--spacing") {
            options.nesting.spacing = std::stod(require_value(arg));
        } else if (arg == "--rotations") {
            options.nesting.rotations = parse_rotations(require_value(arg));
        } else if (arg == "--quantity") {
            options.load.quantity = std::stoi(require_value(arg));
        } else if (arg == "--flattening-distance") {
            options.load.flattening_distance = std::stod(require_value(arg));
        } else if (arg == "--min-part-area") {
            options.load.min_part_area = std::stod(require_value(arg));
        } else if (arg == "--endpoint-tolerance") {
            options.load.endpoint_tolerance = std::stod(require_value(arg));
        } else if (arg == "--iterations") {
            options.iterations = std::stoi(require_value(arg));
        } else if (arg == "--cpu-cores") {
            options.cpu_cores = std::stoi(require_value(arg));
        } else if (arg == "--ga-population") {
            options.ga_population = std::stoi(require_value(arg));
        } else if (arg == "--ga-mutation-rate") {
            options.ga_mutation_rate = std::stod(require_value(arg));
        } else if (arg == "--ga-random-shuffle-prob") {
            options.ga_random_shuffle_prob = std::stod(require_value(arg));
        } else if (arg == "--ga-random-shuffle-intensity") {
            options.ga_random_shuffle_intensity = std::stod(require_value(arg));
        } else if (arg == "--optimization-ratio") {
            options.nesting.optimization_ratio = std::stod(require_value(arg));
        } else if (arg == "--optimization-type") {
            const std::string value = require_value(arg);
            options.nesting.optimization_type =
                value == "compact-area" ? smn::OptimizationType::CompactArea : smn::OptimizationType::BoundingBox;
        } else if (arg == "--solver") {
            options.solver_mode = smn::solver_mode_from_string(require_value(arg));
        } else if (arg == "--merge-common-lines") {
            options.merge_common_lines = true;
        } else if (arg == "--perf-log") {
            options.perf_log_path = require_value(arg);
        } else if (arg == "--metrics-csv") {
            options.metrics_csv_path = require_value(arg);
        } else {
            options.inputs.emplace_back(arg);
        }
    }

    if (options.inputs.empty()) {
        throw std::runtime_error("Pass at least one DXF file/folder.");
    }
    return options;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const CliOptions options = parse_args(argc, argv);

        // Initialize GPU if requested
        if (smn::solver_mode_uses_gpu(options.solver_mode)) {
            std::cout << "Initializing GPU...\n";
            if (!smn::GpuContext::instance().init()) {
                std::cerr << "Warning: GPU init failed, falling back to CPU.\n";
            } else {
                auto& gpu = smn::GpuContext::instance();
                std::cout << "GPU ready: " << gpu.device_count() << " device(s)\n";
                for (int i = 0; i < gpu.device_count(); ++i) {
                    std::cout << "  [" << i << "] " << gpu.device(i).name
                              << " (" << gpu.device(i).compute_units << " CUs, "
                              << (gpu.device(i).global_mem / (1024 * 1024)) << " MB VRAM)\n";
                }
            }
        }

        // Performance logging
        smn::PerfLog perf_log;
        if (!options.perf_log_path.empty()) {
            perf_log.set_output(options.perf_log_path);
            std::cout << "Performance log: " << options.perf_log_path << "\n";
        }

        const auto parts = smn::load_dxf_parts(options.inputs, options.load);
        if (parts.empty()) {
            throw std::runtime_error("No closed DXF profiles were found.");
        }

        smn::NestingResult result;
        std::vector<smn::IterationStats> history;

        smn::SolverMode effective_solver = options.solver_mode;
        if (smn::solver_mode_uses_gpu(effective_solver) && !smn::GpuContext::instance().available()) {
            std::cerr << "GPU not available, using CPU.\n";
            effective_solver = smn::SolverMode::CPU;
        }
        if (effective_solver == smn::SolverMode::GPU_Dual && smn::GpuContext::instance().device_count() < 2) {
            std::cerr << "Dual GPU requested but fewer than 2 GPUs are available, using single GPU.\n";
            effective_solver = smn::SolverMode::GPU_Single;
        }

        if (options.iterations > 1) {
            smn::IterativeNestingOptions iterative;
            iterative.base = options.nesting;
            iterative.base.solver_mode = effective_solver;
            iterative.iterations = options.iterations;
            iterative.cpu_cores = options.cpu_cores;
            iterative.ga_population = options.ga_population;
            iterative.ga_mutation_rate = options.ga_mutation_rate;
            iterative.ga_random_shuffle_prob = options.ga_random_shuffle_prob;
            iterative.ga_random_shuffle_intensity = options.ga_random_shuffle_intensity;
            iterative.perf_log = &perf_log;

            std::ofstream metrics_csv;
            if (!options.metrics_csv_path.empty()) {
                const std::filesystem::path metrics_path(options.metrics_csv_path);
                if (metrics_path.has_parent_path()) {
                    std::filesystem::create_directories(metrics_path.parent_path());
                }
                metrics_csv.open(metrics_path);
                if (!metrics_csv) {
                    throw std::runtime_error("Cannot write metrics CSV: " + options.metrics_csv_path);
                }
                metrics_csv
                    << "iteration,elapsed_seconds,delta_seconds,"
                    << "current_sheet_count,best_sheet_count,"
                    << "current_placements,best_placements,"
                    << "current_unplaced,best_unplaced,"
                    << "current_occupied_area,best_occupied_area,"
                    << "saved_area,best_utilization,time_to_find_best,improved\n";
            }

            auto run_started = std::chrono::steady_clock::now();
            auto previous_iteration_time = run_started;
            double previous_best_area = std::numeric_limits<double>::infinity();
            int previous_best_sheets = std::numeric_limits<int>::max();
            int previous_best_unplaced = std::numeric_limits<int>::max();

            auto cb = [
                &metrics_csv,
                &run_started,
                &previous_iteration_time,
                &previous_best_area,
                &previous_best_sheets,
                &previous_best_unplaced
            ](const smn::IterationStats& stats, const smn::NestingResult& current, const smn::NestingResult& best) {
                const auto now = std::chrono::steady_clock::now();
                const double elapsed = std::chrono::duration<double>(now - run_started).count();
                const double delta = std::chrono::duration<double>(now - previous_iteration_time).count();
                previous_iteration_time = now;

                const int best_sheets = best.sheet_count();
                const int best_unplaced = static_cast<int>(best.unplaced.size());
                const double best_area = stats.best_occupied_area;
                const bool improved =
                    best_unplaced < previous_best_unplaced ||
                    (best_unplaced == previous_best_unplaced && best_sheets < previous_best_sheets) ||
                    (best_unplaced == previous_best_unplaced &&
                     best_sheets == previous_best_sheets &&
                     best_area + 1e-6 < previous_best_area);

                if (improved) {
                    previous_best_unplaced = best_unplaced;
                    previous_best_sheets = best_sheets;
                    previous_best_area = best_area;
                }

                if (metrics_csv) {
                    metrics_csv << stats.iteration << ","
                                << std::fixed << std::setprecision(6)
                                << elapsed << ","
                                << delta << ","
                                << current.sheet_count() << ","
                                << best_sheets << ","
                                << current.placements.size() << ","
                                << best.placements.size() << ","
                                << current.unplaced.size() << ","
                                << best.unplaced.size() << ","
                                << stats.occupied_area << ","
                                << stats.best_occupied_area << ","
                                << stats.saved_area << ","
                                << best.utilization() << ","
                                << stats.time_to_find << ","
                                << (improved ? 1 : 0)
                                << "\n";
                }
                std::cout << "Iter " << stats.iteration << ": current sheets=" << current.sheet_count() << ", best sheets=" << best.sheet_count() << "\n";
                return true;
            };
            auto iterative_result = smn::nest_parts_iterative(options.sheet, parts, iterative, cb);
            result = iterative_result.best;
            history = std::move(iterative_result.history);
        } else {
            smn::NestingOptions single_opts = options.nesting;
            single_opts.solver_mode = effective_solver;
            result = smn::nest_parts(options.sheet, parts, single_opts);
        }
        const auto violations = smn::find_nesting_violations(result, options.nesting.spacing);
        if (!violations.empty()) {
            std::cerr << "Warning: " << violations.size() << " spacing/overlap violation(s) found in result.\n";
            const size_t limit = std::min<size_t>(violations.size(), 5);
            for (size_t index = 0; index < limit; ++index) {
                const auto& violation = violations[index];
                const auto& first = result.placements[violation.first_index];
                const auto& second = result.placements[violation.second_index];
                std::cerr << "  - " << first.name << " #" << first.instance_number
                          << " vs " << second.name << " #" << second.instance_number
                          << ": distance " << violation.distance << " mm"
                          << (violation.intersects ? " (intersects)" : "")
                          << "\n";
            }
        }
        smn::write_nesting_dxf(result, options.output, 250.0, {}, 4, options.merge_common_lines);

        std::cout << "Sheet Metal Nesting C++\n";
        std::cout << "Solver: " << smn::solver_mode_name(effective_solver) << "\n";
        std::cout << "Loaded profiles: " << parts.size() << "\n";
        std::cout << "Placed parts: " << result.placements.size() << "\n";
        std::cout << "Sheets used: " << result.sheet_count() << "\n";
        std::cout << "Utilization: " << result.utilization() * 100.0 << "%\n";
        if (!history.empty()) {
            std::cout << "Iterations: " << history.size() << "\n";
            std::cout << "Saved area vs first solution: " << history.back().saved_area << " mm2\n";
        }
        std::cout << "Scrap area: " << result.scrap_area() << " mm2\n";
        std::cout << "Output: " << options.output.string() << "\n";
        if (!result.unplaced.empty()) {
            std::cout << "Unplaced parts:\n";
            for (const auto& part : result.unplaced) {
                const auto bounds = part.bounds();
                std::cout << "  - " << part.name << " (" << bounds.width() << " x " << bounds.height() << ")\n";
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n";
        print_usage();
        return 1;
    }
    return 0;
}
