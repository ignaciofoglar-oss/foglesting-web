#pragma once

#include "core/geometry.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace smn {

struct DxfLoadOptions {
    int quantity = 1;
    double flattening_distance = 2.0;
    double endpoint_tolerance = 0.2;
    double min_part_area = 1.0;
};

struct TextAnnotation {
    double x;
    double y;
    std::string text;
};

enum class DraftEntityType {
    Line,
    Rectangle,
    Circle,
    Dimension,
    Text,
    Arc,
    Polyline,
};

struct DraftEntity {
    DraftEntityType type = DraftEntityType::Line;
    Point start;
    Point end;
    Point mid;  // used for 3-point arcs
    std::string text;
    std::vector<Point> points;  // used for polylines
};

std::vector<std::filesystem::path> expand_dxf_inputs(const std::vector<std::filesystem::path>& inputs);
std::vector<Part> load_dxf_parts(const std::vector<std::filesystem::path>& inputs, const DxfLoadOptions& options);
void write_nesting_dxf(
    const NestingResult& result,
    const std::filesystem::path& output_path,
    double sheet_gap = 250.0,
    const std::vector<TextAnnotation>& text_annotations = {},
    int insunits = 4,
    bool merge_common_lines = false,
    const std::vector<DraftEntity>& draft_entities = {}
);

}  // namespace smn
