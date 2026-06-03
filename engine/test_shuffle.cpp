#include <iostream>
#include <vector>
#include <string>
#include "core/nesting.hpp"

int main() {
    smn::Sheet sheet;
    sheet.width = 3000;
    sheet.height = 1500;

    std::vector<smn::Part> parts;
    smn::Part p1;
    p1.geometry.outer = {{0,0}, {1000,0}, {1000,1000}, {0,1000}};
    p1.quantity = 2;
    parts.push_back(p1);

    smn::Part p2;
    p2.geometry.outer = {{0,0}, {500,0}, {500,500}, {0,500}};
    p2.quantity = 5;
    parts.push_back(p2);

    smn::IterativeNestingOptions options;
    options.iterations = 50;
    options.cpu_cores = 1;
    options.ga_population = 10;
    options.ga_mutation_rate = 10.0;
    options.ga_random_shuffle_prob = 5.0;
    options.ga_random_shuffle_intensity = 2.0;
    options.base.solver_mode = smn::SolverMode::CPU;

    auto cb = [](const smn::IterationStats& stats, const smn::NestingResult& current, const smn::NestingResult& best) {
        if (current.sheet_count() > best.sheet_count() || stats.iteration == 1) {
            std::cout << "Iteration: " << stats.iteration 
                      << " | Current sheets: " << current.sheet_count()
                      << " | Best sheets: " << best.sheet_count()
                      << " | Best area: " << best.utilization() << "\n";
        }
        return true;
    };

    auto result = smn::nest_parts_iterative(sheet, parts, options, cb);
    std::cout << "Final best sheets: " << result.best.sheet_count() << "\n";
    return 0;
}
