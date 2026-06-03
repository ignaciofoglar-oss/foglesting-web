#include "core/dxf.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace smn {
namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kCommonLineTolerance = 0.01;

struct Group {
    int code = 0;
    std::string value;
};

struct RawPolygon {
    std::vector<Point> ring;
    double area = 0.0;
};

struct DxfVertex {
    Point point;
    double bulge = 0.0;
};

struct DxfBlockDefinition {
    std::string name;
    Point base;
    std::vector<std::vector<Group>> entities;
};

struct DxfDocument {
    std::vector<std::vector<Group>> entities;
    std::unordered_map<std::string, DxfBlockDefinition> blocks;
};

struct DxfTransform {
    double a = 1.0;
    double b = 0.0;
    double c = 0.0;
    double d = 1.0;
    double tx = 0.0;
    double ty = 0.0;
};

struct CutSegment {
    Point a;
    Point b;
    std::string layer;
};

struct SegmentInterval {
    double start = 0.0;
    double end = 0.0;
    std::string layer;
};

struct CommonLineGroup {
    Point direction;
    double offset = 0.0;
    std::vector<SegmentInterval> intervals;
};

double triangle_cross(Point a, Point b, Point c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

bool point_on_line(Point a, Point b, Point p, double tolerance) {
    if (std::abs(triangle_cross(a, b, p)) > tolerance) {
        return false;
    }
    return p.x >= std::min(a.x, b.x) - tolerance &&
           p.x <= std::max(a.x, b.x) + tolerance &&
           p.y >= std::min(a.y, b.y) - tolerance &&
           p.y <= std::max(a.y, b.y) + tolerance;
}

std::vector<Point> simplify_straight_vertices(std::vector<Point> ring) {
    ring = ensure_closed(std::move(ring));
    if (ring.size() <= 4) {
        return ring;
    }

    constexpr double tolerance = 1e-7;
    bool changed = true;
    while (changed && ring.size() > 4) {
        changed = false;
        for (size_t index = 1; index + 1 < ring.size(); ++index) {
            const Point previous = ring[index - 1];
            const Point current = ring[index];
            const Point next = ring[index + 1];
            if (distance(previous, current) <= tolerance ||
                point_on_line(previous, next, current, tolerance)) {
                ring.erase(ring.begin() + static_cast<std::ptrdiff_t>(index));
                ring.back() = ring.front();
                changed = true;
                break;
            }
        }
    }
    return ring;
}

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    std::string result = value.substr(first, last - first + 1);
    if (result.size() >= 3 &&
        static_cast<unsigned char>(result[0]) == 0xEF &&
        static_cast<unsigned char>(result[1]) == 0xBB &&
        static_cast<unsigned char>(result[2]) == 0xBF) {
        result.erase(0, 3);
    }
    return result;
}

double to_double(const std::string& value) {
    std::string normalized = trim(value);
    std::replace(normalized.begin(), normalized.end(), ',', '.');
    return std::stod(normalized);
}

int to_int(const std::string& value) {
    return std::stoi(trim(value));
}

std::string lower_string(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string upper_string(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return value;
}

std::string entity_type(const std::vector<Group>& entity) {
    if (entity.empty() || entity.front().code != 0) {
        return "";
    }
    return upper_string(trim(entity.front().value));
}

bool is_dxf_path(const std::filesystem::path& path) {
    return lower_string(path.extension().string()) == ".dxf";
}

long long quantize(double value, double tolerance) {
    return static_cast<long long>(std::llround(value / tolerance));
}

std::vector<Group> read_groups(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Cannot open DXF: " + path.string());
    }

    std::vector<Group> groups;
    std::string code_line;
    std::string value_line;
    while (std::getline(input, code_line)) {
        if (!std::getline(input, value_line)) {
            break;
        }
        try {
            groups.push_back({to_int(code_line), trim(value_line)});
        } catch (...) {
            continue;
        }
    }
    return groups;
}

std::vector<std::vector<Group>> entity_groups(const std::vector<Group>& groups) {
    std::vector<std::vector<Group>> entities;
    for (size_t i = 0; i < groups.size();) {
        if (groups[i].code != 0) {
            ++i;
            continue;
        }

        std::vector<Group> entity;
        entity.push_back(groups[i++]);
        while (i < groups.size() && groups[i].code != 0) {
            entity.push_back(groups[i++]);
        }
        entities.push_back(std::move(entity));
    }
    return entities;
}

DxfBlockDefinition parse_block_header(const std::vector<Group>& entity) {
    DxfBlockDefinition block;
    bool has_base_x = false;
    bool has_base_y = false;
    for (const auto& group : entity) {
        if (group.code == 2) {
            block.name = upper_string(trim(group.value));
        } else if (group.code == 10) {
            block.base.x = to_double(group.value);
            has_base_x = true;
        } else if (group.code == 20) {
            block.base.y = to_double(group.value);
            has_base_y = true;
        }
    }
    if (!has_base_x) {
        block.base.x = 0.0;
    }
    if (!has_base_y) {
        block.base.y = 0.0;
    }
    return block;
}

void parse_blocks_section(
    const std::vector<Group>& groups,
    std::unordered_map<std::string, DxfBlockDefinition>& blocks
) {
    DxfBlockDefinition current;
    bool in_block = false;
    for (const auto& entity : entity_groups(groups)) {
        const std::string type = entity_type(entity);
        if (type == "BLOCK") {
            if (in_block && !current.name.empty()) {
                blocks[current.name] = current;
            }
            current = parse_block_header(entity);
            in_block = true;
            continue;
        }
        if (type == "ENDBLK") {
            if (in_block && !current.name.empty()) {
                blocks[current.name] = current;
            }
            current = {};
            in_block = false;
            continue;
        }
        if (in_block && !type.empty()) {
            current.entities.push_back(entity);
        }
    }
    if (in_block && !current.name.empty()) {
        blocks[current.name] = current;
    }
}

DxfDocument parse_dxf_document(const std::vector<Group>& groups) {
    DxfDocument document;
    bool found_entities_section = false;

    for (size_t index = 0; index < groups.size();) {
        if (groups[index].code != 0 || upper_string(trim(groups[index].value)) != "SECTION") {
            ++index;
            continue;
        }
        ++index;

        std::string section_name;
        if (index < groups.size() && groups[index].code == 2) {
            section_name = upper_string(trim(groups[index].value));
            ++index;
        }

        std::vector<Group> section_groups;
        while (index < groups.size()) {
            if (groups[index].code == 0 && upper_string(trim(groups[index].value)) == "ENDSEC") {
                ++index;
                break;
            }
            section_groups.push_back(groups[index++]);
        }

        if (section_name == "ENTITIES") {
            found_entities_section = true;
            auto entities = entity_groups(section_groups);
            document.entities.insert(document.entities.end(), entities.begin(), entities.end());
        } else if (section_name == "BLOCKS") {
            parse_blocks_section(section_groups, document.blocks);
        }
    }

    if (!found_entities_section) {
        document.entities = entity_groups(groups);
    }

    return document;
}

std::vector<Point> circle_points(Point center, double radius, double flattening_distance) {
    const int segments = std::clamp(
        static_cast<int>(std::ceil((2.0 * kPi * radius) / std::max(0.1, flattening_distance))),
        24,
        256
    );
    std::vector<Point> points;
    points.reserve(static_cast<size_t>(segments) + 1);
    for (int i = 0; i < segments; ++i) {
        const double angle = 2.0 * kPi * static_cast<double>(i) / static_cast<double>(segments);
        points.push_back({center.x + radius * std::cos(angle), center.y + radius * std::sin(angle)});
    }
    points.push_back(points.front());
    return points;
}

