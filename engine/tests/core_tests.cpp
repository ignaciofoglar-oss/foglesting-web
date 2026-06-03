#include "core/dxf.hpp"
#include "core/nesting.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {

bool close_to(double actual, double expected, double tolerance = 1e-6) {
    return std::abs(actual - expected) <= tolerance;
}

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::exit(1);
    }
}

smn::Part rectangle_part(const std::string& name, double width, double height) {
    smn::Part part;
    part.name = name;
    part.geometry.outer = {
        {0.0, 0.0},
        {width, 0.0},
        {width, height},
        {0.0, height},
        {0.0, 0.0},
    };
    return part;
}

smn::Part frame_part(const std::string& name, double width, double height, double hole_x, double hole_y, double hole_w, double hole_h) {
    smn::Part part = rectangle_part(name, width, height);
    part.geometry.holes.push_back({
        {hole_x, hole_y},
        {hole_x + hole_w, hole_y},
        {hole_x + hole_w, hole_y + hole_h},
        {hole_x, hole_y + hole_h},
        {hole_x, hole_y},
    });
    return part;
}

void test_geometry_area() {
    const auto part = rectangle_part("panel", 100.0, 50.0);
    require(close_to(part.area(), 5000.0), "rectangle area");
    const auto bounds = part.bounds();
    require(close_to(bounds.width(), 100.0), "rectangle width");
    require(close_to(bounds.height(), 50.0), "rectangle height");
}

void test_nesting_places_without_overlap() {
    smn::Sheet sheet{100.0, 50.0};
    std::vector<smn::Part> parts = {
        rectangle_part("a", 40.0, 50.0),
        rectangle_part("b", 60.0, 50.0),
    };

    smn::NestingOptions options;
    options.spacing = 0.0;
    options.rotations = {0.0};

    const auto result = smn::nest_parts(sheet, parts, options);
    require(result.sheet_count() == 1, "single sheet");
    require(result.placements.size() == 2, "two placements");
    require(!smn::polygon_intersects(result.placements[0].geometry, result.placements[1].geometry), "no overlap");
}

void test_nesting_can_use_internal_cutouts() {
    smn::Sheet sheet{100.0, 100.0};
    std::vector<smn::Part> parts = {
        frame_part("frame", 100.0, 100.0, 25.0, 25.0, 50.0, 50.0),
        rectangle_part("insert", 40.0, 40.0),
    };

    smn::NestingOptions options;
    options.spacing = 5.0;
    options.rotations = {0.0};

    const auto result = smn::nest_parts(sheet, parts, options);
    require(result.sheet_count() == 1, "cutout single sheet");
    require(result.placements.size() == 2, "cutout places both parts");
    require(!smn::polygon_intersects(result.placements[0].geometry, result.placements[1].geometry), "cutout no material overlap");
    require(smn::polygon_distance(result.placements[0].geometry, result.placements[1].geometry) >= 5.0 - 1e-6, "cutout spacing");
}

void test_random_solver_places_valid_layout() {
    smn::Sheet sheet{500.0, 300.0};
    std::vector<smn::Part> parts = {
        rectangle_part("a", 80.0, 50.0),
        rectangle_part("b", 60.0, 90.0),
        rectangle_part("c", 120.0, 35.0),
    };

    smn::NestingOptions options;
    options.solver_mode = smn::SolverMode::Random;
    options.spacing = 3.0;
    options.rotations = {0.0, 90.0};
    options.placement_variant = 17;
    options.use_compaction = true;

    const auto result = smn::nest_parts(sheet, parts, options);
    require(result.sheet_count() == 1, "random solver single sheet");
    require(result.placements.size() == 3, "random solver places all parts");
    require(result.unplaced.empty(), "random solver no unplaced parts");
    require(smn::find_nesting_violations(result, options.spacing).empty(), "random solver no overlaps");
}

void test_repeated_pattern_places_large_identical_quantity() {
    smn::Sheet sheet{100.0, 100.0};
    smn::Part part = rectangle_part("repeat", 10.0, 10.0);
    part.quantity = 300;

    smn::NestingOptions options;
    options.spacing = 0.0;
    options.rotations = {0.0, 90.0};

    const auto result = smn::nest_parts(sheet, {part}, options);
    require(result.placements.size() == 300, "repeated pattern places every copy");
    require(result.sheet_count() == 3, "repeated pattern repeats full sheets");
    require(result.unplaced.empty(), "repeated pattern no unplaced parts");
    require(smn::find_nesting_violations(result, options.spacing).empty(), "repeated pattern no overlaps");
}

