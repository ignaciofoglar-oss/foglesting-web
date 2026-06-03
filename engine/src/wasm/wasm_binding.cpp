#include "core/dxf.hpp"
#include "core/nesting.hpp"
#include "core/geometry.hpp"

#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>
#include <limits>

using namespace emscripten;

// Helper: build a JSON array of points
static std::string points_to_json(const std::vector<smn::Point>& pts) {
    std::ostringstream ss;
    ss << "[";
    for (size_t i = 0; i < pts.size(); ++i) {
        if (i > 0) ss << ",";
        ss << "[" << pts[i].x << "," << pts[i].y << "]";
    }
    ss << "]";
    return ss.str();
}

// Helper: escape string for JSON
static std::string json_escape(const std::string& s) {
    std::string result;
    for (char c : s) {
        if (c == '"') result += "\\\"";
        else if (c == '\\') result += "\\\\";
        else if (c == '\n') result += "\\n";
        else if (c == '\r') result += "\\r";
        else result += c;
    }
    return result;
}

// Helper: Build JSON for placements only (for preview)
static std::string build_placements_json(const smn::NestingResult& result) {
    std::ostringstream json;
    json << "[";
    for (size_t i = 0; i < result.placements.size(); ++i) {
        if (i > 0) json << ",";
        const auto& p = result.placements[i];
        json << "{";
        json << "\"sheet_index\": " << p.sheet_index << ",";
        json << "\"name\": \"" << json_escape(p.name) << "\",";
        json << "\"source\": \"" << json_escape(p.source) << "\",";
        json << "\"x\": " << p.x << ",";
        json << "\"y\": " << p.y << ",";
        json << "\"rotation\": " << p.rotation << ",";
        json << "\"area\": " << p.source_area << ",";
        json << "\"outer\": " << points_to_json(p.geometry.outer);
        if (!p.geometry.holes.empty()) {
            json << ", \"holes\": [";
            for (size_t h = 0; h < p.geometry.holes.size(); ++h) {
                if (h > 0) json << ",";
                json << points_to_json(p.geometry.holes[h]);
            }
            json << "]";
        }
        json << "}";
    }
    json << "]";
    return json.str();
}