std::vector<Point> arc_points(
    Point center,
    double radius,
    double start_degrees,
    double end_degrees,
    double flattening_distance
) {
    while (end_degrees < start_degrees) {
        end_degrees += 360.0;
    }
    const double sweep = end_degrees - start_degrees;
    const double length = 2.0 * kPi * radius * sweep / 360.0;
    const int segments = std::clamp(
        static_cast<int>(std::ceil(length / std::max(0.1, flattening_distance))),
        4,
        128
    );

    std::vector<Point> points;
    points.reserve(static_cast<size_t>(segments) + 1);
    for (int i = 0; i <= segments; ++i) {
        const double degrees = start_degrees + sweep * static_cast<double>(i) / static_cast<double>(segments);
        const double radians = degrees * kPi / 180.0;
        points.push_back({center.x + radius * std::cos(radians), center.y + radius * std::sin(radians)});
    }
    return points;
}

void append_bulge_span(
    std::vector<Point>& output,
    Point start,
    Point end,
    double bulge,
    double flattening_distance
) {
    if (output.empty() || distance(output.back(), start) > 1e-7) {
        output.push_back(start);
    }

    const double chord = distance(start, end);
    if (chord <= 1e-9 || std::abs(bulge) <= 1e-12) {
        if (distance(output.back(), end) > 1e-7) {
            output.push_back(end);
        }
        return;
    }

    const double sweep = 4.0 * std::atan(bulge);
    const double abs_sweep = std::abs(sweep);
    const double radius = chord / (2.0 * std::sin(abs_sweep * 0.5));
    if (!std::isfinite(radius) || radius <= 0.0) {
        output.push_back(end);
        return;
    }

    const Point mid{(start.x + end.x) * 0.5, (start.y + end.y) * 0.5};
    const Point direction{(end.x - start.x) / chord, (end.y - start.y) / chord};
    const Point left{-direction.y, direction.x};
    const double center_offset = radius * std::cos(abs_sweep * 0.5);
    const double sign = bulge >= 0.0 ? 1.0 : -1.0;
    const Point center{
        mid.x + left.x * center_offset * sign,
        mid.y + left.y * center_offset * sign,
    };

    double start_angle = std::atan2(start.y - center.y, start.x - center.x);
    double end_angle = std::atan2(end.y - center.y, end.x - center.x);
    if (sweep > 0.0) {
        while (end_angle <= start_angle) {
            end_angle += 2.0 * kPi;
        }
    } else {
        while (end_angle >= start_angle) {
            end_angle -= 2.0 * kPi;
        }
    }

    const double actual_sweep = end_angle - start_angle;
    const double arc_length = std::abs(actual_sweep) * radius;
    const int segments = std::clamp(
        static_cast<int>(std::ceil(arc_length / std::max(0.1, flattening_distance))),
        4,
        256
    );
    for (int segment = 1; segment <= segments; ++segment) {
        const double t = static_cast<double>(segment) / static_cast<double>(segments);
        const double angle = start_angle + actual_sweep * t;
        const Point point{center.x + radius * std::cos(angle), center.y + radius * std::sin(angle)};
        if (distance(output.back(), point) > 1e-7) {
            output.push_back(point);
        }
    }
}

std::vector<Point> polyline_vertices_to_points(
    const std::vector<DxfVertex>& vertices,
    bool closed,
    double flattening_distance
) {
    std::vector<Point> points;
    if (vertices.empty()) {
        return points;
    }

    points.push_back(vertices.front().point);
    for (size_t index = 0; index + 1 < vertices.size(); ++index) {
        append_bulge_span(
            points,
            vertices[index].point,
            vertices[index + 1].point,
            vertices[index].bulge,
            flattening_distance
        );
    }
    if (closed && vertices.size() > 2) {
        append_bulge_span(points, vertices.back().point, vertices.front().point, vertices.back().bulge, flattening_distance);
        if (distance(points.back(), points.front()) > 1e-7) {
            points.push_back(points.front());
        } else {
            points.back() = points.front();
        }
    }
    return points;
}

bool parse_line_segment(const std::vector<Group>& entity, std::vector<Point>& segment) {
    Point start;
    Point end;
    bool has_start_x = false;
    bool has_start_y = false;
    bool has_end_x = false;
    bool has_end_y = false;
    for (const auto& group : entity) {
        if (group.code == 10) {
            start.x = to_double(group.value);
            has_start_x = true;
        } else if (group.code == 20) {
            start.y = to_double(group.value);
            has_start_y = true;
        } else if (group.code == 11) {
            end.x = to_double(group.value);
            has_end_x = true;
        } else if (group.code == 21) {
            end.y = to_double(group.value);
            has_end_y = true;
        }
    }
    if (!(has_start_x && has_start_y && has_end_x && has_end_y)) {
        return false;
    }
    segment = {start, end};
    return true;
}

bool parse_arc_segment(const std::vector<Group>& entity, double flattening_distance, std::vector<Point>& segment) {
    Point center;
    double radius = 0.0;
    double start = 0.0;
    double end = 0.0;
    bool has_x = false;
    bool has_y = false;
    bool has_radius = false;
    bool has_start = false;
    bool has_end = false;

    for (const auto& group : entity) {
        if (group.code == 10) {
            center.x = to_double(group.value);
            has_x = true;
        } else if (group.code == 20) {
            center.y = to_double(group.value);
            has_y = true;
        } else if (group.code == 40) {
            radius = to_double(group.value);
            has_radius = true;
        } else if (group.code == 50) {
            start = to_double(group.value);
            has_start = true;
        } else if (group.code == 51) {
            end = to_double(group.value);
            has_end = true;
        }
    }

    if (!(has_x && has_y && has_radius && has_start && has_end) || radius <= 0.0) {
        return false;
    }
    segment = arc_points(center, radius, start, end, flattening_distance);
    return true;
}

Point linear_interpolate(Point a, Point b, double t) {
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
    };
}

Point evaluate_nonrational_bspline(
    const std::vector<Point>& control_points,
    const std::vector<double>& knots,
    int degree,
    double u
) {
    const int control_count = static_cast<int>(control_points.size());
    if (control_count == 0) {
        return {};
    }
    if (control_count == 1 || degree <= 0) {
        return control_points.front();
    }

    const int n = control_count - 1;
    degree = std::clamp(degree, 1, n);
    const double last_knot = knots[static_cast<size_t>(n + 1)];
    if (u >= last_knot) {
        return control_points.back();
    }

    int span = degree;
    for (int index = degree; index <= n; ++index) {
        if (u >= knots[static_cast<size_t>(index)] && u < knots[static_cast<size_t>(index + 1)]) {
            span = index;
            break;
        }
    }

    std::vector<Point> d;
    d.reserve(static_cast<size_t>(degree) + 1);
    for (int j = 0; j <= degree; ++j) {
        d.push_back(control_points[static_cast<size_t>(span - degree + j)]);
    }

    for (int r = 1; r <= degree; ++r) {
        for (int j = degree; j >= r; --j) {
            const int knot_index = span - degree + j;
            const double denominator =
                knots[static_cast<size_t>(knot_index + degree - r + 1)] -
                knots[static_cast<size_t>(knot_index)];
            const double alpha = std::abs(denominator) <= 1e-12
                ? 0.0
                : (u - knots[static_cast<size_t>(knot_index)]) / denominator;
            d[static_cast<size_t>(j)] = linear_interpolate(
                d[static_cast<size_t>(j - 1)],
                d[static_cast<size_t>(j)],
                std::clamp(alpha, 0.0, 1.0)
            );
        }
    }

    return d[static_cast<size_t>(degree)];
}

