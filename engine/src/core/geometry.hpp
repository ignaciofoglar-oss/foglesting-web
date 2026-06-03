#pragma once

#include <limits>
#include <string>
#include <vector>

namespace smn {

struct Point {
    double x = 0.0;
    double y = 0.0;
};

struct Bounds {
    double min_x = std::numeric_limits<double>::infinity();
    double min_y = std::numeric_limits<double>::infinity();
    double max_x = -std::numeric_limits<double>::infinity();
    double max_y = -std::numeric_limits<double>::infinity();

    double width() const;
    double height() const;
    bool valid() const;
    bool overlaps(const Bounds& other, double spacing = 0.0) const;
};

struct Polygon {
    std::vector<Point> outer;
    std::vector<std::vector<Point>> holes;
};

struct Sheet {
    double width = 3000.0;
    double height = 1500.0;

    double area() const;
};

struct Part {
    std::string name;
    Polygon geometry;
    int quantity = 1;
    bool can_rotate = true;
    std::string source;

    double area() const;
    Bounds bounds() const;
};

struct PlacedPart {
    std::string name;
    std::string source;
    Polygon geometry;
    int sheet_index = 0;
    int instance_number = 1;
    double x = 0.0;
    double y = 0.0;
    double rotation = 0.0;
    double source_area = 0.0;

    Bounds bounds() const;
};

struct NestingResult {
    Sheet sheet;
    std::vector<PlacedPart> placements;
    std::vector<Part> unplaced;

    int sheet_count() const;
    double used_area() const;
    double material_area() const;
    double utilization() const;
    double scrap_area() const;
};

double distance(Point a, Point b);
double signed_ring_area(const std::vector<Point>& ring);
double ring_area(const std::vector<Point>& ring);
double polygon_area(const Polygon& polygon);
Bounds ring_bounds(const std::vector<Point>& ring);
Bounds polygon_bounds(const Polygon& polygon);

std::vector<Point> ensure_closed(std::vector<Point> ring);
Polygon normalize_to_origin(const Polygon& polygon);
Polygon rotate_to_origin(const Polygon& polygon, double degrees);
Polygon scale_polygon(const Polygon& polygon, double factor);
Polygon translate_polygon(const Polygon& polygon, double dx, double dy);

bool point_in_ring(Point point, const std::vector<Point>& ring);
bool polygon_intersects(const Polygon& first, const Polygon& second);
double polygon_distance(const Polygon& first, const Polygon& second);

}  // namespace smn