void test_repeated_pattern_seeds_large_group_before_other_parts() {
    smn::Sheet sheet{100.0, 100.0};
    smn::Part repeated = rectangle_part("repeat", 10.0, 10.0);
    repeated.quantity = 300;
    smn::Part extra = rectangle_part("extra", 15.0, 15.0);

    smn::NestingOptions options;
    options.spacing = 0.0;
    options.rotations = {0.0, 90.0};

    const auto result = smn::nest_parts(sheet, {repeated, extra}, options);
    require(result.placements.size() == 301, "mixed repeated seed places every copy and extra");
    require(result.unplaced.empty(), "mixed repeated seed no unplaced parts");
    require(smn::find_nesting_violations(result, options.spacing).empty(), "mixed repeated seed no overlaps");
}

void test_iterative_repeated_pattern_seeds_large_group_before_other_parts() {
    smn::Sheet sheet{100.0, 100.0};
    smn::Part repeated = rectangle_part("repeat", 10.0, 10.0);
    repeated.quantity = 250;
    smn::Part extra = rectangle_part("extra", 15.0, 15.0);

    smn::IterativeNestingOptions options;
    options.base.spacing = 0.0;
    options.base.rotations = {0.0, 90.0};
    options.iterations = 20;

    const auto result = smn::nest_parts_iterative(sheet, {repeated, extra}, options);
    require(result.history.size() == 1, "mixed repeated iterative uses fast seeded pass");
    require(result.best.placements.size() == 251, "mixed repeated iterative places every copy and extra");
    require(result.best.unplaced.empty(), "mixed repeated iterative no unplaced parts");
    require(smn::find_nesting_violations(result.best, options.base.spacing).empty(), "mixed repeated iterative no overlaps");
}

void test_geometry_rejects_hole_boundary_overlap() {
    const auto frame = frame_part("frame", 100.0, 100.0, 25.0, 25.0, 50.0, 50.0);
    const auto safe_insert = smn::translate_polygon(rectangle_part("safe", 40.0, 40.0).geometry, 30.0, 30.0);
    const auto crossing_insert = smn::translate_polygon(rectangle_part("crossing", 60.0, 60.0).geometry, 20.0, 20.0);
    const auto material_strip = smn::translate_polygon(rectangle_part("strip", 60.0, 10.0).geometry, 20.0, 10.0);

    require(!smn::polygon_intersects(frame.geometry, safe_insert), "part can sit inside a cutout without overlap");
    require(close_to(smn::polygon_distance(frame.geometry, safe_insert), 5.0), "distance is measured from hole edge");
    require(smn::polygon_intersects(frame.geometry, crossing_insert), "crossing a hole boundary is overlap");
    require(smn::polygon_intersects(frame.geometry, material_strip), "material inside frame border is overlap");
}

void test_result_validation_reports_overlap() {
    const auto frame = frame_part("frame", 100.0, 100.0, 25.0, 25.0, 50.0, 50.0);
    const auto crossing_insert = smn::translate_polygon(rectangle_part("crossing", 60.0, 60.0).geometry, 20.0, 20.0);

    smn::NestingResult result;
    result.sheet = {150.0, 150.0};
    result.placements.push_back({"frame", "", frame.geometry, 0, 1, 0.0, 0.0, 0.0, frame.area()});
    result.placements.push_back({"crossing", "", crossing_insert, 0, 1, 20.0, 20.0, 0.0, smn::polygon_area(crossing_insert)});

    const auto violations = smn::find_nesting_violations(result, 5.0);
    require(violations.size() == 1, "result validation detects overlap");
    require(violations[0].intersects, "result validation reports intersection");
}

void test_iterative_nesting_records_history() {
    smn::Sheet sheet{120.0, 80.0};
    std::vector<smn::Part> parts = {
        rectangle_part("a", 60.0, 30.0),
        rectangle_part("b", 40.0, 50.0),
        rectangle_part("c", 30.0, 30.0),
    };

    smn::IterativeNestingOptions options;
    options.base.spacing = 0.0;
    options.base.rotations = {0.0, 90.0};
    options.iterations = 8;

    const auto result = smn::nest_parts_iterative(sheet, parts, options);
    require(result.history.size() == 8, "iterative history length");
    require(!result.best.placements.empty(), "iterative best placements");
}