std::vector<double> make_open_uniform_knots(int control_count, int degree) {
    const int knot_count = control_count + degree + 1;
    std::vector<double> knots(static_cast<size_t>(knot_count), 0.0);
    const int inner_count = control_count - degree - 1;
    for (int index = 0; index < knot_count; ++index) {
        if (index <= degree) {
            knots[static_cast<size_t>(index)] = 0.0;
        } else if (index >= control_count) {
            knots[static_cast<size_t>(index)] = 1.0;
        } else {
            knots[static_cast<size_t>(index)] =
                static_cast<double>(index - degree) / static_cast<double>(std::max(1, inner_count + 1));
        }
    }
    return knots;
}

bool parse_spline_segment(const std::vector<Group>& entity, double flattening_distance, std::vector<Point>& segment) {
    int degree = 3;
    std::vector<double> knots;
    std::vector<Point> control_points;
    Point current{};
    bool has_current_x = false;

    for (const auto& group : entity) {
        if (group.code == 71) {
            degree = std::max(1, to_int(group.value));
        } else if (group.code == 40) {
            knots.push_back(to_double(group.value));
        } else if (group.code == 10) {
            if (has_current_x) {
                control_points.push_back(current);
            }
            current = {};
            current.x = to_double(group.value);
            has_current_x = true;
        } else if (group.code == 20 && has_current_x) {
            current.y = to_double(group.value);
        }
    }
    if (has_current_x) {
        control_points.push_back(current);
    }

    if (control_points.size() < 2) {
        return false;
    }
    degree = std::clamp(degree, 1, static_cast<int>(control_points.size()) - 1);
    if (knots.size() < control_points.size() + static_cast<size_t>(degree) + 1) {
        knots = make_open_uniform_knots(static_cast<int>(control_points.size()), degree);
    }

    double chord_length = 0.0;
    for (size_t index = 1; index < control_points.size(); ++index) {
        chord_length += distance(control_points[index - 1], control_points[index]);
    }
    const int samples = std::clamp(
        static_cast<int>(std::ceil(chord_length / std::max(0.35, flattening_distance * 0.6))),
        32,
        900
    );

    const int n = static_cast<int>(control_points.size()) - 1;
    double start_u = knots[static_cast<size_t>(degree)];
    double end_u = knots[static_cast<size_t>(n + 1)];
    if (end_u <= start_u) {
        start_u = knots.front();
        end_u = knots.back();
    }
    if (end_u <= start_u) {
        return false;
    }

    segment.clear();
    segment.reserve(static_cast<size_t>(samples) + 1);
    for (int sample = 0; sample <= samples; ++sample) {
        const double t = static_cast<double>(sample) / static_cast<double>(samples);
        const double u = start_u + (end_u - start_u) * t;
        const Point point = evaluate_nonrational_bspline(control_points, knots, degree, u);
        if (segment.empty() || distance(segment.back(), point) > 1e-5) {
            segment.push_back(point);
        }
    }

    return segment.size() >= 2;
}

bool parse_circle_polygon(const std::vector<Group>& entity, double flattening_distance, RawPolygon& polygon) {
    Point center;
    double radius = 0.0;
    bool has_x = false;
    bool has_y = false;
    bool has_radius = false;

    for (const auto& group : entity) {
        if (group.code == 10) {
            center.x = to_double(group.value);
            has_x = true;
        } else if (group.code == 20) {
            center.y = to_double(group.value);
            has_y = true;
        } else if (group.code == 40) {
            radius = to_double(group.value);
            has_radius = true;
        }
    }

    if (!(has_x && has_y && has_radius) || radius <= 0.0) {
        return false;
    }
    polygon.ring = circle_points(center, radius, flattening_distance);
    polygon.area = ring_area(polygon.ring);
    return true;
}

bool parse_ellipse_path(
    const std::vector<Group>& entity,
    double flattening_distance,
    std::vector<Point>& points,
    bool& closed
) {
    Point center;
    Point major_axis;
    double ratio = 1.0;
    double start = 0.0;
    double end = 2.0 * kPi;
    bool has_center_x = false;
    bool has_center_y = false;
    bool has_major_x = false;
    bool has_major_y = false;
    bool has_start = false;
    bool has_end = false;

    for (const auto& group : entity) {
        if (group.code == 10) {
            center.x = to_double(group.value);
            has_center_x = true;
        } else if (group.code == 20) {
            center.y = to_double(group.value);
            has_center_y = true;
        } else if (group.code == 11) {
            major_axis.x = to_double(group.value);
            has_major_x = true;
        } else if (group.code == 21) {
            major_axis.y = to_double(group.value);
            has_major_y = true;
        } else if (group.code == 40) {
            ratio = std::abs(to_double(group.value));
        } else if (group.code == 41) {
            start = to_double(group.value);
            has_start = true;
        } else if (group.code == 42) {
            end = to_double(group.value);
            has_end = true;
        }
    }

    const double major_radius = std::hypot(major_axis.x, major_axis.y);
    if (!(has_center_x && has_center_y && has_major_x && has_major_y) ||
        major_radius <= 0.0 || ratio <= 0.0) {
        return false;
    }

    if (!has_start && !has_end) {
        start = 0.0;
        end = 2.0 * kPi;
    }
    while (end < start) {
        end += 2.0 * kPi;
    }
    double sweep = end - start;
    closed = sweep >= 2.0 * kPi - 1e-5;
    if (closed) {
        sweep = 2.0 * kPi;
        end = start + sweep;
    }

    const Point minor_axis{-major_axis.y * ratio, major_axis.x * ratio};
    const double minor_radius = major_radius * ratio;
    const double approx_length = std::max(major_radius, minor_radius) * sweep;
    const int samples = std::clamp(
        static_cast<int>(std::ceil(approx_length / std::max(0.1, flattening_distance))),
        closed ? 24 : 4,
        512
    );

    points.clear();
    points.reserve(static_cast<size_t>(samples) + 1);
    for (int sample = 0; sample <= samples; ++sample) {
        const double t = start + sweep * static_cast<double>(sample) / static_cast<double>(samples);
        points.push_back({
            center.x + major_axis.x * std::cos(t) + minor_axis.x * std::sin(t),
            center.y + major_axis.y * std::cos(t) + minor_axis.y * std::sin(t),
        });
    }
    if (closed) {
        points.back() = points.front();
    }
    return points.size() >= (closed ? 4 : 2);
}

bool parse_lwpolyline_path(
    const std::vector<Group>& entity,
    double flattening_distance,
    std::vector<Point>& points,
    bool& closed
) {
    closed = false;
    std::vector<DxfVertex> vertices;
    Point current;
    double current_bulge = 0.0;
    bool has_x = false;

    for (const auto& group : entity) {
        if (group.code == 70) {
            closed = (to_int(group.value) & 1) != 0;
        } else if (group.code == 10) {
            if (has_x) {
                vertices.push_back({current, current_bulge});
            }
            current = {};
            current_bulge = 0.0;
            current.x = to_double(group.value);
            has_x = true;
        } else if (group.code == 20 && has_x) {
            current.y = to_double(group.value);
        } else if (group.code == 42 && has_x) {
            current_bulge = to_double(group.value);
        }
    }
    if (has_x) {
        vertices.push_back({current, current_bulge});
    }

    if (vertices.size() < 2) {
        return false;
    }
    points = polyline_vertices_to_points(vertices, closed, flattening_distance);
    return points.size() >= (closed ? 4 : 2);
}

