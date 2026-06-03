#include "core/nesting.hpp"
#include "core/gpu_context.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <future>
#include <limits>
#include <random>
#include <stdexcept>
#include <tuple>

namespace smn {
namespace {

constexpr double kTolerance = 1e-6;
constexpr int kRepeatedPatternThreshold = 200;

struct PartInstance {
    const Part* part = nullptr;
    int instance_number = 1;
    bool locked_rotation = false;
    double rotation = 0.0;
};

struct PlacementScore {
    double primary = 0.0;
    double secondary = 0.0;
    double bounding_area = 0.0;
    double used_height = 0.0;
    double used_width = 0.0;
    double y = 0.0;
    double x = 0.0;

    bool operator<(const PlacementScore& other) const {
        return std::tie(primary, secondary, bounding_area, used_height, used_width, y, x) <
               std::tie(other.primary, other.secondary, other.bounding_area, other.used_height, other.used_width, other.y, other.x);
    }
};

struct PlacementCandidate {
    PlacementScore score;
    PlacedPart placement;
};

struct RepeatedPattern {
    Polygon geometry;
    double angle = 0.0;
    double step_x = 0.0;
    double step_y = 0.0;
    double row_offset = 0.0;
    int columns = 0;
    int offset_columns = 0;
    int rows = 0;
    int per_sheet = 0;
    double used_area = std::numeric_limits<double>::infinity();
};

std::vector<PartInstance> expand_instances(const std::vector<Part>& parts) {
    std::vector<PartInstance> instances;
    for (const auto& part : parts) {
        for (int index = 1; index <= part.quantity; ++index) {
            instances.push_back({&part, index});
        }
    }
    return instances;
}

bool pair_clear_with_shift(
    const Polygon& base,
    const Polygon& shifted_source,
    double dx,
    double dy,
    double spacing
) {
    const Polygon shifted = translate_polygon(shifted_source, dx, dy);
    if (spacing <= kTolerance) {
        return !polygon_intersects(base, shifted);
    }
    return !polygon_intersects(base, shifted) &&
        polygon_distance(base, shifted) >= spacing - kTolerance;
}

double minimum_clear_dx(
    const Polygon& base,
    const Polygon& shifted_source,
    double guaranteed_clear_dx,
    double spacing
) {
    const int samples = 160;
    double previous = 0.0;
    double first_clear = guaranteed_clear_dx;
    for (int i = 1; i <= samples; ++i) {
        const double dx = guaranteed_clear_dx * static_cast<double>(i) / static_cast<double>(samples);
        if (pair_clear_with_shift(base, shifted_source, dx, 0.0, spacing)) {
            first_clear = dx;
            break;
        }
        previous = dx;
    }

    double low = previous;
    double high = first_clear;
    for (int i = 0; i < 28; ++i) {
        const double mid = (low + high) * 0.5;
        if (pair_clear_with_shift(base, shifted_source, mid, 0.0, spacing)) {
            high = mid;
        } else {
            low = mid;
        }
    }
    return std::max(high, 1e-3);
}

bool row_offset_clear(
    const Polygon& geometry,
    double step_x,
    double row_offset,
    double dy,
    double spacing
) {
    for (int neighbor = -2; neighbor <= 2; ++neighbor) {
        const double dx = row_offset + static_cast<double>(neighbor) * step_x;
        if (!pair_clear_with_shift(geometry, geometry, dx, dy, spacing)) {
            return false;
        }
    }
    return true;
}

double minimum_clear_dy_for_offset(
    const Polygon& geometry,
    double step_x,
    double row_offset,
    double guaranteed_clear_dy,
    double spacing
) {
    const int samples = 160;
    double previous = 0.0;
    double first_clear = guaranteed_clear_dy;
    for (int i = 1; i <= samples; ++i) {
        const double dy = guaranteed_clear_dy * static_cast<double>(i) / static_cast<double>(samples);
        if (row_offset_clear(geometry, step_x, row_offset, dy, spacing)) {
            first_clear = dy;
            break;
        }
        previous = dy;
    }

    double low = previous;
    double high = first_clear;
    for (int i = 0; i < 28; ++i) {
        const double mid = (low + high) * 0.5;
        if (row_offset_clear(geometry, step_x, row_offset, mid, spacing)) {
            high = mid;
        } else {
            low = mid;
        }
    }
    return std::max(high, 1e-3);
}

bool repeated_pattern_better(const RepeatedPattern& candidate, const RepeatedPattern& best, bool found) {
    return !found ||
        candidate.per_sheet > best.per_sheet ||
        (candidate.per_sheet == best.per_sheet && candidate.used_area < best.used_area);
}

bool find_repeated_pattern(
    const Sheet& sheet,
    const Part& part,
    const NestingOptions& options,
    RepeatedPattern& best
) {
    if (part.quantity < kRepeatedPatternThreshold || part.quantity < 1) {
        return false;
    }

    const std::vector<double> rotations = part.can_rotate ? options.rotations : std::vector<double>{0.0};
    bool found = false;
    for (double angle : rotations) {
        Polygon rotated = rotate_to_origin(part.geometry, angle);
        const Bounds bounds = polygon_bounds(rotated);
        if (!bounds.valid() ||
            bounds.width() <= 0.0 ||
            bounds.height() <= 0.0 ||
            bounds.width() > sheet.width + kTolerance ||
            bounds.height() > sheet.height + kTolerance) {
            continue;
        }

        const double step_x = minimum_clear_dx(
            rotated,
            rotated,
            bounds.width() + options.spacing,
            options.spacing
        );
        if (step_x <= kTolerance) {
            continue;
        }

        const int base_columns = std::max(
            1,
            1 + static_cast<int>(std::floor((sheet.width - bounds.width() + kTolerance) / step_x))
        );
        if (base_columns < 1) {
            continue;
        }

        const std::vector<double> offset_ratios = {0.0, 0.25, 0.5, 0.75};
        for (double offset_ratio : offset_ratios) {
            const double row_offset = step_x * offset_ratio;
            const int offset_columns = row_offset + bounds.width() <= sheet.width + kTolerance
                ? std::max(1, 1 + static_cast<int>(std::floor((sheet.width - row_offset - bounds.width() + kTolerance) / step_x)))
                : 0;
            if (offset_ratio > 0.0 && offset_columns < 1) {
                continue;
            }

            const double step_y = minimum_clear_dy_for_offset(
                rotated,
                step_x,
                row_offset,
                bounds.height() + options.spacing,
                options.spacing
            );
            if (step_y <= kTolerance) {
                continue;
            }

            const int rows = std::max(
                1,
                1 + static_cast<int>(std::floor((sheet.height - bounds.height() + kTolerance) / step_y))
            );
            const int even_rows = (rows + 1) / 2;
            const int odd_rows = rows / 2;
            const int columns = base_columns;
            const int per_sheet = even_rows * columns + odd_rows * offset_columns;
            if (per_sheet <= 0) {
                continue;
            }

            const double even_width = (columns - 1) * step_x + bounds.width();
            const double odd_width = offset_columns > 0
                ? row_offset + (offset_columns - 1) * step_x + bounds.width()
                : 0.0;
            const double used_width = std::max(even_width, odd_width);
            const double used_height = (rows - 1) * step_y + bounds.height();
            RepeatedPattern candidate;
            candidate.geometry = rotated;
            candidate.angle = angle;
            candidate.step_x = step_x;
            candidate.step_y = step_y;
            candidate.row_offset = row_offset;
            candidate.columns = columns;
            candidate.offset_columns = offset_columns;
            candidate.rows = rows;
            candidate.per_sheet = per_sheet;
            candidate.used_area = used_width * used_height;

            if (repeated_pattern_better(candidate, best, found)) {
                best = std::move(candidate);
                found = true;
            }
        }
    }

    return found && best.per_sheet > 0;
}

void append_repeated_pattern_placements(
    NestingResult& result,
    const Part& part,
    const RepeatedPattern& pattern,
    int sheet_offset
) {
    result.placements.reserve(result.placements.size() + static_cast<size_t>(part.quantity));
    int placed = 0;
    int local_sheet_index = 0;
    while (placed < part.quantity) {
        for (int row = 0; row < pattern.rows && placed < part.quantity; ++row) {
            const bool offset_row = (row % 2) == 1;
            const int row_columns = offset_row ? pattern.offset_columns : pattern.columns;
            const double row_offset = offset_row ? pattern.row_offset : 0.0;
            for (int column = 0; column < row_columns && placed < part.quantity; ++column) {
                const double x = row_offset + column * pattern.step_x;
                const double y = row * pattern.step_y;
                Polygon geometry = translate_polygon(pattern.geometry, x, y);
                result.placements.push_back({
                    part.name,
                    part.source,
                    std::move(geometry),
                    sheet_offset + local_sheet_index,
                    placed + 1,
                    x,
                    y,
                    pattern.angle,
                    part.area(),
                });
                ++placed;
            }
        }
        ++local_sheet_index;
    }
}

bool try_repeated_part_pattern(
    const Sheet& sheet,
    const std::vector<Part>& parts,
    const NestingOptions& options,
    NestingResult& result
) {
    if (options.solver_mode == SolverMode::Random || parts.size() != 1) {
        return false;
    }

    RepeatedPattern best;
    if (!find_repeated_pattern(sheet, parts.front(), options, best)) {
        return false;
    }
    result = {};
    result.sheet = sheet;
    append_repeated_pattern_placements(result, parts.front(), best, 0);
    return true;
}

bool build_repeated_seeded_layout(
    const Sheet& sheet,
    const std::vector<Part>& parts,
    const NestingOptions& options,
    NestingResult& seeded_result,
    std::vector<Part>& remaining_parts
) {
    if (options.solver_mode == SolverMode::Random) {
        return false;
    }

    seeded_result = {};
    seeded_result.sheet = sheet;
    remaining_parts.clear();

    bool seeded_any = false;
    for (const auto& part : parts) {
        RepeatedPattern pattern;
        if (find_repeated_pattern(sheet, part, options, pattern)) {
            const int sheet_offset = seeded_result.placements.empty() ? 0 : seeded_result.sheet_count();
            append_repeated_pattern_placements(seeded_result, part, pattern, sheet_offset);
            seeded_any = true;
        } else {
            remaining_parts.push_back(part);
        }
    }
    return seeded_any;
}

std::vector<std::vector<PlacedPart>> extract_sheet_placements(NestingResult&& seeded_result) {
    const int sheet_count = std::max(1, seeded_result.sheet_count());
    std::vector<std::vector<PlacedPart>> sheet_placements(static_cast<size_t>(sheet_count));
    for (auto& placement : seeded_result.placements) {
        if (placement.sheet_index < 0) {
            continue;
        }
        const auto sheet_index = static_cast<size_t>(placement.sheet_index);
        if (sheet_index >= sheet_placements.size()) {
            sheet_placements.resize(sheet_index + 1);
        }
        sheet_placements[sheet_index].push_back(std::move(placement));
    }
    return sheet_placements;
}

std::vector<PartInstance> order_instances(
    std::vector<PartInstance> instances,
    int iteration,
    std::mt19937& rng,
    double mutation_rate,
    double random_shuffle_prob,
    double random_shuffle_intensity,
    const NestingOptions& options,
    bool is_first_member,
    bool& out_was_shuffle
) {
    out_was_shuffle = false;
    if (iteration <= 1) {
        std::sort(instances.begin(), instances.end(), [](const PartInstance& first, const PartInstance& second) {
            return first.part->area() > second.part->area();
        });
        if (is_first_member) {
            return instances;
        }
    }

    // If it's the very first member of an advanced generation, we want to evaluate the EXACT same
    // winning order from the previous generation to guarantee we don't regress.
    if (is_first_member) {
        return instances;
    }

    // For other members, we take the winning order and apply gentle mutations (Hill Climbing / Local Search)
    const double clamped_mutation = std::clamp(mutation_rate, 0.0, 100.0);
    const double clamped_shuffle_prob = std::clamp(random_shuffle_prob, 0.0, 100.0);
    const double clamped_shuffle_intensity = std::clamp(random_shuffle_intensity, 0.0, 100.0);
    std::uniform_real_distribution<double> chance(0.0, 100.0);

    if (instances.size() >= 2 && chance(rng) < clamped_shuffle_prob) {
        out_was_shuffle = true;
        const int last_index = static_cast<int>(instances.size()) - 1;
        const double intensity_ratio = clamped_shuffle_intensity / 100.0;
        const int max_distance = std::max(
            1,
            static_cast<int>(std::round(static_cast<double>(instances.size()) * (0.04 + 0.20 * intensity_ratio)))
        );
        const int shuffle_swaps = std::max(
            1,
            static_cast<int>(std::round(static_cast<double>(instances.size()) * (0.10 + 0.65 * intensity_ratio)))
        );
        std::uniform_int_distribution<int> first_dist(0, last_index);
        std::uniform_int_distribution<int> offset_dist(-max_distance, max_distance);
        for (int i = 0; i < shuffle_swaps; ++i) {
            const int first = first_dist(rng);
            const int second = std::clamp(first + offset_dist(rng), 0, last_index);
            if (first != second) {
                std::swap(instances[static_cast<size_t>(first)], instances[static_cast<size_t>(second)]);
            }
        }
    } else {
        const int swap_count = static_cast<int>(
            std::round(static_cast<double>(instances.size()) * clamped_mutation / 100.0)
        );
        if (swap_count > 0 && instances.size() >= 2) {
            std::uniform_int_distribution<size_t> first_dist(0, instances.size() - 1);
            // Restrict swaps to nearby indices so large pieces only swap with large pieces
            int max_distance = std::max(2, static_cast<int>(instances.size() * 0.15));
            std::uniform_int_distribution<int> offset_dist(-max_distance, max_distance);
            
            for (int swap = 0; swap < swap_count; ++swap) {
                const size_t first = first_dist(rng);
                int second_idx = static_cast<int>(first) + offset_dist(rng);
                second_idx = std::clamp(second_idx, 0, static_cast<int>(instances.size() - 1));
                const size_t second = static_cast<size_t>(second_idx);
                
                if (first != second) {
                    std::swap(instances[first], instances[second]);
                }
            }
        }
    }

    if (iteration > 1 && options.rotations.size() > 1) {
        std::uniform_int_distribution<size_t> rotation_distribution(0, options.rotations.size() - 1);
        const double lock_probability = std::clamp(10.0 + mutation_rate * 0.6, 0.0, 75.0);
        for (auto& instance : instances) {
            if (!instance.part->can_rotate) {
                continue;
            }
            if (chance(rng) <= lock_probability) {
                instance.locked_rotation = true;
                instance.rotation = options.rotations[rotation_distribution(rng)];
            }
        }
    }
    return instances;
}

bool fits_on_sheet(const Sheet& sheet, const Polygon& geometry) {
    const Bounds bounds = polygon_bounds(geometry);
    return bounds.min_x >= -kTolerance &&
           bounds.min_y >= -kTolerance &&
           bounds.max_x <= sheet.width + kTolerance &&
           bounds.max_y <= sheet.height + kTolerance;
}

bool clears_existing(const Polygon& geometry, const std::vector<PlacedPart>& existing, double spacing) {
    const Bounds bounds = polygon_bounds(geometry);
    for (const auto& placement : existing) {
        const Bounds other_bounds = placement.bounds();
        if (!bounds.overlaps(other_bounds, spacing)) {
            continue;
        }
        if (spacing <= kTolerance) {
            if (polygon_intersects(geometry, placement.geometry)) {
                return false;
            }
        } else if (polygon_distance(geometry, placement.geometry) < spacing - kTolerance) {
            return false;
        }
    }
    return true;
}

bool add_unique_position(
    std::vector<Point>& positions,
    Point candidate,
    size_t max_positions = std::numeric_limits<size_t>::max()
) {
    if (positions.size() >= max_positions) {
        return false;
    }
    if (candidate.x < -kTolerance || candidate.y < -kTolerance) {
        return false;
    }
    for (const auto& position : positions) {
        if (std::abs(position.x - candidate.x) <= kTolerance &&
            std::abs(position.y - candidate.y) <= kTolerance) {
            return false;
        }
    }
    positions.push_back(candidate);
    return true;
}

std::vector<Point> sampled_vertices(const std::vector<Point>& ring, size_t max_vertices = 12) {
    if (ring.size() <= max_vertices) {
        return ring;
    }

    std::vector<Point> sampled;
    sampled.reserve(max_vertices);
    const size_t step = std::max<size_t>(1, ring.size() / max_vertices);
    for (size_t index = 0; index < ring.size() && sampled.size() < max_vertices; index += step) {
        sampled.push_back(ring[index]);
    }
    return sampled;
}

std::vector<Point> candidate_positions(
    const std::vector<PlacedPart>& existing,
    const Polygon& rotated_geometry,
    double spacing,
    bool use_nfp_candidates
) {
    std::vector<double> xs = {0.0};
    std::vector<double> ys = {0.0};
    for (const auto& placement : existing) {
        const Bounds bounds = placement.bounds();
        xs.push_back(bounds.min_x);
        xs.push_back(bounds.max_x + spacing);
        ys.push_back(bounds.min_y);
        ys.push_back(bounds.max_y + spacing);
    }

    std::sort(xs.begin(), xs.end());
    xs.erase(std::unique(xs.begin(), xs.end(), [](double a, double b) {
        return std::abs(a - b) <= kTolerance;
    }), xs.end());

    std::sort(ys.begin(), ys.end());
    ys.erase(std::unique(ys.begin(), ys.end(), [](double a, double b) {
        return std::abs(a - b) <= kTolerance;
    }), ys.end());

#ifdef __EMSCRIPTEN__
    // Browser/WASM runs single-threaded here. Keep the candidate cloud useful
    // but bounded so previews stay responsive for real jobs.
    const size_t max_positions = 650;
    const size_t grid_limit = 300;
    const size_t axis_limit = 36;
#else
    // Native builds can afford a larger search cloud.
    const size_t max_positions = 5000;
    const size_t grid_limit = 2000;
    const size_t axis_limit = 100;
#endif
    
    // Instead of cutting off the right/top sides by resizing sorted arrays,
    // we take a strided sample if we exceed the axis limit, preserving the full spread.
    if (xs.size() > axis_limit) {
        std::vector<double> xs_sampled;
        double step = static_cast<double>(xs.size()) / axis_limit;
        for (double i = 0; i < xs.size(); i += step) {
            xs_sampled.push_back(xs[static_cast<size_t>(i)]);
        }
        xs = std::move(xs_sampled);
    }
    if (ys.size() > axis_limit) {
        std::vector<double> ys_sampled;
        double step = static_cast<double>(ys.size()) / axis_limit;
        for (double i = 0; i < ys.size(); i += step) {
            ys_sampled.push_back(ys[static_cast<size_t>(i)]);
        }
        ys = std::move(ys_sampled);
    }

    std::vector<Point> positions;
    for (double y : ys) {
        for (double x : xs) {
            add_unique_position(positions, {x, y}, grid_limit);
        }
    }

    if (!use_nfp_candidates || existing.empty()) {
        return positions;
    }

    const Bounds candidate_bounds = polygon_bounds(rotated_geometry);
    const auto candidate_vertices = sampled_vertices(rotated_geometry.outer);
    for (const auto& placement : existing) {
        const Bounds existing_bounds = placement.bounds();
        const auto existing_vertices = sampled_vertices(placement.geometry.outer);

        add_unique_position(positions, {existing_bounds.max_x + spacing, existing_bounds.min_y}, max_positions);
        add_unique_position(positions, {existing_bounds.min_x, existing_bounds.max_y + spacing}, max_positions);

        for (const auto& hole : placement.geometry.holes) {
            const Bounds hole_bounds = ring_bounds(hole);
            const double usable_width = hole_bounds.width() - spacing * 2.0;
            const double usable_height = hole_bounds.height() - spacing * 2.0;
            if (usable_width + kTolerance < candidate_bounds.width() ||
                usable_height + kTolerance < candidate_bounds.height()) {
                continue;
            }

            const double left = hole_bounds.min_x + spacing - candidate_bounds.min_x;
            const double right = hole_bounds.max_x - spacing - candidate_bounds.max_x;
            const double bottom = hole_bounds.min_y + spacing - candidate_bounds.min_y;
            const double top = hole_bounds.max_y - spacing - candidate_bounds.max_y;
            
            const int steps = 8;
            for (int i = 0; i <= steps; ++i) {
                for (int j = 0; j <= steps; ++j) {
                    double x = left + (right - left) * static_cast<double>(i) / steps;
                    double y = bottom + (top - bottom) * static_cast<double>(j) / steps;
                    add_unique_position(positions, {x, y}, max_positions);
                }
            }

            const double candidate_center_x = (candidate_bounds.min_x + candidate_bounds.max_x) * 0.5;
            const double candidate_center_y = (candidate_bounds.min_y + candidate_bounds.max_y) * 0.5;
            for (int gx = 1; gx <= 4; ++gx) {
                for (int gy = 1; gy <= 4; ++gy) {
                    const Point sample{
                        hole_bounds.min_x + usable_width * static_cast<double>(gx) / 5.0 + spacing,
                        hole_bounds.min_y + usable_height * static_cast<double>(gy) / 5.0 + spacing,
                    };
                    if (!point_in_ring(sample, hole)) {
                        continue;
                    }
                    add_unique_position(positions, {
                        sample.x - candidate_center_x,
                        sample.y - candidate_center_y,
                    }, max_positions);
                }
            }

            const auto hole_vertices = sampled_vertices(hole, 8);
            const auto sparse_candidate_vertices = sampled_vertices(rotated_geometry.outer, 6);
            for (const auto& hole_vertex : hole_vertices) {
                for (const auto& candidate_vertex : sparse_candidate_vertices) {
                    add_unique_position(positions, {
                        hole_vertex.x - candidate_vertex.x + spacing,
                        hole_vertex.y - candidate_vertex.y + spacing,
                    }, max_positions);
                    add_unique_position(positions, {
                        hole_vertex.x - candidate_vertex.x - spacing,
                        hole_vertex.y - candidate_vertex.y - spacing,
                    }, max_positions);
                }
            }
        }

        if (positions.size() >= max_positions) {
            continue;
        }

        for (const auto& existing_vertex : existing_vertices) {
            for (const auto& candidate_vertex : candidate_vertices) {
                add_unique_position(positions, {
                    existing_vertex.x - candidate_vertex.x + spacing,
                    existing_vertex.y - candidate_vertex.y,
                }, max_positions);
                add_unique_position(positions, {
                    existing_vertex.x - candidate_vertex.x - candidate_bounds.width() - spacing,
                    existing_vertex.y - candidate_vertex.y,
                }, max_positions);
                add_unique_position(positions, {
                    existing_vertex.x - candidate_vertex.x,
                    existing_vertex.y - candidate_vertex.y + spacing,
                }, max_positions);
                add_unique_position(positions, {
                    existing_vertex.x - candidate_vertex.x,
                    existing_vertex.y - candidate_vertex.y - candidate_bounds.height() - spacing,
                }, max_positions);
                if (positions.size() >= max_positions) {
                    break;
                }
            }
            if (positions.size() >= max_positions) {
                break;
            }
        }
    }

    std::sort(positions.begin(), positions.end(), [](Point first, Point second) {
        if (std::abs(first.y - second.y) > kTolerance) {
            return first.y < second.y;
        }
        return first.x < second.x;
    });
    if (positions.size() > max_positions) {
        positions.resize(max_positions);
    }
    return positions;
}

PlacementScore score_placement(
    const Polygon& geometry,
    const Bounds& existing_bounds,
    bool has_existing,
    const NestingOptions& options
) {
    Bounds bounds = polygon_bounds(geometry);
    if (has_existing) {
        bounds.min_x = std::min(bounds.min_x, existing_bounds.min_x);
        bounds.min_y = std::min(bounds.min_y, existing_bounds.min_y);
        bounds.max_x = std::max(bounds.max_x, existing_bounds.max_x);
        bounds.max_y = std::max(bounds.max_y, existing_bounds.max_y);
    }
    const Bounds geometry_bounds = polygon_bounds(geometry);
    const double used_width = bounds.width();
    const double used_height = bounds.height();
    const double bounding_area = used_width * used_height;
    const double ratio = std::clamp(options.optimization_ratio, 0.0, 1.0);
    const double shape_bias = used_height * (1.0 - ratio) + used_width * ratio;

    PlacementScore score;
    score.bounding_area = bounding_area;
    score.used_height = used_height;
    score.used_width = used_width;
    score.y = geometry_bounds.min_y;
    score.x = geometry_bounds.min_x;
    if (options.optimization_type == OptimizationType::CompactArea) {
        score.primary = shape_bias;
        score.secondary = bounding_area;
    } else {
        score.primary = bounding_area;
        score.secondary = shape_bias;
    }
    return score;
}

bool find_best_placement(
    const Sheet& sheet,
    const PartInstance& instance,
    const std::vector<PlacedPart>& existing,
    int sheet_index,
    const NestingOptions& options,
    PlacedPart& output
) {
    std::vector<PlacementCandidate> feasible;
    feasible.reserve(64);

    Bounds existing_bounds;
    bool has_existing = false;
    for (const auto& placement : existing) {
        const Bounds bounds = placement.bounds();
        if (!has_existing) {
            existing_bounds = bounds;
            has_existing = true;
        } else {
            existing_bounds.min_x = std::min(existing_bounds.min_x, bounds.min_x);
            existing_bounds.min_y = std::min(existing_bounds.min_y, bounds.min_y);
            existing_bounds.max_x = std::max(existing_bounds.max_x, bounds.max_x);
            existing_bounds.max_y = std::max(existing_bounds.max_y, bounds.max_y);
        }
    }

    std::vector<double> locked_rotations;
    if (instance.part->can_rotate && instance.locked_rotation) {
        locked_rotations = {instance.rotation};
    } else {
        locked_rotations = instance.part->can_rotate ? options.rotations : std::vector<double>{0.0};
    }
    const auto& rotations = locked_rotations;

    for (double angle : rotations) {
        const Polygon rotated = rotate_to_origin(instance.part->geometry, angle);
        const auto positions = candidate_positions(existing, rotated, options.spacing, options.use_nfp_candidates);
        for (Point position : positions) {
            Polygon geometry = translate_polygon(rotated, position.x, position.y);
            if (!fits_on_sheet(sheet, geometry)) {
                continue;
            }
            if (!clears_existing(geometry, existing, options.spacing)) {
                continue;
            }

            const PlacementScore score = score_placement(geometry, existing_bounds, has_existing, options);
            feasible.push_back({
                score,
                {
                    instance.part->name,
                    instance.part->source,
                    std::move(geometry),
                    sheet_index,
                    instance.instance_number,
                    position.x,
                    position.y,
                    angle,
                    instance.part->area(),
                },
            });
        }
    }

    if (feasible.empty()) {
        return false;
    }

    std::sort(feasible.begin(), feasible.end(), [](const PlacementCandidate& first, const PlacementCandidate& second) {
        return first.score < second.score;
    });

    const int pool = std::clamp(
        options.placement_pool,
        1,
        static_cast<int>(feasible.size())
    );
    const int offset = std::max(0, options.placement_variant) +
        static_cast<int>(existing.size()) * 37 +
        sheet_index * 11;
    const int chosen = pool == 1 ? 0 : offset % pool;
    output = std::move(feasible[static_cast<size_t>(chosen)].placement);
    return true;
}

bool find_random_placement(
    const Sheet& sheet,
    const PartInstance& instance,
    const std::vector<PlacedPart>& existing,
    int sheet_index,
    const NestingOptions& options,
    PlacedPart& output
) {
    std::vector<double> rotations;
    if (instance.part->can_rotate && instance.locked_rotation) {
        rotations = {instance.rotation};
    } else {
        rotations = instance.part->can_rotate ? options.rotations : std::vector<double>{0.0};
    }
    if (rotations.empty()) {
        return false;
    }

    const size_t name_hash = std::hash<std::string>{}(instance.part->name);
    const unsigned int seed =
        0x9E3779B9u ^
        static_cast<unsigned int>(options.placement_variant * 2654435761u) ^
        static_cast<unsigned int>((sheet_index + 1) * 2246822519u) ^
        static_cast<unsigned int>((existing.size() + 1) * 3266489917u) ^
        static_cast<unsigned int>(instance.instance_number * 668265263u) ^
        static_cast<unsigned int>(name_hash);
    std::mt19937 rng(seed);
    std::uniform_int_distribution<size_t> rotation_dist(0, rotations.size() - 1);

    const int attempts = std::clamp(
        1000 + static_cast<int>(existing.size()) * 180 + std::max(1, options.placement_pool) * 120,
        1000,
        12000
    );

    for (int attempt = 0; attempt < attempts; ++attempt) {
        const double angle = rotations[rotation_dist(rng)];
        const Polygon rotated = rotate_to_origin(instance.part->geometry, angle);
        const Bounds bounds = polygon_bounds(rotated);
        const double max_x = sheet.width - bounds.width();
        const double max_y = sheet.height - bounds.height();
        if (max_x < -kTolerance || max_y < -kTolerance) {
            continue;
        }

        std::uniform_real_distribution<double> x_dist(0.0, std::max(0.0, max_x));
        std::uniform_real_distribution<double> y_dist(0.0, std::max(0.0, max_y));
        const double x = x_dist(rng);
        const double y = y_dist(rng);
        Polygon geometry = translate_polygon(rotated, x, y);
        if (!fits_on_sheet(sheet, geometry)) {
            continue;
        }
        if (!clears_existing(geometry, existing, options.spacing)) {
            continue;
        }

        output = {
            instance.part->name,
            instance.part->source,
            std::move(geometry),
            sheet_index,
            instance.instance_number,
            x,
            y,
            angle,
            instance.part->area(),
        };
        return true;
    }
    return false;
}

// ---- GPU-accelerated placement ----
bool find_best_placement_gpu(
    const Sheet& sheet,
    const PartInstance& instance,
    const std::vector<PlacedPart>& existing,
    int sheet_index,
    const NestingOptions& options,
    PlacedPart& output,
    int gpu_device_index
) {
    auto& gpu = GpuContext::instance();
    if (!gpu.available() || gpu_device_index >= gpu.device_count()) {
        return find_best_placement(sheet, instance, existing, sheet_index, options, output);
    }

    // Build existing segments + bounds flat arrays
    std::vector<float> existing_segments;
    std::vector<float> existing_bounds_flat;
    std::vector<int> existing_seg_offsets;
    int seg_offset = 0;

    Bounds global_eb;
    bool has_existing = false;

    for (const auto& placement : existing) {
        const Bounds bounds = placement.bounds();
        existing_bounds_flat.push_back(static_cast<float>(bounds.min_x));
        existing_bounds_flat.push_back(static_cast<float>(bounds.min_y));
        existing_bounds_flat.push_back(static_cast<float>(bounds.max_x));
        existing_bounds_flat.push_back(static_cast<float>(bounds.max_y));

        if (!has_existing) {
            global_eb = bounds;
            has_existing = true;
        } else {
            global_eb.min_x = std::min(global_eb.min_x, bounds.min_x);
            global_eb.min_y = std::min(global_eb.min_y, bounds.min_y);
            global_eb.max_x = std::max(global_eb.max_x, bounds.max_x);
            global_eb.max_y = std::max(global_eb.max_y, bounds.max_y);
        }

        existing_seg_offsets.push_back(seg_offset);
        auto ring = ensure_closed(placement.geometry.outer);
        for (size_t i = 0; i + 1 < ring.size(); ++i) {
            existing_segments.push_back(static_cast<float>(ring[i].x));
            existing_segments.push_back(static_cast<float>(ring[i].y));
            existing_segments.push_back(static_cast<float>(ring[i + 1].x));
            existing_segments.push_back(static_cast<float>(ring[i + 1].y));
            ++seg_offset;
        }
    }
    existing_seg_offsets.push_back(seg_offset);

    float geb[4] = {
        static_cast<float>(global_eb.min_x), static_cast<float>(global_eb.min_y),
        static_cast<float>(global_eb.max_x), static_cast<float>(global_eb.max_y)
    };

    // Determine rotations
    std::vector<double> rotations;
    if (instance.part->can_rotate && instance.locked_rotation) {
        rotations = {instance.rotation};
    } else {
        rotations = instance.part->can_rotate ? options.rotations : std::vector<double>{0.0};
    }

    // Collect ALL (position, rotation) candidates across all rotations
    struct FullCandidate {
        double x, y, angle;
        Polygon rotated;
    };

    std::vector<FullCandidate> all_candidates_flat;
    PlacedPart best_output;
    bool found_any = false;
    PlacementScore best_score;

    std::vector<float> all_cand_segs;
    std::vector<int> cand_seg_offsets;
    std::vector<float> all_cand_bounds;
    std::vector<float> all_positions;
    std::vector<int> all_pos_rotations;
    int current_cand_seg_offset = 0;
    cand_seg_offsets.push_back(0);

    for (int r = 0; r < static_cast<int>(rotations.size()); ++r) {
        double angle = rotations[r];
        const Polygon rotated = rotate_to_origin(instance.part->geometry, angle);
        const auto positions = candidate_positions(existing, rotated, options.spacing, options.use_nfp_candidates);

        if (positions.empty()) {
            cand_seg_offsets.push_back(current_cand_seg_offset);
            all_cand_bounds.push_back(0.f); all_cand_bounds.push_back(0.f);
            all_cand_bounds.push_back(0.f); all_cand_bounds.push_back(0.f);
            continue;
        }

        // Build candidate segments
        auto cand_ring = ensure_closed(rotated.outer);
        for (size_t i = 0; i + 1 < cand_ring.size(); ++i) {
            all_cand_segs.push_back(static_cast<float>(cand_ring[i].x));
            all_cand_segs.push_back(static_cast<float>(cand_ring[i].y));
            all_cand_segs.push_back(static_cast<float>(cand_ring[i + 1].x));
            all_cand_segs.push_back(static_cast<float>(cand_ring[i + 1].y));
            current_cand_seg_offset++;
        }
        cand_seg_offsets.push_back(current_cand_seg_offset);

        Bounds cb = polygon_bounds(rotated);
        all_cand_bounds.push_back(static_cast<float>(cb.width()));
        all_cand_bounds.push_back(static_cast<float>(cb.height()));
        all_cand_bounds.push_back(static_cast<float>(cb.min_x));
        all_cand_bounds.push_back(static_cast<float>(cb.min_y));

        for (const auto& p : positions) {
            all_positions.push_back(static_cast<float>(p.x));
            all_positions.push_back(static_cast<float>(p.y));
            all_pos_rotations.push_back(r);
            all_candidates_flat.push_back({p.x, p.y, angle, rotated});
        }
    }

    if (all_positions.empty()) {
        return find_best_placement(sheet, instance, existing, sheet_index, options, output);
    }

    auto results = gpu.evaluate_positions_gpu(
        gpu_device_index,
        existing_segments.empty() ? nullptr : existing_segments.data(),
        seg_offset,
        existing_bounds_flat.empty() ? nullptr : existing_bounds_flat.data(),
        existing_seg_offsets.data(),
        static_cast<int>(existing.size()),
        all_cand_segs.data(),
        current_cand_seg_offset,
        cand_seg_offsets.data(),
        all_cand_bounds.data(),
        static_cast<int>(rotations.size()),
        all_positions.data(),
        all_pos_rotations.data(),
        static_cast<int>(all_pos_rotations.size()),
        static_cast<float>(sheet.width),
        static_cast<float>(sheet.height),
        static_cast<float>(options.spacing),
        geb,
        has_existing
    );

    if (results.size() != all_candidates_flat.size()) {
        return find_best_placement(sheet, instance, existing, sheet_index, options, output);
    }

    for (size_t i = 0; i < results.size(); ++i) {
        if (!results[i].valid) continue;

        Polygon geometry = translate_polygon(all_candidates_flat[i].rotated, results[i].pos_x, results[i].pos_y);

        // Final CPU validation for point-in-polygon, hole intersection and exact precision.
        // This filters out GPU false-positives (where candidate is entirely inside an existing piece).
        if (!fits_on_sheet(sheet, geometry)) {
            continue;
        }
        if (!clears_existing(geometry, existing, options.spacing)) {
            continue;
        }

        PlacementScore score;
        double ratio = std::clamp(options.optimization_ratio, 0.0, 1.0);
        double shape_bias = results[i].used_height * (1.0 - ratio) + results[i].used_width * ratio;
        score.bounding_area = results[i].bounding_area;
        score.used_height = results[i].used_height;
        score.used_width = results[i].used_width;
        score.y = results[i].pos_y;
        score.x = results[i].pos_x;
        
        if (options.optimization_type == OptimizationType::CompactArea) {
            score.primary = shape_bias;
            score.secondary = results[i].bounding_area;
        } else {
            score.primary = results[i].bounding_area;
            score.secondary = shape_bias;
        }

        if (!found_any || score < best_score) {
            best_score = score;
            best_output = {
                instance.part->name,
                instance.part->source,
                std::move(geometry),
                sheet_index,
                instance.instance_number,
                results[i].pos_x,
                results[i].pos_y,
                all_candidates_flat[i].angle,
                instance.part->area(),
            };
            found_any = true;
        }
    }

    if (!found_any) {
        return find_best_placement(sheet, instance, existing, sheet_index, options, output);
    }
    output = std::move(best_output);
    return true;
}

std::vector<Point> compact_y_candidates(const std::vector<PlacedPart>& existing, double spacing) {
    std::vector<Point> candidates = {{0.0, 0.0}};
    for (const auto& placement : existing) {
        const Bounds bounds = placement.bounds();
        candidates.push_back({0.0, bounds.max_y + spacing});
    }
    std::sort(candidates.begin(), candidates.end(), [](Point first, Point second) {
        return first.y < second.y;
    });
    return candidates;
}

std::vector<Point> compact_x_candidates(const std::vector<PlacedPart>& existing, double spacing) {
    std::vector<Point> candidates = {{0.0, 0.0}};
    for (const auto& placement : existing) {
        const Bounds bounds = placement.bounds();
        candidates.push_back({bounds.max_x + spacing, 0.0});
    }
    std::sort(candidates.begin(), candidates.end(), [](Point first, Point second) {
        return first.x < second.x;
    });
    return candidates;
}

void compact_result(NestingResult& result, const NestingOptions& options) {
    const int passes = result.placements.size() > 12 ? 1 : 3;
    for (int pass = 0; pass < passes; ++pass) {
        std::sort(result.placements.begin(), result.placements.end(), [](const PlacedPart& first, const PlacedPart& second) {
            const Bounds a = first.bounds();
            const Bounds b = second.bounds();
            if (std::abs(a.min_y - b.min_y) > kTolerance) {
                return a.min_y < b.min_y;
            }
            return a.min_x < b.min_x;
        });

        for (auto& placement : result.placements) {
            std::vector<PlacedPart> others;
            for (const auto& other : result.placements) {
                if (&other != &placement && other.sheet_index == placement.sheet_index) {
                    others.push_back(other);
                }
            }

            Bounds bounds = placement.bounds();
            for (Point candidate : compact_y_candidates(others, options.spacing)) {
                if (candidate.y >= bounds.min_y - kTolerance) {
                    continue;
                }
                Polygon moved = translate_polygon(placement.geometry, 0.0, candidate.y - bounds.min_y);
                if (fits_on_sheet(result.sheet, moved) && clears_existing(moved, others, options.spacing)) {
                    placement.geometry = std::move(moved);
                    placement.y += candidate.y - bounds.min_y;
                    bounds = placement.bounds();
                    break;
                }
            }

            bounds = placement.bounds();
            for (Point candidate : compact_x_candidates(others, options.spacing)) {
                if (candidate.x >= bounds.min_x - kTolerance) {
                    continue;
                }
                Polygon moved = translate_polygon(placement.geometry, candidate.x - bounds.min_x, 0.0);
                if (fits_on_sheet(result.sheet, moved) && clears_existing(moved, others, options.spacing)) {
                    placement.geometry = std::move(moved);
                    placement.x += candidate.x - bounds.min_x;
                    break;
                }
            }
        }
    }
}

NestingResult nest_instances(
    const Sheet& sheet,
    const std::vector<PartInstance>& instances,
    const NestingOptions& options,
    std::vector<std::vector<PlacedPart>> sheet_placements = {}
) {
    NestingResult result;
    result.sheet = sheet;
    if (sheet_placements.empty()) {
        sheet_placements.resize(1);
    }

    for (const auto& instance : instances) {
        bool placed = false;
        for (int sheet_index = 0; sheet_index < static_cast<int>(sheet_placements.size()); ++sheet_index) {
            PlacedPart candidate;
            bool success = false;
            bool use_gpu_for_this = solver_mode_uses_gpu(options.solver_mode);
            if (use_gpu_for_this) {
                size_t cand_verts = instance.part->geometry.outer.size();
                size_t existing_verts = 0;
                for (const auto& p : sheet_placements[static_cast<size_t>(sheet_index)]) {
                    existing_verts += p.geometry.outer.size();
                    for (const auto& hole : p.geometry.holes) existing_verts += hole.size();
                }
                
                // Heuristic: If candidate has very few vertices, or the sheet is mostly empty,
                // the CPU's point-in-polygon algorithm is much faster than the GPU's memory transfer overhead.
                // Threshold of 2500 segment-segment pairs roughly balances the PCIe latency.
                if (cand_verts * existing_verts < 2500) {
                    use_gpu_for_this = false;
                }
            }

            if (options.solver_mode == SolverMode::Random) {
                success = find_random_placement(sheet, instance, sheet_placements[static_cast<size_t>(sheet_index)], sheet_index, options, candidate);
            } else if (use_gpu_for_this) {
                int dev = (options.solver_mode == SolverMode::GPU_Dual && GpuContext::instance().device_count() > 1) 
                    ? (options.placement_variant % std::max(1, GpuContext::instance().device_count()))
                    : 0;
                success = find_best_placement_gpu(sheet, instance, sheet_placements[static_cast<size_t>(sheet_index)], sheet_index, options, candidate, dev);
            } else {
                success = find_best_placement(sheet, instance, sheet_placements[static_cast<size_t>(sheet_index)], sheet_index, options, candidate);
            }
            if (success) {
                sheet_placements[static_cast<size_t>(sheet_index)].push_back(std::move(candidate));
                placed = true;
                break;
            }
        }

        if (!placed && static_cast<int>(sheet_placements.size()) < options.max_sheets) {
            std::vector<PlacedPart> empty_existing;
            PlacedPart candidate;
            const int new_sheet_index = static_cast<int>(sheet_placements.size());
            bool success = false;
            bool use_gpu_for_new_sheet = solver_mode_uses_gpu(options.solver_mode);
            // Since the new sheet is empty, existing_verts is 0, so cand_verts * 0 is 0.
            // This means the hybrid heuristic will correctly ALWAYS use CPU for the first piece of an empty sheet,
            // which is extremely optimal because the first piece is just placed at the corner (0,0) instantly!
            if (use_gpu_for_new_sheet) {
                size_t cand_verts = instance.part->geometry.outer.size();
                size_t existing_verts = 0;
                if (cand_verts * existing_verts < 2500) {
                    use_gpu_for_new_sheet = false;
                }
            }

            if (options.solver_mode == SolverMode::Random) {
                success = find_random_placement(sheet, instance, empty_existing, new_sheet_index, options, candidate);
            } else if (use_gpu_for_new_sheet) {
                int dev = (options.solver_mode == SolverMode::GPU_Dual && GpuContext::instance().device_count() > 1) 
                    ? (options.placement_variant % std::max(1, GpuContext::instance().device_count()))
                    : 0;
                success = find_best_placement_gpu(sheet, instance, empty_existing, new_sheet_index, options, candidate, dev);
            } else {
                success = find_best_placement(sheet, instance, empty_existing, new_sheet_index, options, candidate);
            }
            if (success) {
                sheet_placements.push_back({std::move(candidate)});
                placed = true;
            }
        }

        if (!placed) {
            result.unplaced.push_back(*instance.part);
        }
    }

    for (auto& placements : sheet_placements) {
        for (auto& placement : placements) {
            result.placements.push_back(std::move(placement));
        }
    }

    if (options.use_compaction && options.solver_mode != SolverMode::Random) {
        compact_result(result, options);
    }
    return result;
}

void validate_options(const Sheet& sheet, const NestingOptions& options) {
    if (sheet.width <= 0.0 || sheet.height <= 0.0) {
        throw std::runtime_error("Sheet dimensions must be greater than zero.");
    }
    if (options.spacing < 0.0) {
        throw std::runtime_error("Spacing cannot be negative.");
    }
    if (options.rotations.empty()) {
        throw std::runtime_error("At least one rotation angle is required.");
    }
    if (options.max_sheets < 1) {
        throw std::runtime_error("max_sheets must be at least 1.");
    }
}

struct CandidateEvaluation {
    NestingResult result;
    std::vector<PartInstance> instances;
    double occupied_area = std::numeric_limits<double>::infinity();
    int unplaced_count = std::numeric_limits<int>::max();
    int sheet_count = std::numeric_limits<int>::max();
    double ratio = 1.0;
    bool was_shuffle = false;
};

void refresh_candidate_metrics(CandidateEvaluation& evaluation) {
    evaluation.occupied_area = occupied_layout_area(evaluation.result);
    evaluation.unplaced_count = static_cast<int>(evaluation.result.unplaced.size());
    evaluation.sheet_count = evaluation.result.sheet_count();
}

void compact_candidate_if_needed(CandidateEvaluation& evaluation, const NestingOptions& options) {
    if (options.use_compaction && options.solver_mode != SolverMode::Random) {
        compact_result(evaluation.result, options);
    }
    refresh_candidate_metrics(evaluation);
}

CandidateEvaluation evaluate_candidate(
    const Sheet& sheet,
    const std::vector<PartInstance>& base_instances,
    const IterativeNestingOptions& options,
    int generation,
    int member,
    double base_ratio
) {
    std::mt19937 rng(options.seed + static_cast<unsigned int>(generation * 1009 + member * 9176));
    const bool preserve_exact_order = member == 0 && (generation == 1 || options.ga_population > 1);
    bool was_shuffle = false;
    auto ordered = order_instances(
        base_instances,
        generation,
        rng,
        options.ga_mutation_rate,
        options.ga_random_shuffle_prob,
        options.ga_random_shuffle_intensity,
        options.base,
        preserve_exact_order,
        was_shuffle
    );

    NestingOptions candidate_options = options.base;
    candidate_options.use_compaction = false;
    if (generation > 1 && (generation + member) % 4 == 0) {
        std::shuffle(candidate_options.rotations.begin(), candidate_options.rotations.end(), rng);
    }
    if (generation > 1 || member > 0) {
        const double mutation = std::clamp(options.ga_mutation_rate, 0.0, 100.0);
        candidate_options.placement_pool = std::clamp(2 + static_cast<int>(std::round(mutation / 12.5)), 2, 12);
        candidate_options.placement_variant = generation * 37 + member * 101;
    }

    double max_part_width = 0.0;
    for (const auto& instance : base_instances) {
        max_part_width = std::max(max_part_width, instance.part->bounds().width());
        if (instance.part->can_rotate) {
            max_part_width = std::max(max_part_width, instance.part->bounds().height());
        }
    }

    Sheet candidate_sheet = sheet;
    double ratio = base_ratio;
    
    if (options.base.optimization_type == smn::OptimizationType::CompactArea) {
        if (member == 0) {
            // Elitist member: preserve the winning ratio EXACTLY
            ratio = base_ratio;
        } else {
            // Other members: explore new ratios
            const double min_ratio_from_parts = std::min(1.0, (max_part_width + options.base.spacing * 2.0) / sheet.width);
            const double ratio_floor = std::clamp(std::max(0.28, min_ratio_from_parts), 0.1, 1.0);
            const double ratio_ceiling = 1.0;
            std::uniform_real_distribution<double> width_ratio(ratio_floor, ratio_ceiling);
            ratio = width_ratio(rng);

            if (member % 5 == 1) {
                ratio = ratio_floor;
            } else if (member % 5 == 2) {
                ratio = (ratio_floor + ratio_ceiling) * 0.5;
            } else if (member % 5 == 3) {
                ratio = 0.72;
            } else if (member % 5 == 4) {
                ratio = 0.55;
            }
            ratio = std::clamp(ratio, ratio_floor, ratio_ceiling);
        }
        candidate_sheet.width = sheet.width * ratio;
    }

    CandidateEvaluation evaluation;
    evaluation.was_shuffle = was_shuffle;
    evaluation.ratio = ratio;
    evaluation.instances = ordered;
    evaluation.result = nest_instances(candidate_sheet, ordered, candidate_options);
    evaluation.result.sheet = sheet;
    refresh_candidate_metrics(evaluation);
    return evaluation;
}

bool better_candidate(
    int unplaced_count,
    int sheet_count,
    double occupied_area,
    int best_unplaced_count,
    int best_sheet_count,
    double best_area
) {
    return unplaced_count < best_unplaced_count ||
        (unplaced_count == best_unplaced_count && sheet_count < best_sheet_count) ||
        (unplaced_count == best_unplaced_count && sheet_count == best_sheet_count && occupied_area < best_area);
}

}  // namespace

NestingResult nest_parts(const Sheet& sheet, const std::vector<Part>& parts, const NestingOptions& options) {
    validate_options(sheet, options);

    NestingResult repeated_result;
    if (try_repeated_part_pattern(sheet, parts, options, repeated_result)) {
        return repeated_result;
    }

    NestingResult seeded_result;
    std::vector<Part> remaining_parts;
    std::vector<std::vector<PlacedPart>> seeded_sheet_placements;
    const bool has_repeated_seed = build_repeated_seeded_layout(sheet, parts, options, seeded_result, remaining_parts);
    if (has_repeated_seed) {
        if (remaining_parts.empty()) {
            return seeded_result;
        }
        seeded_sheet_placements = extract_sheet_placements(std::move(seeded_result));
    }

    auto instances = expand_instances(has_repeated_seed ? remaining_parts : parts);
    if (options.solver_mode == SolverMode::Random) {
        std::mt19937 rng(
            0xA341316Cu ^
            static_cast<unsigned int>(options.placement_variant * 2654435761u) ^
            static_cast<unsigned int>(instances.size() * 668265263u)
        );
        std::shuffle(instances.begin(), instances.end(), rng);
    } else {
        std::sort(instances.begin(), instances.end(), [](const PartInstance& first, const PartInstance& second) {
            return first.part->area() > second.part->area();
        });
    }

    NestingOptions final_options = options;
    if (has_repeated_seed) {
        final_options.use_compaction = false;
        final_options.max_sheets = std::max(
            final_options.max_sheets,
            static_cast<int>(seeded_sheet_placements.size()) + std::max(1, options.max_sheets)
        );
    }
    return nest_instances(sheet, instances, final_options, std::move(seeded_sheet_placements));
}

double occupied_layout_area(const NestingResult& result) {
    double total = 0.0;
    for (int sheet_index = 0; sheet_index < result.sheet_count(); ++sheet_index) {
        Bounds bounds;
        bool has_part = false;
        for (const auto& placement : result.placements) {
            if (placement.sheet_index != sheet_index) {
                continue;
            }
            const Bounds part_bounds = placement.bounds();
            if (!has_part) {
                bounds = part_bounds;
                has_part = true;
            } else {
                bounds.min_x = std::min(bounds.min_x, part_bounds.min_x);
                bounds.min_y = std::min(bounds.min_y, part_bounds.min_y);
                bounds.max_x = std::max(bounds.max_x, part_bounds.max_x);
                bounds.max_y = std::max(bounds.max_y, part_bounds.max_y);
            }
        }
        if (has_part) {
            total += bounds.width() * bounds.height();
        }
    }
    return total;
}

std::vector<NestingViolation> find_nesting_violations(const NestingResult& result, double spacing) {
    std::vector<NestingViolation> violations;
    for (size_t first = 0; first < result.placements.size(); ++first) {
        const auto& first_part = result.placements[first];
        const Bounds first_bounds = first_part.bounds();
        for (size_t second = first + 1; second < result.placements.size(); ++second) {
            const auto& second_part = result.placements[second];
            if (first_part.sheet_index != second_part.sheet_index) {
                continue;
            }
            const Bounds second_bounds = second_part.bounds();
            if (!first_bounds.overlaps(second_bounds, spacing)) {
                continue;
            }

            const bool intersects = polygon_intersects(first_part.geometry, second_part.geometry);
            const double distance = intersects ? 0.0 : polygon_distance(first_part.geometry, second_part.geometry);
            if (intersects || distance < spacing - kTolerance) {
                violations.push_back({first, second, distance, intersects});
            }
        }
    }
    return violations;
}

IterativeNestingResult nest_parts_iterative(
    const Sheet& sheet,
    const std::vector<Part>& parts,
    const IterativeNestingOptions& options,
    const IterationCallback& callback
) {
    validate_options(sheet, options.base);
    if (options.iterations < 1) {
        throw std::runtime_error("Iterations must be at least 1.");
    }

    IterativeNestingResult output;
    if (try_repeated_part_pattern(sheet, parts, options.base, output.best)) {
        IterationStats stats;
        stats.iteration = 1;
        stats.occupied_area = occupied_layout_area(output.best);
        stats.best_occupied_area = stats.occupied_area;
        stats.saved_area = 0.0;
        stats.utilization = output.best.utilization();
        stats.time_to_find = 0.0;
        output.history.push_back(stats);
        if (callback) {
            callback(stats, output.best, output.best);
        }
        return output;
    }

    NestingResult seeded_result;
    std::vector<Part> remaining_parts;
    if (build_repeated_seeded_layout(sheet, parts, options.base, seeded_result, remaining_parts)) {
        if (remaining_parts.empty()) {
            output.best = std::move(seeded_result);
        } else {
            auto seeded_sheet_placements = extract_sheet_placements(std::move(seeded_result));
            auto instances = expand_instances(remaining_parts);
            std::sort(instances.begin(), instances.end(), [](const PartInstance& first, const PartInstance& second) {
                return first.part->area() > second.part->area();
            });
            NestingOptions seeded_options = options.base;
            seeded_options.use_compaction = false;
            seeded_options.max_sheets = std::max(
                seeded_options.max_sheets,
                static_cast<int>(seeded_sheet_placements.size()) + std::max(1, options.base.max_sheets)
            );
            output.best = nest_instances(sheet, instances, seeded_options, std::move(seeded_sheet_placements));
        }

        IterationStats stats;
        stats.iteration = 1;
        stats.occupied_area = occupied_layout_area(output.best);
        stats.best_occupied_area = stats.occupied_area;
        stats.saved_area = 0.0;
        stats.utilization = output.best.utilization();
        stats.time_to_find = 0.0;
        output.history.push_back(stats);
        if (callback) {
            callback(stats, output.best, output.best);
        }
        if (options.perf_log && options.perf_log->enabled()) {
            PerfEntry entry;
            entry.iteration = 1;
            entry.solver = options.base.solver_mode;
            entry.parts_placed = static_cast<int>(output.best.placements.size());
            if (solver_mode_uses_gpu(options.base.solver_mode) && GpuContext::instance().available()) {
                entry.gpu_name = GpuContext::instance().device(0).name;
            }
            options.perf_log->add(entry);
            options.perf_log->flush();
        }
        return output;
    }

    std::vector<PartInstance> current_base_instances = expand_instances(parts);
    double best_ratio = 1.0;

    int best_unplaced_count = std::numeric_limits<int>::max();
    int best_sheet_count = std::numeric_limits<int>::max();
    double best_area = std::numeric_limits<double>::infinity();
    double baseline_area = 0.0;
    
    CandidateEvaluation global_best_eval;
    global_best_eval.unplaced_count = std::numeric_limits<int>::max();
    
    auto last_success_time = std::chrono::steady_clock::now();
    double current_time_to_find = 0.0;

    const int cpu_cores = std::max(1, options.cpu_cores);
    const int population = std::max(1, options.ga_population);

    for (int iteration = 1; iteration <= options.iterations; ++iteration) {
        CandidateEvaluation generation_best;
        generation_best.unplaced_count = std::numeric_limits<int>::max();
        const int batch_size = population;
        std::vector<MutationInfo> current_discarded_mutations;

        int evaluated = 0;
        while (evaluated < batch_size) {
            std::vector<std::future<CandidateEvaluation>> futures;
            const int chunk = std::min(cpu_cores, batch_size - evaluated);
            futures.reserve(static_cast<size_t>(chunk));
            
            for (int batch_index = 0; batch_index < chunk; ++batch_index) {
                const int member = evaluated + batch_index;
#ifdef __EMSCRIPTEN__
                futures.push_back(std::async(
                    std::launch::deferred, 
                    [&, member]() {
                        return evaluate_candidate(sheet, current_base_instances, options, iteration, member, best_ratio);
                    }
                ));
#else
                futures.push_back(std::async(
                    std::launch::async, 
                    [&, member]() {
                        return evaluate_candidate(sheet, current_base_instances, options, iteration, member, best_ratio);
                    }
                ));
#endif
            }

            for (auto& future : futures) {
                CandidateEvaluation candidate = future.get();
                current_discarded_mutations.push_back({candidate.occupied_area, candidate.was_shuffle});
                if (generation_best.unplaced_count == std::numeric_limits<int>::max()) {
                    generation_best = std::move(candidate);
                } else {
                    if (better_candidate(
                        candidate.unplaced_count,
                        candidate.sheet_count,
                        candidate.occupied_area,
                        generation_best.unplaced_count,
                        generation_best.sheet_count,
                        generation_best.occupied_area
                    )) {
                        generation_best = std::move(candidate);
                    }
                }
            }
            evaluated += chunk;
        }

        if (generation_best.unplaced_count == std::numeric_limits<int>::max()) {
            continue;
        }

        const bool is_new_best = better_candidate(
            generation_best.unplaced_count,
            generation_best.sheet_count,
            generation_best.occupied_area,
            best_unplaced_count,
            best_sheet_count,
            best_area
        );

        if (is_new_best) {
            global_best_eval = generation_best;
            best_unplaced_count = generation_best.unplaced_count;
            best_sheet_count = generation_best.sheet_count;
            best_area = generation_best.occupied_area;
            output.best = generation_best.result;
            // Evolve from this success! Update the genetic base for the next iteration.
            current_base_instances = generation_best.instances;
            best_ratio = generation_best.ratio;
            
            auto now = std::chrono::steady_clock::now();
            current_time_to_find = std::chrono::duration<double>(now - last_success_time).count();
            last_success_time = now;
        }

        if (global_best_eval.unplaced_count == std::numeric_limits<int>::max()) {
            global_best_eval = generation_best;
            best_unplaced_count = generation_best.unplaced_count;
            best_sheet_count = generation_best.sheet_count;
            best_area = generation_best.occupied_area;
            output.best = generation_best.result;
            current_base_instances = generation_best.instances;
            best_ratio = generation_best.ratio;
        }

        if (iteration == 1) {
            baseline_area = generation_best.occupied_area;
        }

        IterationStats stats;
        stats.iteration = iteration;
        stats.occupied_area = generation_best.occupied_area;
        stats.best_occupied_area = best_area;
        stats.saved_area = std::max(0.0, baseline_area - best_area);
        stats.utilization = output.best.utilization();
        stats.time_to_find = current_time_to_find;
        
        // Remove the winner from the discarded list so it's not shown twice
        auto it = std::find_if(current_discarded_mutations.begin(), current_discarded_mutations.end(), 
            [&](const MutationInfo& info) { return info.area == generation_best.occupied_area; });
        if (it != current_discarded_mutations.end()) {
            current_discarded_mutations.erase(it);
        }
        
        stats.discarded_mutations = std::move(current_discarded_mutations);
        output.history.push_back(stats);
        if (callback && !callback(stats, generation_best.result, output.best)) {
            break;
        }

        // Performance logging
        if (options.perf_log && options.perf_log->enabled()) {
            PerfEntry entry;
            entry.iteration = iteration;
            entry.solver = options.base.solver_mode;
            entry.parts_placed = static_cast<int>(output.best.placements.size());
            if (solver_mode_uses_gpu(options.base.solver_mode) && GpuContext::instance().available()) {
                entry.gpu_name = GpuContext::instance().device(0).name;
            }
            options.perf_log->add(entry);
        }
    }

    if (options.perf_log) {
        options.perf_log->flush();
    }

    return output;
}

}  // namespace smn