void test_dxf_line_profile_loads() {
    const auto temp = std::filesystem::temp_directory_path() / "smn_cpp_line_panel.dxf";
    {
        std::ofstream output(temp);
        output
            << "0\nSECTION\n2\nENTITIES\n"
            << "0\nLINE\n10\n0\n20\n0\n11\n100\n21\n0\n"
            << "0\nLINE\n10\n100\n20\n0\n11\n100\n21\n50\n"
            << "0\nLINE\n10\n100\n20\n50\n11\n0\n21\n50\n"
            << "0\nLINE\n10\n0\n20\n50\n11\n0\n21\n0\n"
            << "0\nENDSEC\n0\nEOF\n";
    }

    smn::DxfLoadOptions options;
    const auto parts = smn::load_dxf_parts({temp}, options);
    require(parts.size() == 1, "one loaded part");
    require(close_to(parts[0].bounds().width(), 100.0), "loaded width");
    require(close_to(parts[0].bounds().height(), 50.0), "loaded height");
    std::filesystem::remove(temp);
}

void write_lwpolyline(std::ofstream& output, const std::vector<smn::Point>& points) {
    output << "0\nLWPOLYLINE\n70\n1\n";
    for (const auto& point : points) {
        output << "10\n" << point.x << "\n20\n" << point.y << "\n";
    }
}

void write_line(std::ofstream& output, smn::Point start, smn::Point end) {
    output << "0\nLINE\n"
           << "10\n" << start.x << "\n20\n" << start.y << "\n"
           << "11\n" << end.x << "\n21\n" << end.y << "\n";
}

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

size_t count_occurrences(const std::string& text, const std::string& needle) {
    size_t count = 0;
    size_t position = 0;
    while ((position = text.find(needle, position)) != std::string::npos) {
        ++count;
        position += needle.size();
    }
    return count;
}

void test_dxf_export_merges_common_lines() {
    smn::NestingResult result;
    result.sheet = {100.0, 50.0};
    const auto left = rectangle_part("left", 10.0, 10.0);
    const auto right = rectangle_part("right", 10.0, 10.0);
    result.placements.push_back({"left", "", left.geometry, 0, 1, 0.0, 0.0, 0.0, left.area()});
    result.placements.push_back({"right", "", smn::translate_polygon(right.geometry, 10.0, 0.0), 0, 1, 10.0, 0.0, 0.0, right.area()});

    const auto temp = std::filesystem::temp_directory_path() / "smn_cpp_common_lines.dxf";
    smn::write_nesting_dxf(result, temp, 250.0, {}, 4, true);
    const std::string dxf = read_text_file(temp);

    require(count_occurrences(dxf, "\nLINE\r\n") == 5, "common-line export writes the shared cut once");
    require(count_occurrences(dxf, "\nPOLYLINE\r\n") == 1, "common-line export keeps only sheet polyline");
    std::filesystem::remove(temp);
}

void test_dxf_export_writes_draft_entities() {
    smn::NestingResult result;
    result.sheet = {100.0, 80.0};
    const auto panel = rectangle_part("panel", 20.0, 20.0);
    result.placements.push_back({"panel", "", panel.geometry, 0, 1, 0.0, 0.0, 0.0, panel.area()});

    const auto temp = std::filesystem::temp_directory_path() / "smn_cpp_draft_entities.dxf";
    std::vector<smn::DraftEntity> draft = {
        {smn::DraftEntityType::Line, {30.0, 10.0}, {60.0, 10.0}, {0.0, 0.0}, "", {}},
        {smn::DraftEntityType::Circle, {45.0, 40.0}, {55.0, 40.0}, {0.0, 0.0}, "", {}},
        {smn::DraftEntityType::Text, {10.0, 70.0}, {10.0, 70.0}, {0.0, 0.0}, "nota", {}},
        {smn::DraftEntityType::Dimension, {0.0, 65.0}, {25.0, 65.0}, {0.0, 0.0}, "", {}},
    };
    smn::write_nesting_dxf(result, temp, 250.0, {}, 4, false, draft);
    const std::string dxf = read_text_file(temp);

    require(count_occurrences(dxf, "\nLINE\r\n") >= 2, "draft export writes lines and dimensions");
    require(count_occurrences(dxf, "\nCIRCLE\r\n") == 1, "draft export writes circles");
    require(count_occurrences(dxf, "\nTEXT\r\n") >= 2, "draft export writes text and dimension labels");
    std::filesystem::remove(temp);
}