bool parse_lwpolyline_polygon(const std::vector<Group>& entity, double flattening_distance, RawPolygon& polygon) {
    std::vector<Point> points;
    bool closed = false;
    if (!parse_lwpolyline_path(entity, flattening_distance, points, closed) || !closed || points.size() < 4) {
        return false;
    }
    polygon.ring = simplify_straight_vertices(points);
    polygon.area = ring_area(polygon.ring);
    return polygon.area > 0.0;
}

bool parse_legacy_polyline_path(
    const std::vector<std::vector<Group>>& entities,
    size_t& index,
    double flattening_distance,
    std::vector<Point>& points,
    bool& closed
) {
    if (index >= entities.size() || entity_type(entities[index]) != "POLYLINE") {
        return false;
    }

    closed = false;
    int polyline_flags = 0;
    for (const auto& group : entities[index]) {
        if (group.code == 70) {
            polyline_flags = to_int(group.value);
        }
    }
    closed = (polyline_flags & 1) != 0;

    std::vector<DxfVertex> vertices;
    size_t cursor = index + 1;
    for (; cursor < entities.size(); ++cursor) {
        const std::string type = entity_type(entities[cursor]);
        if (type == "SEQEND") {
            break;
        }
        if (type != "VERTEX") {
            continue;
        }

        Point point;
        double bulge = 0.0;
        bool has_x = false;
        bool has_y = false;
        for (const auto& group : entities[cursor]) {
            if (group.code == 10) {
                point.x = to_double(group.value);
                has_x = true;
            } else if (group.code == 20) {
                point.y = to_double(group.value);
                has_y = true;
            } else if (group.code == 42) {
                bulge = to_double(group.value);
            }
        }
        if (has_x && has_y) {
            vertices.push_back({point, bulge});
        }
    }

    index = cursor < entities.size() && entity_type(entities[cursor]) == "SEQEND" ? cursor : cursor - 1;
    if (vertices.size() < 2) {
        return false;
    }
    points = polyline_vertices_to_points(vertices, closed, flattening_distance);
    return points.size() >= (closed ? 4 : 2);
}

DxfTransform identity_transform() {
    return {};
}

Point transform_point(Point point, const DxfTransform& transform) {
    return {
        transform.a * point.x + transform.c * point.y + transform.tx,
        transform.b * point.x + transform.d * point.y + transform.ty,
    };
}

std::vector<Point> transform_points(const std::vector<Point>& points, const DxfTransform& transform) {
    std::vector<Point> output;
    output.reserve(points.size());
    for (Point point : points) {
        output.push_back(transform_point(point, transform));
    }
    return output;
}

RawPolygon transform_polygon(RawPolygon polygon, const DxfTransform& transform) {
    polygon.ring = transform_points(polygon.ring, transform);
    polygon.area = ring_area(polygon.ring);
    return polygon;
}

DxfTransform combine_transform(const DxfTransform& parent, const DxfTransform& child) {
    return {
        parent.a * child.a + parent.c * child.b,
        parent.b * child.a + parent.d * child.b,
        parent.a * child.c + parent.c * child.d,
        parent.b * child.c + parent.d * child.d,
        parent.a * child.tx + parent.c * child.ty + parent.tx,
        parent.b * child.tx + parent.d * child.ty + parent.ty,
    };
}

bool parse_insert_transform(const std::vector<Group>& entity, std::string& block_name, DxfTransform& transform, int& columns, int& rows, double& column_spacing, double& row_spacing) {
    Point insert;
    double scale_x = 1.0;
    double scale_y = 1.0;
    double rotation_degrees = 0.0;
    columns = 1;
    rows = 1;
    column_spacing = 0.0;
    row_spacing = 0.0;
    bool has_name = false;

    for (const auto& group : entity) {
        if (group.code == 2) {
            block_name = upper_string(trim(group.value));
            has_name = !block_name.empty();
        } else if (group.code == 10) {
            insert.x = to_double(group.value);
        } else if (group.code == 20) {
            insert.y = to_double(group.value);
        } else if (group.code == 41) {
            scale_x = to_double(group.value);
        } else if (group.code == 42) {
            scale_y = to_double(group.value);
        } else if (group.code == 50) {
            rotation_degrees = to_double(group.value);
        } else if (group.code == 70) {
            columns = std::max(1, to_int(group.value));
        } else if (group.code == 71) {
            rows = std::max(1, to_int(group.value));
        } else if (group.code == 44) {
            column_spacing = to_double(group.value);
        } else if (group.code == 45) {
            row_spacing = to_double(group.value);
        }
    }

    const double radians = rotation_degrees * kPi / 180.0;
    const double sin_value = std::sin(radians);
    const double cos_value = std::cos(radians);
    transform = {
        scale_x * cos_value,
        scale_x * sin_value,
        -scale_y * sin_value,
        scale_y * cos_value,
        insert.x,
        insert.y,
    };
    return has_name;
}

void collect_geometry_from_entities(
    const std::vector<std::vector<Group>>& entities,
    const std::unordered_map<std::string, DxfBlockDefinition>& blocks,
    const DxfLoadOptions& options,
    const DxfTransform& transform,
    std::vector<RawPolygon>& polygons,
    std::vector<std::vector<Point>>& segments,
    int depth = 0
) {
    if (depth > 12) {
        return;
    }

    for (size_t index = 0; index < entities.size(); ++index) {
        if (entities[index].empty() || entities[index].front().code != 0) {
            continue;
        }

        const std::string type = entity_type(entities[index]);
        RawPolygon polygon;
        std::vector<Point> segment;

        try {
            if (type == "LINE" && parse_line_segment(entities[index], segment)) {
                segments.push_back(transform_points(segment, transform));
            } else if (type == "ARC" && parse_arc_segment(entities[index], options.flattening_distance, segment)) {
                segments.push_back(transform_points(segment, transform));
            } else if (type == "SPLINE" && parse_spline_segment(entities[index], options.flattening_distance, segment)) {
                segments.push_back(transform_points(segment, transform));
            } else if (type == "ELLIPSE") {
                bool closed = false;
                if (parse_ellipse_path(entities[index], options.flattening_distance, segment, closed)) {
                    segment = transform_points(segment, transform);
                    if (closed && segment.size() >= 4) {
                        polygon.ring = simplify_straight_vertices(segment);
                        polygon.area = ring_area(polygon.ring);
                        if (polygon.area > 0.0) {
                            polygons.push_back(std::move(polygon));
                        }
                    } else {
                        segments.push_back(std::move(segment));
                    }
                }
            } else if (type == "CIRCLE" && parse_circle_polygon(entities[index], options.flattening_distance, polygon)) {
                polygons.push_back(transform_polygon(std::move(polygon), transform));
            } else if (type == "LWPOLYLINE") {
                bool closed = false;
                if (parse_lwpolyline_path(entities[index], options.flattening_distance, segment, closed)) {
                    segment = transform_points(segment, transform);
                    if (closed && segment.size() >= 4) {
                        polygon.ring = simplify_straight_vertices(segment);
                        polygon.area = ring_area(polygon.ring);
                        if (polygon.area > 0.0) {
                            polygons.push_back(std::move(polygon));
                        }
                    } else {
                        segments.push_back(std::move(segment));
                    }
                }
            } else if (type == "POLYLINE") {
                bool closed = false;
                if (parse_legacy_polyline_path(entities, index, options.flattening_distance, segment, closed)) {
                    segment = transform_points(segment, transform);
                    if (closed && segment.size() >= 4) {
                        polygon.ring = simplify_straight_vertices(segment);
                        polygon.area = ring_area(polygon.ring);
                        if (polygon.area > 0.0) {
                            polygons.push_back(std::move(polygon));
                        }
                    } else {
                        segments.push_back(std::move(segment));
                    }
                }
            } else if (type == "INSERT") {
                std::string block_name;
                DxfTransform insert_transform;
                int columns = 1;
                int rows = 1;
                double column_spacing = 0.0;
                double row_spacing = 0.0;
                if (!parse_insert_transform(
                        entities[index],
                        block_name,
                        insert_transform,
                        columns,
                        rows,
                        column_spacing,
                        row_spacing)) {
                    continue;
                }

                const auto block_iter = blocks.find(block_name);
                if (block_iter == blocks.end()) {
                    continue;
                }
                const auto& block = block_iter->second;
                for (int row = 0; row < rows; ++row) {
                    for (int column = 0; column < columns; ++column) {
                        const DxfTransform local_offset{
                            1.0,
                            0.0,
                            0.0,
                            1.0,
                            static_cast<double>(column) * column_spacing - block.base.x,
                            static_cast<double>(row) * row_spacing - block.base.y,
                        };
                        const DxfTransform block_transform = combine_transform(transform, combine_transform(insert_transform, local_offset));
                        collect_geometry_from_entities(
                            block.entities,
                            blocks,
                            options,
                            block_transform,
                            polygons,
                            segments,
                            depth + 1
                        );
                    }
                }
            }
        } catch (...) {
            continue;
        }
    }
}