// Run nesting with multiple input files
// quantities_str: comma separated quantities, e.g. "200,10"
std::string run_nesting_wasm(
    std::string quantities_str,
    double sheet_width,
    double sheet_height,
    int population,
    int iterations,
    double mutation_rate,
    double shuffle_prob,
    double spacing,
    int rotations,
    std::string optimization_type,
    emscripten::val progress_callback
) {
    try {
        std::cout << "Running nesting from WebAssembly! Quantities: " << quantities_str << std::endl;
        
        smn::Sheet sheet;
        sheet.width = sheet_width;
        sheet.height = sheet_height;

        smn::DxfLoadOptions load_opts;
        std::vector<smn::Part> parts;
        
        std::vector<int> quantities;
        std::stringstream ss(quantities_str);
        std::string item;
        while (std::getline(ss, item, ',')) {
            try { quantities.push_back(std::stoi(item)); } catch(...) { quantities.push_back(1); }
        }

        for (size_t i = 0; i < quantities.size(); ++i) {
            std::vector<std::filesystem::path> single_path = {"input_" + std::to_string(i) + ".dxf"};
            auto single_part = smn::load_dxf_parts(single_path, load_opts);
            int qty = quantities[i];
            for(int q = 0; q < qty; q++) {
                for(auto& p : single_part) {
                    parts.push_back(p);
                }
            }
        }
        
        if (parts.empty()) {
            return "{\"error\": \"No se encontraron piezas válidas en los archivos DXF.\"}";
        }
        
        std::cout << "Loaded " << parts.size() << " total parts from " << quantities.size() << " unique files." << std::endl;
        
        smn::IterativeNestingOptions opts;
        opts.base.spacing = spacing;
        
        if (rotations <= 1) {
            opts.base.rotations = {0.0};
        } else {
            double step = 360.0 / rotations;
            opts.base.rotations.clear();
            for (int i = 0; i < rotations; ++i) {
                opts.base.rotations.push_back(i * step);
            }
        }
        
        opts.base.solver_mode = smn::SolverMode::CPU;
        opts.base.optimization_type =
            optimization_type == "bounding-box"
                ? smn::OptimizationType::BoundingBox
                : smn::OptimizationType::CompactArea;
        opts.base.optimization_ratio = 0.5;
        opts.base.use_nfp_candidates = true;
        opts.base.use_compaction = true;
        opts.iterations = iterations;
        opts.ga_population = population;
        opts.ga_mutation_rate = mutation_rate;
        opts.ga_random_shuffle_prob = shuffle_prob;
        
        // --- SEND INPUT PREVIEW FIRST ---
        if (!progress_callback.isUndefined() && !progress_callback.isNull()) {
            std::stringstream pjson;
            pjson << "{\"type\": \"input_preview\", \"input_parts\": [";
            for (size_t i = 0; i < parts.size(); ++i) {
                if (i > 0) pjson << ",";
                const auto& part = parts[i];
                auto normalized = smn::normalize_to_origin(part.geometry);
                auto bounds = smn::polygon_bounds(normalized);
                pjson << "{";
                pjson << "\"name\": \"" << json_escape(part.name) << "\",";
                pjson << "\"source\": \"" << json_escape(part.source) << "\",";
                pjson << "\"quantity\": " << part.quantity << ",";
                pjson << "\"width\": " << bounds.width() << ",";
                pjson << "\"height\": " << bounds.height() << ",";
                pjson << "\"area\": " << part.area() << ",";
                pjson << "\"outer\": " << points_to_json(normalized.outer);
                if (!normalized.holes.empty()) {
                    pjson << ", \"holes\": [";
                    for (size_t h = 0; h < normalized.holes.size(); ++h) {
                        if (h > 0) pjson << ",";
                        pjson << points_to_json(normalized.holes[h]);
                    }
                    pjson << "]";
                }
                pjson << "}";
            }
            pjson << "]}";
            progress_callback(emscripten::val(pjson.str()));
        }

        // Iteration callback for LIVE PREVIEW
        smn::IterationCallback cb = nullptr;
        if (!progress_callback.isUndefined() && !progress_callback.isNull()) {
            cb = [&progress_callback, last_preview_area = std::numeric_limits<double>::infinity()](
                const smn::IterationStats& stats,
                const smn::NestingResult& cb_current,
                const smn::NestingResult& cb_global
            ) mutable {
                const bool is_best_preview =
                    stats.iteration == 1 ||
                    stats.best_occupied_area < last_preview_area - 1e-6;
                if (is_best_preview) {
                    last_preview_area = stats.best_occupied_area;
                }

                std::ostringstream json;
                json << "{";
                json << "\"iteration\": " << stats.iteration << ",";
                json << "\"utilization\": " << (cb_global.utilization() * 100.0) << ",";
                json << "\"placed\": " << cb_global.placements.size() << ",";
                json << "\"unplaced\": " << cb_global.unplaced.size() << ",";
                json << "\"sheets\": " << cb_global.sheet_count() << ",";
                json << "\"is_best\": " << (is_best_preview ? "true" : "false");
                if (is_best_preview) {
                    json << ",\"placements\": " << build_placements_json(cb_global);
                }
                json << "}";
                
                // Call JS function
                progress_callback(json.str());
                return true; // continue iterations
            };
        }

        auto iterative_result = smn::nest_parts_iterative(sheet, parts, opts, cb);
        
        // Write output DXF
        smn::write_nesting_dxf(iterative_result.best, "output.dxf", 250.0, {}, 4, false);
        
        // Build detailed JSON response
        std::ostringstream json;
        json << "{";
        json << "\"placed\": " << iterative_result.best.placements.size() << ",";
        json << "\"unplaced\": " << iterative_result.best.unplaced.size() << ",";
        json << "\"sheets\": " << iterative_result.best.sheet_count() << ",";
        json << "\"utilization\": " << (iterative_result.best.utilization() * 100.0) << ",";
        json << "\"sheet_width\": " << sheet.width << ",";
        json << "\"sheet_height\": " << sheet.height << ",";
        
        // INPUT PARTS geometry (for input DXF preview)
        json << "\"input_parts\": [";
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) json << ",";
            const auto& part = parts[i];
            auto normalized = smn::normalize_to_origin(part.geometry);
            auto bounds = smn::polygon_bounds(normalized);
            
            json << "{";
            json << "\"name\": \"" << json_escape(part.name) << "\",";
            json << "\"source\": \"" << json_escape(part.source) << "\",";
            json << "\"quantity\": " << part.quantity << ",";
            json << "\"width\": " << bounds.width() << ",";
            json << "\"height\": " << bounds.height() << ",";
            json << "\"area\": " << part.area() << ",";
            json << "\"outer\": " << points_to_json(normalized.outer);
            if (!normalized.holes.empty()) {
                json << ", \"holes\": [";
                for (size_t h = 0; h < normalized.holes.size(); ++h) {
                    if (h > 0) json << ",";
                    json << points_to_json(normalized.holes[h]);
                }
                json << "]";
            }
            json << "}";
        }
        json << "],";
        
        // OUTPUT PLACEMENTS geometry (for output DXF preview)
        json << "\"placements\": " << build_placements_json(iterative_result.best);
        
        json << "}";
        
        return json.str();
    } catch (const std::exception& e) {
        std::string err = e.what();
        return "{\"error\": \"" + json_escape(err) + "\"}";
    } catch (...) {
        return "{\"error\": \"Error desconocido durante el nesting\"}";
    }
}

// Bindings
EMSCRIPTEN_BINDINGS(nesting_module) {
    function("run_nesting_wasm", &run_nesting_wasm);
}
