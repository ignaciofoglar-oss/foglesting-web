#include "core/geometry.hpp"

#include <algorithm>
#include <cmath>

namespace smn {
namespace {

constexpr double kEpsilon = 1e-7;
constexpr double kPi = 3.141592653589793238462643383279502884;

double cross(Point a, Point b, Point c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

bool nearly_equal(double a, double b, double eps = kEpsilon) {
    return std::abs(a - b) <= eps;
}

bool points_equal(Point a, Point b, double eps = kEpsilon) {
    return distance(a, b) <= eps;
}

bool on_segment(Point a, Point b, Point p) {
    if (std::abs(cross(a, b, p)) > kEpsilon) {
        return false;
    }
    return p.x >= std::min(a.x, b.x) - kEpsilon &&
           p.x <= std::max(a.x, b.x) + kEpsilon &&
           p.y >= std::min(a.y, b.y) - kEpsilon &&
           p.y <= std::max(a.y, b.y) + kEpsilon;
}

bool segments_intersect(Point a, Point b, Point c, Point d) {
    const double c1 = cross(a, b, c);
    const double c2 = cross(a, b, d);
    const double c3 = cross(c, d, a);
    const double c4 = cross(c, d, b);

    if (((c1 > kEpsilon && c2 < -kEpsilon) || (c1 < -kEpsilon && c2 > kEpsilon)) &&
        ((c3 > kEpsilon && c4 < -kEpsilon) || (c3 < -kEpsilon && c4 > kEpsilon))) {
        return true;
    }

    return on_segment(a, b, c) || on_segment(a, b, d) ||
           on_segment(c, d, a) || on_segment(c, d, b);
}

double point_segment_distance(Point p, Point a, Point b) {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double length2 = dx * dx + dy * dy;
    if (length2 <= kEpsilon) {
        return distance(p, a);
    }

    double t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / length2;
    t = std::clamp(t, 0.0, 1.0);
    return distance(p, {a.x + t * dx, a.y + t * dy});
}

double segment_distance(Point a, Point b, Point c, Point d) {
    if (segments_intersect(a, b, c, d)) {
        return 0.0;
    }
    return std::min({
        point_segment_distance(a, c, d),
        point_segment_distance(b, c, d),
        point_segment_distance(c, a, b),
        point_segment_distance(d, a, b),
    });
}

double bounds_distance(const Bounds& first, const Bounds& second) {
    const double dx = std::max({
        first.min_x - second.max_x,
        second.min_x - first.max_x,
        0.0,
    });
    const double dy = std::max({
        first.min_y - second.max_y,
        second.min_y - first.max_y,
        0.0,
    });
    return std::sqrt(dx * dx + dy * dy);
}

Bounds segment_bounds(Point first, Point second) {
    return {
        std::min(first.x, second.x),
        std::min(first.y, second.y),
        std::max(first.x, second.x),
        std::max(first.y, second.y),
    };
}

bool bounds_contains_point(const Bounds& bounds, Point point) {
    return point.x >= bounds.min_x - kEpsilon &&
           point.x <= bounds.max_x + kEpsilon &&
           point.y >= bounds.min_y - kEpsilon &&
           point.y <= bounds.max_y + kEpsilon;
}

bool rings_intersect(const std::vector<Point>& first, const std::vector<Point>& second) {
    const Bounds first_bounds = ring_bounds(first);
    const Bounds second_bounds = ring_bounds(second);
    if (!first_bounds.overlaps(second_bounds, 0.0)) {
        return false;
    }

    const auto first_ring = ensure_closed(first);
    const auto second_ring = ensure_closed(second);
    for (size_t i = 0; i + 1 < first_ring.size(); ++i) {
        const Bounds first_segment_bounds = segment_bounds(first_ring[i], first_ring[i + 1]);
        for (size_t j = 0; j + 1 < second_ring.size(); ++j) {
            const Bounds second_segment_bounds = segment_bounds(second_ring[j], second_ring[j + 1]);
            if (!first_segment_bounds.overlaps(second_segment_bounds, 0.0)) {
                continue;
            }
            if (segments_intersect(first_ring[i], first_ring[i + 1], second_ring[j], second_ring[j + 1])) {
                return true;
            }
        }
    }
    return false;
}

double ring_distance(const std::vector<Point>& first, const std::vector<Point>& second, double current_best = std::numeric_limits<double>::infinity()) {
    const Bounds first_bounds = ring_bounds(first);
    const Bounds second_bounds = ring_bounds(second);
    if (bounds_distance(first_bounds, second_bounds) >= current_best) {
        return current_best;
    }

    const auto first_ring = ensure_closed(first);
    const auto second_ring = ensure_closed(second);
    double best = current_best;
    for (size_t i = 0; i + 1 < first_ring.size(); ++i) {
        const Bounds first_segment_bounds = segment_bounds(first_ring[i], first_ring[i + 1]);
        for (size_t j = 0; j + 1 < second_ring.size(); ++j) {
            const Bounds second_segment_bounds = segment_bounds(second_ring[j], second_ring[j + 1]);
            if (bounds_distance(first_segment_bounds, second_segment_bounds) >= best) {
                continue;
            }
            best = std::min(best, segment_distance(first_ring[i], first_ring[i + 1], second_ring[j], second_ring[j + 1]));
        }
    }
    return best;
}

bool point_in_material(Point point, const Polygon& polygon) {
    if (!point_in_ring(point, polygon.outer)) {
        return false;
    }
    for (const auto& hole : polygon.holes) {
        if (bounds_contains_point(ring_bounds(hole), point) && point_in_ring(point, hole)) {
            return false;
        }
    }
    return true;
}

bool material_point_inside(const Polygon& source, const Polygon& target) {
    const auto outer = ensure_closed(source.outer);
    if (outer.size() < 2) {
        return false;
    }

    for (size_t index = 0; index + 1 < outer.size(); ++index) {
        const Point point = outer[index];
        const Point next = outer[index + 1];
        const Point midpoint{
            (point.x + next.x) * 0.5,
            (point.y + next.y) * 0.5,
        };
        if (point_in_material(point, target) ||
            point_in_material(midpoint, target)) {
            return true;
        }
    }
    return false;
}

std::vector<const std::vector<Point>*> boundary_rings(const Polygon& polygon) {
    std::vector<const std::vector<Point>*> rings;
    rings.reserve(1 + polygon.holes.size());
    rings.push_back(&polygon.outer);
    for (const auto& hole : polygon.holes) {
        rings.push_back(&hole);
    }
    return rings;
}

std::vector<Point> transform_ring(
    const std::vector<Point>& ring,
    double degrees,
    double dx,
    double dy
) {
    const double radians = degrees * kPi / 180.0;
    const double sin_value = std::sin(radians);
    const double cos_value = std::cos(radians);
    std::vector<Point> transformed;
    transformed.reserve(ring.size());

    for (Point point : ring) {
        transformed.push_back({
            point.x * cos_value - point.y * sin_value + dx,
            point.x * sin_value + point.y * cos_value + dy,
        });
    }
    return transformed;
}

}  // namespace

double Bounds::width() const {
    return max_x - min_x;
}

double Bounds::height() const {
    return max_y - min_y;
}

bool Bounds::valid() const {
    return std::isfinite(min_x) && std::isfinite(min_y) &&
           std::isfinite(max_x) && std::isfinite(max_y);
}

bool Bounds::overlaps(const Bounds& other, double spacing) const {
    return min_x < other.max_x + spacing &&
           max_x + spacing > other.min_x &&
           min_y < other.max_y + spacing &&
           max_y + spacing > other.min_y;
}

double Sheet::area() const {
    return width * height;
}

double Part::area() const {
    return polygon_area(geometry);
}

Bounds Part::bounds() const {
    return polygon_bounds(geometry);
}

Bounds PlacedPart::bounds() const {
    return polygon_bounds(geometry);
}

int NestingResult::sheet_count() const {
    int count = 0;
    for (const auto& placement : placements) {
        count = std::max(count, placement.sheet_index + 1);
    }
    return count;
}

double NestingResult::used_area() const {
    double total = 0.0;
    for (const auto& placement : placements) {
        total += placement.source_area;
    }
    return total;
}

double NestingResult::material_area() const {
    return sheet.area() * sheet_count();
}

double NestingResult::utilization() const {
    const double material = material_area();
    if (material <= 0.0) {
        return 0.0;
    }
    return used_area() / material;
}

double NestingResult::scrap_area() const {
    return material_area() - used_area();
}

double distance(Point a, Point b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

double signed_ring_area(const std::vector<Point>& ring) {
    if (ring.size() < 3) {
        return 0.0;
    }

    double total = 0.0;
    for (size_t i = 0; i + 1 < ring.size(); ++i) {
        total += ring[i].x * ring[i + 1].y - ring[i + 1].x * ring[i].y;
    }
    return total * 0.5;
}

double ring_area(const std::vector<Point>& ring) {
    return std::abs(signed_ring_area(ensure_closed(ring)));
}

double polygon_area(const Polygon& polygon) {
    double area = ring_area(polygon.outer);
    for (const auto& hole : polygon.holes) {
        area -= ring_area(hole);
    }
    return std::max(0.0, area);
}

Bounds ring_bounds(const std::vector<Point>& ring) {
    Bounds bounds;
    for (Point point : ring) {
        bounds.min_x = std::min(bounds.min_x, point.x);
        bounds.min_y = std::min(bounds.min_y, point.y);
        bounds.max_x = std::max(bounds.max_x, point.x);
        bounds.max_y = std::max(bounds.max_y, point.y);
    }
    return bounds;
}

Bounds polygon_bounds(const Polygon& polygon) {
    return ring_bounds(polygon.outer);
}

std::vector<Point> ensure_closed(std::vector<Point> ring) {
    if (ring.empty()) {
        return ring;
    }
    if (!points_equal(ring.front(), ring.back())) {
        ring.push_back(ring.front());
    }
    return ring;
}

Polygon normalize_to_origin(const Polygon& polygon) {
    const Bounds bounds = polygon_bounds(polygon);
    return translate_polygon(polygon, -bounds.min_x, -bounds.min_y);
}

Polygon rotate_to_origin(const Polygon& polygon, double degrees) {
    Polygon rotated;
    rotated.outer = transform_ring(polygon.outer, degrees, 0.0, 0.0);
    for (const auto& hole : polygon.holes) {
        rotated.holes.push_back(transform_ring(hole, degrees, 0.0, 0.0));
    }
    return normalize_to_origin(rotated);
}

Polygon scale_polygon(const Polygon& polygon, double factor) {
    Polygon scaled;
    scaled.outer.reserve(polygon.outer.size());
    for (Point point : polygon.outer) {
        scaled.outer.push_back({point.x * factor, point.y * factor});
    }
    for (const auto& hole : polygon.holes) {
        std::vector<Point> scaled_hole;
        scaled_hole.reserve(hole.size());
        for (Point point : hole) {
            scaled_hole.push_back({point.x * factor, point.y * factor});
        }
        scaled.holes.push_back(std::move(scaled_hole));
    }
    return scaled;
}

Polygon translate_polygon(const Polygon& polygon, double dx, double dy) {
    Polygon translated;
    translated.outer.reserve(polygon.outer.size());
    for (Point point : polygon.outer) {
        translated.outer.push_back({point.x + dx, point.y + dy});
    }
    for (const auto& hole : polygon.holes) {
        std::vector<Point> translated_hole;
        translated_hole.reserve(hole.size());
        for (Point point : hole) {
            translated_hole.push_back({point.x + dx, point.y + dy});
        }
        translated.holes.push_back(std::move(translated_hole));
    }
    return translated;
}

bool point_in_ring(Point point, const std::vector<Point>& raw_ring) {
    const auto ring = ensure_closed(raw_ring);
    bool inside = false;
    for (size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
        const Point a = ring[i];
        const Point b = ring[j];
        if (on_segment(a, b, point)) {
            return true;
        }
        const bool crosses = ((a.y > point.y) != (b.y > point.y)) &&
            (point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y + kEpsilon) + a.x);
        if (crosses) {
            inside = !inside;
        }
    }
    return inside;
}

bool polygon_intersects(const Polygon& first, const Polygon& second) {
    const Bounds first_bounds = polygon_bounds(first);
    const Bounds second_bounds = polygon_bounds(second);
    if (!first_bounds.overlaps(second_bounds, 0.0)) {
        return false;
    }

    const auto first_rings = boundary_rings(first);
    const auto second_rings = boundary_rings(second);
    for (const auto* first_ring : first_rings) {
        for (const auto* second_ring : second_rings) {
            if (rings_intersect(*first_ring, *second_ring)) {
                return true;
            }
        }
    }

    return material_point_inside(first, second) ||
           material_point_inside(second, first);
}

double polygon_distance(const Polygon& first, const Polygon& second) {
    if (polygon_intersects(first, second)) {
        return 0.0;
    }

    double best = std::numeric_limits<double>::infinity();
    const auto first_rings = boundary_rings(first);
    const auto second_rings = boundary_rings(second);
    for (const auto* first_ring : first_rings) {
        for (const auto* second_ring : second_rings) {
            best = std::min(best, ring_distance(*first_ring, *second_ring, best));
        }
    }
    return best;
}

}  // namespace smn