std::vector<Point> try_merge_segment(
    const std::vector<Point>& chain,
    const std::vector<Point>& segment,
    double tolerance,
    bool& merged
) {
    merged = true;
    if (distance(chain.back(), segment.front()) <= tolerance) {
        std::vector<Point> result = chain;
        result.insert(result.end(), segment.begin() + 1, segment.end());
        return result;
    }
    if (distance(chain.back(), segment.back()) <= tolerance) {
        std::vector<Point> result = chain;
        result.insert(result.end(), segment.rbegin() + 1, segment.rend());
        return result;
    }
    if (distance(chain.front(), segment.back()) <= tolerance) {
        std::vector<Point> result = segment;
        result.insert(result.end(), chain.begin() + 1, chain.end());
        return result;
    }
    if (distance(chain.front(), segment.front()) <= tolerance) {
        std::vector<Point> reversed = segment;
        std::reverse(reversed.begin(), reversed.end());
        reversed.insert(reversed.end(), chain.begin() + 1, chain.end());
        return reversed;
    }
    merged = false;
    return chain;
}

double chain_length(const std::vector<Point>& chain) {
    double length = 0.0;
    for (size_t index = 1; index < chain.size(); ++index) {
        length += distance(chain[index - 1], chain[index]);
    }
    return length;
}

bool should_close_nearly_closed_chain(const std::vector<Point>& chain, double tolerance) {
    if (chain.size() < 4) {
        return false;
    }

    const double gap = distance(chain.front(), chain.back());
    if (gap <= tolerance) {
        return true;
    }

    const double path_length = chain_length(chain);
    if (path_length <= 1e-6) {
        return false;
    }

    const Bounds bounds = ring_bounds(chain);
    const double diagonal = std::hypot(bounds.width(), bounds.height());
    const double max_repair_gap = std::min({5.0, path_length * 0.025, diagonal * 0.08});
    return gap <= std::max(tolerance, max_repair_gap);
}

std::vector<RawPolygon> segments_to_polygons(std::vector<std::vector<Point>> segments, double tolerance) {
    std::vector<RawPolygon> polygons;
    while (!segments.empty()) {
        std::vector<Point> chain = segments.front();
        segments.erase(segments.begin());

        bool changed = true;
        while (changed) {
            changed = false;
            for (size_t index = 0; index < segments.size(); ++index) {
                bool merged = false;
                auto next_chain = try_merge_segment(chain, segments[index], tolerance, merged);
                if (!merged) {
                    continue;
                }
                chain = std::move(next_chain);
                segments.erase(segments.begin() + static_cast<std::ptrdiff_t>(index));
                changed = true;
                break;
            }
        }

        if (chain.size() >= 3 && should_close_nearly_closed_chain(chain, tolerance)) {
            chain.back() = chain.front();
            chain = simplify_straight_vertices(std::move(chain));
            const double area = ring_area(chain);
            if (area > 0.0) {
                polygons.push_back({chain, area});
            }
        }
    }
    return polygons;
}

Point ring_representative_point(const std::vector<Point>& raw_ring) {
    const auto ring = ensure_closed(raw_ring);
    Point center{};
    size_t count = 0;
    const size_t limit = ring.size() > 1 ? ring.size() - 1 : ring.size();
    for (size_t index = 0; index < limit; ++index) {
        center.x += ring[index].x;
        center.y += ring[index].y;
        ++count;
    }
    if (count > 0) {
        center.x /= static_cast<double>(count);
        center.y /= static_cast<double>(count);
    }
    return center;
}

bool bounds_contains_bounds(const Bounds& outer, const Bounds& inner, double tolerance) {
    return outer.valid() && inner.valid() &&
           inner.min_x >= outer.min_x - tolerance &&
           inner.max_x <= outer.max_x + tolerance &&
           inner.min_y >= outer.min_y - tolerance &&
           inner.max_y <= outer.max_y + tolerance;
}

bool ring_contains_ring(const std::vector<Point>& outer, const std::vector<Point>& inner) {
    constexpr double tolerance = 1e-4;
    const Bounds outer_bounds = ring_bounds(outer);
    const Bounds inner_bounds = ring_bounds(inner);
    if (!bounds_contains_bounds(outer_bounds, inner_bounds, tolerance)) {
        return false;
    }

    const Point representative = ring_representative_point(inner);
    if (point_in_ring(representative, outer)) {
        return true;
    }

    size_t tested = 0;
    const size_t limit = inner.size() > 1 ? inner.size() - 1 : inner.size();
    for (size_t index = 0; index < limit && tested < 12; ++index) {
        if (point_in_ring(inner[index], outer)) {
            return true;
        }
        ++tested;
    }

    // Some CAD exports made from independent LINE/ARC chains produce rings that
    // display correctly but fail point-in-polygon because of tiny ordering or
    // closure defects. For sheet-metal DXF import, a smaller closed contour fully
    // inside the larger contour's bounds should be treated as an internal cutout.
    return true;
}