void test_dxf_repairs_nearly_closed_line_chain() {
    const auto temp = std::filesystem::temp_directory_path() / "smn_cpp_nearly_closed_chain.dxf";
    {
        std::ofstream output(temp);
        output << "0\nSECTION\n2\nENTITIES\n";
        write_line(output, {0.0, 0.0}, {60.0, 0.0});
        write_line(output, {60.0, 0.0}, {60.0, 40.0});
        write_line(output, {60.0, 40.0}, {0.0, 40.0});
        write_line(output, {0.0, 40.0}, {0.0, 3.0});
        output << "0\nENDSEC\n0\nEOF\n";
    }

    smn::DxfLoadOptions options;
    options.endpoint_tolerance = 0.2;
    const auto parts = smn::load_dxf_parts({temp}, options);
    require(parts.size() == 1, "nearly closed line chain is repaired");
    require(close_to(parts[0].bounds().width(), 60.0), "repaired chain width");
    require(close_to(parts[0].bounds().height(), 40.0), "repaired chain height");
    std::filesystem::remove(temp);
}

void test_dxf_nested_contours_become_hole() {
    const auto temp = std::filesystem::temp_directory_path() / "smn_cpp_nested_octagon.dxf";
    {
        std::ofstream output(temp);
        output << "0\nSECTION\n2\nENTITIES\n";
        write_lwpolyline(output, {
            {30.0, 0.0}, {70.0, 0.0}, {100.0, 30.0}, {100.0, 70.0},
            {70.0, 100.0}, {30.0, 100.0}, {0.0, 70.0}, {0.0, 30.0},
        });
        write_lwpolyline(output, {
            {40.0, 25.0}, {60.0, 25.0}, {75.0, 40.0}, {75.0, 60.0},
            {60.0, 75.0}, {40.0, 75.0}, {25.0, 60.0}, {25.0, 40.0},
        });
        output << "0\nENDSEC\n0\nEOF\n";
    }

    smn::DxfLoadOptions options;
    const auto parts = smn::load_dxf_parts({temp}, options);
    require(parts.size() == 1, "nested contours load as one part");
    require(parts[0].geometry.holes.size() == 1, "nested contour is a hole");
    require(close_to(parts[0].bounds().width(), 100.0), "nested width");
    require(close_to(parts[0].bounds().height(), 100.0), "nested height");
    std::filesystem::remove(temp);
}

void test_dxf_deep_nested_cutouts_stay_with_outer_part() {
    const auto temp = std::filesystem::temp_directory_path() / "smn_cpp_deep_nested_cutouts.dxf";
    {
        std::ofstream output(temp);
        output << "0\nSECTION\n2\nENTITIES\n";
        write_lwpolyline(output, {
            {0.0, 0.0}, {100.0, 0.0}, {100.0, 80.0}, {0.0, 80.0},
        });
        write_lwpolyline(output, {
            {20.0, 20.0}, {80.0, 20.0}, {80.0, 60.0}, {20.0, 60.0},
        });
        write_lwpolyline(output, {
            {40.0, 30.0}, {60.0, 30.0}, {60.0, 50.0}, {40.0, 50.0},
        });
        output << "0\nENDSEC\n0\nEOF\n";
    }

    smn::DxfLoadOptions options;
    const auto parts = smn::load_dxf_parts({temp}, options);
    require(parts.size() == 1, "deep nested cutouts load as one part");
    require(parts[0].geometry.holes.size() == 2, "all inner cut paths stay attached to outer part");
    std::filesystem::remove(temp);
}

void test_dxf_legacy_polyline_loads() {
    const auto temp = std::filesystem::temp_directory_path() / "smn_cpp_legacy_polyline.dxf";
    {
        std::ofstream output(temp);
        output << "0\nSECTION\n2\nENTITIES\n"
               << "0\nPOLYLINE\n70\n1\n"
               << "0\nVERTEX\n10\n0\n20\n0\n"
               << "0\nVERTEX\n10\n90\n20\n0\n"
               << "0\nVERTEX\n10\n90\n20\n35\n"
               << "0\nVERTEX\n10\n0\n20\n35\n"
               << "0\nSEQEND\n"
               << "0\nENDSEC\n0\nEOF\n";
    }

    smn::DxfLoadOptions options;
    const auto parts = smn::load_dxf_parts({temp}, options);
    require(parts.size() == 1, "legacy POLYLINE loads as one part");
    require(close_to(parts[0].bounds().width(), 90.0), "legacy POLYLINE width");
    require(close_to(parts[0].bounds().height(), 35.0), "legacy POLYLINE height");
    std::filesystem::remove(temp);
}