std::vector<Part> build_parts_from_polygons(
    std::vector<RawPolygon> polygons,
    const std::string& base_name,
    const std::string& source,
    const DxfLoadOptions& options
) {
    std::sort(polygons.begin(), polygons.end(), [](const RawPolygon& first, const RawPolygon& second) {
        return first.area > second.area;
    });

    struct GroupedPart {
        RawPolygon outer;
        std::vector<RawPolygon> holes;
    };

    std::vector<int> parent(polygons.size(), -1);
    for (size_t child = 0; child < polygons.size(); ++child) {
        double smallest_parent_area = std::numeric_limits<double>::infinity();
        for (size_t candidate = 0; candidate < polygons.size(); ++candidate) {
            if (candidate == child || polygons[candidate].area <= polygons[child].area) {
                continue;
            }
            if (polygons[candidate].area >= smallest_parent_area) {
                continue;
            }
            if (!ring_contains_ring(polygons[candidate].ring, polygons[child].ring)) {
                continue;
            }
            parent[child] = static_cast<int>(candidate);
            smallest_parent_area = polygons[candidate].area;
        }
    }

    std::vector<GroupedPart> groups;
    std::vector<int> group_for_polygon(polygons.size(), -1);
    for (size_t index = 0; index < polygons.size(); ++index) {
        if (parent[index] >= 0) {
            continue;
        }
        group_for_polygon[index] = static_cast<int>(groups.size());
        groups.push_back({polygons[index], {}});
    }

    for (size_t index = 0; index < polygons.size(); ++index) {
        if (parent[index] < 0) {
            continue;
        }

        int root_index = static_cast<int>(index);
        int guard = 0;
        while (parent[static_cast<size_t>(root_index)] >= 0 && guard < static_cast<int>(polygons.size())) {
            root_index = parent[static_cast<size_t>(root_index)];
            ++guard;
        }

        if (root_index >= 0 && root_index < static_cast<int>(group_for_polygon.size())) {
            const int group_index = group_for_polygon[static_cast<size_t>(root_index)];
            if (group_index >= 0 && group_index < static_cast<int>(groups.size())) {
                groups[static_cast<size_t>(group_index)].holes.push_back(polygons[index]);
            }
        }
    }

    std::vector<Part> parts;
    int index = 1;
    for (const auto& group : groups) {
        Polygon geometry;
        geometry.outer = simplify_straight_vertices(group.outer.ring);
        for (const auto& hole : group.holes) {
            geometry.holes.push_back(simplify_straight_vertices(hole.ring));
        }

        geometry = normalize_to_origin(geometry);
        const double area = polygon_area(geometry);
        if (area < options.min_part_area) {
            continue;
        }

        Part part;
        part.name = base_name;
        if (groups.size() > 1) {
            part.name += "-" + std::to_string(index);
        }
        part.geometry = std::move(geometry);
        part.quantity = options.quantity;
        part.source = source;
        parts.push_back(std::move(part));
        ++index;
    }
    return parts;
}

std::string stem_string(const std::filesystem::path& path) {
    return path.stem().string();
}

template <typename T>
void write_group(std::ostream& output, int code, const T& value) {
    output << code << "\r\n" << value << "\r\n";
}

thread_local unsigned int g_dxf_handle = 0x10;
thread_local bool g_write_dxf_handles = false;

std::string next_handle() {
    std::ostringstream handle;
    handle << std::uppercase << std::hex << g_dxf_handle++;
    return handle.str();
}

void write_handle(std::ostream& output) {
    if (!g_write_dxf_handles) {
        return;
    }
    write_group(output, 5, next_handle());
}

std::string sanitize_dxf_text(std::string text) {
    for (char& ch : text) {
        if (ch == '\r' || ch == '\n') {
            ch = ' ';
        }
    }
    return text;
}

double open_ring_area(const std::vector<Point>& ring) {
    double area = 0.0;
    if (ring.size() < 3) {
        return area;
    }
    for (size_t index = 0; index < ring.size(); ++index) {
        const Point a = ring[index];
        const Point b = ring[(index + 1) % ring.size()];
        area += a.x * b.y - b.x * a.y;
    }
    return area * 0.5;
}

std::vector<Point> clean_open_ring(const std::vector<Point>& raw_ring) {
    auto closed_ring = ensure_closed(raw_ring);
    std::vector<Point> ring;
    ring.reserve(closed_ring.size());

    for (const Point point : closed_ring) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
            continue;
        }
        if (!ring.empty() && distance(ring.back(), point) <= 1e-6) {
            continue;
        }
        ring.push_back(point);
    }

    if (ring.size() > 1 && distance(ring.front(), ring.back()) <= 1e-6) {
        ring.pop_back();
    }
    if (ring.size() < 3 || std::abs(open_ring_area(ring)) <= 1e-6) {
        return {};
    }
    return ring;
}

void write_header(std::ostream& output, int insunits) {
    insunits = insunits == 1 ? 1 : 4; // 1 = inches, 4 = millimeters
    write_group(output, 999, "Created by Sheet Metal Nesting");
    write_group(output, 0, "SECTION");
    write_group(output, 2, "HEADER");
    write_group(output, 9, "$ACADVER");
    write_group(output, 1, "AC1009");
    write_group(output, 9, "$DWGCODEPAGE");
    write_group(output, 3, "ANSI_1252");
    write_group(output, 9, "$INSUNITS");
    write_group(output, 70, insunits);
    write_group(output, 9, "$MEASUREMENT");
    write_group(output, 70, insunits == 1 ? 0 : 1);
    write_group(output, 9, "$LUNITS");
    write_group(output, 70, 2);
    write_group(output, 9, "$LUPREC");
    write_group(output, 70, 3);
    write_group(output, 0, "ENDSEC");
}

void write_ltype_table(std::ostream& output) {
    write_group(output, 0, "TABLE");
    write_group(output, 2, "LTYPE");
    write_group(output, 70, 1);
    write_group(output, 0, "LTYPE");
    write_handle(output);
    write_group(output, 2, "Continuous");
    write_group(output, 70, 0);
    write_group(output, 3, "Solid line");
    write_group(output, 72, 65);
    write_group(output, 73, 0);
    write_group(output, 40, "0.0");
    write_group(output, 0, "ENDTAB");
}

void write_layer(std::ostream& output, const std::string& name, int color) {
    write_group(output, 0, "LAYER");
    write_handle(output);
    write_group(output, 2, name);
    write_group(output, 70, 0);
    write_group(output, 62, color);
    write_group(output, 6, "Continuous");
}

void write_style_table(std::ostream& output) {
    write_group(output, 0, "TABLE");
    write_group(output, 2, "STYLE");
    write_group(output, 70, 1);
    write_group(output, 0, "STYLE");
    write_handle(output);
    write_group(output, 2, "Standard");
    write_group(output, 70, 0);
    write_group(output, 40, "0.0");
    write_group(output, 41, "1.0");
    write_group(output, 50, "0.0");
    write_group(output, 71, 0);
    write_group(output, 42, "2.5");
    write_group(output, 3, "txt");
    write_group(output, 4, "");
    write_group(output, 0, "ENDTAB");
}

void write_appid_table(std::ostream& output) {
    write_group(output, 0, "TABLE");
    write_group(output, 2, "APPID");
    write_group(output, 70, 1);
    write_group(output, 0, "APPID");
    write_handle(output);
    write_group(output, 2, "ACAD");
    write_group(output, 70, 0);
    write_group(output, 0, "ENDTAB");
}

void write_tables(std::ostream& output) {
    write_group(output, 0, "SECTION");
    write_group(output, 2, "TABLES");
    write_ltype_table(output);
    write_group(output, 0, "TABLE");
    write_group(output, 2, "LAYER");
    write_group(output, 70, 5);
    write_layer(output, "0", 7);
    write_layer(output, "SHEET", 8);
    write_layer(output, "PARTS", 7);
    write_layer(output, "HOLES", 3);
    write_layer(output, "ANNOTATION", 2);
    write_group(output, 0, "ENDTAB");
    write_style_table(output);
    write_appid_table(output);
    write_group(output, 0, "ENDSEC");
}

void write_blocks(std::ostream& output) {
    write_group(output, 0, "SECTION");
    write_group(output, 2, "BLOCKS");
    write_group(output, 0, "ENDSEC");
}

void write_lwpolyline(std::ostream& output, const std::vector<Point>& raw_ring, const std::string& layer) {
    const auto ring = clean_open_ring(raw_ring);
    if (ring.empty()) {
        return;
    }

    output << std::fixed << std::setprecision(6);
    write_group(output, 0, "POLYLINE");
    write_handle(output);
    write_group(output, 8, layer);
    write_group(output, 66, 1);
    write_group(output, 70, 1);
    write_group(output, 10, "0.0");
    write_group(output, 20, "0.0");
    write_group(output, 30, "0.0");
    for (Point point : ring) {
        write_group(output, 0, "VERTEX");
        write_handle(output);
        write_group(output, 8, layer);
        write_group(output, 10, point.x);
        write_group(output, 20, point.y);
        write_group(output, 30, "0.0");
    }
    write_group(output, 0, "SEQEND");
    write_handle(output);
    write_group(output, 8, layer);
}

void write_line(std::ostream& output, Point a, Point b, const std::string& layer) {
    if (!std::isfinite(a.x) || !std::isfinite(a.y) ||
        !std::isfinite(b.x) || !std::isfinite(b.y) ||
        distance(a, b) <= 1e-9) {
        return;
    }

    output << std::fixed << std::setprecision(6);
    write_group(output, 0, "LINE");
    write_handle(output);
    write_group(output, 8, layer);
    write_group(output, 10, a.x);
    write_group(output, 20, a.y);
    write_group(output, 30, "0.0");
    write_group(output, 11, b.x);
    write_group(output, 21, b.y);
    write_group(output, 31, "0.0");
}

void write_circle(std::ostream& output, Point center, double radius, const std::string& layer) {
    if (!std::isfinite(center.x) || !std::isfinite(center.y) ||
        !std::isfinite(radius) || radius <= 1e-9) {
        return;
    }

    output << std::fixed << std::setprecision(6);
    write_group(output, 0, "CIRCLE");
    write_handle(output);
    write_group(output, 8, layer);
    write_group(output, 10, center.x);
    write_group(output, 20, center.y);
    write_group(output, 30, "0.0");
    write_group(output, 40, radius);
}

void collect_ring_segments(
    const std::vector<Point>& raw_ring,
    const std::string& layer,
    std::vector<CutSegment>& segments
) {
    const auto ring = clean_open_ring(raw_ring);
    if (ring.empty()) {
        return;
    }

    for (size_t index = 0; index < ring.size(); ++index) {
        const Point a = ring[index];
        const Point b = ring[(index + 1) % ring.size()];
        if (distance(a, b) > 1e-9) {
            segments.push_back({a, b, layer});
        }
    }
}

int layer_priority(const std::string& layer) {
    if (layer == "PARTS") {
        return 3;
    }
    if (layer == "HOLES") {
        return 2;
    }
    return 1;
}

void write_merged_cut_segments(std::ostream& output, const std::vector<CutSegment>& segments) {
    std::vector<CommonLineGroup> groups;
    std::unordered_map<std::string, size_t> group_by_key;

    for (const auto& segment : segments) {
        const double dx = segment.b.x - segment.a.x;
        const double dy = segment.b.y - segment.a.y;
        const double length = std::hypot(dx, dy);
        if (length <= 1e-9) {
            continue;
        }

        Point direction{dx / length, dy / length};
        Point a = segment.a;
        Point b = segment.b;
        if (direction.x < -1e-12 || (std::abs(direction.x) <= 1e-12 && direction.y < 0.0)) {
            direction.x = -direction.x;
            direction.y = -direction.y;
            std::swap(a, b);
        }

        const Point normal{-direction.y, direction.x};
        const double offset = normal.x * a.x + normal.y * a.y;
        const double angle = std::atan2(direction.y, direction.x);
        const std::string key =
            std::to_string(quantize(angle, 1e-7)) + ":" +
            std::to_string(quantize(offset, kCommonLineTolerance));

        auto found = group_by_key.find(key);
        if (found == group_by_key.end()) {
            found = group_by_key.emplace(key, groups.size()).first;
            groups.push_back({direction, offset, {}});
        }

        auto& group = groups[found->second];
        double start = group.direction.x * a.x + group.direction.y * a.y;
        double end = group.direction.x * b.x + group.direction.y * b.y;
        if (start > end) {
            std::swap(start, end);
        }
        if (end - start > 1e-9) {
            group.intervals.push_back({start, end, segment.layer});
        }
    }

    for (auto& group : groups) {
        if (group.intervals.empty()) {
            continue;
        }

        std::sort(group.intervals.begin(), group.intervals.end(), [](const SegmentInterval& first, const SegmentInterval& second) {
            if (std::abs(first.start - second.start) > kCommonLineTolerance) {
                return first.start < second.start;
            }
            return first.end < second.end;
        });

        std::vector<SegmentInterval> merged;
        for (const auto& interval : group.intervals) {
            if (merged.empty() || interval.start > merged.back().end + kCommonLineTolerance) {
                merged.push_back(interval);
                continue;
            }

            auto& current = merged.back();
            current.end = std::max(current.end, interval.end);
            if (layer_priority(interval.layer) > layer_priority(current.layer)) {
                current.layer = interval.layer;
            }
        }

        const Point normal{-group.direction.y, group.direction.x};
        for (const auto& interval : merged) {
            const Point a{
                group.direction.x * interval.start + normal.x * group.offset,
                group.direction.y * interval.start + normal.y * group.offset,
            };
            const Point b{
                group.direction.x * interval.end + normal.x * group.offset,
                group.direction.y * interval.end + normal.y * group.offset,
            };
            write_line(output, a, b, interval.layer);
        }
    }
}

void write_text(std::ostream& output, Point point, double height, const std::string& text, const std::string& layer) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(height) || height <= 0.0) {
        return;
    }
    output << std::fixed << std::setprecision(6);
    write_group(output, 0, "TEXT");
    write_handle(output);
    write_group(output, 8, layer);
    write_group(output, 10, point.x);
    write_group(output, 20, point.y);
    write_group(output, 30, "0.0");
    write_group(output, 40, height);
    write_group(output, 1, sanitize_dxf_text(text));
    write_group(output, 7, "Standard");
    write_group(output, 50, "0.0");
}