void test_dxf_lwpolyline_bulge_loads() {
    const auto temp = std::filesystem::temp_directory_path() / "smn_cpp_lwpolyline_bulge.dxf";
    {
        std::ofstream output(temp);
        output << "0\nSECTION\n2\nENTITIES\n"
               << "0\nLWPOLYLINE\n70\n1\n"
               << "10\n0\n20\n0\n42\n0.41421356237\n"
               << "10\n80\n20\n0\n"
               << "10\n80\n20\n40\n"
               << "10\n0\n20\n40\n"
               << "0\nENDSEC\n0\nEOF\n";
    }

    smn::DxfLoadOptions options;
    options.flattening_distance = 1.0;
    const auto parts = smn::load_dxf_parts({temp}, options);
    require(parts.size() == 1, "LWPOLYLINE bulge loads as one part");
    require(parts[0].geometry.outer.size() > 5, "LWPOLYLINE bulge is flattened into curve points");
    require(parts[0].area() > 1000.0, "LWPOLYLINE bulge area");
    std::filesystem::remove(temp);
}

void test_dxf_insert_block_loads() {
    const auto temp = std::filesystem::temp_directory_path() / "smn_cpp_insert_block.dxf";
    {
        std::ofstream output(temp);
        output << "0\nSECTION\n2\nBLOCKS\n"
               << "0\nBLOCK\n2\nPROFILE\n10\n0\n20\n0\n"
               << "0\nLINE\n10\n0\n20\n0\n11\n100\n21\n0\n"
               << "0\nLINE\n10\n100\n20\n0\n11\n100\n21\n50\n"
               << "0\nLINE\n10\n100\n20\n50\n11\n0\n21\n50\n"
               << "0\nLINE\n10\n0\n20\n50\n11\n0\n21\n0\n"
               << "0\nENDBLK\n"
               << "0\nENDSEC\n"
               << "0\nSECTION\n2\nENTITIES\n"
               << "0\nINSERT\n2\nPROFILE\n10\n25\n20\n30\n"
               << "0\nENDSEC\n0\nEOF\n";
    }

    smn::DxfLoadOptions options;
    const auto parts = smn::load_dxf_parts({temp}, options);
    require(parts.size() == 1, "INSERT block loads as one part");
    require(close_to(parts[0].bounds().width(), 100.0), "INSERT block width");
    require(close_to(parts[0].bounds().height(), 50.0), "INSERT block height");
    std::filesystem::remove(temp);
}

void test_dxf_full_ellipse_loads() {
    const auto temp = std::filesystem::temp_directory_path() / "smn_cpp_full_ellipse.dxf";
    {
        std::ofstream output(temp);
        output << "0\nSECTION\n2\nENTITIES\n"
               << "0\nELLIPSE\n"
               << "10\n50\n20\n25\n"
               << "11\n50\n21\n0\n"
               << "40\n0.5\n"
               << "41\n0\n42\n6.283185307179586\n"
               << "0\nENDSEC\n0\nEOF\n";
    }

    smn::DxfLoadOptions options;
    options.flattening_distance = 1.0;
    const auto parts = smn::load_dxf_parts({temp}, options);
    require(parts.size() == 1, "full ELLIPSE loads as one part");
    require(close_to(parts[0].bounds().width(), 100.0, 0.25), "ELLIPSE width");
    require(close_to(parts[0].bounds().height(), 50.0, 0.25), "ELLIPSE height");
    std::filesystem::remove(temp);
}

}  // namespace

int main() {
    test_geometry_area();
    test_nesting_places_without_overlap();
    test_nesting_can_use_internal_cutouts();
    test_random_solver_places_valid_layout();
    test_repeated_pattern_places_large_identical_quantity();
    test_repeated_pattern_seeds_large_group_before_other_parts();
    test_iterative_repeated_pattern_seeds_large_group_before_other_parts();
    test_geometry_rejects_hole_boundary_overlap();
    test_result_validation_reports_overlap();
    test_iterative_nesting_records_history();
    test_dxf_line_profile_loads();
    test_dxf_repairs_nearly_closed_line_chain();
    test_dxf_export_merges_common_lines();
    test_dxf_export_writes_draft_entities();
    test_dxf_nested_contours_become_hole();
    test_dxf_deep_nested_cutouts_stay_with_outer_part();
    test_dxf_legacy_polyline_loads();
    test_dxf_lwpolyline_bulge_loads();
    test_dxf_insert_block_loads();
    test_dxf_full_ellipse_loads();
    std::cout << "core_tests passed\n";
    return 0;
}