void write_draft_entities(std::ostream& output, const std::vector<DraftEntity>& draft_entities) {
    for (const auto& entity : draft_entities) {
        switch (entity.type) {
            case DraftEntityType::Line:
                write_line(output, entity.start, entity.end, "ANNOTATION");
                break;
            case DraftEntityType::Rectangle: {
                write_line(output, entity.start, {entity.end.x, entity.start.y}, "ANNOTATION");
                write_line(output, {entity.end.x, entity.start.y}, entity.end, "ANNOTATION");
                write_line(output, entity.end, {entity.start.x, entity.end.y}, "ANNOTATION");
                write_line(output, {entity.start.x, entity.end.y}, entity.start, "ANNOTATION");
                break;
            }
            case DraftEntityType::Circle:
                write_circle(output, entity.start, distance(entity.start, entity.end), "ANNOTATION");
                break;
            case DraftEntityType::Arc: {
                // 3-point arc: compute center and angles from start, mid, end
                const double ax = entity.start.x, ay = entity.start.y;
                const double bx = entity.mid.x, by = entity.mid.y;
                const double cx = entity.end.x, cy = entity.end.y;
                const double d = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
                if (std::abs(d) > 1e-9) {
                    const double ux = ((ax*ax + ay*ay) * (by - cy) + (bx*bx + by*by) * (cy - ay) + (cx*cx + cy*cy) * (ay - by)) / d;
                    const double uy = ((ax*ax + ay*ay) * (cx - bx) + (bx*bx + by*by) * (ax - cx) + (cx*cx + cy*cy) * (bx - ax)) / d;
                    const double radius = std::hypot(ax - ux, ay - uy);
                    const double start_angle = std::atan2(ay - uy, ax - ux) * 180.0 / kPi;
                    const double end_angle = std::atan2(cy - uy, cx - ux) * 180.0 / kPi;
                    // Write as DXF ARC
                    output << std::fixed << std::setprecision(6);
                    write_group(output, 0, "ARC");
                    write_handle(output);
                    write_group(output, 8, "ANNOTATION");
                    write_group(output, 10, ux);
                    write_group(output, 20, uy);
                    write_group(output, 30, "0.0");
                    write_group(output, 40, radius);
                    write_group(output, 50, start_angle < 0 ? start_angle + 360.0 : start_angle);
                    write_group(output, 51, end_angle < 0 ? end_angle + 360.0 : end_angle);
                }
                break;
            }
            case DraftEntityType::Polyline: {
                if (entity.points.size() >= 2) {
                    for (size_t i = 0; i + 1 < entity.points.size(); ++i) {
                        write_line(output, entity.points[i], entity.points[i + 1], "ANNOTATION");
                    }
                }
                break;
            }
            case DraftEntityType::Dimension: {
                // Extension lines + measurement line with arrowheads
                const double dx = entity.end.x - entity.start.x;
                const double dy = entity.end.y - entity.start.y;
                const double len = std::hypot(dx, dy);
                if (len < 1e-6) break;
                const double ux = dx / len, uy = dy / len;  // unit direction
                const double nx = -uy, ny = ux;  // normal
                constexpr double arrow_len = 8.0;
                constexpr double arrow_w = 3.0;
                const Point measured_mid{(entity.start.x + entity.end.x) * 0.5, (entity.start.y + entity.end.y) * 0.5};
                double offset = 15.0;
                if (std::hypot(entity.mid.x, entity.mid.y) > 1e-6) {
                    offset = (entity.mid.x - measured_mid.x) * nx + (entity.mid.y - measured_mid.y) * ny;
                    if (std::abs(offset) < 1.0) {
                        offset = offset < 0.0 ? -15.0 : 15.0;
                    }
                }
                const double extension_extra = offset < 0.0 ? -4.0 : 4.0;
                const Point ms{entity.start.x + nx * offset, entity.start.y + ny * offset};
                const Point me{entity.end.x + nx * offset, entity.end.y + ny * offset};
                // Extension lines perpendicular at both ends
                write_line(output, entity.start, {ms.x + nx * extension_extra, ms.y + ny * extension_extra}, "ANNOTATION");
                write_line(output, entity.end, {me.x + nx * extension_extra, me.y + ny * extension_extra}, "ANNOTATION");
                write_line(output, ms, me, "ANNOTATION");
                // Arrowheads (as small lines forming a V)
                const Point a1{ms.x + ux * arrow_len + nx * arrow_w, ms.y + uy * arrow_len + ny * arrow_w};
                const Point a2{ms.x + ux * arrow_len - nx * arrow_w, ms.y + uy * arrow_len - ny * arrow_w};
                write_line(output, ms, a1, "ANNOTATION");
                write_line(output, ms, a2, "ANNOTATION");
                const Point b1{me.x - ux * arrow_len + nx * arrow_w, me.y - uy * arrow_len + ny * arrow_w};
                const Point b2{me.x - ux * arrow_len - nx * arrow_w, me.y - uy * arrow_len - ny * arrow_w};
                write_line(output, me, b1, "ANNOTATION");
                write_line(output, me, b2, "ANNOTATION");
                // Dimension text
                const Point middle{(ms.x + me.x) * 0.5 + nx * 4, (ms.y + me.y) * 0.5 + ny * 4};
                std::string label = entity.text;
                if (label.empty()) {
                    std::ostringstream stream;
                    stream << std::fixed << std::setprecision(1) << len << " mm";
                    label = stream.str();
                }
                write_text(output, middle, 10.0, label, "ANNOTATION");
                break;
            }
            case DraftEntityType::Text:
                write_text(output, entity.start, 12.0, entity.text, "ANNOTATION");
                break;
        }
    }
}

}  // namespace

std::vector<std::filesystem::path> expand_dxf_inputs(const std::vector<std::filesystem::path>& inputs) {
    std::vector<std::filesystem::path> paths;
    for (const auto& input : inputs) {
        if (std::filesystem::is_directory(input)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(input)) {
                if (entry.is_regular_file() && is_dxf_path(entry.path())) {
                    paths.push_back(entry.path());
                }
            }
            continue;
        }
        if (std::filesystem::is_regular_file(input) && is_dxf_path(input)) {
            paths.push_back(input);
            continue;
        }
        throw std::runtime_error("DXF input not found: " + input.string());
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

std::vector<Part> load_dxf_parts(const std::vector<std::filesystem::path>& inputs, const DxfLoadOptions& options) {
    std::vector<Part> all_parts;
    for (const auto& path : expand_dxf_inputs(inputs)) {
        std::vector<RawPolygon> polygons;
        std::vector<std::vector<Point>> segments;
        const auto groups = read_groups(path);
        const auto document = parse_dxf_document(groups);
        collect_geometry_from_entities(
            document.entities,
            document.blocks,
            options,
            identity_transform(),
            polygons,
            segments
        );

        auto chained = segments_to_polygons(std::move(segments), std::max(0.001, options.endpoint_tolerance));
        polygons.insert(polygons.end(), chained.begin(), chained.end());

        auto parts = build_parts_from_polygons(polygons, stem_string(path), path.string(), options);
        all_parts.insert(all_parts.end(), parts.begin(), parts.end());
    }
    return all_parts;
}

void write_nesting_dxf(
    const NestingResult& result,
    const std::filesystem::path& output_path,
    double sheet_gap,
    const std::vector<TextAnnotation>& text_annotations,
    int insunits,
    bool merge_common_lines,
    const std::vector<DraftEntity>& draft_entities
) {
    if (output_path.has_parent_path()) {
        std::filesystem::create_directories(output_path.parent_path());
    }

    std::ofstream output(output_path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Cannot write DXF: " + output_path.string());
    }

    g_dxf_handle = 0x10;
    g_write_dxf_handles = false;
    write_header(output, insunits);
    write_tables(output);
    write_blocks(output);
    write_group(output, 0, "SECTION");
    write_group(output, 2, "ENTITIES");
    const double pitch = result.sheet.width + sheet_gap;

    for (int sheet_index = 0; sheet_index < result.sheet_count(); ++sheet_index) {
        const double x = static_cast<double>(sheet_index) * pitch;
        write_lwpolyline(output, {
            {x, 0.0},
            {x + result.sheet.width, 0.0},
            {x + result.sheet.width, result.sheet.height},
            {x, result.sheet.height},
        }, "SHEET");
        write_text(output, {x, result.sheet.height + 35.0}, 24.0, "Sheet " + std::to_string(sheet_index + 1), "SHEET");
    }

    if (merge_common_lines) {
        std::vector<CutSegment> cut_segments;
        for (const auto& placement : result.placements) {
            const double offset_x = static_cast<double>(placement.sheet_index) * pitch;
            const Polygon shifted = translate_polygon(placement.geometry, offset_x, 0.0);
            collect_ring_segments(shifted.outer, "PARTS", cut_segments);
            for (const auto& hole : shifted.holes) {
                collect_ring_segments(hole, "HOLES", cut_segments);
            }
        }
        write_merged_cut_segments(output, cut_segments);
    } else {
        for (const auto& placement : result.placements) {
            const double offset_x = static_cast<double>(placement.sheet_index) * pitch;
            const Polygon shifted = translate_polygon(placement.geometry, offset_x, 0.0);
            write_lwpolyline(output, shifted.outer, "PARTS");
            for (const auto& hole : shifted.holes) {
                write_lwpolyline(output, hole, "HOLES");
            }
        }
    }

    for (const auto& ann : text_annotations) {
        write_text(output, {ann.x, ann.y}, 12.0, ann.text, "ANNOTATION");
    }
    write_draft_entities(output, draft_entities);

    write_group(output, 0, "ENDSEC");
    write_group(output, 0, "EOF");
}

}  // namespace smn
