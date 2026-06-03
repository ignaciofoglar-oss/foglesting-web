#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <objidl.h>
#include <gdiplus.h>

#include "core/dxf.hpp"
#include "core/gpu_context.hpp"
#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>
#include <gdiplus.h>

#include "core/dxf.hpp"
#include "core/gpu_context.hpp"
#include "core/nesting.hpp"
#include "core/perf_log.hpp"

#include <commctrl.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <future>
#include <limits>
#include <iterator>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <string>

#include "core/dxf.hpp"
#include "core/gpu_context.hpp"
#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>
#include <gdiplus.h>

#include "core/dxf.hpp"
#include "core/gpu_context.hpp"
#include "core/nesting.hpp"
#include "core/perf_log.hpp"

#include <commctrl.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <future>
#include <limits>
#include <iterator>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <thread>
#include <chrono>
#include <cctype>
#include <cstring>
#include <vector>

namespace {

bool g_lang_en = false;
const char* T(const char* es, const char* en) {
    return g_lang_en ? en : es;
}

constexpr int kPanelWidth = 520;
constexpr int kMargin = 18;
constexpr int kBottomPanelHeight = 96;
constexpr int kBottomPanelMargin = 24;
constexpr int kBottomPanelGap = 16;
constexpr int kFileListDefaultHeight = 240;
constexpr int kFileListMinHeight = 140;
constexpr int kFileListSplitterHeight = 18;
constexpr int kLeftFileLabelY = 116;
constexpr int kLeftFileTop = 138;
constexpr int kLeftContentClipTop = 104;
constexpr int kAppIconResourceId = 1;
constexpr int kLogoPngResourceId = 2;
constexpr UINT_PTR kDimensionDeleteTimer = 401;
constexpr UINT kRunProgress = WM_APP + 1;
constexpr UINT kRunFinished = WM_APP + 2;
constexpr UINT kInputPreviewReady = WM_APP + 3;

const COLORREF kFoglarOrange = RGB(232, 91, 46);
const COLORREF kFoglarGreen = RGB(46, 101, 71);
const COLORREF kFoglarCream = RGB(255, 223, 184);
const COLORREF kFoglarCoal = RGB(38, 38, 38);
const COLORREF kFoglarDeep = RGB(21, 22, 22);
const COLORREF kFoglarPanel = RGB(31, 37, 35);
const COLORREF kFoglarInput = RGB(28, 32, 31);
const COLORREF kFoglarSteel = RGB(132, 143, 138);
const COLORREF kFoglarGrid = RGB(45, 54, 52);
const COLORREF kCadPartStroke = RGB(230, 226, 216);

enum ControlId {
    IdFileList = 100,
    IdAddFolder,
    IdAddFiles,
    IdImportCsv,
    IdRemoveFile,
    IdSelectedQuantity,
    IdApplyQuantity,
    IdOutput,
    IdBrowseOutput,
    IdDisplayInches,
    IdDisplayMm,
    IdSpacing,
    IdCurveTolerance,
    IdPartRotations,
    IdOptimizationType,
    IdRoughApprox,
    IdCpuCores,
    IdIterations,
    IdSvgScale,
    IdEndpointTolerance,
    IdDxfImportUnits,
    IdDxfExportUnits,
    IdMergeCommonLines,
    IdOptimizationRatio,
    IdGaPopulation,
    IdGaMutationRate,
    IdGaRandomShuffle,
    IdGaShuffleIntensity,
    IdSheetWidth,
    IdSheetHeight,
    IdSheetPriceKg,
    IdSheetDensity,
    IdSheetThickness,
    IdRun,
    IdPause,
    IdSaveDxf,
    IdLog,
    IdSolutionList,
    IdSolverMode,
    IdPerfLog,
    IdToolSelect,
    IdToolPan,
    IdToolFit,
    IdToolMeasure,
    IdToolLine,
    IdToolRectangle,
    IdToolCircle,
    IdToolArc,
    IdToolPolyline,
    IdToolText,
    IdToolDelete,
    IdToolClearAnnotations,
    IdToolSnap,
    IdToolUndo,
    IdFeedback,
    IdFileQuantityEdit,
};

enum class ToolMode {
    Select,
    Pan,
    Measure,
    Line,
    Rectangle,
    Circle,
    Arc,
    Polyline,
    Text,
    Delete
};

struct Annotation {
    enum class Type { Measure, Text, Line, Rectangle, Circle, Arc, Polyline };
    Type type;
    smn::Point start;
    smn::Point end;
    smn::Point mid;  // for 3-point arcs
    std::string text;
    std::vector<smn::Point> points;  // for polylines
    bool diameter_dimension = false;
    bool radius_dimension = false;
    int target_annotation_index = -1;
    double override_measure = 0.0;
    std::string text_font = "ISOCPEUR";
    double text_size = 16.0;
};

struct TextAnnotationOptions {
    bool accepted = false;
    std::string text;
    std::string font = "ISOCPEUR";
    double size = 16.0;
};

struct InfoTip {
    int id = 0;
    RECT rect{0, 0, 0, 0};
    std::string text;
    bool visible = true;
};

struct DxfSelection {
    std::filesystem::path path;
    int quantity = 1;
};

struct RunSettings {
    std::vector<DxfSelection> selections;
    std::vector<Annotation> editor_annotations;
    std::filesystem::path output;
    smn::Sheet sheet;
    smn::DxfLoadOptions load;
    smn::IterativeNestingOptions nesting;
    double import_scale = 1.0;
    double export_scale = 1.0;
    int cpu_cores = 1;
    smn::SolverMode solver_mode = smn::SolverMode::CPU;
    bool perf_log_enabled = false;
    bool merge_common_lines = true;
    bool rough_approximation = false;
    std::string optimization_type = "Bounding Box";
    double sheet_price_kg = 6500.0;
    double sheet_density = 8000.0;
    double sheet_thickness = 2.5;
};

struct RankedSolution {
    int iteration = 0;
    double occupied_area = 0.0;
    double saved_area = 0.0;
    double utilization = 0.0;
    double time_to_find = 0.0;
    int sheet_count = 0;
    int unplaced_count = 0;
    smn::NestingResult result;
    double scrap_area = 0.0;
};

struct AppState {
    HWND hwnd = nullptr;
    HWND file_list = nullptr;
    HWND file_quantity_edit = nullptr;
    int file_quantity_edit_index = -1;
    bool destroying_file_quantity_edit = false;
    HWND selected_quantity = nullptr;
    HWND output = nullptr;
    HWND sheet_width = nullptr;
    HWND sheet_height = nullptr;
    HWND sheet_price_kg = nullptr;
    HWND sheet_density = nullptr;
    HWND sheet_thickness = nullptr;
    HWND display_inches = nullptr;
    HWND display_mm = nullptr;
    HWND spacing = nullptr;
    HWND curve_tolerance = nullptr;
    HWND part_rotations = nullptr;
    HWND optimization_type = nullptr;
    HWND rough_approx = nullptr;
    HWND cpu_cores = nullptr;
    HWND iterations = nullptr;
    HWND svg_scale = nullptr;
    HWND endpoint_tolerance = nullptr;
    HWND dxf_import_units = nullptr;
    HWND dxf_export_units = nullptr;
    HWND merge_common_lines = nullptr;
    HWND optimization_ratio = nullptr;
    HWND ga_population = nullptr;
    HWND ga_mutation_rate = nullptr;
    HWND ga_random_shuffle = nullptr;
    HWND ga_shuffle_intensity = nullptr;
    HWND solver_mode = nullptr;
    HWND perf_log = nullptr;
    HWND run = nullptr;
    HWND pause = nullptr;
    HWND save_dxf = nullptr;
    HWND log = nullptr;
    HWND solution_list = nullptr;
    HWND tool_select = nullptr;
    HWND tool_pan = nullptr;
    HWND tool_fit = nullptr;
    HWND tool_measure = nullptr;
    HWND tool_line = nullptr;
    HWND tool_rectangle = nullptr;
    HWND tool_circle = nullptr;
    HWND tool_arc = nullptr;
    HWND tool_polyline = nullptr;
    HWND tool_text = nullptr;
    HWND tool_delete = nullptr;
    HWND tool_clear_annotations = nullptr;
    HWND tool_snap = nullptr;
    HWND tool_undo = nullptr;
    HWND feedback = nullptr;
    int left_scroll_y = 0;
    int left_max_scroll = 0;
    HWND graph_hwnd = nullptr;
    HWND tree_hwnd = nullptr;
    int tree_scroll_y = 0;
    int tree_max_scroll = 0;
    int tree_scroll_x = 0;
    int tree_max_scroll_x = 0;
    HFONT title_font = nullptr;
    HFONT normal_font = nullptr;
    HFONT small_font = nullptr;
    HFONT iso_font = nullptr;
    HICON brand_icon = nullptr;
    Gdiplus::Bitmap* brand_logo = nullptr;
    ULONG_PTR gdiplus_token = 0;
    HWND tooltip = nullptr;
    HBRUSH panel_brush = nullptr;
    HBRUSH input_brush = nullptr;
    HBRUSH list_brush = nullptr;
    std::vector<InfoTip> info_tips;
    std::vector<DxfSelection> selections;
    std::mutex mutex;
    smn::NestingResult result;
    smn::NestingResult input_preview_result;
    std::vector<smn::IterationStats> history;
    std::vector<RankedSolution> ranked_solutions;
    int selected_solution_index = -1;
    int current_iteration = 0;
    double current_occupied_area = 0.0;
    double current_best_area = 0.0;
    double current_saved_area = 0.0;
    std::filesystem::path last_output;
    double last_export_scale = 1.0;
    
    // View and interactive tools state
    double preview_zoom = 1.0;
    double preview_pan_x = 0.0;
    double preview_pan_y = 0.0;
    bool preview_interacting = false;
    bool preview_panning_view = false;
    POINT preview_last_mouse{0, 0};
    POINT preview_down_pos{0, 0};
    ToolMode tool_mode = ToolMode::Select;
    std::vector<Annotation> annotations;
    std::vector<std::vector<Annotation>> undo_stack;  // for Ctrl+Z
    int selected_annotation_index = -1;
    bool dragging_annotation = false;
    int file_list_height = kFileListDefaultHeight;
    bool resizing_file_list = false;
    POINT resize_start_mouse{0, 0};
    int resize_start_height = kFileListDefaultHeight;
    bool snap_enabled = false;  // grid snap toggle; off by default for continuous CAD drawing
    bool grid_visible = true;
    bool osnap_active = false; // object snap toggle/status
    POINT crosshair_pos{-1, -1};  // last mouse pos for crosshair
    smn::Point crosshair_sheet_pos{0, 0};  // in mm
    bool crosshair_valid = false;
    bool draft_active = false;
    ToolMode draft_tool = ToolMode::Select;
    ToolMode last_draw_tool = ToolMode::Line;
    bool dimension_building = false;
    int dimension_stage = 0;
    smn::Point dimension_first_point{0, 0};
    // Polyline construction state
    bool polyline_building = false;
    std::vector<smn::Point> polyline_points;
    // Arc construction state
    int arc_click_count = 0;
    smn::Point arc_p1{}, arc_p2{}, arc_p3{};
    int pending_dimension_delete_index = -1;
    std::string text_tool_font = "ISOCPEUR";
    double text_tool_size = 16.0;
    
    bool has_result = false;
    bool has_input_preview = false;
    bool input_preview_loading = false;
    std::string input_preview_message;
    std::string worker_message;
    bool worker_ok = false;
    std::atomic_bool running = false;
    std::atomic_bool stop_requested = false;
    std::atomic_bool pause_requested = false;
    std::atomic_uint input_preview_generation = 0;
};

AppState g_app;

std::string wide_to_utf8(const wchar_t* value) {
    if (!value) {
        return "";
    }
    const int needed = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    std::string output(static_cast<size_t>(needed > 0 ? needed - 1 : 0), '\0');
    if (needed > 1) {
        WideCharToMultiByte(CP_UTF8, 0, value, -1, output.data(), needed, nullptr, nullptr);
    }
    return output;
}

std::wstring utf8_to_wide(const std::string& value) {
    const int needed = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    std::wstring output(static_cast<size_t>(needed > 0 ? needed - 1 : 0), L'\0');
    if (needed > 1) {
        MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, output.data(), needed);
    }
    return output;
}

std::string get_text(HWND control) {
    const int length = GetWindowTextLengthA(control);
    std::string value(static_cast<size_t>(length), '\0');
    GetWindowTextA(control, value.data(), length + 1);
    return value;
}

void set_text(HWND control, const std::string& value) {
    SetWindowTextA(control, value.c_str());
}

void set_text(HWND control, const std::filesystem::path& path) {
    SetWindowTextW(control, path.wstring().c_str());
}

void append_log(const std::string& text) {
    const int length = GetWindowTextLengthA(g_app.log);
    SendMessageA(g_app.log, EM_SETSEL, length, length);
    SendMessageA(g_app.log, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str()));
}

bool is_checked(HWND control) {
    return SendMessageA(control, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

std::string combo_text(HWND combo) {
    const int index = static_cast<int>(SendMessageA(combo, CB_GETCURSEL, 0, 0));
    if (index < 0) {
        return "";
    }
    char buffer[128] = {};
    SendMessageA(combo, CB_GETLBTEXT, index, reinterpret_cast<LPARAM>(buffer));
    return buffer;
}

std::vector<double> rotations_from_count(int count) {
    count = std::max(1, count);
    std::vector<double> rotations;
    rotations.reserve(static_cast<size_t>(count));
    const double step = 360.0 / static_cast<double>(count);
    for (int index = 0; index < count; ++index) {
        rotations.push_back(step * static_cast<double>(index));
    }
    return rotations;
}

double unit_scale_from_combo(HWND combo) {
    return combo_text(combo) == "inches" ? 25.4 : 1.0;
}

double display_unit_scale() {
    return is_checked(g_app.display_inches) ? 25.4 : 1.0;
}


int gui_safe_cpu_cores(int requested) {
    requested = std::max(1, requested);
    const unsigned int available = std::thread::hardware_concurrency();
    if (available <= 2) return requested;
    return std::clamp(requested, 1, static_cast<int>(available - 1));
}
std::vector<std::filesystem::path> choose_folders(HWND owner) {
    std::vector<std::filesystem::path> paths;
    IFileOpenDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
        return paths;
    }

    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    dialog->SetTitle(L"Elegir carpeta con DXF");
    if (SUCCEEDED(dialog->Show(owner))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item))) {
            PWSTR raw_path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &raw_path))) {
                paths.emplace_back(raw_path);
                CoTaskMemFree(raw_path);
            }
            item->Release();
        }
    }
    dialog->Release();
    return paths;
}

std::vector<std::filesystem::path> choose_dxf_files(HWND owner) {
    std::vector<std::filesystem::path> paths;
    IFileOpenDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
        return paths;
    }

    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_ALLOWMULTISELECT | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST);
    COMDLG_FILTERSPEC filters[] = {
        {L"DXF files", L"*.dxf"},
        {L"All files", L"*.*"},
    };
    dialog->SetFileTypes(2, filters);
    dialog->SetTitle(L"Elegir archivos DXF");

    if (SUCCEEDED(dialog->Show(owner))) {
        IShellItemArray* items = nullptr;
        if (SUCCEEDED(dialog->GetResults(&items))) {
            DWORD count = 0;
            items->GetCount(&count);
            for (DWORD index = 0; index < count; ++index) {
                IShellItem* item = nullptr;
                if (SUCCEEDED(items->GetItemAt(index, &item))) {
                    PWSTR raw_path = nullptr;
                    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &raw_path))) {
                        paths.emplace_back(raw_path);
                        CoTaskMemFree(raw_path);
                    }
                    item->Release();
                }
            }
            items->Release();
        }
    }
    dialog->Release();
    return paths;
}

std::filesystem::path choose_output(HWND owner, const std::filesystem::path& suggested_path = {}) {
    std::filesystem::path output_path;
    IFileSaveDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
        return output_path;
    }

    COMDLG_FILTERSPEC filters[] = {
        {L"DXF files", L"*.dxf"},
        {L"All files", L"*.*"},
    };
    dialog->SetFileTypes(2, filters);
    dialog->SetDefaultExtension(L"dxf");

    std::filesystem::path suggested_file = "nesting_result_cpp.dxf";
    std::filesystem::path suggested_folder;
    if (!suggested_path.empty()) {
        if (suggested_path.has_filename()) {
            suggested_file = suggested_path.filename();
        }
        suggested_folder = suggested_path.parent_path();
    }
    const std::wstring wide_file = suggested_file.wstring();
    dialog->SetFileName(wide_file.c_str());

    if (!suggested_folder.empty() && std::filesystem::exists(suggested_folder)) {
        IShellItem* folder_item = nullptr;
        const std::wstring wide_folder = suggested_folder.wstring();
        if (SUCCEEDED(SHCreateItemFromParsingName(wide_folder.c_str(), nullptr, IID_PPV_ARGS(&folder_item)))) {
            dialog->SetFolder(folder_item);
            folder_item->Release();
        }
    }
    dialog->SetTitle(L"Guardar DXF de nesting");

    if (SUCCEEDED(dialog->Show(owner))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item))) {
            PWSTR raw_path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &raw_path))) {
                output_path = raw_path;
                CoTaskMemFree(raw_path);
            }
            item->Release();
        }
    }
    dialog->Release();
    return output_path;
}

void add_selection(const std::filesystem::path& path, int quantity = 1) {
    for (auto& selection : g_app.selections) {
        if (std::filesystem::equivalent(selection.path, path)) {
            selection.quantity += quantity;
            return;
        }
    }
    g_app.selections.push_back({path, std::max(1, quantity)});
}

void start_input_preview(HWND hwnd);

void refresh_file_list() {
    ListView_DeleteAllItems(g_app.file_list);
    for (size_t i = 0; i < g_app.selections.size(); ++i) {
        const auto& selection = g_app.selections[i];
        std::string text = selection.path.filename().string();
        std::string qty_text = std::to_string(selection.quantity);
        LVITEMA lvi = {0};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = static_cast<int>(i);
        lvi.iSubItem = 0;
        lvi.pszText = &text[0];
        SendMessageA(g_app.file_list, LVM_INSERTITEMA, 0, reinterpret_cast<LPARAM>(&lvi));
        ListView_SetItemText(g_app.file_list, static_cast<int>(i), 1, &qty_text[0]);
    }
}

bool is_better_solution(const RankedSolution& candidate, const RankedSolution& current) {
    constexpr double tolerance = 1e-4;
    if (candidate.unplaced_count != current.unplaced_count) {
        return candidate.unplaced_count < current.unplaced_count;
    }
    if (candidate.sheet_count != current.sheet_count) {
        return candidate.sheet_count < current.sheet_count;
    }
    return candidate.occupied_area + tolerance < current.occupied_area;
}

void refresh_solution_list() {
    SendMessageA(g_app.solution_list, LB_RESETCONTENT, 0, 0);
    int horizontal_extent = 0;

    double sheet_price_kg = 0.0;
    double sheet_density = 0.0;
    double sheet_thickness = 0.0;
    try { sheet_price_kg = std::stod(get_text(g_app.sheet_price_kg)); } catch(...) {}
    try { sheet_density = std::stod(get_text(g_app.sheet_density)); } catch(...) {}
    try { sheet_thickness = std::stod(get_text(g_app.sheet_thickness)); } catch(...) {}

    std::vector<RankedSolution> ranked;
    int selected = -1;
    {
        std::lock_guard<std::mutex> lock(g_app.mutex);
        ranked = g_app.ranked_solutions;
        selected = g_app.selected_solution_index;
    }

    for (size_t index = 0; index < ranked.size(); ++index) {
        const auto& solution = ranked[index];
        std::ostringstream label;
        label << "#" << (index + 1)
              << "  it " << solution.iteration
              << "  area " << std::fixed << std::setprecision(3) << (solution.occupied_area / 1000000.0) << " m2"
              << "  scrap " << std::fixed << std::setprecision(3) << (solution.scrap_area / 1000000.0) << " m2";

        if (sheet_density > 0 && sheet_thickness > 0) {
             double occ_vol_m3 = (solution.occupied_area * sheet_thickness) / 1e9;
             double occ_mass_kg = occ_vol_m3 * sheet_density;
             double occupied_cost = occ_mass_kg * sheet_price_kg;
             label << "  costo $" << std::fixed << std::setprecision(2) << occupied_cost;
        }

        label << "  ahorro " << std::fixed << std::setprecision(3) << (solution.saved_area / 1000000.0) << " m2"
              << "  tiempo " << std::fixed << std::setprecision(1) << solution.time_to_find << "s";
        const std::string text = label.str();
        SendMessageA(g_app.solution_list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
        horizontal_extent = std::max(horizontal_extent, static_cast<int>(text.size() * 8 + 28));
    }
    SendMessageA(g_app.solution_list, LB_SETHORIZONTALEXTENT, static_cast<WPARAM>(horizontal_extent), 0);

    if (selected >= 0 && selected < static_cast<int>(ranked.size())) {
        SendMessageA(g_app.solution_list, LB_SETCURSEL, selected, 0);
    }
}

void add_ranked_solution_locked(const smn::IterationStats& stats, const smn::NestingResult& result) {
    RankedSolution candidate;
    candidate.iteration = stats.iteration;
    candidate.occupied_area = smn::occupied_layout_area(result);
    candidate.saved_area = stats.saved_area;
    candidate.utilization = result.utilization();
    candidate.time_to_find = stats.time_to_find;
    candidate.sheet_count = result.sheet_count();
    candidate.unplaced_count = static_cast<int>(result.unplaced.size());
    candidate.result = result;
    candidate.scrap_area = candidate.occupied_area - result.used_area();

    if (!g_app.ranked_solutions.empty() && !is_better_solution(candidate, g_app.ranked_solutions.front())) {
        return;
    }

    g_app.ranked_solutions.push_back(std::move(candidate));
    std::sort(g_app.ranked_solutions.begin(), g_app.ranked_solutions.end(), is_better_solution);
    if (g_app.ranked_solutions.size() > 80) {
        g_app.ranked_solutions.resize(80);
    }
}

int selected_file_index() {
    return ListView_GetNextItem(g_app.file_list, -1, LVNI_SELECTED);
}

void update_selected_quantity_field() {
    const int index = selected_file_index();
    if (index >= 0 && index < static_cast<int>(g_app.selections.size())) {
        set_text(g_app.selected_quantity, std::to_string(g_app.selections[static_cast<size_t>(index)].quantity));
    }
}

LRESULT CALLBACK file_quantity_edit_subclass(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR);

int parse_quantity_or_one(const std::string& text) {
    try {
        return std::max(1, std::stoi(text));
    } catch (...) {
        return 1;
    }
}

void commit_file_quantity_edit(bool apply) {
    HWND edit = g_app.file_quantity_edit;
    if (!edit || g_app.destroying_file_quantity_edit) {
        return;
    }

    const int index = g_app.file_quantity_edit_index;
    int next_quantity = 1;
    if (apply) {
        next_quantity = parse_quantity_or_one(get_text(edit));
    }

    g_app.file_quantity_edit = nullptr;
    g_app.file_quantity_edit_index = -1;
    g_app.destroying_file_quantity_edit = true;
    RemoveWindowSubclass(edit, file_quantity_edit_subclass, 1);
    DestroyWindow(edit);
    g_app.destroying_file_quantity_edit = false;

    if (apply && index >= 0 && index < static_cast<int>(g_app.selections.size())) {
        g_app.selections[static_cast<size_t>(index)].quantity = next_quantity;
        refresh_file_list();
        ListView_SetItemState(g_app.file_list, index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        update_selected_quantity_field();
        start_input_preview(g_app.hwnd);
    }
}

LRESULT CALLBACK file_quantity_edit_subclass(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR) {
    if (message == WM_KEYDOWN) {
        if (wparam == VK_RETURN) {
            commit_file_quantity_edit(true);
            return 0;
        }
        if (wparam == VK_ESCAPE) {
            commit_file_quantity_edit(false);
            return 0;
        }
    } else if (message == WM_KILLFOCUS) {
        commit_file_quantity_edit(true);
    }
    return DefSubclassProc(hwnd, message, wparam, lparam);
}

void begin_file_quantity_edit(int index) {
    if (index < 0 || index >= static_cast<int>(g_app.selections.size())) {
        return;
    }
    commit_file_quantity_edit(true);

    RECT cell{};
    if (!ListView_GetSubItemRect(g_app.file_list, index, 1, LVIR_BOUNDS, &cell)) {
        return;
    }

    const int cell_width = std::max(42, static_cast<int>(cell.right - cell.left - 8));
    const int cell_height = std::max(22, static_cast<int>(cell.bottom - cell.top - 4));
    HWND edit = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        "EDIT",
        std::to_string(g_app.selections[static_cast<size_t>(index)].quantity).c_str(),
        WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_AUTOHSCROLL,
        cell.left + 4,
        cell.top + 2,
        cell_width,
        cell_height,
        g_app.file_list,
        reinterpret_cast<HMENU>(IdFileQuantityEdit),
        nullptr,
        nullptr
    );
    if (!edit) {
        return;
    }
    SendMessageA(edit, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.normal_font), TRUE);
    SetWindowSubclass(edit, file_quantity_edit_subclass, 1, 0);
    g_app.file_quantity_edit = edit;
    g_app.file_quantity_edit_index = index;
    SetFocus(edit);
    SendMessageA(edit, EM_SETSEL, 0, -1);
}

HWND make_edit(HWND parent, const char* text, int id) {
    HWND handle = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        "EDIT",
        text,
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0,
        0,
        10,
        24,
        parent,
        reinterpret_cast<HMENU>(id),
        nullptr,
        nullptr
    );
    SendMessageA(handle, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.normal_font), TRUE);
    return handle;
}

HWND make_button(HWND parent, const char* text, int id) {
    HWND handle = CreateWindowA(
        "BUTTON",
        text,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
        0,
        0,
        10,
        28,
        parent,
        reinterpret_cast<HMENU>(id),
        nullptr,
        nullptr
    );
    SendMessageA(handle, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.normal_font), TRUE);
    return handle;
}

HWND make_check(HWND parent, const char* text, int id) {
    HWND handle = CreateWindowA(
        "BUTTON",
        text,
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        0,
        0,
        10,
        22,
        parent,
        reinterpret_cast<HMENU>(id),
        nullptr,
        nullptr
    );
    SendMessageA(handle, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.normal_font), TRUE);
    return handle;
}

HWND make_radio(HWND parent, const char* text, int id) {
    HWND handle = CreateWindowA(
        "BUTTON",
        text,
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        0,
        0,
        10,
        22,
        parent,
        reinterpret_cast<HMENU>(id),
        nullptr,
        nullptr
    );
    SendMessageA(handle, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.normal_font), TRUE);
    return handle;
}

HWND make_combo(HWND parent, int id, const std::vector<std::string>& values, int selected) {
    HWND handle = CreateWindowA(
        "COMBOBOX",
        "",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        0,
        0,
        10,
        140,
        parent,
        reinterpret_cast<HMENU>(id),
        nullptr,
        nullptr
    );
    SendMessageA(handle, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.normal_font), TRUE);
    for (const auto& value : values) {
        SendMessageA(handle, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value.c_str()));
    }
    SendMessageA(handle, CB_SETCURSEL, selected, 0);
    return handle;
}

InfoTip* find_info_tip(int id) {
    for (auto& tip : g_app.info_tips) {
        if (tip.id == id) {
            return &tip;
        }
    }
    return nullptr;
}

void place_info_tip(int id, int x, int y) {
    InfoTip* tip = find_info_tip(id);
    if (!tip) {
        return;
    }
    tip->rect = {x, y, x + 15, y + 15};
    if (g_app.tooltip) {
        TOOLINFOA tool = {};
        tool.cbSize = sizeof(tool);
        tool.hwnd = g_app.hwnd;
        tool.uId = static_cast<UINT_PTR>(tip->id);
        tool.rect = tip->rect;
        SendMessageA(g_app.tooltip, TTM_NEWTOOLRECTA, 0, reinterpret_cast<LPARAM>(&tool));
    }
}

RECT preview_rect(const RECT& rect);
RECT graph_rect(const RECT& rect);
RECT solution_list_rect(const RECT& rect);
void layout_controls(HWND hwnd);
void start_input_preview(HWND hwnd);

struct LeftPanelLayout {
    int x = kMargin;
    int w = kPanelWidth - 2 * kMargin;
    int c2 = kMargin + 288;
    int col1_control = kMargin + 148;
    int col2_control = kMargin + 404;
    int file_label_y = kLeftFileLabelY;
    int file_top = kLeftFileTop;
    int file_height = kFileListDefaultHeight;
    int splitter_top = 0;
    int buttons_y = 0;
    int quantity_label_y = 0;
    int quantity_y = 0;
    int sheet_label_y = 0;
    int sheet_y = 0;
    int cost_label_y = 0;
    int cost_y = 0;
    int config_title_y = 0;
    int config_y = 0;
    int import_title_y = 0;
    int import_y = 0;
    int laser_title_y = 0;
    int laser_y = 0;
};

int clamp_file_list_height(const RECT& client, int requested_height) {
    // The controls below the DXF list are scrollable, so the list should not be
    // capped by a large fixed "remaining controls" budget. That made the list
    // nearly unusable on scaled notebook/4K layouts.
    const int visible_panel_height = std::max(260, static_cast<int>(client.bottom) - kLeftFileTop - 96);
    const int max_height = std::max(kFileListMinHeight, std::min(520, visible_panel_height));
    return std::clamp(requested_height, kFileListMinHeight, max_height);
}

LeftPanelLayout left_panel_layout(const RECT& client) {
    LeftPanelLayout layout;
    layout.file_height = clamp_file_list_height(client, g_app.file_list_height);
    layout.splitter_top = layout.file_top + layout.file_height + 4;
    layout.buttons_y = layout.splitter_top + kFileListSplitterHeight + 8;
    layout.quantity_label_y = layout.buttons_y + 39;
    layout.quantity_y = layout.quantity_label_y + 22;
    layout.sheet_label_y = layout.quantity_y + 42;
    layout.sheet_y = layout.sheet_label_y + 22;
    layout.cost_label_y = layout.sheet_y + 32;
    layout.cost_y = layout.cost_label_y + 22;
    layout.config_title_y = layout.cost_y + 56;
    layout.config_y = layout.config_title_y + 32;
    layout.import_title_y = layout.config_y + 4 * 27 + 22;
    layout.import_y = layout.import_title_y + 28;
    layout.laser_title_y = layout.import_y + 2 * 27 + 23;
    layout.laser_y = layout.laser_title_y + 28;
    return layout;
}

RECT file_list_splitter_rect(const RECT& client) {
    const LeftPanelLayout layout = left_panel_layout(client);
    return {
        layout.x,
        layout.splitter_top - g_app.left_scroll_y,
        layout.x + layout.w,
        layout.splitter_top + kFileListSplitterHeight - g_app.left_scroll_y
    };
}

int left_panel_content_bottom(const LeftPanelLayout& layout) {
    return layout.laser_y + 4 * 27 + 36;
}

void clamp_left_panel_scroll(const RECT& client, const LeftPanelLayout& layout) {
    const int visible_bottom = std::max(kLeftContentClipTop + 1, static_cast<int>(client.bottom) - 8);
    g_app.left_max_scroll = std::max(0, left_panel_content_bottom(layout) - visible_bottom);
    g_app.left_scroll_y = std::clamp(g_app.left_scroll_y, 0, g_app.left_max_scroll);
}

bool scroll_left_panel(HWND hwnd, int delta_pixels) {
    RECT rect;
    GetClientRect(hwnd, &rect);
    const LeftPanelLayout layout = left_panel_layout(rect);
    clamp_left_panel_scroll(rect, layout);
    const int old_scroll = g_app.left_scroll_y;
    g_app.left_scroll_y = std::clamp(g_app.left_scroll_y + delta_pixels, 0, g_app.left_max_scroll);
    if (g_app.left_scroll_y == old_scroll) {
        return false;
    }
    layout_controls(hwnd);
    InvalidateRect(hwnd, nullptr, FALSE);
    return true;
}

void update_file_list_columns(int width) {
    if (!g_app.file_list) {
        return;
    }
    const int quantity_width = 74;
    ListView_SetColumnWidth(g_app.file_list, 0, std::max(180, width - quantity_width - 8));
    ListView_SetColumnWidth(g_app.file_list, 1, quantity_width);
}

int toolbar_rows_for_client(const RECT& rect) {
    const int right_x = kPanelWidth + 24;
    const int right_limit = std::max(right_x + 260, static_cast<int>(rect.right) - 24);
    const int widths[] = {78, 62, 66, 58, 58, 54, 62, 50, 70, 58, 64, 58, 58, 70};
    int tool_x = right_x;
    int rows = 1;
    for (const int width : widths) {
        if (tool_x > right_x && tool_x + width > right_limit) {
            ++rows;
            tool_x = right_x;
        }
        tool_x += width + 4;
    }
    return rows;
}

int toolbar_bottom_for_client(const RECT& rect) {
    const int rows = toolbar_rows_for_client(rect);
    return 16 + rows * 28 + (rows - 1) * 6;
}

void layout_controls(HWND hwnd) {
    RECT rect;
    GetClientRect(hwnd, &rect);
    g_app.file_list_height = clamp_file_list_height(rect, g_app.file_list_height);
    const LeftPanelLayout layout = left_panel_layout(rect);
    clamp_left_panel_scroll(rect, layout);
    const int scroll = g_app.left_scroll_y;
    const auto move_left = [&](HWND control, int x, int y, int width, int height) {
        if (!control) {
            return;
        }
        const int visible_y = y - scroll;
        MoveWindow(control, x, visible_y, width, height, TRUE);
        const bool visible = visible_y + height > kLeftContentClipTop && visible_y < rect.bottom;
        ShowWindow(control, visible ? SW_SHOW : SW_HIDE);
    };

    const int x = layout.x;
    const int w = layout.w;
    move_left(g_app.file_list, x, layout.file_top, w, layout.file_height);
    update_file_list_columns(w);
    move_left(GetDlgItem(hwnd, IdAddFolder), x, layout.buttons_y, 110, 26);
    move_left(GetDlgItem(hwnd, IdAddFiles), x + 118, layout.buttons_y, 110, 26);
    ShowWindow(GetDlgItem(hwnd, IdImportCsv), SW_HIDE);
    move_left(GetDlgItem(hwnd, IdRemoveFile), x + 236, layout.buttons_y, 150, 26);
    move_left(g_app.selected_quantity, x + 214, layout.quantity_y - 1, 72, 24);
    move_left(GetDlgItem(hwnd, IdApplyQuantity), x + 294, layout.quantity_y - 2, 190, 26);
    ShowWindow(g_app.output, SW_HIDE);
    ShowWindow(GetDlgItem(hwnd, IdBrowseOutput), SW_HIDE);

    move_left(g_app.sheet_width, x, layout.sheet_y, 96, 24);
    move_left(g_app.sheet_height, x + 104, layout.sheet_y, 96, 24);
    move_left(g_app.sheet_price_kg, x, layout.cost_y, 96, 24);
    move_left(g_app.sheet_density, x + 104, layout.cost_y, 96, 24);
    move_left(g_app.sheet_thickness, x + 208, layout.cost_y, 96, 24);
    move_left(g_app.run, x + 210, layout.sheet_y - 3, 116, 30);
    move_left(g_app.pause, x + 334, layout.sheet_y - 3, 72, 30);
    move_left(g_app.save_dxf, x + 414, layout.sheet_y - 3, 88, 30);
    if (g_app.feedback) {
        ShowWindow(g_app.feedback, SW_HIDE);
    }

    const int col1_control = layout.col1_control;
    const int col2_control = layout.col2_control;
    const int config_input_w = 80;
    const int row = 27;

    int y = layout.config_y;
    move_left(g_app.display_inches, col1_control, y - 1, 76, 21);
    move_left(g_app.display_mm, col1_control + 80, y - 1, 58, 21);
    move_left(g_app.spacing, col2_control, y - 3, config_input_w, 24);
    y += row;
    move_left(g_app.curve_tolerance, col1_control, y - 3, config_input_w, 24);
    move_left(g_app.part_rotations, col2_control, y - 3, config_input_w, 24);
    y += row;
    move_left(g_app.optimization_type, col1_control, y - 5, 126, 110);
    move_left(g_app.rough_approx, col2_control, y - 1, 26, 21);
    y += row;
    move_left(g_app.cpu_cores, col1_control, y - 3, config_input_w, 24);
    move_left(g_app.iterations, col2_control, y - 3, config_input_w, 24);

    y = layout.import_y;
    move_left(g_app.svg_scale, col1_control, y - 3, config_input_w, 24);
    move_left(g_app.endpoint_tolerance, col2_control, y - 3, config_input_w, 24);
    y += row;
    move_left(g_app.dxf_import_units, col1_control, y - 5, 86, 110);
    move_left(g_app.dxf_export_units, col2_control, y - 5, 86, 110);

    y = layout.laser_y;
    move_left(g_app.merge_common_lines, col1_control, y - 1, 26, 21);
    move_left(g_app.optimization_ratio, col2_control, y - 3, config_input_w, 24);
    y += row;
    move_left(g_app.ga_population, col1_control, y - 3, config_input_w, 24);
    move_left(g_app.ga_mutation_rate, col2_control, y - 3, config_input_w, 24);
    y += row;
    move_left(g_app.ga_random_shuffle, col1_control, y - 3, config_input_w, 24);
    move_left(g_app.ga_shuffle_intensity, col2_control, y - 3, config_input_w, 24);
    y += row;
    move_left(g_app.solver_mode, col1_control, y - 5, 118, 110);
    move_left(g_app.perf_log, col2_control, y - 1, 26, 21);

    const int right_x = kPanelWidth + 24;
    
    // CAD tools
    int tool_x = right_x;
    int tool_y = 16;
    const int right_limit = std::max(right_x + 260, static_cast<int>(rect.right) - 24);
    const auto place_tool = [&](HWND control, int width) {
        if (!control) {
            return;
        }
        if (tool_x > right_x && tool_x + width > right_limit) {
            tool_x = right_x;
            tool_y += 34;
        }
        MoveWindow(control, tool_x, tool_y, width, 28, TRUE);
        tool_x += width + 4;
    };
    place_tool(g_app.tool_select, 78);
    place_tool(g_app.tool_pan, 62);
    place_tool(g_app.tool_fit, 66);
    place_tool(g_app.tool_measure, 58);
    place_tool(g_app.tool_line, 58);
    place_tool(g_app.tool_rectangle, 54);
    place_tool(g_app.tool_circle, 62);
    place_tool(g_app.tool_arc, 50);
    place_tool(g_app.tool_polyline, 70);
    place_tool(g_app.tool_text, 58);
    place_tool(g_app.tool_delete, 64);
    place_tool(g_app.tool_undo, 58);
    place_tool(g_app.tool_snap, 58);
    place_tool(g_app.tool_clear_annotations, 70);

    const int bottom_y = rect.bottom - kBottomPanelHeight - kBottomPanelMargin;
    const int sol_w = static_cast<int>(std::min<LONG>(430, std::max<LONG>(320, (rect.right - right_x - 34) / 2)));
    const int log_w = static_cast<int>(std::max<LONG>(200, rect.right - right_x - sol_w - 24));
    
    // Solutions list
    MoveWindow(g_app.solution_list, right_x, bottom_y, sol_w, kBottomPanelHeight, TRUE);
    
    // Log
    MoveWindow(g_app.log, right_x + sol_w + 10, bottom_y, log_w, kBottomPanelHeight, TRUE);

    const int info_left = col1_control - 27;
    const int info_right = col2_control - 27;
    place_info_tip(IdSelectedQuantity, kMargin + 205, layout.quantity_label_y + 1 - scroll);
    place_info_tip(IdSheetWidth, kMargin + 82, layout.sheet_label_y + 1 - scroll);
    place_info_tip(IdSheetHeight, kMargin + 190, layout.sheet_label_y + 1 - scroll);
    place_info_tip(IdSheetPriceKg, kMargin + 78, layout.cost_label_y + 1 - scroll);
    place_info_tip(IdSheetDensity, kMargin + 186, layout.cost_label_y + 1 - scroll);
    place_info_tip(IdSheetThickness, kMargin + 292, layout.cost_label_y + 1 - scroll);

    place_info_tip(IdDisplayMm, info_left, layout.config_y + 1 - scroll);
    place_info_tip(IdSpacing, info_right, layout.config_y + 1 - scroll);
    place_info_tip(IdCurveTolerance, info_left, layout.config_y + 28 - scroll);
    place_info_tip(IdPartRotations, info_right, layout.config_y + 28 - scroll);
    place_info_tip(IdOptimizationType, info_left, layout.config_y + 55 - scroll);
    place_info_tip(IdRoughApprox, info_right, layout.config_y + 55 - scroll);
    place_info_tip(IdCpuCores, info_left, layout.config_y + 82 - scroll);
    place_info_tip(IdIterations, info_right, layout.config_y + 82 - scroll);

    place_info_tip(IdSvgScale, info_left, layout.import_y + 1 - scroll);
    place_info_tip(IdEndpointTolerance, info_right, layout.import_y + 1 - scroll);
    place_info_tip(IdDxfImportUnits, info_left, layout.import_y + 28 - scroll);
    place_info_tip(IdDxfExportUnits, info_right, layout.import_y + 28 - scroll);

    place_info_tip(IdMergeCommonLines, info_left, layout.laser_y + 1 - scroll);
    place_info_tip(IdOptimizationRatio, info_right, layout.laser_y + 1 - scroll);
    place_info_tip(IdGaPopulation, info_left, layout.laser_y + 28 - scroll);
    place_info_tip(IdGaMutationRate, info_right, layout.laser_y + 28 - scroll);
    place_info_tip(IdGaRandomShuffle, info_left, layout.laser_y + 55 - scroll);
    place_info_tip(IdGaShuffleIntensity, info_right, layout.laser_y + 55 - scroll);
    place_info_tip(IdSolverMode, info_left, layout.laser_y + 82 - scroll);
    place_info_tip(IdPerfLog, info_right, layout.laser_y + 82 - scroll);
}

void draw_text(HDC dc, int x, int y, const char* text, HFONT font, COLORREF color) {
    SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    TextOutA(dc, x, y, text, static_cast<int>(std::strlen(text)));
}

void draw_info_tips(HDC dc) {
    HPEN border = CreatePen(PS_SOLID, 1, RGB(92, 111, 101));
    HBRUSH fill = CreateSolidBrush(RGB(36, 43, 39));
    HGDIOBJ old_pen = SelectObject(dc, border);
    HGDIOBJ old_brush = SelectObject(dc, fill);
    HFONT old_font = static_cast<HFONT>(SelectObject(dc, g_app.small_font ? g_app.small_font : GetStockObject(DEFAULT_GUI_FONT)));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, kFoglarCream);

    for (const auto& tip : g_app.info_tips) {
        if (!tip.visible || tip.rect.right <= tip.rect.left) {
            continue;
        }
        Ellipse(dc, tip.rect.left, tip.rect.top, tip.rect.right, tip.rect.bottom);
        RECT text_rect = tip.rect;
        OffsetRect(&text_rect, 0, -1);
        DrawTextA(dc, "i", -1, &text_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    SelectObject(dc, old_font);
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(fill);
    DeleteObject(border);
}

int text_width(HDC dc, HFONT font, const char* text) {
    HGDIOBJ old_font = SelectObject(dc, font);
    SIZE size{0, 0};
    GetTextExtentPoint32A(dc, text, static_cast<int>(std::strlen(text)), &size);
    SelectObject(dc, old_font);
    return size.cx;
}

bool ensure_gdiplus_started() {
    if (g_app.gdiplus_token != 0) {
        return true;
    }
    Gdiplus::GdiplusStartupInput startup_input;
    return Gdiplus::GdiplusStartup(&g_app.gdiplus_token, &startup_input, nullptr) == Gdiplus::Ok;
}

Gdiplus::Bitmap* make_transparent_logo_mark(Gdiplus::Bitmap& source) {
    const UINT width = source.GetWidth();
    const UINT height = source.GetHeight();
    if (width == 0 || height == 0) {
        return nullptr;
    }

    auto logo_strength = [](const Gdiplus::Color& color) {
        if (color.GetA() <= 10 || color.GetR() < 205 || color.GetG() < 118 || color.GetB() < 70) {
            return 0;
        }
        const int green_lift = static_cast<int>(color.GetG()) - 90;
        const int blue_lift = static_cast<int>(color.GetB()) - 45;
        return std::clamp(std::min(green_lift * 2, blue_lift * 2), 0, 255);
    };

    UINT min_x = width;
    UINT min_y = height;
    UINT max_x = 0;
    UINT max_y = 0;
    for (UINT y = 0; y < height; ++y) {
        for (UINT x = 0; x < width; ++x) {
            Gdiplus::Color color;
            source.GetPixel(x, y, &color);
            if (logo_strength(color) > 8) {
                min_x = std::min(min_x, x);
                min_y = std::min(min_y, y);
                max_x = std::max(max_x, x);
                max_y = std::max(max_y, y);
            }
        }
    }

    if (min_x > max_x || min_y > max_y) {
        return nullptr;
    }

    const UINT mark_w = max_x - min_x + 1;
    const UINT mark_h = max_y - min_y + 1;
    const UINT padding = std::max<UINT>(4, std::max(mark_w, mark_h) / 10);
    min_x = min_x > padding ? min_x - padding : 0;
    min_y = min_y > padding ? min_y - padding : 0;
    max_x = std::min(width - 1, max_x + padding);
    max_y = std::min(height - 1, max_y + padding);

    const UINT crop_w = max_x - min_x + 1;
    const UINT crop_h = max_y - min_y + 1;
    auto* output = new Gdiplus::Bitmap(crop_w, crop_h, PixelFormat32bppARGB);
    if (!output || output->GetLastStatus() != Gdiplus::Ok) {
        delete output;
        return nullptr;
    }

    for (UINT y = 0; y < crop_h; ++y) {
        for (UINT x = 0; x < crop_w; ++x) {
            Gdiplus::Color color;
            source.GetPixel(min_x + x, min_y + y, &color);
            const int alpha = logo_strength(color);
            if (alpha <= 0) {
                output->SetPixel(x, y, Gdiplus::Color(0, 0, 0, 0));
            } else {
                output->SetPixel(x, y, Gdiplus::Color(alpha, GetRValue(kFoglarCream), GetGValue(kFoglarCream), GetBValue(kFoglarCream)));
            }
        }
    }

    return output;
}

Gdiplus::Bitmap* load_transparent_logo_mark(const std::filesystem::path& path) {
    if (!ensure_gdiplus_started() || !std::filesystem::exists(path)) {
        return nullptr;
    }

    auto* source = Gdiplus::Bitmap::FromFile(path.wstring().c_str(), FALSE);
    if (!source || source->GetLastStatus() != Gdiplus::Ok) {
        delete source;
        return nullptr;
    }

    auto* output = make_transparent_logo_mark(*source);
    delete source;
    return output;
}

Gdiplus::Bitmap* load_transparent_logo_mark_from_resource(HINSTANCE instance, int resource_id) {
    if (!ensure_gdiplus_started()) {
        return nullptr;
    }

    HRSRC resource = FindResourceA(instance, MAKEINTRESOURCEA(resource_id), RT_RCDATA);
    if (!resource) {
        return nullptr;
    }
    HGLOBAL loaded = LoadResource(instance, resource);
    const DWORD size = SizeofResource(instance, resource);
    const void* data = loaded ? LockResource(loaded) : nullptr;
    if (!data || size == 0) {
        return nullptr;
    }

    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!memory) {
        return nullptr;
    }
    void* buffer = GlobalLock(memory);
    if (!buffer) {
        GlobalFree(memory);
        return nullptr;
    }
    std::memcpy(buffer, data, size);
    GlobalUnlock(memory);

    IStream* stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &stream)) || !stream) {
        GlobalFree(memory);
        return nullptr;
    }

    auto* source = Gdiplus::Bitmap::FromStream(stream, FALSE);
    Gdiplus::Bitmap* output = nullptr;
    if (source && source->GetLastStatus() == Gdiplus::Ok) {
        output = make_transparent_logo_mark(*source);
    }
    delete source;
    stream->Release();
    return output;
}

void draw_foglar_mark_fallback(HDC dc, int x, int y, int size) {
    HBRUSH cream = CreateSolidBrush(kFoglarCream);
    HPEN cream_pen = CreatePen(PS_SOLID, 1, kFoglarCream);
    HGDIOBJ old_brush = SelectObject(dc, cream);
    HGDIOBJ old_pen = SelectObject(dc, cream_pen);

    const auto px = [&](double ratio) { return x + static_cast<int>(std::round(ratio * size)); };
    const auto py = [&](double ratio) { return y + static_cast<int>(std::round(ratio * size)); };

    POINT base[] = {
        {px(0.12), py(0.82)}, {px(0.30), py(0.55)}, {px(0.42), py(0.68)},
        {px(0.42), py(0.74)}, {px(0.58), py(0.74)}, {px(0.58), py(0.68)},
        {px(0.70), py(0.55)}, {px(0.88), py(0.82)}
    };
    Polygon(dc, base, static_cast<int>(std::size(base)));

    POINT flame[] = {
        {px(0.48), py(0.08)}, {px(0.58), py(0.00)}, {px(0.58), py(0.28)},
        {px(0.52), py(0.41)}, {px(0.60), py(0.50)}, {px(0.70), py(0.22)},
        {px(0.78), py(0.44)}, {px(0.78), py(0.62)}, {px(0.70), py(0.70)},
        {px(0.60), py(0.66)}, {px(0.50), py(0.66)}, {px(0.42), py(0.72)},
        {px(0.34), py(0.62)}, {px(0.34), py(0.48)}, {px(0.48), py(0.28)}
    };
    Polygon(dc, flame, static_cast<int>(std::size(flame)));

    SelectObject(dc, GetStockObject(NULL_BRUSH));
    HPEN cut_pen = CreatePen(PS_SOLID, std::max(2, size / 18), kFoglarCoal);
    SelectObject(dc, cut_pen);
    RoundRect(dc, px(0.49), py(0.58), px(0.61), py(0.75), size / 8, size / 8);
    DeleteObject(cut_pen);

    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(cream_pen);
    DeleteObject(cream);
}

void draw_brand_header(HDC dc) {
    const int logo_x = kMargin;
    const int logo_y = 10;
    const int logo_size = 78;
    if (g_app.brand_logo) {
        const int image_w = static_cast<int>(g_app.brand_logo->GetWidth());
        const int image_h = static_cast<int>(g_app.brand_logo->GetHeight());
        const double scale = image_w > 0 && image_h > 0
            ? std::min(
                static_cast<double>(logo_size) / static_cast<double>(image_w),
                static_cast<double>(logo_size) / static_cast<double>(image_h)
            )
            : 1.0;
        const int draw_w = std::max(1, static_cast<int>(std::round(image_w * scale)));
        const int draw_h = std::max(1, static_cast<int>(std::round(image_h * scale)));
        const int draw_x = logo_x + (logo_size - draw_w) / 2;
        const int draw_y = logo_y + (logo_size - draw_h) / 2;
        Gdiplus::Graphics graphics(dc);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        graphics.DrawImage(
            g_app.brand_logo,
            Gdiplus::Rect(draw_x, draw_y, draw_w, draw_h),
            0,
            0,
            image_w,
            image_h,
            Gdiplus::UnitPixel
        );
    } else if (g_app.brand_icon) {
        DrawIconEx(dc, logo_x, logo_y, g_app.brand_icon, logo_size, logo_size, 0, nullptr, DI_NORMAL);
    } else {
        draw_foglar_mark_fallback(dc, logo_x, logo_y, logo_size);
    }

    const int word_x = logo_x + logo_size + 12;
    const int word_y = 18;
    draw_text(dc, word_x, word_y, "FOGL", g_app.title_font, kFoglarOrange);
    const int fogl_width = text_width(dc, g_app.title_font, "FOGL");
    draw_text(dc, word_x + fogl_width + 2, word_y, "ESTING", g_app.title_font, kFoglarCream);
    const int title_width = fogl_width + 2 + text_width(dc, g_app.title_font, "ESTING");
    draw_text(dc, word_x + title_width + 12, word_y + 6, "V5.1", g_app.normal_font, kFoglarGreen);
    draw_text(dc, word_x, 58, "Soluciones termicas y metalicas", g_app.small_font, RGB(224, 214, 198));
    draw_text(dc, word_x, 82, "Cualquier lugar se vuelve hogar.", g_app.small_font, kFoglarGreen);
}

std::filesystem::path executable_directory() {
    char buffer[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return {};
    }
    return std::filesystem::path(buffer).parent_path();
}

void load_brand_icon(HWND hwnd) {
    HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrA(hwnd, GWLP_HINSTANCE));
    g_app.brand_logo = load_transparent_logo_mark_from_resource(instance, kLogoPngResourceId);

    HICON embedded_big = reinterpret_cast<HICON>(LoadImageA(
        instance,
        MAKEINTRESOURCEA(kAppIconResourceId),
        IMAGE_ICON,
        64,
        64,
        LR_DEFAULTCOLOR
    ));
    HICON embedded_small = reinterpret_cast<HICON>(LoadImageA(
        instance,
        MAKEINTRESOURCEA(kAppIconResourceId),
        IMAGE_ICON,
        16,
        16,
        LR_DEFAULTCOLOR
    ));
    if (embedded_big) {
        g_app.brand_icon = embedded_big;
        SendMessageA(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(embedded_big));
        SendMessageA(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(embedded_small ? embedded_small : embedded_big));
    }
}

bool point_in_rect(const RECT& rect, POINT point) {
    return point.x >= rect.left && point.x < rect.right &&
           point.y >= rect.top && point.y < rect.bottom;
}

template <typename Painter>
void paint_buffered(HWND hwnd, HDC target_dc, Painter painter) {
    RECT rect;
    GetClientRect(hwnd, &rect);
    const int width = std::max(1L, rect.right - rect.left);
    const int height = std::max(1L, rect.bottom - rect.top);

    HDC memory_dc = CreateCompatibleDC(target_dc);
    HBITMAP bitmap = CreateCompatibleBitmap(target_dc, width, height);
    if (!memory_dc || !bitmap) {
        if (bitmap) DeleteObject(bitmap);
        if (memory_dc) DeleteDC(memory_dc);
        painter(target_dc);
        return;
    }

    HGDIOBJ old_bitmap = SelectObject(memory_dc, bitmap);
    painter(memory_dc);
    BitBlt(target_dc, 0, 0, width, height, memory_dc, 0, 0, SRCCOPY);
    SelectObject(memory_dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(memory_dc);
}

struct PreviewTransform {
    double sheet_gap = 0.0;
    double base_scale = 1.0;
    double scale = 1.0;
    int base_origin_x = 0;
    int base_origin_y = 0;
    int origin_x = 0;
    int origin_y = 0;
};

PreviewTransform preview_transform(const RECT& preview, const smn::NestingResult& result) {
    constexpr int margin = 46;
    PreviewTransform transform;
    transform.sheet_gap = std::max(80.0, result.sheet.width * 0.08);
    const double logical_width = result.sheet_count() * result.sheet.width +
        (result.sheet_count() - 1) * transform.sheet_gap;
    const double scale_x = static_cast<double>(preview.right - preview.left - 2 * margin) / logical_width;
    const double scale_y = static_cast<double>(preview.bottom - preview.top - 2 * margin - 28) / result.sheet.height;
    transform.base_scale = std::max(0.01, std::min(scale_x, scale_y));
    transform.scale = transform.base_scale * std::clamp(g_app.preview_zoom, 0.1, 50.0);
    transform.base_origin_x = preview.left + margin;
    transform.base_origin_y = preview.top + margin + 28;
    transform.origin_x = transform.base_origin_x + static_cast<int>(std::round(g_app.preview_pan_x));
    transform.origin_y = transform.base_origin_y + static_cast<int>(std::round(g_app.preview_pan_y));
    return transform;
}

POINT project_point(const smn::Point& point, double offset_x, double sheet_height, double scale, int origin_x, int origin_y) {
    POINT projected;
    projected.x = origin_x + static_cast<int>(std::round((point.x + offset_x) * scale));
    projected.y = origin_y + static_cast<int>(std::round((sheet_height - point.y) * scale));
    return projected;
}

smn::Point unproject_point(POINT pt, double sheet_height, double scale, int origin_x, int origin_y) {
    smn::Point p;
    p.x = (pt.x - origin_x) / scale;
    p.y = sheet_height - (pt.y - origin_y) / scale;
    return p;
}

void draw_polygon(HDC dc, const smn::Polygon& polygon, double offset_x, double sheet_height, double scale, int origin_x, int origin_y, HBRUSH brush) {
    std::vector<POINT> points;
    points.reserve(polygon.outer.size());
    for (const auto& point : polygon.outer) {
        points.push_back(project_point(point, offset_x, sheet_height, scale, origin_x, origin_y));
    }
    SelectObject(dc, brush);
    Polygon(dc, points.data(), static_cast<int>(points.size()));

    HBRUSH white = CreateSolidBrush(RGB(255, 255, 255));
    SelectObject(dc, white);
    for (const auto& hole : polygon.holes) {
        std::vector<POINT> hole_points;
        hole_points.reserve(hole.size());
        for (const auto& point : hole) {
            hole_points.push_back(project_point(point, offset_x, sheet_height, scale, origin_x, origin_y));
        }
        Polygon(dc, hole_points.data(), static_cast<int>(hole_points.size()));
    }
    DeleteObject(white);
}

void draw_ring_outline(
    HDC dc,
    const std::vector<smn::Point>& ring,
    double offset_x,
    double sheet_height,
    double scale,
    int origin_x,
    int origin_y
) {
    if (ring.size() < 2) {
        return;
    }

    std::vector<POINT> points;
    points.reserve(ring.size());
    for (const auto& point : ring) {
        points.push_back(project_point(point, offset_x, sheet_height, scale, origin_x, origin_y));
    }
    Polyline(dc, points.data(), static_cast<int>(points.size()));
}

void draw_polygon_outline(
    HDC dc,
    const smn::Polygon& polygon,
    double offset_x,
    double sheet_height,
    double scale,
    int origin_x,
    int origin_y
) {
    draw_ring_outline(dc, polygon.outer, offset_x, sheet_height, scale, origin_x, origin_y);
    for (const auto& hole : polygon.holes) {
        draw_ring_outline(dc, hole, offset_x, sheet_height, scale, origin_x, origin_y);
    }
}

double nice_grid_step(double raw_step) {
    if (raw_step <= 0.0) {
        return 100.0;
    }
    const double exponent = std::floor(std::log10(raw_step));
    const double base = std::pow(10.0, exponent);
    const double normalized = raw_step / base;
    if (normalized <= 1.0) {
        return base;
    }
    if (normalized <= 2.0) {
        return 2.0 * base;
    }
    if (normalized <= 5.0) {
        return 5.0 * base;
    }
    return 10.0 * base;
}

smn::Point snap_to_grid(smn::Point point, double grid_step) {
    if (!g_app.snap_enabled || grid_step <= 0.0) {
        return point;
    }
    point.x = std::round(point.x / grid_step) * grid_step;
    point.y = std::round(point.y / grid_step) * grid_step;
    return point;
}

void push_undo() {
    g_app.undo_stack.push_back(g_app.annotations);
    if (g_app.undo_stack.size() > 100) {
        g_app.undo_stack.erase(g_app.undo_stack.begin());
    }
}

void clear_dimension_state();

void perform_undo(HWND hwnd) {
    if (g_app.undo_stack.empty()) return;
    g_app.annotations = g_app.undo_stack.back();
    g_app.undo_stack.pop_back();
    g_app.selected_annotation_index = -1;
    g_app.draft_active = false;
    clear_dimension_state();
    g_app.polyline_building = false;
    g_app.polyline_points.clear();
    g_app.arc_click_count = 0;
    RECT client;
    GetClientRect(hwnd, &client);
    RECT preview = preview_rect(client);
    InvalidateRect(hwnd, &preview, FALSE);
}

bool is_two_click_tool(ToolMode mode) {
    return mode == ToolMode::Line ||
        mode == ToolMode::Rectangle ||
        mode == ToolMode::Circle;
}

Annotation::Type annotation_type_for_tool(ToolMode mode) {
    switch (mode) {
        case ToolMode::Measure:
            return Annotation::Type::Measure;
        case ToolMode::Rectangle:
            return Annotation::Type::Rectangle;
        case ToolMode::Circle:
            return Annotation::Type::Circle;
        case ToolMode::Line:
        case ToolMode::Delete:
        default:
            return Annotation::Type::Line;
    }
}

const char* tool_command_name(ToolMode mode) {
    switch (mode) {
        case ToolMode::Select: return "SELECCIONAR";
        case ToolMode::Pan: return "PAN";
        case ToolMode::Measure: return "COTA";
        case ToolMode::Line: return "LINEA";
        case ToolMode::Rectangle: return "RECTANGULO";
        case ToolMode::Circle: return "CIRCULO";
        case ToolMode::Arc: return "ARCO";
        case ToolMode::Polyline: return "POLILINEA";
        case ToolMode::Text: return "TEXTO";
        case ToolMode::Delete: return "BORRAR";
    }
    return "COMANDO";
}

std::string tool_instruction() {
    if (g_app.draft_active) {
        switch (g_app.draft_tool) {
            case ToolMode::Line:
                return "segundo punto de linea. Enter/click derecho confirma, Esc cancela.";
            case ToolMode::Rectangle:
                return "esquina opuesta. Enter/click derecho confirma, Esc cancela.";
            case ToolMode::Circle:
                return "radio del circulo. Enter/click derecho confirma, Esc cancela.";
            default:
                break;
        }
    }
    if (g_app.polyline_building) {
        return "siguiente punto. Enter/click derecho termina, Esc cancela.";
    }
    if (g_app.dimension_building) {
        if (g_app.dimension_stage == 1) {
            return "segundo punto de la cota.";
        }
        return "ubica la cota con el mouse y click para confirmar.";
    }
    if (g_app.arc_click_count == 1) {
        return "segundo punto del arco.";
    }
    if (g_app.arc_click_count == 2) {
        return "punto final del arco.";
    }

    switch (g_app.tool_mode) {
        case ToolMode::Select:
            return "click para seleccionar, arrastra para mover, Delete para borrar.";
        case ToolMode::Pan:
            return "arrastra la vista. Rueda = zoom, doble click rueda = ajustar.";
        case ToolMode::Measure:
            return "selecciona un circulo o el primer punto de la cota.";
        case ToolMode::Line:
            return "primer punto de linea.";
        case ToolMode::Rectangle:
            return "primera esquina.";
        case ToolMode::Circle:
            return "centro del circulo.";
        case ToolMode::Arc:
            return "primer punto del arco.";
        case ToolMode::Polyline:
            return "primer punto de polilinea.";
        case ToolMode::Text:
            return "click para ubicar texto con fuente y tamano.";
        case ToolMode::Delete:
            return "click sobre una entidad para borrarla. Supr borra lo seleccionado.";
    }
    return "";
}

void invalidate_preview(HWND hwnd) {
    RECT client;
    GetClientRect(hwnd, &client);
    RECT preview = preview_rect(client);
    InvalidateRect(hwnd, &preview, FALSE);
}

void clear_dimension_state() {
    g_app.dimension_building = false;
    g_app.dimension_stage = 0;
    g_app.dimension_first_point = {0, 0};
}

void set_tool_mode(HWND hwnd, ToolMode mode) {
    if (g_app.draft_active && !g_app.annotations.empty()) {
        g_app.annotations.pop_back();
    }
    if (g_app.dimension_building && g_app.dimension_stage == 2 && !g_app.annotations.empty()) {
        g_app.annotations.pop_back();
    }
    clear_dimension_state();
    g_app.tool_mode = mode;
    if (mode != ToolMode::Polyline) {
        g_app.polyline_building = false;
        g_app.polyline_points.clear();
    }
    if (mode != ToolMode::Arc) {
        g_app.arc_click_count = 0;
    }
    g_app.draft_active = false;
    g_app.selected_annotation_index = -1;
    if (is_two_click_tool(mode) || mode == ToolMode::Arc || mode == ToolMode::Polyline || mode == ToolMode::Text) {
        g_app.last_draw_tool = mode;
    }
    SetFocus(hwnd);
    InvalidateRect(hwnd, nullptr, FALSE);
}

void cancel_cad_command(HWND hwnd) {
    if (g_app.draft_active && !g_app.annotations.empty()) {
        g_app.annotations.pop_back();
    }
    if (g_app.dimension_building && g_app.dimension_stage == 2 && !g_app.annotations.empty()) {
        g_app.annotations.pop_back();
    }
    clear_dimension_state();
    g_app.draft_active = false;
    g_app.polyline_building = false;
    g_app.polyline_points.clear();
    g_app.arc_click_count = 0;
    g_app.selected_annotation_index = -1;
    invalidate_preview(hwnd);
}

bool finish_polyline(HWND hwnd) {
    if (!g_app.polyline_building || g_app.polyline_points.size() < 2) {
        return false;
    }
    push_undo();
    Annotation ann;
    ann.type = Annotation::Type::Polyline;
    ann.start = g_app.polyline_points.front();
    ann.end = g_app.polyline_points.back();
    ann.points = g_app.polyline_points;
    g_app.annotations.push_back(std::move(ann));
    g_app.polyline_building = false;
    g_app.polyline_points.clear();
    invalidate_preview(hwnd);
    return true;
}

bool finish_draft(HWND hwnd) {
    if (!g_app.draft_active) {
        return false;
    }
    if (!g_app.annotations.empty()) {
        auto& ann = g_app.annotations.back();
        if (std::hypot(ann.end.x - ann.start.x, ann.end.y - ann.start.y) < 0.01) {
            g_app.annotations.pop_back();
        }
    }
    g_app.draft_active = false;
    invalidate_preview(hwnd);
    return true;
}

void start_or_finish_two_click_tool(HWND hwnd, smn::Point point) {
    if (!g_app.draft_active || g_app.draft_tool != g_app.tool_mode) {
        push_undo();
        Annotation ann;
        ann.type = annotation_type_for_tool(g_app.tool_mode);
        ann.start = point;
        ann.end = point;
        g_app.annotations.push_back(std::move(ann));
        g_app.draft_active = true;
        g_app.draft_tool = g_app.tool_mode;
        g_app.last_draw_tool = g_app.tool_mode;
    } else {
        g_app.annotations.back().end = point;
        finish_draft(hwnd);
        return;
    }
    invalidate_preview(hwnd);
}

RECT preview_rect(const RECT& rect) {
    const int toolbar_bottom = toolbar_bottom_for_client(rect);
    const int top = std::max(60, toolbar_bottom + 8);
    return {
        kPanelWidth + 24,
        top,
        rect.right - 24,
        rect.bottom - kBottomPanelHeight - kBottomPanelMargin - kBottomPanelGap
    };
}

RECT solution_list_rect(const RECT& rect) {
    return {0, 0, 0, 0}; // Hidden, layout manually
}

void draw_cad_command_bar(HDC dc, const RECT& preview) {
    RECT bar{preview.left, preview.bottom - 46, preview.right, preview.bottom};
    HBRUSH brush = CreateSolidBrush(RGB(14, 18, 17));
    FillRect(dc, &bar, brush);
    DeleteObject(brush);

    HPEN top_pen = CreatePen(PS_SOLID, 1, RGB(55, 68, 62));
    HGDIOBJ old_pen = SelectObject(dc, top_pen);
    MoveToEx(dc, bar.left, bar.top, nullptr);
    LineTo(dc, bar.right, bar.top);
    SelectObject(dc, old_pen);
    DeleteObject(top_pen);

    std::ostringstream command;
    command << "Comando: " << tool_command_name(g_app.tool_mode)
            << "  -  " << tool_instruction();
    draw_text(dc, bar.left + 12, bar.top + 7, command.str().c_str(), g_app.small_font, kFoglarCream);

    std::ostringstream coords;
    if (g_app.crosshair_valid) {
        coords << "X " << std::fixed << std::setprecision(1) << g_app.crosshair_sheet_pos.x
               << "   Y " << std::fixed << std::setprecision(1) << g_app.crosshair_sheet_pos.y
               << " mm";
    } else {
        coords << "L/LINEA  R/REC  C/CIRCULO  P/POLILINEA  A/ARCO  M/COTA  T/TEXTO  S/GRILLA  ESC/CANCELAR";
    }
    draw_text(dc, bar.left + 12, bar.top + 26, coords.str().c_str(), g_app.small_font, kFoglarSteel);

    const char* snap_label = g_app.osnap_active ? "OSNAP" : (g_app.grid_visible ? (g_app.snap_enabled ? "GRID+SNAP" : "GRID ON") : "GRID OFF");
    draw_text(dc, bar.right - 86, bar.top + 26, snap_label, g_app.small_font, g_app.osnap_active ? RGB(50, 205, 50) : kFoglarOrange);
}

double distance_between(smn::Point a, smn::Point b);
smn::Point default_dimension_position(smn::Point start, smn::Point end, smn::Point reference);
std::string format_dimension_value(double value, bool diameter, bool radius = false);

std::string lower_ascii_copy(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

std::string normalized_path_key(const std::filesystem::path& path) {
    std::error_code error;
    const auto canonical = std::filesystem::weakly_canonical(path, error);
    const auto normalized = (error ? path : canonical).lexically_normal();
    return lower_ascii_copy(normalized.string());
}

bool placement_matches_selection(const smn::PlacedPart& placement, const DxfSelection& selection) {
    if (!placement.source.empty() && placement.source != "editor") {
        if (normalized_path_key(placement.source) == normalized_path_key(selection.path)) {
            return true;
        }
    }

    const std::string stem = selection.path.stem().string();
    return placement.name == stem || placement.name.rfind(stem + "-", 0) == 0;
}

void draw_preview(HDC dc, const RECT& rect) {
    RECT preview = preview_rect(rect);
    HBRUSH background = CreateSolidBrush(RGB(18, 24, 23));
    FillRect(dc, &preview, background);
    DeleteObject(background);

    smn::NestingResult result;
    bool has_result = false;
    bool is_input_preview = false;
    bool input_preview_loading = false;
    std::string input_preview_message;
    size_t selection_count = 0;
    int current_iteration = 0;
    double current_saved_area = 0.0;
    bool running = g_app.running.load();
    bool paused = g_app.pause_requested.load();
    const int selected_dxf_index = selected_file_index();
    bool has_highlighted_selection = false;
    DxfSelection highlighted_selection;
    {
        std::lock_guard<std::mutex> lock(g_app.mutex);
        selection_count = g_app.selections.size();
        if (selected_dxf_index >= 0 && selected_dxf_index < static_cast<int>(g_app.selections.size())) {
            highlighted_selection = g_app.selections[static_cast<size_t>(selected_dxf_index)];
            has_highlighted_selection = true;
        }
        input_preview_loading = g_app.input_preview_loading;
        input_preview_message = g_app.input_preview_message;
        if (g_app.has_result && !g_app.result.placements.empty()) {
            result = g_app.result;
            has_result = true;
        } else if (g_app.has_input_preview && !g_app.input_preview_result.placements.empty()) {
            result = g_app.input_preview_result;
            has_result = true;
            is_input_preview = true;
        }
        current_iteration = g_app.current_iteration;
        current_saved_area = g_app.current_saved_area;
    }

    draw_text(
        dc,
        preview.left + 14,
        preview.top + 12,
        is_input_preview ? "Previsualizacion de DXF cargados" : "Vista previa del acomodo",
        g_app.normal_font,
        kFoglarCream
    );
    if (current_iteration > 0) {
        std::ostringstream status;
        status << (paused ? "Pausado" : running ? "Corriendo" : "Listo")
               << "  |  Iteracion " << current_iteration
               << "  |  Ahorro " << static_cast<long long>(current_saved_area) << " mm2";
        draw_text(dc, preview.left + 240, preview.top + 14, status.str().c_str(), g_app.small_font, kFoglarSteel);
    } else if (is_input_preview) {
        std::ostringstream status;
        status << result.placements.size() << " perfil(es) detectados";
        draw_text(dc, preview.left + 300, preview.top + 14, status.str().c_str(), g_app.small_font, kFoglarSteel);
    }
    if (has_highlighted_selection) {
        const std::string selected_label = "Resaltado: " + highlighted_selection.path.filename().string();
        draw_text(dc, preview.right - 270, preview.top + 14, selected_label.c_str(), g_app.small_font, kFoglarOrange);
    }
    if (!has_result || result.placements.empty()) {
        if (input_preview_loading) {
            draw_text(dc, preview.left + 18, preview.top + 46, "Cargando previsualizacion de los DXF...", g_app.small_font, kFoglarSteel);
        } else if (!input_preview_message.empty()) {
            draw_text(dc, preview.left + 18, preview.top + 46, input_preview_message.c_str(), g_app.small_font, RGB(238, 128, 104));
        } else if (selection_count > 0) {
            draw_text(dc, preview.left + 18, preview.top + 46, "Los DXF estan en la lista; preparando previsualizacion.", g_app.small_font, kFoglarSteel);
        } else {
            draw_text(dc, preview.left + 18, preview.top + 46, "Agrega una carpeta o archivos DXF para verlos antes del nesting.", g_app.small_font, kFoglarSteel);
        }
        return;
    }

    const PreviewTransform view = preview_transform(preview, result);
    const double sheet_gap = view.sheet_gap;
    const double scale = view.scale;
    const int origin_x = view.origin_x;
    const int origin_y = view.origin_y;

    const double grid_step = nice_grid_step((preview.right - preview.left) > 0 ? 46.0 / scale : 100.0);
    HPEN grid_pen = CreatePen(PS_SOLID, 1, RGB(38, 48, 45));
    HPEN major_grid_pen = CreatePen(PS_SOLID, 1, RGB(50, 63, 58));
    HPEN sheet_pen = CreatePen(PS_SOLID, 1, kFoglarGreen);
    HPEN part_pen = CreatePen(PS_SOLID, 1, kCadPartStroke);
    HPEN hole_pen = CreatePen(PS_SOLID, 1, kFoglarOrange);
    HPEN highlight_pen = CreatePen(PS_SOLID, 3, kFoglarOrange);
    HPEN highlight_hole_pen = CreatePen(PS_SOLID, 2, kFoglarCream);
    HBRUSH hollow = reinterpret_cast<HBRUSH>(GetStockObject(HOLLOW_BRUSH));

    const int saved_dc = SaveDC(dc);
    IntersectClipRect(dc, preview.left, preview.top + 38, preview.right, preview.bottom);
    SelectObject(dc, hollow);
    for (int sheet_index = 0; sheet_index < result.sheet_count(); ++sheet_index) {
        const double offset_x = sheet_index * (result.sheet.width + sheet_gap);
        if (g_app.grid_visible) {
            SelectObject(dc, grid_pen);
            for (double x = 0.0; x <= result.sheet.width + 1e-6; x += grid_step) {
                const bool major = std::fmod(std::round(x / grid_step), 5.0) == 0.0;
                SelectObject(dc, major ? major_grid_pen : grid_pen);
                POINT a = project_point({x, 0.0}, offset_x, result.sheet.height, scale, origin_x, origin_y);
                POINT b = project_point({x, result.sheet.height}, offset_x, result.sheet.height, scale, origin_x, origin_y);
                MoveToEx(dc, a.x, a.y, nullptr);
                LineTo(dc, b.x, b.y);
            }
            for (double y = 0.0; y <= result.sheet.height + 1e-6; y += grid_step) {
                const bool major = std::fmod(std::round(y / grid_step), 5.0) == 0.0;
                SelectObject(dc, major ? major_grid_pen : grid_pen);
                POINT a = project_point({0.0, y}, offset_x, result.sheet.height, scale, origin_x, origin_y);
                POINT b = project_point({result.sheet.width, y}, offset_x, result.sheet.height, scale, origin_x, origin_y);
                MoveToEx(dc, a.x, a.y, nullptr);
                LineTo(dc, b.x, b.y);
            }
        }

        SelectObject(dc, sheet_pen);
        POINT a = project_point({0.0, 0.0}, offset_x, result.sheet.height, scale, origin_x, origin_y);
        POINT b = project_point({result.sheet.width, result.sheet.height}, offset_x, result.sheet.height, scale, origin_x, origin_y);
        Rectangle(dc, a.x, b.y, b.x, a.y);
    }

    SelectObject(dc, part_pen);
    for (const auto& placement : result.placements) {
        const double offset_x = placement.sheet_index * (result.sheet.width + sheet_gap);
        SelectObject(dc, part_pen);
        draw_ring_outline(dc, placement.geometry.outer, offset_x, result.sheet.height, scale, origin_x, origin_y);
        SelectObject(dc, hole_pen);
        for (const auto& hole : placement.geometry.holes) {
            draw_ring_outline(dc, hole, offset_x, result.sheet.height, scale, origin_x, origin_y);
        }
    }
    if (has_highlighted_selection) {
        for (const auto& placement : result.placements) {
            if (!placement_matches_selection(placement, highlighted_selection)) {
                continue;
            }
            const double offset_x = placement.sheet_index * (result.sheet.width + sheet_gap);
            SelectObject(dc, highlight_pen);
            draw_ring_outline(dc, placement.geometry.outer, offset_x, result.sheet.height, scale, origin_x, origin_y);
            SelectObject(dc, highlight_hole_pen);
            for (const auto& hole : placement.geometry.holes) {
                draw_ring_outline(dc, hole, offset_x, result.sheet.height, scale, origin_x, origin_y);
            }
        }
    }

    RestoreDC(dc, saved_dc);

    if (is_input_preview) {
        draw_text(
            dc,
            preview.left + 14,
            preview.bottom - 24,
            "Previsualizacion solamente: no es el acomodo optimizado.",
            g_app.small_font,
            kFoglarSteel
        );
    }

    // Draw annotations over everything
    const int annotation_dc = SaveDC(dc);
    IntersectClipRect(dc, preview.left, preview.top + 38, preview.right, preview.bottom - 46);

    HPEN measure_pen = CreatePen(PS_SOLID, 1, kFoglarOrange);
    HPEN draft_pen = CreatePen(PS_SOLID, 1, kCadPartStroke);
    HPEN select_pen = CreatePen(PS_SOLID, 3, RGB(255, 236, 205));
    HPEN polyline_pen = CreatePen(PS_SOLID, 1, kCadPartStroke);
    HBRUSH measure_brush = CreateSolidBrush(kFoglarOrange);
    HBRUSH select_brush = CreateSolidBrush(RGB(255, 236, 205));

    for (size_t i = 0; i < g_app.annotations.size(); ++i) {
        const auto& ann = g_app.annotations[i];
        bool is_selected = (static_cast<int>(i) == g_app.selected_annotation_index);

        POINT pt_start = project_point(ann.start, 0.0, result.sheet.height, scale, origin_x, origin_y);
        POINT pt_end = project_point(ann.end, 0.0, result.sheet.height, scale, origin_x, origin_y);
        
        if (ann.type == Annotation::Type::Measure) {
            SelectObject(dc, is_selected ? select_pen : measure_pen);
            SelectObject(dc, is_selected ? select_brush : measure_brush);
            const auto draw_arrow = [&](smn::Point tip, double direction_x, double direction_y) {
                const double direction_length = std::max(1e-6, std::hypot(direction_x, direction_y));
                const double ux = direction_x / direction_length;
                const double uy = direction_y / direction_length;
                const double nx = -uy;
                const double ny = ux;
                const double length = 11.0 / scale;
                const double width = 4.0 / scale;
                const smn::Point base{tip.x - ux * length, tip.y - uy * length};
                POINT arrow[] = {
                    project_point(tip, 0.0, result.sheet.height, scale, origin_x, origin_y),
                    project_point({base.x + nx * width, base.y + ny * width}, 0.0, result.sheet.height, scale, origin_x, origin_y),
                    project_point({base.x - nx * width, base.y - ny * width}, 0.0, result.sheet.height, scale, origin_x, origin_y)
                };
                Polygon(dc, arrow, 3);
            };

            if (ann.diameter_dimension || ann.radius_dimension) {
                const double radius = distance_between(ann.start, ann.end);
                smn::Point label = distance_between(ann.mid, {0.0, 0.0}) > 1e-6 ? ann.mid : ann.end;
                smn::Point dir{label.x - ann.start.x, label.y - ann.start.y};
                double dir_len = std::max(1e-6, std::hypot(dir.x, dir.y));
                dir.x /= dir_len;
                dir.y /= dir_len;
                smn::Point circle_point{ann.start.x + dir.x * radius, ann.start.y + dir.y * radius};
                POINT p_circle = project_point(circle_point, 0.0, result.sheet.height, scale, origin_x, origin_y);
                POINT p_label = project_point(label, 0.0, result.sheet.height, scale, origin_x, origin_y);
                MoveToEx(dc, p_circle.x, p_circle.y, nullptr);
                LineTo(dc, p_label.x, p_label.y);
                draw_arrow(circle_point, -dir.x, -dir.y);
                const std::string value = ann.text.empty()
                    ? format_dimension_value(ann.radius_dimension ? radius : radius * 2.0, ann.diameter_dimension, ann.radius_dimension)
                    : ann.text;
                {
                    // Scaled dimension font - grows/shrinks with zoom
                    const int dim_font_h = -static_cast<int>(std::round(std::clamp(scale * 3.5, 9.0, 48.0)));
                    HFONT dim_font = CreateFontA(dim_font_h, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        CLEARTYPE_QUALITY, DEFAULT_PITCH, "ISOCPEUR");
                    HFONT dim_font_use = dim_font ? dim_font : g_app.iso_font;
                    draw_text(dc, p_label.x + 6, p_label.y - 8, value.c_str(), dim_font_use, is_selected ? RGB(255, 236, 205) : kFoglarOrange);
                    if (dim_font) DeleteObject(dim_font);
                }
            } else {
                const double dx = ann.end.x - ann.start.x;
                const double dy = ann.end.y - ann.start.y;
                const double dist_mm = std::sqrt(dx * dx + dy * dy);
                if (dist_mm > 1e-6) {
                    const double ux = dx / dist_mm;
                    const double uy = dy / dist_mm;
                    const double nx = -uy;
                    const double ny = ux;
                    const smn::Point measured_mid{(ann.start.x + ann.end.x) * 0.5, (ann.start.y + ann.end.y) * 0.5};
                    smn::Point label = distance_between(ann.mid, {0.0, 0.0}) > 1e-6
                        ? ann.mid
                        : default_dimension_position(ann.start, ann.end, measured_mid);
                    double offset = (label.x - measured_mid.x) * nx + (label.y - measured_mid.y) * ny;
                    if (std::abs(offset) < 2.0 / scale) {
                        offset = 22.0 / scale;
                    }
                    const smn::Point dim_start{ann.start.x + nx * offset, ann.start.y + ny * offset};
                    const smn::Point dim_end{ann.end.x + nx * offset, ann.end.y + ny * offset};
                    const smn::Point dim_mid{(dim_start.x + dim_end.x) * 0.5, (dim_start.y + dim_end.y) * 0.5};
                    POINT p_dim_start = project_point(dim_start, 0.0, result.sheet.height, scale, origin_x, origin_y);
                    POINT p_dim_end = project_point(dim_end, 0.0, result.sheet.height, scale, origin_x, origin_y);
                    POINT p_ann_start = project_point(ann.start, 0.0, result.sheet.height, scale, origin_x, origin_y);
                    POINT p_ann_end = project_point(ann.end, 0.0, result.sheet.height, scale, origin_x, origin_y);
                    MoveToEx(dc, p_ann_start.x, p_ann_start.y, nullptr);
                    LineTo(dc, p_dim_start.x, p_dim_start.y);
                    MoveToEx(dc, p_ann_end.x, p_ann_end.y, nullptr);
                    LineTo(dc, p_dim_end.x, p_dim_end.y);
                    MoveToEx(dc, p_dim_start.x, p_dim_start.y, nullptr);
                    LineTo(dc, p_dim_end.x, p_dim_end.y);
                    draw_arrow(dim_start, ux, uy);
                    draw_arrow(dim_end, -ux, -uy);
                    const std::string value = ann.text.empty()
                        ? format_dimension_value(ann.override_measure > 0.0 ? ann.override_measure : dist_mm, false)
                        : ann.text;
                    POINT p_text = project_point(dim_mid, 0.0, result.sheet.height, scale, origin_x, origin_y);
                    {
                        // Scaled dimension font - grows/shrinks with zoom
                        const int dim_font_h = -static_cast<int>(std::round(std::clamp(scale * 3.5, 9.0, 48.0)));
                        HFONT dim_font = CreateFontA(dim_font_h, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH, "ISOCPEUR");
                        HFONT dim_font_use = dim_font ? dim_font : g_app.iso_font;
                        draw_text(dc, p_text.x - 22, p_text.y - 18, value.c_str(), dim_font_use, is_selected ? RGB(255, 236, 205) : kFoglarOrange);
                        if (dim_font) DeleteObject(dim_font);
                    }
                }
            }
        } else if (ann.type == Annotation::Type::Text) {
            const int font_height = -static_cast<int>(std::round(std::clamp(ann.text_size, 6.0, 96.0)));
            HFONT text_font = CreateFontA(
                font_height,
                0,
                0,
                0,
                FW_NORMAL,
                FALSE,
                FALSE,
                FALSE,
                DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY,
                DEFAULT_PITCH,
                ann.text_font.empty() ? "ISOCPEUR" : ann.text_font.c_str()
            );
            draw_text(dc, pt_start.x, pt_start.y, ann.text.c_str(), text_font ? text_font : g_app.iso_font, is_selected ? RGB(255, 236, 205) : kCadPartStroke);
            if (text_font) {
                DeleteObject(text_font);
            }
        } else if (ann.type == Annotation::Type::Line) {
            SelectObject(dc, is_selected ? select_pen : draft_pen);
            MoveToEx(dc, pt_start.x, pt_start.y, nullptr);
            LineTo(dc, pt_end.x, pt_end.y);
        } else if (ann.type == Annotation::Type::Rectangle) {
            SelectObject(dc, is_selected ? select_pen : draft_pen);
            SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
            Rectangle(dc, std::min(pt_start.x, pt_end.x), std::min(pt_start.y, pt_end.y), std::max(pt_start.x, pt_end.x), std::max(pt_start.y, pt_end.y));
        } else if (ann.type == Annotation::Type::Circle) {
            SelectObject(dc, is_selected ? select_pen : draft_pen);
            SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
            const int radius = static_cast<int>(std::round(std::hypot(ann.end.x - ann.start.x, ann.end.y - ann.start.y) * scale));
            Ellipse(dc, pt_start.x - radius, pt_start.y - radius, pt_start.x + radius, pt_start.y + radius);
        } else if (ann.type == Annotation::Type::Arc) {
            // Draw 3-point arc as polyline approximation
            SelectObject(dc, is_selected ? select_pen : draft_pen);
            const double ax = ann.start.x, ay = ann.start.y;
            const double bx = ann.mid.x, by = ann.mid.y;
            const double cx = ann.end.x, cy = ann.end.y;
            const double d = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
            if (std::abs(d) > 1e-9) {
                const double cux = ((ax*ax+ay*ay)*(by-cy)+(bx*bx+by*by)*(cy-ay)+(cx*cx+cy*cy)*(ay-by))/d;
                const double cuy = ((ax*ax+ay*ay)*(cx-bx)+(bx*bx+by*by)*(ax-cx)+(cx*cx+cy*cy)*(bx-ax))/d;
                const double r = std::hypot(ax-cux, ay-cuy);
                double sa = std::atan2(ay-cuy, ax-cux);
                double ea = std::atan2(cy-cuy, cx-cux);
                double ma = std::atan2(by-cuy, bx-cux);
                // Ensure mid is between start and end going the right way
                if (sa > ea) std::swap(sa, ea);
                if (ma < sa || ma > ea) { double t = sa; sa = ea; ea = t + 2.0*3.14159265358979; }
                constexpr int segs = 48;
                POINT prev_pt = project_point({cux + r*std::cos(sa), cuy + r*std::sin(sa)}, 0.0, result.sheet.height, scale, origin_x, origin_y);
                for (int s = 1; s <= segs; ++s) {
                    double a = sa + (ea - sa) * s / segs;
                    POINT cur_pt = project_point({cux + r*std::cos(a), cuy + r*std::sin(a)}, 0.0, result.sheet.height, scale, origin_x, origin_y);
                    MoveToEx(dc, prev_pt.x, prev_pt.y, nullptr);
                    LineTo(dc, cur_pt.x, cur_pt.y);
                    prev_pt = cur_pt;
                }
            }
        } else if (ann.type == Annotation::Type::Polyline) {
            SelectObject(dc, is_selected ? select_pen : polyline_pen);
            for (size_t j = 0; j + 1 < ann.points.size(); ++j) {
                POINT p1 = project_point(ann.points[j], 0.0, result.sheet.height, scale, origin_x, origin_y);
                POINT p2 = project_point(ann.points[j+1], 0.0, result.sheet.height, scale, origin_x, origin_y);
                MoveToEx(dc, p1.x, p1.y, nullptr);
                LineTo(dc, p2.x, p2.y);
            }
        }
    }
    
    // Draw polyline being constructed
    if (g_app.polyline_building && g_app.polyline_points.size() >= 1) {
        SelectObject(dc, polyline_pen);
        for (size_t j = 0; j + 1 < g_app.polyline_points.size(); ++j) {
            POINT p1 = project_point(g_app.polyline_points[j], 0.0, result.sheet.height, scale, origin_x, origin_y);
            POINT p2 = project_point(g_app.polyline_points[j+1], 0.0, result.sheet.height, scale, origin_x, origin_y);
            MoveToEx(dc, p1.x, p1.y, nullptr);
            LineTo(dc, p2.x, p2.y);
        }
        if (g_app.crosshair_valid) {
            POINT p1 = project_point(g_app.polyline_points.back(), 0.0, result.sheet.height, scale, origin_x, origin_y);
            POINT p2 = project_point(g_app.crosshair_sheet_pos, 0.0, result.sheet.height, scale, origin_x, origin_y);
            MoveToEx(dc, p1.x, p1.y, nullptr);
            LineTo(dc, p2.x, p2.y);
        }
    }

    if (g_app.arc_click_count > 0 && g_app.crosshair_valid) {
        SelectObject(dc, draft_pen);
        POINT p1 = project_point(g_app.arc_p1, 0.0, result.sheet.height, scale, origin_x, origin_y);
        POINT current = project_point(g_app.crosshair_sheet_pos, 0.0, result.sheet.height, scale, origin_x, origin_y);
        if (g_app.arc_click_count == 1) {
            MoveToEx(dc, p1.x, p1.y, nullptr);
            LineTo(dc, current.x, current.y);
        } else {
            POINT p2 = project_point(g_app.arc_p2, 0.0, result.sheet.height, scale, origin_x, origin_y);
            MoveToEx(dc, p1.x, p1.y, nullptr);
            LineTo(dc, p2.x, p2.y);
            LineTo(dc, current.x, current.y);
        }
    }
    
    // Crosshair
    if (g_app.crosshair_valid && g_app.crosshair_pos.x >= preview.left && g_app.crosshair_pos.x <= preview.right) {
        HPEN cross_pen = CreatePen(PS_DOT, 1, RGB(120, 140, 130));
        SelectObject(dc, cross_pen);
        MoveToEx(dc, g_app.crosshair_pos.x, preview.top + 38, nullptr);
        LineTo(dc, g_app.crosshair_pos.x, preview.bottom);
        MoveToEx(dc, preview.left, g_app.crosshair_pos.y, nullptr);
        LineTo(dc, preview.right, g_app.crosshair_pos.y);
        DeleteObject(cross_pen);
        
        // Coordinates display
        char coord_buf[64];
        snprintf(coord_buf, sizeof(coord_buf), "X: %.1f  Y: %.1f mm", g_app.crosshair_sheet_pos.x, g_app.crosshair_sheet_pos.y);
        draw_text(dc, preview.left + 14, preview.bottom - 22, coord_buf, g_app.small_font, kFoglarSteel);
        
        // Snap indicator and OSNAP marker
        if (g_app.osnap_active) {
            POINT osnap_px = project_point(g_app.crosshair_sheet_pos, 0.0, result.sheet.height, scale, origin_x, origin_y);
            HPEN osnap_pen = CreatePen(PS_SOLID, 2, RGB(50, 205, 50)); // Green AutoCAD snap box
            SelectObject(dc, osnap_pen);
            SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
            Rectangle(dc, osnap_px.x - 5, osnap_px.y - 5, osnap_px.x + 6, osnap_px.y + 6);
            DeleteObject(osnap_pen);
            draw_text(dc, preview.right - 90, preview.bottom - 22, "OSNAP", g_app.small_font, RGB(50, 205, 50));
        } else if (g_app.grid_visible) {
            draw_text(dc, preview.right - 104, preview.bottom - 22, g_app.snap_enabled ? "GRID+SNAP" : "GRID", g_app.small_font, kFoglarOrange);
        }
    }

    RestoreDC(dc, annotation_dc);
    DeleteObject(measure_pen);
    DeleteObject(draft_pen);
    DeleteObject(select_pen);
    DeleteObject(polyline_pen);
    DeleteObject(measure_brush);
    DeleteObject(select_brush);

    draw_cad_command_bar(dc, preview);

    DeleteObject(grid_pen);
    DeleteObject(major_grid_pen);
    DeleteObject(sheet_pen);
    DeleteObject(part_pen);
    DeleteObject(hole_pen);
    DeleteObject(highlight_pen);
    DeleteObject(highlight_hole_pen);
}

std::string format_area_m2(double area_mm2) {
    std::ostringstream text;
    text << std::fixed << std::setprecision(area_mm2 >= 10000000.0 ? 1 : 3)
         << (area_mm2 / 1000000.0);
    return text.str();
}

std::string format_percent_axis(double value) {
    std::ostringstream text;
    text << std::fixed << std::setprecision(value >= 10.0 ? 1 : 2) << value << "%";
    return text.str();
}

void draw_graph_y_ticks(
    HDC dc,
    const RECT& plot,
    double min_value,
    double max_value,
    bool area_units,
    HPEN grid_pen
) {
    constexpr int ticks = 3;
    for (int tick = 0; tick < ticks; ++tick) {
        const double ratio = static_cast<double>(tick) / static_cast<double>(ticks - 1);
        const double value = min_value + (max_value - min_value) * ratio;
        const int y = plot.bottom - static_cast<int>(std::round(ratio * (plot.bottom - plot.top)));
        SelectObject(dc, grid_pen);
        MoveToEx(dc, plot.left, y, nullptr);
        LineTo(dc, plot.right, y);

        const std::string label = area_units ? format_area_m2(value) : format_percent_axis(value);
        draw_text(dc, plot.left - 68, y - 7, label.c_str(), g_app.small_font, kFoglarSteel);
    }
}

void draw_graph_x_labels(HDC dc, const RECT& plot, int first_iteration, int last_iteration) {
    if (first_iteration <= 0 || last_iteration <= 0) {
        return;
    }

    const std::string first = std::to_string(first_iteration);
    const std::string last = std::to_string(last_iteration);
    draw_text(dc, plot.left - 2, plot.bottom + 5, first.c_str(), g_app.small_font, kFoglarSteel);
    draw_text(dc, plot.right - 34, plot.bottom + 5, last.c_str(), g_app.small_font, kFoglarSteel);

    if (last_iteration - first_iteration > 2) {
        const int middle_iteration = first_iteration + (last_iteration - first_iteration) / 2;
        const std::string middle = std::to_string(middle_iteration);
        const int middle_x = plot.left + (plot.right - plot.left) / 2 - 14;
        draw_text(dc, middle_x, plot.bottom + 5, middle.c_str(), g_app.small_font, kFoglarSteel);
    }
}

void draw_graph(HDC dc, const RECT& rect) {
    HBRUSH background = CreateSolidBrush(kFoglarCoal);
    FillRect(dc, &rect, background);
    DeleteObject(background);

    std::vector<smn::IterationStats> history_raw;
    int current_iteration = 0;
    {
        std::lock_guard<std::mutex> lock(g_app.mutex);
        history_raw = g_app.history;
        current_iteration = g_app.current_iteration;
    }

    std::vector<smn::IterationStats> history = history_raw;

    if (history.size() < 2) {
        draw_text(dc, rect.left + 14, rect.top + 10, "Area ocupada (m2)", g_app.normal_font, kFoglarCream);
        draw_text(dc, rect.left + 58, rect.top + 42, "El grafico se actualiza en cada iteracion...", g_app.small_font, kFoglarSteel);
        return;
    }

    const int mid_y = rect.top + (rect.bottom - rect.top) / 2;
    const RECT plot1{rect.left + 82, rect.top + 42, rect.right - 22, mid_y - 26};
    const RECT plot2{rect.left + 82, mid_y + 42, rect.right - 22, rect.bottom - 46};

    draw_text(dc, rect.left + 14, rect.top + 10, "Area ocupada mejor (m2)", g_app.normal_font, kFoglarCream);
    draw_text(dc, rect.left + 14, mid_y + 10, "Convergencia (saltos entre exitos)", g_app.normal_font, kFoglarCream);

    HPEN grid_pen = CreatePen(PS_SOLID, 1, RGB(62, 67, 63));
    HPEN axis_pen = CreatePen(PS_SOLID, 1, kFoglarGreen);

    const size_t max_visible_points = 220;
    const size_t first_visible = history.size() > max_visible_points
        ? history.size() - max_visible_points
        : 0;
    const size_t visible_count = history.size() - first_visible;

    double min_occ = history[first_visible].best_occupied_area;
    double max_occ = history[first_visible].best_occupied_area;
    
    for (size_t visible_index = 0; visible_index < visible_count; ++visible_index) {
        size_t i = first_visible + visible_index;
        min_occ = std::min(min_occ, history[i].best_occupied_area);
        max_occ = std::max(max_occ, history[i].best_occupied_area);
    }

    struct SuccessJump {
        int iteration = 0;
        double area_jump = 0.0;
        double percent_jump = 0.0;
    };
    std::vector<SuccessJump> success_jumps;
    double previous_success_area = history.front().best_occupied_area;
    for (size_t i = 1; i < history.size(); ++i) {
        const double area = history[i].best_occupied_area;
        if (area + 1e-6 >= previous_success_area) {
            continue;
        }
        const double jump = previous_success_area - area;
        success_jumps.push_back({
            history[i].iteration,
            jump,
            jump / std::max(1.0, previous_success_area) * 100.0,
        });
        previous_success_area = area;
    }

    const size_t max_visible_successes = 120;
    const size_t first_visible_success = success_jumps.size() > max_visible_successes
        ? success_jumps.size() - max_visible_successes
        : 0;
    std::vector<SuccessJump> plotted_jumps;
    plotted_jumps.reserve(success_jumps.size() - first_visible_success);
    for (size_t i = first_visible_success; i < success_jumps.size(); ++i) {
        plotted_jumps.push_back(success_jumps[i]);
    }

    double max_conv = 0.0;
    for (const auto& jump : plotted_jumps) {
        max_conv = std::max(max_conv, jump.percent_jump);
    }
    
    const double occ_span = std::max(1.0, max_occ - min_occ);
    const double conv_span = std::max(0.01, max_conv);
    const double conv_axis_max = std::max(0.01, max_conv);

    draw_graph_y_ticks(dc, plot1, min_occ, max_occ, true, grid_pen);
    draw_graph_y_ticks(dc, plot2, 0.0, conv_axis_max, false, grid_pen);
    draw_graph_x_labels(dc, plot1, history[first_visible].iteration, history.back().iteration);
    if (!plotted_jumps.empty()) {
        draw_graph_x_labels(dc, plot2, plotted_jumps.front().iteration, plotted_jumps.back().iteration);
    } else {
        draw_graph_x_labels(dc, plot2, history[first_visible].iteration, history.back().iteration);
    }

    SelectObject(dc, axis_pen);
    MoveToEx(dc, plot1.left, plot1.top, nullptr);
    LineTo(dc, plot1.left, plot1.bottom);
    LineTo(dc, plot1.right, plot1.bottom);
    MoveToEx(dc, plot2.left, plot2.top, nullptr);
    LineTo(dc, plot2.left, plot2.bottom);
    LineTo(dc, plot2.right, plot2.bottom);
    DeleteObject(axis_pen);

    // Draw Plot 1 (Occupied Area)
    HPEN line_pen1 = CreatePen(PS_SOLID, 2, kFoglarOrange);
    SelectObject(dc, line_pen1);
    for (size_t visible_index = 0; visible_index < visible_count; ++visible_index) {
        const size_t i = first_visible + visible_index;
        const double x_ratio = visible_count <= 1 ? 0.0 : static_cast<double>(visible_index) / static_cast<double>(visible_count - 1);
        
        // Since smaller area is better, we might want lower values at the bottom.
        const double y_ratio = (history[i].best_occupied_area - min_occ) / occ_span;
        
        const int x = plot1.left + static_cast<int>(x_ratio * (plot1.right - plot1.left));
        const int y = plot1.bottom - static_cast<int>(y_ratio * (plot1.bottom - plot1.top));
        if (visible_index == 0) MoveToEx(dc, x, y, nullptr);
        else LineTo(dc, x, y);
    }
    DeleteObject(line_pen1);

    HPEN line_pen2 = CreatePen(PS_SOLID, 2, kFoglarCream);
    HBRUSH point_brush = CreateSolidBrush(kFoglarCream);
    SelectObject(dc, line_pen2);
    SelectObject(dc, point_brush);
    if (plotted_jumps.empty()) {
        draw_text(dc, plot2.left + 4, plot2.top + 8, "Todavia no hubo otro caso de exito.", g_app.small_font, kFoglarSteel);
    } else {
        for (size_t i = 0; i < plotted_jumps.size(); ++i) {
            const double x_ratio = plotted_jumps.size() <= 1
                ? 0.0
                : static_cast<double>(i) / static_cast<double>(plotted_jumps.size() - 1);
            const double y_ratio = plotted_jumps[i].percent_jump / conv_span;
            const int x = plot2.left + static_cast<int>(x_ratio * (plot2.right - plot2.left));
            const int y = plot2.bottom - static_cast<int>(y_ratio * (plot2.bottom - plot2.top));
            if (i == 0) {
                MoveToEx(dc, x, y, nullptr);
            } else {
                LineTo(dc, x, y);
            }
            Ellipse(dc, x - 3, y - 3, x + 4, y + 4);
        }
    }
    DeleteObject(point_brush);
    DeleteObject(line_pen2);
    DeleteObject(grid_pen);

    std::ostringstream label;
    const double last_jump = success_jumps.empty() ? 0.0 : success_jumps.back().percent_jump;
    const double last_jump_area = success_jumps.empty() ? 0.0 : success_jumps.back().area_jump;
    label << "Iteracion: " << current_iteration
          << "   Area: " << std::fixed << std::setprecision(4) << (history.back().best_occupied_area / 1000000.0) << " m2"
          << "   Ultimo salto exitoso: " << std::fixed << std::setprecision(2) << last_jump << " %"
          << " (" << std::fixed << std::setprecision(0) << last_jump_area << " mm2)"
          << "   Exitos: " << success_jumps.size();
    draw_text(dc, plot2.left, rect.bottom - 23, label.str().c_str(), g_app.small_font, kFoglarSteel);
}

void draw_solution_panel(HDC dc, const RECT& rect) {
    RECT panel = solution_list_rect(rect);
    if (panel.right <= panel.left || panel.bottom <= panel.top) {
        return;
    }

    HBRUSH background = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(dc, &panel, background);
    DeleteObject(background);
    draw_text(dc, panel.left + 14, panel.top + 10, "Mejores soluciones", g_app.normal_font, RGB(51, 65, 85));

    std::vector<RankedSolution> ranked;
    {
        std::lock_guard<std::mutex> lock(g_app.mutex);
        ranked = g_app.ranked_solutions;
    }

    if (ranked.empty()) {
        draw_text(dc, panel.left + 14, panel.top + 42, "Aca aparecen solo las mejoras.", g_app.small_font, RGB(100, 116, 139));
        return;
    }

    std::ostringstream summary;
    summary << ranked.size() << " mejora(s) guardadas";
    draw_text(dc, panel.left + 14, panel.bottom - 24, summary.str().c_str(), g_app.small_font, RGB(71, 85, 105));
}

smn::NestingResult scale_result(const smn::NestingResult& input, double factor) {
    if (std::abs(factor - 1.0) < 1e-9) {
        return input;
    }
    smn::NestingResult output = input;
    output.sheet.width *= factor;
    output.sheet.height *= factor;
    for (auto& placement : output.placements) {
        placement.geometry = smn::scale_polygon(placement.geometry, factor);
        placement.x *= factor;
        placement.y *= factor;
        placement.source_area *= factor * factor;
    }
    for (auto& part : output.unplaced) {
        part.geometry = smn::scale_polygon(part.geometry, factor);
    }
    return output;
}

void scale_parts(std::vector<smn::Part>& parts, double factor) {
    if (std::abs(factor - 1.0) < 1e-9) {
        return;
    }
    for (auto& part : parts) {
        part.geometry = smn::scale_polygon(part.geometry, factor);
    }
}

bool close_points(smn::Point first, smn::Point second, double tolerance = 1.0) {
    return std::hypot(first.x - second.x, first.y - second.y) <= tolerance;
}

void append_editor_part_from_ring(
    std::vector<smn::Part>& parts,
    std::vector<smn::Point> ring,
    const std::string& name
) {
    ring = smn::ensure_closed(std::move(ring));
    if (ring.size() < 4 || smn::ring_area(ring) < 1.0) {
        return;
    }

    smn::Part part;
    part.name = name;
    part.source = "editor";
    part.quantity = 1;
    part.can_rotate = true;
    part.geometry.outer = std::move(ring);
    part.geometry = smn::normalize_to_origin(part.geometry);
    if (part.bounds().valid() && part.area() >= 1.0) {
        parts.push_back(std::move(part));
    }
}

std::vector<smn::Point> circle_ring(smn::Point center, double radius) {
    std::vector<smn::Point> ring;
    if (radius < 0.5) {
        return ring;
    }

    constexpr int kSegments = 96;
    ring.reserve(kSegments + 1);
    for (int index = 0; index < kSegments; ++index) {
        const double angle = 2.0 * 3.14159265358979323846 * static_cast<double>(index) / static_cast<double>(kSegments);
        ring.push_back({
            center.x + std::cos(angle) * radius,
            center.y + std::sin(angle) * radius,
        });
    }
    ring.push_back(ring.front());
    return ring;
}

std::vector<smn::Part> editor_parts_from_annotations(const std::vector<Annotation>& annotations) {
    std::vector<smn::Part> parts;
    int next_index = 1;

    for (const auto& ann : annotations) {
        if (ann.type == Annotation::Type::Rectangle) {
            std::vector<smn::Point> ring = {
                ann.start,
                {ann.end.x, ann.start.y},
                ann.end,
                {ann.start.x, ann.end.y},
                ann.start,
            };
            append_editor_part_from_ring(parts, std::move(ring), "editor_rect_" + std::to_string(next_index++));
        } else if (ann.type == Annotation::Type::Circle) {
            const double radius = std::hypot(ann.end.x - ann.start.x, ann.end.y - ann.start.y);
            append_editor_part_from_ring(parts, circle_ring(ann.start, radius), "editor_circle_" + std::to_string(next_index++));
        } else if (ann.type == Annotation::Type::Polyline && ann.points.size() >= 3) {
            append_editor_part_from_ring(parts, ann.points, "editor_polyline_" + std::to_string(next_index++));
        }
    }

    struct Segment {
        smn::Point start;
        smn::Point end;
        bool used = false;
    };
    std::vector<Segment> segments;
    for (const auto& ann : annotations) {
        if (ann.type == Annotation::Type::Line && !close_points(ann.start, ann.end, 0.01)) {
            segments.push_back({ann.start, ann.end, false});
        }
    }

    for (size_t seed = 0; seed < segments.size(); ++seed) {
        if (segments[seed].used) {
            continue;
        }

        std::vector<smn::Point> ring;
        ring.push_back(segments[seed].start);
        ring.push_back(segments[seed].end);
        segments[seed].used = true;
        smn::Point current = segments[seed].end;

        bool closed = false;
        while (true) {
            if (ring.size() >= 4 && close_points(current, ring.front())) {
                ring.back() = ring.front();
                closed = true;
                break;
            }

            int next = -1;
            bool reverse = false;
            for (size_t candidate = 0; candidate < segments.size(); ++candidate) {
                if (segments[candidate].used) {
                    continue;
                }
                if (close_points(current, segments[candidate].start)) {
                    next = static_cast<int>(candidate);
                    reverse = false;
                    break;
                }
                if (close_points(current, segments[candidate].end)) {
                    next = static_cast<int>(candidate);
                    reverse = true;
                    break;
                }
            }
            if (next < 0) {
                break;
            }

            auto& segment = segments[static_cast<size_t>(next)];
            segment.used = true;
            current = reverse ? segment.start : segment.end;
            ring.push_back(current);
        }

        if (closed) {
            append_editor_part_from_ring(parts, std::move(ring), "editor_line_chain_" + std::to_string(next_index++));
        }
    }

    return parts;
}

bool has_editor_nesting_geometry(const std::vector<Annotation>& annotations) {
    return !editor_parts_from_annotations(annotations).empty();
}

std::vector<smn::Part> load_selected_parts_parallel(const RunSettings& settings) {
    std::vector<smn::Part> parts;
    const int workers = std::max(1, settings.cpu_cores);
    size_t next = 0;

    while (next < settings.selections.size()) {
        const size_t batch_size = std::min(
            static_cast<size_t>(workers),
            settings.selections.size() - next
        );
        std::vector<std::future<std::vector<smn::Part>>> futures;
        futures.reserve(batch_size);

        for (size_t index = 0; index < batch_size; ++index) {
            const DxfSelection selection = settings.selections[next + index];
            const smn::DxfLoadOptions base_load = settings.load;
            const double import_scale = settings.import_scale;
            futures.push_back(std::async(std::launch::async, [selection, base_load, import_scale]() {
                smn::DxfLoadOptions load = base_load;
                load.quantity = selection.quantity;
                auto loaded = smn::load_dxf_parts({selection.path}, load);
                scale_parts(loaded, import_scale);
                return loaded;
            }));
        }

        for (auto& future : futures) {
            auto loaded = future.get();
            parts.insert(parts.end(), loaded.begin(), loaded.end());
        }
        next += batch_size;
    }

    return parts;
}

void update_running_options_from_controls(smn::IterativeNestingOptions& options) {
    try {
        const double display_scale = display_unit_scale();
        options.base.spacing = std::stod(get_text(g_app.spacing)) * display_scale;
        options.base.rotations = rotations_from_count(std::stoi(get_text(g_app.part_rotations)));
        options.base.optimization_ratio = std::clamp(std::stod(get_text(g_app.optimization_ratio)), 0.0, 1.0);
        options.base.optimization_type =
            combo_text(g_app.optimization_type) == "Area compacta"
                ? smn::OptimizationType::CompactArea
                : smn::OptimizationType::BoundingBox;
        options.cpu_cores = std::max(1, std::stoi(get_text(g_app.cpu_cores)));
        options.ga_population = std::max(1, std::stoi(get_text(g_app.ga_population)));
        options.ga_mutation_rate = std::clamp(std::stod(get_text(g_app.ga_mutation_rate)), 0.0, 100.0);
        options.ga_random_shuffle_prob = std::clamp(std::stod(get_text(g_app.ga_random_shuffle)), 0.0, 100.0);
        options.ga_random_shuffle_intensity = std::clamp(std::stod(get_text(g_app.ga_shuffle_intensity)), 0.0, 100.0);
    } catch (...) {
        append_log("No pude aplicar algun cambio de configuracion; sigo con los valores anteriores.\r\n");
    }
}

RunSettings read_settings() {
    RunSettings settings;
    settings.selections = g_app.selections;
    settings.editor_annotations = g_app.annotations;
    settings.output = get_text(g_app.output);
    const double display_scale = display_unit_scale();
    settings.sheet.width = std::stod(get_text(g_app.sheet_width)) * display_scale;
    settings.sheet.height = std::stod(get_text(g_app.sheet_height)) * display_scale;
    settings.nesting.base.spacing = std::stod(get_text(g_app.spacing)) * display_scale;
    settings.load.flattening_distance = std::stod(get_text(g_app.curve_tolerance)) * display_scale;
    settings.load.endpoint_tolerance = std::stod(get_text(g_app.endpoint_tolerance)) * display_scale;
    settings.nesting.base.rotations = rotations_from_count(std::stoi(get_text(g_app.part_rotations)));
    settings.nesting.base.optimization_ratio = std::clamp(std::stod(get_text(g_app.optimization_ratio)), 0.0, 1.0);
    settings.nesting.base.optimization_type =
        combo_text(g_app.optimization_type) == "Area compacta"
            ? smn::OptimizationType::CompactArea
            : smn::OptimizationType::BoundingBox;
    const int requested_iterations = std::stoi(get_text(g_app.iterations));
    settings.nesting.iterations = requested_iterations <= 0
        ? std::numeric_limits<int>::max()
        : requested_iterations;
    settings.import_scale = unit_scale_from_combo(g_app.dxf_import_units);
    settings.export_scale = 1.0;
    try { settings.sheet_price_kg = std::stod(get_text(g_app.sheet_price_kg)); } catch(...) {}
    try { settings.sheet_density = std::stod(get_text(g_app.sheet_density)); } catch(...) {}
    try { settings.sheet_thickness = std::stod(get_text(g_app.sheet_thickness)); } catch(...) {}
    settings.cpu_cores = std::max(1, std::stoi(get_text(g_app.cpu_cores)));
    settings.nesting.cpu_cores = settings.cpu_cores;
    settings.nesting.ga_population = std::max(1, std::stoi(get_text(g_app.ga_population)));
    settings.nesting.ga_mutation_rate = std::clamp(std::stod(get_text(g_app.ga_mutation_rate)), 0.0, 100.0);
    settings.nesting.ga_random_shuffle_prob = std::clamp(std::stod(get_text(g_app.ga_random_shuffle)), 0.0, 100.0);
    settings.nesting.ga_random_shuffle_intensity = std::clamp(std::stod(get_text(g_app.ga_shuffle_intensity)), 0.0, 100.0);
    {
        std::string solver_str = combo_text(g_app.solver_mode);
        settings.solver_mode = smn::solver_mode_from_string(solver_str);
        settings.nesting.base.solver_mode = settings.solver_mode;
    }
    settings.perf_log_enabled = is_checked(g_app.perf_log);
    settings.merge_common_lines = is_checked(g_app.merge_common_lines);
    settings.rough_approximation = is_checked(g_app.rough_approx);
    settings.optimization_type = combo_text(g_app.optimization_type);
    if (settings.rough_approximation) {
        settings.load.flattening_distance *= 3.0;
        settings.load.endpoint_tolerance *= 2.0;
    }

    if (settings.selections.empty() && !has_editor_nesting_geometry(settings.editor_annotations)) {
        throw std::runtime_error("Agrega una carpeta, archivos DXF o dibuja una pieza cerrada primero.");
    }
    return settings;
}

RunSettings read_preview_settings() {
    RunSettings settings;
    settings.selections = g_app.selections;
    settings.editor_annotations = g_app.annotations;
    const double display_scale = display_unit_scale();
    settings.load.flattening_distance = std::stod(get_text(g_app.curve_tolerance)) * display_scale;
    settings.load.endpoint_tolerance = std::stod(get_text(g_app.endpoint_tolerance)) * display_scale;
    settings.import_scale = unit_scale_from_combo(g_app.dxf_import_units);
    settings.cpu_cores = std::max(1, std::stoi(get_text(g_app.cpu_cores)));
    settings.rough_approximation = is_checked(g_app.rough_approx);
    if (settings.rough_approximation) {
        settings.load.flattening_distance *= 3.0;
        settings.load.endpoint_tolerance *= 2.0;
    }
    return settings;
}

smn::NestingResult build_input_preview_result(const std::vector<smn::Part>& parts) {
    smn::NestingResult preview;
    if (parts.empty()) {
        return preview;
    }

    constexpr int kPreviewQuantityLimit = 120;
    double total_area = 0.0;
    double max_width = 1.0;
    double max_height = 1.0;
    for (const auto& part : parts) {
        const smn::Bounds bounds = part.bounds();
        if (!bounds.valid()) {
            continue;
        }
        total_area += std::max(0.0, part.area());
        max_width = std::max(max_width, bounds.width());
        max_height = std::max(max_height, bounds.height());
    }

    const double gap = std::clamp(std::max(max_width, max_height) * 0.12, 20.0, 180.0);
    const double target_width = std::max(max_width + 2.0 * gap, std::sqrt(std::max(1.0, total_area)) * 1.75);
    double cursor_x = gap;
    double cursor_y = gap;
    double row_height = 0.0;
    double used_width = gap;

    int instance_number = 1;
    for (const auto& part : parts) {
        if (instance_number > kPreviewQuantityLimit) {
            break;
        }
        smn::Polygon geometry = smn::normalize_to_origin(part.geometry);
        const smn::Bounds bounds = smn::polygon_bounds(geometry);
        if (!bounds.valid() || bounds.width() <= 0.0 || bounds.height() <= 0.0) {
            continue;
        }
        if (cursor_x > gap && cursor_x + bounds.width() > target_width) {
            cursor_x = gap;
            cursor_y += row_height + gap;
            row_height = 0.0;
        }

        geometry = smn::translate_polygon(geometry, cursor_x, cursor_y);
        smn::PlacedPart placement;
        placement.name = part.name;
        placement.source = part.source;
        placement.geometry = std::move(geometry);
        placement.sheet_index = 0;
        placement.instance_number = instance_number++;
        placement.x = cursor_x;
        placement.y = cursor_y;
        placement.rotation = 0.0;
        placement.source_area = part.area();
        placement.source = part.source;
        preview.placements.push_back(std::move(placement));
        if (instance_number > kPreviewQuantityLimit) {
            preview.unplaced.push_back(part);
            break;
        }

        cursor_x += bounds.width() + gap;
        row_height = std::max(row_height, bounds.height());
        used_width = std::max(used_width, cursor_x);
    }

    preview.sheet.width = std::max(100.0, used_width + gap);
    preview.sheet.height = std::max(100.0, cursor_y + row_height + gap);
    return preview;
}

void clear_current_nesting_result_locked() {
    g_app.result = {};
    g_app.history.clear();
    g_app.ranked_solutions.clear();
    g_app.selected_solution_index = -1;
    g_app.current_iteration = 0;
    g_app.current_occupied_area = 0.0;
    g_app.current_best_area = 0.0;
    g_app.current_saved_area = 0.0;
    g_app.has_result = false;
}

void start_input_preview(HWND hwnd) {
    RunSettings settings;
    try {
        settings = read_preview_settings();
    } catch (...) {
        std::lock_guard<std::mutex> lock(g_app.mutex);
        g_app.has_input_preview = false;
        g_app.input_preview_loading = false;
        g_app.input_preview_message = "No pude leer la configuracion de importacion para previsualizar.";
        clear_current_nesting_result_locked();
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    const unsigned int generation = g_app.input_preview_generation.fetch_add(1) + 1;
    {
        std::lock_guard<std::mutex> lock(g_app.mutex);
        clear_current_nesting_result_locked();
        g_app.input_preview_result = {};
        g_app.has_input_preview = false;
        g_app.input_preview_loading = !settings.selections.empty();
        g_app.input_preview_message = settings.selections.empty()
            ? ""
            : "Cargando previsualizacion de los DXF...";
        g_app.preview_zoom = 1.0;
        g_app.preview_pan_x = 0.0;
        g_app.preview_pan_y = 0.0;
    }
    refresh_solution_list();
    InvalidateRect(hwnd, nullptr, FALSE);

    if (settings.selections.empty()) {
        return;
    }

    std::thread([hwnd, settings, generation]() {
        smn::NestingResult preview_result;
        std::string message;
        bool ok = false;
        try {
            auto parts = load_selected_parts_parallel(settings);
            preview_result = build_input_preview_result(parts);
            ok = !preview_result.placements.empty();
            std::ostringstream summary;
            if (ok) {
                summary << "Previsualizacion DXF lista: " << preview_result.placements.size()
                        << " perfil(es) detectados.";
            } else {
                summary << "No se encontraron perfiles cerrados para previsualizar.";
            }
            message = summary.str();
        } catch (const std::exception& error) {
            message = std::string("No pude previsualizar los DXF: ") + error.what();
        }

        {
            std::lock_guard<std::mutex> lock(g_app.mutex);
            if (g_app.input_preview_generation.load() != generation) {
                return;
            }
            g_app.input_preview_result = std::move(preview_result);
            g_app.has_input_preview = ok;
            g_app.input_preview_loading = false;
            g_app.input_preview_message = message;
        }
        PostMessageA(hwnd, kInputPreviewReady, 0, 0);
    }).detach();
}

void save_current_result(HWND hwnd) {
    smn::NestingResult result;
    bool has_result = false;
    bool optimized_result = false;
    {
        std::lock_guard<std::mutex> lock(g_app.mutex);
        if (g_app.has_result && !g_app.result.placements.empty()) {
            result = g_app.result;
            has_result = true;
            optimized_result = true;
        } else if (g_app.has_input_preview && !g_app.input_preview_result.placements.empty()) {
            result = g_app.input_preview_result;
            has_result = true;
        }
    }

    if (!has_result) {
        MessageBoxA(hwnd, "Todavia no hay nada cargado para guardar.", "Guardar DXF", MB_ICONINFORMATION);
        return;
    }

    std::filesystem::path suggested = get_text(g_app.output);
    if (suggested.empty()) {
        std::lock_guard<std::mutex> lock(g_app.mutex);
        if (!g_app.last_output.empty()) {
            suggested = g_app.last_output;
        } else if (!g_app.selections.empty()) {
            suggested = g_app.selections.front().path.parent_path() / "nesting_result_cpp.dxf";
        }
    }

    const std::filesystem::path output_path = choose_output(hwnd, suggested);
    if (output_path.empty()) {
        return;
    }
    set_text(g_app.output, output_path);

    constexpr double export_scale = 1.0;
    constexpr int export_insunits = 4; // DXF $INSUNITS: millimeters
    const bool merge_common_lines = is_checked(g_app.merge_common_lines);
    
    std::vector<smn::DraftEntity> export_entities;
    for (const auto& ann : g_app.annotations) {
        const bool geometry_annotation =
            ann.type == Annotation::Type::Line ||
            ann.type == Annotation::Type::Rectangle ||
            ann.type == Annotation::Type::Circle ||
            ann.type == Annotation::Type::Arc ||
            ann.type == Annotation::Type::Polyline;
        if (optimized_result && geometry_annotation) {
            continue;
        }

        smn::DraftEntity entity;
        entity.start = {ann.start.x * export_scale, ann.start.y * export_scale};
        entity.end = {ann.end.x * export_scale, ann.end.y * export_scale};
        entity.mid = {ann.mid.x * export_scale, ann.mid.y * export_scale};
        entity.text = ann.text;
        switch (ann.type) {
            case Annotation::Type::Measure:
                entity.type = smn::DraftEntityType::Dimension;
                break;
            case Annotation::Type::Text:
                entity.type = smn::DraftEntityType::Text;
                break;
            case Annotation::Type::Line:
                entity.type = smn::DraftEntityType::Line;
                break;
            case Annotation::Type::Rectangle:
                entity.type = smn::DraftEntityType::Rectangle;
                break;
            case Annotation::Type::Circle:
                entity.type = smn::DraftEntityType::Circle;
                break;
            case Annotation::Type::Arc:
                entity.type = smn::DraftEntityType::Arc;
                entity.mid = {ann.mid.x * export_scale, ann.mid.y * export_scale};
                break;
            case Annotation::Type::Polyline:
                entity.type = smn::DraftEntityType::Polyline;
                for (const auto& p : ann.points) {
                    entity.points.push_back({p.x * export_scale, p.y * export_scale});
                }
                break;
        }
        export_entities.push_back(std::move(entity));
    }
    
    smn::write_nesting_dxf(scale_result(result, export_scale), output_path, 250.0, {}, export_insunits, merge_common_lines, export_entities);
    {
        std::lock_guard<std::mutex> lock(g_app.mutex);
        g_app.last_output = output_path;
        g_app.last_export_scale = export_scale;
    }

    append_log("DXF guardado: " + output_path.string() + "\r\n");
    if (merge_common_lines) {
        append_log("Merge common lines aplicado en exportacion: los bordes colineales compartidos se escriben una sola vez.\r\n");
    }
}

void run_nesting(HWND hwnd) {
    RunSettings settings = read_settings();
    SetWindowTextA(g_app.log, "");
    g_app.stop_requested.store(false);
    g_app.pause_requested.store(false);
    g_app.running.store(true);
    {
        std::lock_guard<std::mutex> lock(g_app.mutex);
        g_app.history.clear();
        g_app.ranked_solutions.clear();
        g_app.selected_solution_index = -1;
        g_app.current_iteration = 0;
        g_app.current_occupied_area = 0.0;
        g_app.current_best_area = 0.0;
        g_app.current_saved_area = 0.0;
        g_app.last_output = settings.output;
        g_app.last_export_scale = settings.export_scale;
        g_app.preview_zoom = 1.0;
        g_app.preview_pan_x = 0.0;
        g_app.preview_pan_y = 0.0;
        g_app.preview_interacting = false;
        g_app.has_result = false;
    }
    refresh_solution_list();
    append_log("Cargando DXF seleccionados...\r\n");
    if (settings.merge_common_lines && settings.nesting.base.spacing > 1e-6) {
        append_log("Aviso: Merge common lines solo puede unir bordes que queden tocandose; con separacion mayor a 0 mm casi no habra lineas comunes.\r\n");
    }
    if (smn::solver_mode_uses_gpu(settings.solver_mode)) {
        append_log("Inicializando GPU...\r\n");
        if (!smn::GpuContext::instance().init()) {
            append_log("GPU no disponible, usando CPU.\r\n");
            settings.solver_mode = smn::SolverMode::CPU;
            settings.nesting.base.solver_mode = smn::SolverMode::CPU;
        } else {
            auto& gpu = smn::GpuContext::instance();
            for (int i = 0; i < gpu.device_count(); ++i) {
                std::ostringstream gpu_info;
                gpu_info << "GPU[" << i << "]: " << gpu.device(i).name
                         << " (" << gpu.device(i).compute_units << " CUs, "
                         << (gpu.device(i).global_mem / (1024 * 1024)) << " MB)\r\n";
                append_log(gpu_info.str());
            }
            if (settings.solver_mode == smn::SolverMode::GPU_Dual && gpu.device_count() < 2) {
                append_log("Dual GPU requiere 2 GPUs disponibles; usando GPU simple.\r\n");
                settings.solver_mode = smn::SolverMode::GPU_Single;
                settings.nesting.base.solver_mode = smn::SolverMode::GPU_Single;
            }
            append_log(std::string("Solver: ") + smn::solver_mode_name(settings.solver_mode) + "\r\n");
        }
    } else {
        append_log(std::string("Solver: ") + smn::solver_mode_name(settings.solver_mode) + "\r\n");
    }
    EnableWindow(g_app.run, TRUE);
    EnableWindow(g_app.pause, TRUE);
    SetWindowTextA(g_app.run, "Detener solver");
    SetWindowTextA(g_app.pause, "Pausar");
    InvalidateRect(hwnd, nullptr, FALSE);

    std::thread([hwnd, settings]() mutable {
        try {
            std::vector<smn::Part> parts = load_selected_parts_parallel(settings);
            auto editor_parts = editor_parts_from_annotations(settings.editor_annotations);
            const size_t editor_part_count = editor_parts.size();
            parts.insert(
                parts.end(),
                std::make_move_iterator(editor_parts.begin()),
                std::make_move_iterator(editor_parts.end())
            );
            if (parts.empty()) {
                throw std::runtime_error("No se encontraron perfiles cerrados.");
            }
            if (editor_part_count > 0) {
                std::ostringstream editor_message;
                editor_message << "Piezas dibujadas agregadas al nesting: " << editor_part_count << "\r\n";
                append_log(editor_message.str());
            }

            auto iterative = smn::nest_parts_iterative(
                settings.sheet,
                parts,
                settings.nesting,
                [hwnd, &settings](
                    const smn::IterationStats& stats,
                    const smn::NestingResult& current,
                    const smn::NestingResult& best
                ) {
                    bool was_paused = false;
                    {
                        std::lock_guard<std::mutex> lock(g_app.mutex);
                        g_app.history.push_back(stats);
                        add_ranked_solution_locked(stats, best);
                        g_app.result = current;
                        g_app.current_iteration = stats.iteration;
                        g_app.current_occupied_area = stats.occupied_area;
                        g_app.current_best_area = stats.best_occupied_area;
                        g_app.current_saved_area = stats.saved_area;
                        g_app.has_result = true;
                    }
                    PostMessageA(hwnd, kRunProgress, 0, 0);
                    while (g_app.pause_requested.load() && !g_app.stop_requested.load()) {
                        was_paused = true;
                        std::this_thread::sleep_for(std::chrono::milliseconds(120));
                    }
                    if (was_paused && !g_app.stop_requested.load()) {
                        update_running_options_from_controls(settings.nesting);
                        PostMessageA(hwnd, kRunProgress, 0, 0);
                    }
                    return !g_app.stop_requested.load();
                }
            );

            double total_parts_cost = 0.0;
            double total_sheet_cost = 0.0;
            double occupied_cost = 0.0;
            if (settings.sheet_density > 0 && settings.sheet_thickness > 0) {
                double sheet_area = settings.sheet.width * settings.sheet.height;
                double parts_area = sheet_area * iterative.best.sheet_count() * iterative.best.utilization();
                double occupied_area = smn::occupied_layout_area(iterative.best);
                
                double parts_vol_m3 = (parts_area * settings.sheet_thickness) / 1e9;
                double parts_mass_kg = parts_vol_m3 * settings.sheet_density;
                total_parts_cost = parts_mass_kg * settings.sheet_price_kg;

                double sheet_vol_m3 = (sheet_area * settings.sheet_thickness) / 1e9;
                double sheet_mass_kg = sheet_vol_m3 * settings.sheet_density;
                total_sheet_cost = sheet_mass_kg * settings.sheet_price_kg * iterative.best.sheet_count();

                double occ_vol_m3 = (occupied_area * settings.sheet_thickness) / 1e9;
                double occ_mass_kg = occ_vol_m3 * settings.sheet_density;
                occupied_cost = occ_mass_kg * settings.sheet_price_kg;
            }

            std::ostringstream message;
            message << (g_app.stop_requested.load() ? "Detenido por usuario.\r\n" : "Listo.\r\n")

                    << "Archivos DXF elegidos: " << settings.selections.size() << "\r\n"
                    << "Perfiles cargados: " << parts.size() << "\r\n"
                    << "Piezas dibujadas usadas: " << editor_part_count << "\r\n"
                    << "Piezas ubicadas: " << iterative.best.placements.size() << "\r\n"
                    << "Chapas usadas: " << iterative.best.sheet_count() << "\r\n"
                    << "Iteraciones ejecutadas: " << iterative.history.size() << "\r\n"
                    << "Poblacion GA: " << settings.nesting.ga_population << "\r\n"
                    << "CPU cores usados: " << settings.nesting.cpu_cores << "\r\n"
                    << "Mutation rate: " << settings.nesting.ga_mutation_rate << "%\r\n"
                    << "Solver: " << smn::solver_mode_name(settings.solver_mode) << "\r\n"
                    << "Aprovechamiento: " << iterative.best.utilization() * 100.0 << "%\r\n"
                    << "Area ocupada final: " << smn::occupied_layout_area(iterative.best) << " mm2\r\n"
                    << "Costo neto de las piezas: $" << std::fixed << std::setprecision(2) << total_parts_cost << "\r\n"
                    << "Costo area ocupada (piezas + scrap): $" << occupied_cost << "\r\n"
                    << "Costo chapas enteras necesarias: $" << total_sheet_cost << "\r\n"

                    << "Ahorro vs primera solucion: "
                    << (iterative.history.empty() ? 0.0 : iterative.history.back().saved_area)
                    << " mm2\r\n"
                    << "No se guardo automaticamente. Usa Guardar DXF cuando quieras exportar.\r\n";

            {
                std::lock_guard<std::mutex> lock(g_app.mutex);
                g_app.history = iterative.history;
                g_app.selected_solution_index = -1;
                g_app.result = iterative.best;
                g_app.has_result = true;
                g_app.worker_message = message.str();
                g_app.worker_ok = true;
            }
        } catch (const std::exception& error) {
            std::lock_guard<std::mutex> lock(g_app.mutex);
            g_app.worker_message = std::string("Error: ") + error.what() + "\r\n";
            g_app.worker_ok = false;
        }
        PostMessageA(hwnd, kRunFinished, 0, 0);
    }).detach();
}

int CALLBACK count_font_family(
    const LOGFONTA*,
    const TEXTMETRICA*,
    DWORD,
    LPARAM lparam
) {
    *reinterpret_cast<int*>(lparam) = 1;
    return 0;
}

bool font_family_available(const char* family) {
    HDC dc = GetDC(nullptr);
    if (!dc) {
        return false;
    }
    LOGFONTA logfont = {};
    logfont.lfCharSet = DEFAULT_CHARSET;
    lstrcpynA(logfont.lfFaceName, family, LF_FACESIZE);
    int found = 0;
    EnumFontFamiliesExA(dc, &logfont, count_font_family, reinterpret_cast<LPARAM>(&found), 0);
    ReleaseDC(nullptr, dc);
    return found != 0;
}

void init_info_tips() {
    g_app.info_tips.clear();
    g_app.info_tips.reserve(32);
    auto add = [](int id, const char* modifies, const char* recommended, const char* def) {
        InfoTip tip;
        tip.id = id;
        std::ostringstream text;
        text << "Que modifica: " << modifies << "\r\n"
             << "Recomendado: " << recommended << "\r\n"
             << "Default: " << def;
        tip.text = text.str();
        g_app.info_tips.push_back(std::move(tip));
    };

    add(IdSelectedQuantity,
        "cuantas copias de este DXF entran al nesting.",
        "usar la cantidad real a fabricar; para muchas piezas iguales el programa agrupa repetidas.",
        "1");
    add(IdSheetWidth,
        "el ancho util de la chapa donde se acomodan las piezas.",
        "3000 mm si trabajas con chapa 3000 x 1500.",
        "3000 mm");
    add(IdSheetHeight,
        "el alto util de la chapa donde se acomodan las piezas.",
        "1500 mm si trabajas con chapa 3000 x 1500.",
        "1500 mm");
    add(IdSheetPriceKg,
        "el costo por kilo usado para estimar ahorro economico.",
        "actualizarlo con tu precio real de compra.",
        "6500");
    add(IdSheetDensity,
        "la densidad del material para calcular peso y costo.",
        "8000 kg/m3 para acero al carbono aproximado.",
        "8000 kg/m3");
    add(IdSheetThickness,
        "el espesor de chapa para estimar peso y costo.",
        "poner el espesor real del trabajo.",
        "2.5 mm");

    add(IdDisplayMm,
        "la unidad visual de la interfaz.",
        "mm para fabricacion metalica y DXF de corte.",
        "mm");
    add(IdSpacing,
        "la separacion minima entre piezas para corte, kerf y seguridad.",
        "5 mm para empezar; bajalo si tu proceso permite menos separacion.",
        "5 mm");
    add(IdCurveTolerance,
        "que tan fina se aproxima una curva al cargar DXF.",
        "2 mm para velocidad; bajalo para curvas chicas o piezas delicadas.",
        "2 mm");
    add(IdPartRotations,
        "cuantas orientaciones prueba por pieza.",
        "2 para rapido; 4 u 8 cuando buscas mas aprovechamiento.",
        "2");
    add(IdOptimizationType,
        "el criterio principal para comparar acomodos.",
        "Bounding Box para reducir largo/ancho ocupado; Area compacta para empaquetar mas cerrado.",
        "Bounding Box");
    add(IdRoughApprox,
        "usa una aproximacion mas gruesa de curvas para acelerar pruebas.",
        "apagado para resultados finales; encendido solo para exploracion rapida.",
        "apagado");
    add(IdCpuCores,
        "cuantos nucleos de CPU usa el solver y la carga de DXF.",
        "usar 8 o la cantidad de nucleos reales disponibles.",
        "8");
    add(IdIterations,
        "cantidad maxima de iteraciones del solver.",
        "0 para que siga hasta que lo pauses o detengas.",
        "0");

    add(IdSvgScale,
        "escala historica para importar SVG si se usa esa ruta.",
        "dejarlo como esta salvo que un archivo entre con escala incorrecta.",
        "2.834645 units/mm");
    add(IdEndpointTolerance,
        "tolerancia para unir extremos cercanos al reconstruir contornos DXF.",
        "0.2 mm para cerrar pequenas diferencias sin deformar piezas.",
        "0.2 mm");
    add(IdDxfImportUnits,
        "unidades con las que se interpretan los DXF de entrada.",
        "mm para SolidWorks/AutoCAD cuando los planos estan en milimetros.",
        "mm");
    add(IdDxfExportUnits,
        "unidades del DXF guardado.",
        "mm siempre para evitar que AutoCAD/eDrawings lo lea en pulgadas.",
        "mm");

    add(IdMergeCommonLines,
        "intenta no duplicar lineas compartidas cuando dos piezas quedan tocandose.",
        "encendido si queres ahorrar cortes; funciona mejor con separacion 0.",
        "encendido");
    add(IdOptimizationRatio,
        "balance interno entre compactacion y criterio de ocupacion.",
        "0.5 como punto medio; subirlo o bajarlo para comparar estrategias.",
        "0.5");
    add(IdGaPopulation,
        "cuantas variantes mantiene el algoritmo genetico en cada ciclo.",
        "4 para velocidad; subirlo mejora busqueda pero aumenta tiempo por iteracion.",
        "4");
    add(IdGaMutationRate,
        "probabilidad de mutar orden/rotacion de piezas.",
        "8% para explorar sin destruir siempre las mejores soluciones.",
        "8%");
    add(IdGaRandomShuffle,
        "probabilidad de hacer un salto aleatorio fuerte.",
        "10% para escapar de soluciones trabadas.",
        "10%");
    add(IdGaShuffleIntensity,
        "cuanta parte del ordenamiento se mezcla cuando hay shuffle.",
        "15% para cambios moderados; subirlo si se estanca mucho.",
        "15%");
    add(IdSolverMode,
        "motor que calcula los acomodos.",
        "cpu para estabilidad; dual-gpu si queres probar velocidad y tu placa lo soporta.",
        "cpu");
    add(IdPerfLog,
        "guarda metricas de rendimiento para analizar tiempos e iteraciones.",
        "apagado en uso normal; encendido cuando estas tuneando parametros.",
        "apagado");
}

void create_info_tooltip(HWND hwnd) {
    g_app.tooltip = CreateWindowExA(
        WS_EX_TOPMOST,
        TOOLTIPS_CLASSA,
        nullptr,
        WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        hwnd,
        nullptr,
        nullptr,
        nullptr
    );
    if (!g_app.tooltip) {
        return;
    }

    SendMessageA(g_app.tooltip, TTM_SETMAXTIPWIDTH, 0, 360);
    SendMessageA(g_app.tooltip, TTM_SETDELAYTIME, TTDT_INITIAL, 250);
    SendMessageA(g_app.tooltip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 12000);

    for (auto& tip : g_app.info_tips) {
        TOOLINFOA tool = {};
        tool.cbSize = sizeof(tool);
        tool.uFlags = TTF_SUBCLASS;
        tool.hwnd = hwnd;
        tool.uId = static_cast<UINT_PTR>(tip.id);
        tool.rect = tip.rect;
        tool.lpszText = const_cast<LPSTR>(tip.text.c_str());
        SendMessageA(g_app.tooltip, TTM_ADDTOOLA, 0, reinterpret_cast<LPARAM>(&tool));
    }
}


#include <fstream>
void create_controls(HWND hwnd) { 
 
    const char* ui_font = font_family_available("DM Sans") ? "DM Sans" : "Segoe UI";
    g_app.title_font = CreateFontA(34, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, ui_font);
    g_app.normal_font = CreateFontA(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, ui_font);
    g_app.small_font = CreateFontA(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, ui_font);
    
    g_app.file_list = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | WS_VSCROLL | WS_TABSTOP, 0, 0, 10, 10, hwnd, reinterpret_cast<HMENU>(IdFileList), nullptr, nullptr);
    ListView_SetExtendedListViewStyle(g_app.file_list, LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);
    ListView_SetBkColor(g_app.file_list, kFoglarDeep);
    ListView_SetTextBkColor(g_app.file_list, kFoglarDeep);
    ListView_SetTextColor(g_app.file_list, RGB(245, 236, 222));
    LVCOLUMNA lvc = {0};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    lvc.fmt = LVCFMT_LEFT;
    lvc.cx = 370;
    lvc.pszText = const_cast<LPSTR>("DXF");
    SendMessageA(g_app.file_list, LVM_INSERTCOLUMNA, 0, reinterpret_cast<LPARAM>(&lvc));
    lvc.fmt = LVCFMT_CENTER;
    lvc.cx = 74;
    lvc.pszText = const_cast<LPSTR>("Cant.");
    SendMessageA(g_app.file_list, LVM_INSERTCOLUMNA, 1, reinterpret_cast<LPARAM>(&lvc));
    SendMessageA(g_app.file_list, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.normal_font), TRUE);
    // ISO font for CAD-like annotations
    g_app.iso_font = CreateFontA(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, "ISOCPEUR");
    g_app.panel_brush = CreateSolidBrush(kFoglarCoal);
    g_app.input_brush = CreateSolidBrush(kFoglarInput);
    g_app.list_brush = CreateSolidBrush(kFoglarDeep);


    make_button(hwnd, "Agregar carpeta", IdAddFolder);
    make_button(hwnd, "Agregar DXF", IdAddFiles);
    make_button(hwnd, "Importar CSV", IdImportCsv);
    make_button(hwnd, "Quitar seleccionado", IdRemoveFile);
    g_app.selected_quantity = make_edit(hwnd, "1", IdSelectedQuantity);
    make_button(hwnd, "Aplicar cantidad", IdApplyQuantity);
    g_app.output = make_edit(hwnd, "outputs\\cpp_gui_nesting_result.dxf", IdOutput);
    make_button(hwnd, "Salida", IdBrowseOutput);

    g_app.display_inches = make_radio(hwnd, "inches", IdDisplayInches);
    g_app.display_mm = make_radio(hwnd, "mm", IdDisplayMm);
    SendMessageA(g_app.display_mm, BM_SETCHECK, BST_CHECKED, 0);
    g_app.spacing = make_edit(hwnd, "5", IdSpacing);
    g_app.curve_tolerance = make_edit(hwnd, "2", IdCurveTolerance);
    g_app.part_rotations = make_edit(hwnd, "2", IdPartRotations);
    g_app.optimization_type = make_combo(hwnd, IdOptimizationType, {"Bounding Box", "Area compacta"}, 0);
    g_app.rough_approx = make_check(hwnd, "", IdRoughApprox);
    const std::string default_cpu_cores = std::to_string(gui_safe_cpu_cores(8));
    g_app.cpu_cores = make_edit(hwnd, default_cpu_cores.c_str(), IdCpuCores);
    g_app.iterations = make_edit(hwnd, "0", IdIterations);

    g_app.svg_scale = make_edit(hwnd, "2.834645", IdSvgScale);
    g_app.endpoint_tolerance = make_edit(hwnd, "0.2", IdEndpointTolerance);
    g_app.dxf_import_units = make_combo(hwnd, IdDxfImportUnits, {"mm", "inches"}, 0);
    g_app.dxf_export_units = make_combo(hwnd, IdDxfExportUnits, {"mm"}, 0);
    EnableWindow(g_app.dxf_export_units, FALSE);

    g_app.merge_common_lines = make_check(hwnd, "", IdMergeCommonLines);
    SendMessageA(g_app.merge_common_lines, BM_SETCHECK, BST_CHECKED, 0);
    g_app.optimization_ratio = make_edit(hwnd, "0.5", IdOptimizationRatio);
    g_app.ga_population = make_edit(hwnd, "4", IdGaPopulation);
    g_app.ga_mutation_rate = make_edit(hwnd, "8", IdGaMutationRate);
    g_app.ga_random_shuffle = make_edit(hwnd, "10", IdGaRandomShuffle);
    g_app.ga_shuffle_intensity = make_edit(hwnd, "15", IdGaShuffleIntensity);
    g_app.solver_mode = make_combo(hwnd, IdSolverMode, { "cpu", "gpu", "dual-gpu", "random" }, 0);
    g_app.perf_log = make_check(hwnd, "Perf Log", IdPerfLog);
    g_app.feedback = make_button(hwnd, "Enviar comentarios / Reportar error", IdFeedback);
    g_app.sheet_width = make_edit(hwnd, "3000", IdSheetWidth);
    g_app.sheet_height = make_edit(hwnd, "1500", IdSheetHeight);
    g_app.sheet_price_kg = make_edit(hwnd, "6500", IdSheetPriceKg);
    g_app.sheet_density = make_edit(hwnd, "8000", IdSheetDensity);
    g_app.sheet_thickness = make_edit(hwnd, "2.5", IdSheetThickness);
    g_app.run = make_button(hwnd, "Ejecutar nesting", IdRun);
    g_app.pause = make_button(hwnd, "Pausar", IdPause);
    EnableWindow(g_app.pause, FALSE);
    g_app.save_dxf = make_button(hwnd, "Guardar", IdSaveDxf);

    g_app.tool_select = make_button(hwnd, "V Select", IdToolSelect);
    g_app.tool_pan = make_button(hwnd, "H Pan", IdToolPan);
    g_app.tool_fit = make_button(hwnd, "F Fit", IdToolFit);
    g_app.tool_measure = make_button(hwnd, "M Cota", IdToolMeasure);
    g_app.tool_line = make_button(hwnd, "L Linea", IdToolLine);
    g_app.tool_rectangle = make_button(hwnd, "R Rect", IdToolRectangle);
    g_app.tool_circle = make_button(hwnd, "C Circ", IdToolCircle);
    g_app.tool_arc = make_button(hwnd, "A Arco", IdToolArc);
    g_app.tool_polyline = make_button(hwnd, "P Poli", IdToolPolyline);
    g_app.tool_text = make_button(hwnd, "T Texto", IdToolText);
    g_app.tool_delete = make_button(hwnd, "E Borrar", IdToolDelete);
    g_app.tool_undo = make_button(hwnd, "Undo", IdToolUndo);
    g_app.tool_snap = make_button(hwnd, "Grilla ON", IdToolSnap);
    g_app.tool_clear_annotations = make_button(hwnd, "Limpiar", IdToolClearAnnotations);

    g_app.log = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY, 0, 0, 10, 100, hwnd, reinterpret_cast<HMENU>(IdLog), nullptr, nullptr);
    SendMessageA(g_app.log, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.normal_font), TRUE);
    g_app.solution_list = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        "LISTBOX",
        "",
        WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL | WS_HSCROLL,
        0,
        0,
        10,
        100,
        hwnd,
        reinterpret_cast<HMENU>(IdSolutionList),
        nullptr,
        nullptr
    );
    SendMessageA(g_app.solution_list, WM_SETFONT, reinterpret_cast<WPARAM>(g_app.small_font), TRUE);
    init_info_tips();
    create_info_tooltip(hwnd);
    layout_controls(hwnd);
}

void draw_shell(HWND hwnd, HDC dc) {
    RECT rect;
    GetClientRect(hwnd, &rect);
    const LeftPanelLayout layout = left_panel_layout(rect);

    HBRUSH panel = CreateSolidBrush(kFoglarCoal);
    RECT left{0, 0, kPanelWidth, rect.bottom};
    FillRect(dc, &left, panel);
    DeleteObject(panel);

    HBRUSH bg = CreateSolidBrush(RGB(30, 35, 33));
    RECT right{kPanelWidth, 0, rect.right, rect.bottom};
    FillRect(dc, &right, bg);
    DeleteObject(bg);

    HBRUSH accent = CreateSolidBrush(kFoglarOrange);
    RECT accent_bar{0, 0, 5, rect.bottom};
    FillRect(dc, &accent_bar, accent);
    DeleteObject(accent);

    draw_brand_header(dc);

    SaveDC(dc);
    IntersectClipRect(dc, 0, kLeftContentClipTop, kPanelWidth, rect.bottom);
    const int scroll = g_app.left_scroll_y;
    const auto sy = [&](int y) { return y - scroll; };
    const auto label = [&](int x, int y, const char* text, COLORREF color = RGB(216, 218, 209)) {
        draw_text(dc, x, sy(y), text, g_app.small_font, color);
    };
    const auto section = [&](int y, const char* text) {
        const int yy = sy(y);
        draw_text(dc, kMargin, yy, text, g_app.normal_font, kFoglarCream);
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(69, 79, 73));
        HGDIOBJ old = SelectObject(dc, pen);
        MoveToEx(dc, kMargin, yy + 23, nullptr);
        LineTo(dc, kPanelWidth - kMargin, yy + 23);
        SelectObject(dc, old);
        DeleteObject(pen);
    };

    label(kMargin, layout.file_label_y, "DXF a procesar", kFoglarCream);

    RECT splitter = file_list_splitter_rect(rect);
    HBRUSH splitter_brush = CreateSolidBrush(RGB(55, 62, 58));
    FillRect(dc, &splitter, splitter_brush);
    DeleteObject(splitter_brush);
    HPEN splitter_pen = CreatePen(PS_SOLID, 1, kFoglarOrange);
    HGDIOBJ old_pen = SelectObject(dc, splitter_pen);
    const int grip_y = splitter.top + (splitter.bottom - splitter.top) / 2;
    MoveToEx(dc, splitter.left + 120, grip_y - 3, nullptr);
    LineTo(dc, splitter.right - 120, grip_y - 3);
    MoveToEx(dc, splitter.left + 120, grip_y + 3, nullptr);
    LineTo(dc, splitter.right - 120, grip_y + 3);
    SelectObject(dc, old_pen);
    DeleteObject(splitter_pen);
    draw_text(dc, splitter.left + 14, splitter.top + 1, "Arrastra para agrandar la lista", g_app.small_font, kFoglarSteel);

    label(kMargin, layout.quantity_label_y, "Cantidad del DXF seleccionado");
    label(kMargin, layout.sheet_label_y, "Ancho chapa");
    label(kMargin + 104, layout.sheet_label_y, "Alto chapa");
    label(kMargin, layout.cost_label_y, "Precio/Kg");
    label(kMargin + 104, layout.cost_label_y, "Densidad");
    label(kMargin + 208, layout.cost_label_y, "Espesor");

    section(layout.config_title_y, "Nesting configuration");
    label(kMargin, layout.config_y, "Display units");
    label(layout.c2, layout.config_y, "Space between");
    label(kMargin, layout.config_y + 27, "Curve tolerance");
    label(layout.c2, layout.config_y + 27, "Part rotations");
    label(kMargin, layout.config_y + 54, "Optimization type");
    label(layout.c2, layout.config_y + 54, "Rough approx");
    label(kMargin, layout.config_y + 81, "CPU cores");
    label(layout.c2, layout.config_y + 81, "Iterations");

    section(layout.import_title_y, "Import / Export");
    label(kMargin, layout.import_y, "SVG scale");
    label(layout.c2, layout.import_y, "Endpoint tol.");
    label(kMargin, layout.import_y + 27, "DXF import");
    label(layout.c2, layout.import_y + 27, "DXF export mm");

    section(layout.laser_title_y, "Laser / meta-heuristic");
    label(kMargin, layout.laser_y, "Merge common");
    label(layout.c2, layout.laser_y, "Opt. ratio");
    label(kMargin, layout.laser_y + 27, "GA population");
    label(layout.c2, layout.laser_y + 27, "Mutation %");
    label(kMargin, layout.laser_y + 54, "Shuffle %");
    label(layout.c2, layout.laser_y + 54, "Intensity %");
    label(kMargin, layout.laser_y + 81, "Solver", kFoglarOrange);
    label(layout.c2, layout.laser_y + 81, "Perf log");
    draw_info_tips(dc);
    if (g_app.left_max_scroll > 0) {
        const int track_top = kLeftContentClipTop + 8;
        const int track_bottom = rect.bottom - 8;
        const int track_h = std::max(1, track_bottom - track_top);
        const int thumb_h = std::max(34, track_h * track_h / std::max(track_h, track_h + g_app.left_max_scroll));
        const int thumb_y = track_top + (track_h - thumb_h) * g_app.left_scroll_y / std::max(1, g_app.left_max_scroll);
        RECT track{kPanelWidth - 10, track_top, kPanelWidth - 6, track_bottom};
        RECT thumb{kPanelWidth - 12, thumb_y, kPanelWidth - 4, thumb_y + thumb_h};
        HBRUSH track_brush = CreateSolidBrush(RGB(48, 55, 51));
        HBRUSH thumb_brush = CreateSolidBrush(kFoglarOrange);
        FillRect(dc, &track, track_brush);
        FillRect(dc, &thumb, thumb_brush);
        DeleteObject(track_brush);
        DeleteObject(thumb_brush);
    }
    RestoreDC(dc, -1);

    draw_preview(dc, rect);
    draw_solution_panel(dc, rect);
}

void add_folder(HWND hwnd) {
    for (const auto& folder : choose_folders(hwnd)) {
        for (const auto& dxf : smn::expand_dxf_inputs({folder})) {
            add_selection(dxf);
        }
        if (get_text(g_app.output) == "outputs\\cpp_gui_nesting_result.dxf") {
            set_text(g_app.output, (folder / "nesting_result_cpp.dxf").string());
        }
    }
    refresh_file_list();
    start_input_preview(hwnd);
}

void add_files(HWND hwnd) {
    auto files = choose_dxf_files(hwnd);
    for (const auto& file : files) {
        add_selection(file);
    }
    if (!files.empty() && get_text(g_app.output) == "outputs\\cpp_gui_nesting_result.dxf") {
        set_text(g_app.output, (files.front().parent_path() / "nesting_result_cpp.dxf").string());
    }
    refresh_file_list();
    start_input_preview(hwnd);
}
bool current_preview_result(smn::NestingResult& result) {
    std::lock_guard<std::mutex> lock(g_app.mutex);
    if (g_app.has_result && !g_app.result.placements.empty()) {
        result = g_app.result;
        return true;
    }
    if (g_app.has_input_preview && !g_app.input_preview_result.placements.empty()) {
        result = g_app.input_preview_result;
        return true;
    }
    return false;
}

POINT point_from_lparam(LPARAM lparam) {
    return {
        static_cast<LONG>(static_cast<short>(LOWORD(lparam))),
        static_cast<LONG>(static_cast<short>(HIWORD(lparam))),
    };
}

bool begin_file_list_resize(HWND hwnd, LPARAM lparam) {
    RECT client;
    GetClientRect(hwnd, &client);
    const RECT splitter = file_list_splitter_rect(client);
    const POINT point = point_from_lparam(lparam);
    if (!point_in_rect(splitter, point)) {
        return false;
    }

    g_app.resizing_file_list = true;
    g_app.resize_start_mouse = point;
    g_app.resize_start_height = g_app.file_list_height;
    SetCapture(hwnd);
    SetCursor(LoadCursor(nullptr, IDC_SIZENS));
    return true;
}

bool update_file_list_resize(HWND hwnd, LPARAM lparam) {
    if (!g_app.resizing_file_list) {
        return false;
    }

    RECT client;
    GetClientRect(hwnd, &client);
    const POINT point = point_from_lparam(lparam);
    const int requested_height = g_app.resize_start_height + point.y - g_app.resize_start_mouse.y;
    const int next_height = clamp_file_list_height(client, requested_height);
    if (next_height != g_app.file_list_height) {
        g_app.file_list_height = next_height;
        layout_controls(hwnd);
    }
    InvalidateRect(hwnd, nullptr, FALSE);
    SetCursor(LoadCursor(nullptr, IDC_SIZENS));
    return true;
}

bool end_file_list_resize(HWND hwnd) {
    if (!g_app.resizing_file_list) {
        return false;
    }
    g_app.resizing_file_list = false;
    ReleaseCapture();
    RECT client;
    GetClientRect(hwnd, &client);
    g_app.file_list_height = clamp_file_list_height(client, g_app.file_list_height);
    layout_controls(hwnd);
    InvalidateRect(hwnd, nullptr, FALSE);
    return true;
}

bool set_resize_cursor_if_needed(HWND hwnd) {
    if (g_app.resizing_file_list) {
        SetCursor(LoadCursor(nullptr, IDC_SIZENS));
        return true;
    }

    POINT point;
    GetCursorPos(&point);
    ScreenToClient(hwnd, &point);
    RECT client;
    GetClientRect(hwnd, &client);
    if (point_in_rect(file_list_splitter_rect(client), point)) {
        SetCursor(LoadCursor(nullptr, IDC_SIZENS));
        return true;
    }
    return false;
}

double point_to_segment_distance(smn::Point point, smn::Point start, smn::Point end) {
    const double dx = end.x - start.x;
    const double dy = end.y - start.y;
    const double length2 = dx * dx + dy * dy;
    if (length2 <= 1e-9) {
        return std::hypot(point.x - start.x, point.y - start.y);
    }
    const double t = std::clamp(((point.x - start.x) * dx + (point.y - start.y) * dy) / length2, 0.0, 1.0);
    const double px = start.x + t * dx;
    const double py = start.y + t * dy;
    return std::hypot(point.x - px, point.y - py);
}

double distance_between(smn::Point a, smn::Point b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

double normalize_angle(double angle) {
    constexpr double two_pi = 2.0 * 3.14159265358979323846;
    while (angle < 0.0) {
        angle += two_pi;
    }
    while (angle >= two_pi) {
        angle -= two_pi;
    }
    return angle;
}

double ccw_delta(double from, double to) {
    constexpr double two_pi = 2.0 * 3.14159265358979323846;
    double delta = normalize_angle(to) - normalize_angle(from);
    if (delta < 0.0) {
        delta += two_pi;
    }
    return delta;
}

bool angle_on_arc(double angle, double start, double mid, double end) {
    const double ccw_sweep = ccw_delta(start, end);
    const double ccw_mid = ccw_delta(start, mid);
    const bool mid_on_ccw = ccw_mid <= ccw_sweep;
    if (mid_on_ccw) {
        return ccw_delta(start, angle) <= ccw_sweep;
    }
    return ccw_delta(end, angle) <= ccw_delta(end, start);
}

bool arc_circle_from_points(
    smn::Point start,
    smn::Point mid,
    smn::Point end,
    smn::Point& center,
    double& radius
) {
    const double ax = start.x;
    const double ay = start.y;
    const double bx = mid.x;
    const double by = mid.y;
    const double cx = end.x;
    const double cy = end.y;
    const double d = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
    if (std::abs(d) < 1e-9) {
        return false;
    }
    center.x = ((ax * ax + ay * ay) * (by - cy) +
                (bx * bx + by * by) * (cy - ay) +
                (cx * cx + cy * cy) * (ay - by)) / d;
    center.y = ((ax * ax + ay * ay) * (cx - bx) +
                (bx * bx + by * by) * (ax - cx) +
                (cx * cx + cy * cy) * (bx - ax)) / d;
    radius = distance_between(start, center);
    return radius > 1e-6;
}

struct OSnapResult {
    bool valid = false;
    smn::Point point;
};

struct DimensionTarget {
    bool valid = false;
    bool circle = false;
    bool radius = false;
    smn::Point start;
    smn::Point end;
    int annotation_index = -1;
    double distance = std::numeric_limits<double>::infinity();
};

std::string prompt_for_text(HWND parent, POINT pt);
std::string prompt_for_text_with_default(HWND parent, POINT pt, const std::string& initial, const std::string& title);

void consider_dimension_segment(
    DimensionTarget& best,
    smn::Point point,
    smn::Point start,
    smn::Point end,
    double threshold,
    int annotation_index
) {
    if (distance_between(start, end) < 1e-6) {
        return;
    }
    const double distance = point_to_segment_distance(point, start, end);
    if (distance <= threshold && distance < best.distance) {
        best.valid = true;
        best.circle = false;
        best.start = start;
        best.end = end;
        best.annotation_index = annotation_index;
        best.distance = distance;
    }
}

void consider_dimension_circle(
    DimensionTarget& best,
    smn::Point point,
    smn::Point center,
    double radius,
    double threshold,
    int annotation_index
) {
    if (radius < 1e-6) {
        return;
    }
    const double distance = std::abs(distance_between(point, center) - radius);
    if (distance <= threshold && distance < best.distance) {
        best.valid = true;
        best.circle = true;
        best.start = center;
        best.end = {center.x + radius, center.y};
        best.annotation_index = annotation_index;
        best.distance = distance;
    }
}

void consider_dimension_arc(
    DimensionTarget& best,
    smn::Point point,
    const Annotation& ann,
    double threshold,
    int annotation_index
) {
    smn::Point center;
    double radius = 0.0;
    if (!arc_circle_from_points(ann.start, ann.mid, ann.end, center, radius)) {
        return;
    }
    const double radial_distance = std::abs(distance_between(point, center) - radius);
    if (radial_distance > threshold || radial_distance >= best.distance) {
        return;
    }

    const double angle = std::atan2(point.y - center.y, point.x - center.x);
    const double start_angle = std::atan2(ann.start.y - center.y, ann.start.x - center.x);
    const double mid_angle = std::atan2(ann.mid.y - center.y, ann.mid.x - center.x);
    const double end_angle = std::atan2(ann.end.y - center.y, ann.end.x - center.x);
    if (!angle_on_arc(angle, start_angle, mid_angle, end_angle)) {
        return;
    }

    best.valid = true;
    best.circle = false;
    best.radius = true;
    best.start = center;
    best.end = {center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius};
    best.annotation_index = annotation_index;
    best.distance = radial_distance;
}

bool approximate_circle_from_ring(
    const std::vector<smn::Point>& source_ring,
    double offset_x,
    smn::Point& center,
    double& radius
) {
    if (source_ring.size() < 10) {
        return false;
    }

    std::vector<smn::Point> ring = smn::ensure_closed(source_ring);
    if (ring.size() < 10) {
        return false;
    }
    if (distance_between(ring.front(), ring.back()) < 1e-6) {
        ring.pop_back();
    }
    if (ring.size() < 10) {
        return false;
    }

    center = {0.0, 0.0};
    for (const auto& point : ring) {
        center.x += point.x + offset_x;
        center.y += point.y;
    }
    center.x /= static_cast<double>(ring.size());
    center.y /= static_cast<double>(ring.size());

    radius = 0.0;
    for (const auto& point : ring) {
        radius += distance_between({point.x + offset_x, point.y}, center);
    }
    radius /= static_cast<double>(ring.size());
    if (radius < 0.5) {
        return false;
    }

    double max_deviation = 0.0;
    for (const auto& point : ring) {
        const double distance = distance_between({point.x + offset_x, point.y}, center);
        max_deviation = std::max(max_deviation, std::abs(distance - radius));
    }

    const double tolerance = std::max(0.8, radius * 0.035);
    return max_deviation <= tolerance;
}

void consider_dimension_ring(
    DimensionTarget& best,
    smn::Point point,
    std::vector<smn::Point> ring,
    double offset_x,
    double threshold
) {
    ring = smn::ensure_closed(std::move(ring));
    for (size_t index = 0; index + 1 < ring.size(); ++index) {
        smn::Point start{ring[index].x + offset_x, ring[index].y};
        smn::Point end{ring[index + 1].x + offset_x, ring[index + 1].y};
        consider_dimension_segment(best, point, start, end, threshold, -1);
    }
}

DimensionTarget find_dimension_target(
    smn::Point point,
    const smn::NestingResult& result,
    const std::vector<Annotation>& annotations,
    double threshold,
    double sheet_gap
) {
    DimensionTarget best;

    for (int index = static_cast<int>(annotations.size()) - 1; index >= 0; --index) {
        const auto& ann = annotations[static_cast<size_t>(index)];
        if (ann.type == Annotation::Type::Measure || ann.type == Annotation::Type::Text) {
            continue;
        }
        if (ann.type == Annotation::Type::Circle) {
            consider_dimension_circle(best, point, ann.start, distance_between(ann.start, ann.end), threshold, index);
        } else if (ann.type == Annotation::Type::Arc) {
            consider_dimension_arc(best, point, ann, threshold, index);
        }
    }

    for (const auto& placement : result.placements) {
        const double offset_x = placement.sheet_index * (result.sheet.width + sheet_gap);
        smn::Point center;
        double radius = 0.0;
        if (approximate_circle_from_ring(placement.geometry.outer, offset_x, center, radius)) {
            consider_dimension_circle(best, point, center, radius, threshold, -1);
        }
        for (const auto& hole : placement.geometry.holes) {
            if (approximate_circle_from_ring(hole, offset_x, center, radius)) {
                consider_dimension_circle(best, point, center, radius, threshold, -1);
            }
        }
    }

    return best;
}

int find_matching_line_annotation(smn::Point start, smn::Point end) {
    const double tolerance = std::max(0.5, distance_between(start, end) * 0.002);
    for (int index = static_cast<int>(g_app.annotations.size()) - 1; index >= 0; --index) {
        const auto& ann = g_app.annotations[static_cast<size_t>(index)];
        if (ann.type != Annotation::Type::Line) {
            continue;
        }
        const bool same_direction =
            distance_between(ann.start, start) <= tolerance &&
            distance_between(ann.end, end) <= tolerance;
        const bool reverse_direction =
            distance_between(ann.start, end) <= tolerance &&
            distance_between(ann.end, start) <= tolerance;
        if (same_direction || reverse_direction) {
            return index;
        }
    }
    return -1;
}

smn::Point default_dimension_position(smn::Point start, smn::Point end, smn::Point reference) {
    const double length = distance_between(start, end);
    if (length < 1e-6) {
        return reference;
    }
    const smn::Point mid{(start.x + end.x) * 0.5, (start.y + end.y) * 0.5};
    if (distance_between(reference, mid) > 1.0) {
        return reference;
    }
    const double nx = -(end.y - start.y) / length;
    const double ny = (end.x - start.x) / length;
    const double offset = std::clamp(length * 0.12, 20.0, 120.0);
    return {mid.x + nx * offset, mid.y + ny * offset};
}

std::string format_dimension_value(double value, bool diameter, bool radius) {
    std::ostringstream text;
    if (diameter) {
        text << "DIA ";
    } else if (radius) {
        text << "R ";
    }
    text << std::fixed << std::setprecision(value >= 100.0 ? 0 : 1) << value << " mm";
    return text.str();
}

bool parse_positive_measure(const std::string& raw, double& value) {
    std::string text = raw;
    std::replace(text.begin(), text.end(), ',', '.');
    const auto first_number = std::find_if(text.begin(), text.end(), [](unsigned char ch) {
        return std::isdigit(ch) || ch == '.' || ch == '-';
    });
    if (first_number == text.end()) {
        return false;
    }
    text.erase(text.begin(), first_number);
    try {
        size_t parsed = 0;
        value = std::stod(text, &parsed);
        return parsed > 0 && value > 0.0;
    } catch (...) {
        return false;
    }
}

bool finish_dimension(HWND hwnd);

void begin_dimension_from_target(const DimensionTarget& target, smn::Point placement_point) {
    push_undo();
    Annotation ann;
    ann.type = Annotation::Type::Measure;
    ann.start = target.start;
    ann.end = target.end;
    ann.mid = default_dimension_position(target.start, target.end, placement_point);
    ann.diameter_dimension = target.circle;
    ann.radius_dimension = target.radius;
    ann.target_annotation_index = target.annotation_index;
    g_app.annotations.push_back(std::move(ann));
    g_app.dimension_building = true;
    g_app.dimension_stage = 2;
}

void begin_linear_dimension_placement(HWND hwnd, smn::Point start, smn::Point end) {
    push_undo();
    Annotation ann;
    ann.type = Annotation::Type::Measure;
    ann.start = start;
    ann.end = end;
    const smn::Point measured_mid{(start.x + end.x) * 0.5, (start.y + end.y) * 0.5};
    ann.mid = default_dimension_position(start, end, measured_mid);
    ann.target_annotation_index = find_matching_line_annotation(start, end);
    g_app.annotations.push_back(std::move(ann));
    g_app.dimension_building = true;
    g_app.dimension_stage = 2;
    invalidate_preview(hwnd);
}

void start_or_advance_smart_dimension(
    HWND hwnd,
    const smn::NestingResult& result,
    const PreviewTransform& view,
    smn::Point point
) {
    if (g_app.dimension_building && g_app.dimension_stage == 2) {
        if (!g_app.annotations.empty()) {
            g_app.annotations.back().mid = point;
        }
        finish_dimension(hwnd);
        return;
    }

    if (g_app.dimension_building && g_app.dimension_stage == 1) {
        const smn::Point first = g_app.dimension_first_point;
        begin_linear_dimension_placement(hwnd, first, point);
        return;
    }

    const double target_threshold = 14.0 / view.scale;
    DimensionTarget target = find_dimension_target(point, result, g_app.annotations, target_threshold, view.sheet_gap);
    if (target.valid) {
        begin_dimension_from_target(target, point);
        finish_dimension(hwnd);
        return;
    }

    if (!g_app.dimension_building) {
        g_app.dimension_building = true;
        g_app.dimension_stage = 1;
        g_app.dimension_first_point = point;
        invalidate_preview(hwnd);
        return;
    }

}

bool edit_dimension_value(HWND hwnd, Annotation& dimension) {
    POINT prompt_point = g_app.crosshair_valid ? g_app.crosshair_pos : POINT{0, 0};
    const std::string current_text = dimension.text.empty()
        ? format_dimension_value(
            dimension.override_measure > 0.0
                ? dimension.override_measure
                : (dimension.diameter_dimension ? distance_between(dimension.start, dimension.end) * 2.0 : distance_between(dimension.start, dimension.end)),
            dimension.diameter_dimension,
            dimension.radius_dimension
        )
        : dimension.text;
    const std::string input = prompt_for_text_with_default(hwnd, prompt_point, current_text, "Modificar cota");
    if (input.empty()) {
        return false;
    }

    double desired = 0.0;
    if (!parse_positive_measure(input, desired)) {
        dimension.text = input;
        return true;
    }

    if (!dimension.diameter_dimension &&
        (dimension.target_annotation_index < 0 ||
         dimension.target_annotation_index >= static_cast<int>(g_app.annotations.size()) ||
         g_app.annotations[static_cast<size_t>(dimension.target_annotation_index)].type != Annotation::Type::Line)) {
        dimension.target_annotation_index = find_matching_line_annotation(dimension.start, dimension.end);
    }

    if (dimension.target_annotation_index >= 0 &&
        dimension.target_annotation_index < static_cast<int>(g_app.annotations.size())) {
        Annotation& target = g_app.annotations[static_cast<size_t>(dimension.target_annotation_index)];
        if (!dimension.diameter_dimension && target.type == Annotation::Type::Line) {
            const double current = distance_between(target.start, target.end);
            if (current >= 1e-6) {
                const double ux = (target.end.x - target.start.x) / current;
                const double uy = (target.end.y - target.start.y) / current;
                target.end = {target.start.x + ux * desired, target.start.y + uy * desired};
                dimension.start = target.start;
                dimension.end = target.end;
            }
        } else if (dimension.diameter_dimension && target.type == Annotation::Type::Circle) {
            const double current_radius = distance_between(target.start, target.end);
            smn::Point dir{1.0, 0.0};
            if (current_radius >= 1e-6) {
                dir = {
                    (target.end.x - target.start.x) / current_radius,
                    (target.end.y - target.start.y) / current_radius
                };
            }
            const double radius = desired * 0.5;
            target.end = {target.start.x + dir.x * radius, target.start.y + dir.y * radius};
            dimension.start = target.start;
            dimension.end = target.end;
        } else if (dimension.radius_dimension && target.type == Annotation::Type::Arc) {
            smn::Point center;
            double current_radius = 0.0;
            if (arc_circle_from_points(target.start, target.mid, target.end, center, current_radius) && current_radius >= 1e-6) {
                const auto scale_arc_point = [&](smn::Point point) {
                    const double length = distance_between(point, center);
                    if (length < 1e-6) {
                        return point;
                    }
                    return smn::Point{
                        center.x + (point.x - center.x) * desired / length,
                        center.y + (point.y - center.y) * desired / length
                    };
                };
                target.start = scale_arc_point(target.start);
                target.mid = scale_arc_point(target.mid);
                target.end = scale_arc_point(target.end);
                const double dim_length = distance_between(dimension.end, center);
                if (dim_length >= 1e-6) {
                    dimension.start = center;
                    dimension.end = {
                        center.x + (dimension.end.x - center.x) * desired / dim_length,
                        center.y + (dimension.end.y - center.y) * desired / dim_length
                    };
                }
            }
        }
    }

    dimension.override_measure = desired;
    dimension.text = format_dimension_value(desired, dimension.diameter_dimension, dimension.radius_dimension);
    return true;
}

bool finish_dimension(HWND hwnd) {
    if (!g_app.dimension_building || g_app.dimension_stage != 2 || g_app.annotations.empty()) {
        return false;
    }
    Annotation& dimension = g_app.annotations.back();
    edit_dimension_value(hwnd, dimension);
    clear_dimension_state();
    invalidate_preview(hwnd);
    return true;
}

OSnapResult find_closest_snap(smn::Point pt, const smn::NestingResult& result, const std::vector<Annotation>& annotations, double threshold_sheet_units) {
    OSnapResult best;
    double min_dist = threshold_sheet_units;

    auto check_point = [&](smn::Point p) {
        double d = distance_between(pt, p);
        if (d < min_dist) {
            min_dist = d;
            best.valid = true;
            best.point = p;
        }
    };

    for (const auto& ann : annotations) {
        check_point(ann.start);
        check_point(ann.end);
        if (ann.type == Annotation::Type::Arc || ann.type == Annotation::Type::Circle) {
            check_point(ann.mid);
        }
        for (const auto& p : ann.points) {
            check_point(p);
        }
    }

    for (const auto& place : result.placements) {
        for (const auto& p : place.geometry.outer) check_point(p);
        for (const auto& hole : place.geometry.holes) {
            for (const auto& p : hole) check_point(p);
        }
    }

    return best;
}

bool hit_annotation(const Annotation& ann, smn::Point point, double scale) {
    const double pixel_radius = 15.0;
    if (ann.type == Annotation::Type::Measure) {
        if (ann.diameter_dimension || ann.radius_dimension) {
            const double radius = distance_between(ann.start, ann.end);
            smn::Point label = distance_between(ann.mid, {0.0, 0.0}) > 1e-6 ? ann.mid : ann.end;
            smn::Point dir{label.x - ann.start.x, label.y - ann.start.y};
            const double dir_len = std::max(1e-6, std::hypot(dir.x, dir.y));
            dir.x /= dir_len;
            dir.y /= dir_len;
            const smn::Point circle_point{ann.start.x + dir.x * radius, ann.start.y + dir.y * radius};
            return point_to_segment_distance(point, circle_point, label) * scale < pixel_radius ||
                distance_between(point, label) * scale < 42.0;
        }

        const double length = distance_between(ann.start, ann.end);
        if (length < 1e-6) {
            return false;
        }
        const double ux = (ann.end.x - ann.start.x) / length;
        const double uy = (ann.end.y - ann.start.y) / length;
        const double nx = -uy;
        const double ny = ux;
        const smn::Point measured_mid{(ann.start.x + ann.end.x) * 0.5, (ann.start.y + ann.end.y) * 0.5};
        smn::Point label = distance_between(ann.mid, {0.0, 0.0}) > 1e-6
            ? ann.mid
            : default_dimension_position(ann.start, ann.end, measured_mid);
        double offset = (label.x - measured_mid.x) * nx + (label.y - measured_mid.y) * ny;
        if (std::abs(offset) < 2.0 / scale) {
            offset = 22.0 / scale;
        }
        const smn::Point dim_start{ann.start.x + nx * offset, ann.start.y + ny * offset};
        const smn::Point dim_end{ann.end.x + nx * offset, ann.end.y + ny * offset};
        const smn::Point dim_mid{(dim_start.x + dim_end.x) * 0.5, (dim_start.y + dim_end.y) * 0.5};
        return point_to_segment_distance(point, dim_start, dim_end) * scale < pixel_radius ||
            point_to_segment_distance(point, ann.start, dim_start) * scale < pixel_radius ||
            point_to_segment_distance(point, ann.end, dim_end) * scale < pixel_radius ||
            distance_between(point, dim_mid) * scale < 52.0;
    }
    if (ann.type == Annotation::Type::Text) {
        const double size_px = std::clamp(ann.text_size, 6.0, 96.0);
        const double width_px = std::max(24.0, static_cast<double>(ann.text.size()) * size_px * 0.62);
        const double height_px = std::max(12.0, size_px * 1.25);
        const double dx_px = (point.x - ann.start.x) * scale;
        const double dy_px = (ann.start.y - point.y) * scale;
        constexpr double padding_px = 8.0;
        return dx_px >= -padding_px &&
            dx_px <= width_px + padding_px &&
            dy_px >= -padding_px &&
            dy_px <= height_px + padding_px;
    }
    if (ann.type == Annotation::Type::Circle) {
        const double radius = std::hypot(ann.end.x - ann.start.x, ann.end.y - ann.start.y);
        const double dist = std::abs(std::hypot(point.x - ann.start.x, point.y - ann.start.y) - radius);
        return dist * scale < pixel_radius;
    }
    if (ann.type == Annotation::Type::Arc) {
        const double d1 = std::hypot(point.x - ann.start.x, point.y - ann.start.y);
        const double d2 = std::hypot(point.x - ann.end.x, point.y - ann.end.y);
        const double d3 = std::hypot(point.x - ann.mid.x, point.y - ann.mid.y);
        return std::min({d1, d2, d3}) * scale < pixel_radius;
    }
    if (ann.type == Annotation::Type::Polyline) {
        if (ann.points.size() < 2) return false;
        double min_dist = std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < ann.points.size() - 1; ++i) {
            min_dist = std::min(min_dist, point_to_segment_distance(point, ann.points[i], ann.points[i+1]));
        }
        return min_dist * scale < pixel_radius;
    }
    if (ann.type == Annotation::Type::Rectangle) {
        const smn::Point a = ann.start;
        const smn::Point b{ann.end.x, ann.start.y};
        const smn::Point c = ann.end;
        const smn::Point d{ann.start.x, ann.end.y};
        const double dist = std::min({
            point_to_segment_distance(point, a, b),
            point_to_segment_distance(point, b, c),
            point_to_segment_distance(point, c, d),
            point_to_segment_distance(point, d, a),
        });
        return dist * scale < pixel_radius;
    }
    return point_to_segment_distance(point, ann.start, ann.end) * scale < pixel_radius;
}

int hit_dimension_annotation(POINT point, const RECT& preview, const smn::NestingResult& result, const PreviewTransform& view) {
    RECT drawable{preview.left, preview.top + 38, preview.right, preview.bottom - 46};
    if (!point_in_rect(drawable, point)) {
        return -1;
    }

    smn::Point sheet_pt = unproject_point(point, result.sheet.height, view.scale, view.origin_x, view.origin_y);
    for (int i = static_cast<int>(g_app.annotations.size()) - 1; i >= 0; --i) {
        const auto& ann = g_app.annotations[static_cast<size_t>(i)];
        if (ann.type == Annotation::Type::Measure && hit_annotation(ann, sheet_pt, view.scale)) {
            return i;
        }
    }
    return -1;
}

void cancel_pending_dimension_delete(HWND hwnd) {
    if (g_app.pending_dimension_delete_index != -1) {
        KillTimer(hwnd, kDimensionDeleteTimer);
        g_app.pending_dimension_delete_index = -1;
    }
}

void delete_pending_dimension(HWND hwnd) {
    const int index = g_app.pending_dimension_delete_index;
    g_app.pending_dimension_delete_index = -1;
    if (index < 0 ||
        index >= static_cast<int>(g_app.annotations.size()) ||
        g_app.annotations[static_cast<size_t>(index)].type != Annotation::Type::Measure) {
        return;
    }

    push_undo();
    g_app.annotations.erase(g_app.annotations.begin() + index);
    g_app.selected_annotation_index = -1;
    invalidate_preview(hwnd);
}

bool handle_dimension_click(HWND hwnd, LPARAM lparam, bool double_click) {
    if (g_app.dimension_building) {
        return false;
    }

    RECT client;
    GetClientRect(hwnd, &client);
    const RECT preview = preview_rect(client);
    const POINT point = point_from_lparam(lparam);
    if (!point_in_rect(preview, point)) {
        return false;
    }

    smn::NestingResult result;
    if (!current_preview_result(result)) {
        return false;
    }

    const PreviewTransform view = preview_transform(preview, result);
    const int index = hit_dimension_annotation(point, preview, result, view);
    if (index < 0) {
        return false;
    }

    g_app.selected_annotation_index = index;
    if (double_click) {
        cancel_pending_dimension_delete(hwnd);
        push_undo();
        edit_dimension_value(hwnd, g_app.annotations[static_cast<size_t>(index)]);
        invalidate_preview(hwnd);
        return true;
    }

    cancel_pending_dimension_delete(hwnd);
    invalidate_preview(hwnd);
    return true;
}

bool handle_preview_wheel(HWND hwnd, WPARAM wparam, LPARAM lparam) {
    RECT client;
    GetClientRect(hwnd, &client);
    const RECT preview = preview_rect(client);

    POINT point = point_from_lparam(lparam);
    ScreenToClient(hwnd, &point);
    if (!point_in_rect(preview, point)) {
        return false;
    }

    smn::NestingResult result;
    if (!current_preview_result(result)) {
        return false;
    }

    const PreviewTransform old_view = preview_transform(preview, result);
    const double logical_x = (static_cast<double>(point.x) - old_view.origin_x) / old_view.scale;
    const double logical_y_from_top = (static_cast<double>(point.y) - old_view.origin_y) / old_view.scale;

    const int wheel_delta = GET_WHEEL_DELTA_WPARAM(wparam);
    const double zoom_factor = std::pow(1.18, static_cast<double>(wheel_delta) / WHEEL_DELTA);
    g_app.preview_zoom = std::clamp(g_app.preview_zoom * zoom_factor, 0.15, 80.0);

    const PreviewTransform new_view = preview_transform(preview, result);
    g_app.preview_pan_x = static_cast<double>(point.x) - new_view.base_origin_x - logical_x * new_view.scale;
    g_app.preview_pan_y = static_cast<double>(point.y) - new_view.base_origin_y - logical_y_from_top * new_view.scale;

    InvalidateRect(hwnd, &preview, FALSE);
    return true;
}

void fit_preview_view(HWND hwnd) {
    g_app.preview_zoom = 1.0;
    g_app.preview_pan_x = 0.0;
    g_app.preview_pan_y = 0.0;
    RECT client;
    GetClientRect(hwnd, &client);
    RECT preview = preview_rect(client);
    InvalidateRect(hwnd, &preview, FALSE);
}

void delete_selected_annotation(HWND hwnd) {
    if (g_app.selected_annotation_index < 0 ||
        g_app.selected_annotation_index >= static_cast<int>(g_app.annotations.size())) {
        return;
    }
    g_app.annotations.erase(g_app.annotations.begin() + g_app.selected_annotation_index);
    g_app.selected_annotation_index = -1;
    RECT client;
    GetClientRect(hwnd, &client);
    RECT preview = preview_rect(client);
    InvalidateRect(hwnd, &preview, FALSE);
}

void clear_annotations(HWND hwnd) {
    if (g_app.annotations.empty()) {
        return;
    }
    g_app.annotations.clear();
    g_app.selected_annotation_index = -1;
    g_app.draft_active = false;
    clear_dimension_state();
    g_app.polyline_building = false;
    g_app.polyline_points.clear();
    g_app.arc_click_count = 0;
    RECT client;
    GetClientRect(hwnd, &client);
    RECT preview = preview_rect(client);
    InvalidateRect(hwnd, &preview, FALSE);
}

bool begin_preview_pan(HWND hwnd, LPARAM lparam) {
    RECT client;
    GetClientRect(hwnd, &client);
    const RECT preview = preview_rect(client);
    POINT point = point_from_lparam(lparam);
    if (!point_in_rect(preview, point)) {
        return false;
    }

    smn::NestingResult result;
    if (!current_preview_result(result)) {
        return false;
    }

    g_app.preview_interacting = true;
    g_app.preview_panning_view = true;
    g_app.dragging_annotation = false;
    g_app.preview_last_mouse = point;
    g_app.preview_down_pos = point;
    SetCapture(hwnd);
    SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
    return true;
}

std::string prompt_for_text(HWND parent, POINT pt);
TextAnnotationOptions prompt_for_text_annotation(HWND parent, const TextAnnotationOptions* initial = nullptr);

bool begin_preview_interaction(HWND hwnd, LPARAM lparam) {
    RECT client;
    GetClientRect(hwnd, &client);
    const RECT preview = preview_rect(client);
    POINT point = point_from_lparam(lparam);
    if (!point_in_rect(preview, point)) {
        return false;
    }

    smn::NestingResult result;
    if (!current_preview_result(result)) {
        return false;
    }

    const PreviewTransform view = preview_transform(preview, result);
    smn::Point sheet_pt = unproject_point(point, result.sheet.height, view.scale, view.origin_x, view.origin_y);
    const double snap_threshold = 12.0 / view.scale;
    const double grid_step = nice_grid_step((preview.right - preview.left) > 0 ? 46.0 / view.scale : 100.0);
    OSnapResult osnap = find_closest_snap(sheet_pt, result, g_app.annotations, snap_threshold);
    smn::Point snapped_pt = osnap.valid
        ? osnap.point
        : (g_app.snap_enabled ? snap_to_grid(sheet_pt, grid_step) : sheet_pt);
    g_app.crosshair_valid = true;
    g_app.crosshair_pos = point;
    g_app.crosshair_sheet_pos = snapped_pt;
    g_app.osnap_active = osnap.valid;

    SetFocus(hwnd); // Ensure we receive WM_KEYDOWN (like Delete and Esc)

    if (g_app.tool_mode == ToolMode::Delete) {
        int hit_index = -1;
        for (int i = static_cast<int>(g_app.annotations.size()) - 1; i >= 0; --i) {
            if (hit_annotation(g_app.annotations[static_cast<size_t>(i)], sheet_pt, view.scale)) {
                hit_index = i;
                break;
            }
        }
        if (hit_index != -1) {
            push_undo();
            g_app.annotations.erase(g_app.annotations.begin() + hit_index);
            g_app.selected_annotation_index = -1;
        }
        InvalidateRect(hwnd, &preview, FALSE);
        return true;
    }

    if (is_two_click_tool(g_app.tool_mode)) {
        start_or_finish_two_click_tool(hwnd, snapped_pt);
        return true;
    }

    if (g_app.tool_mode == ToolMode::Measure) {
        start_or_advance_smart_dimension(hwnd, result, view, snapped_pt);
        InvalidateRect(hwnd, &preview, FALSE);
        return true;
    }

    if (g_app.tool_mode == ToolMode::Text) {
        TextAnnotationOptions text_options = prompt_for_text_annotation(hwnd);
        if (text_options.accepted && !text_options.text.empty()) {
            push_undo();
            Annotation ann;
            ann.type = Annotation::Type::Text;
            ann.start = snapped_pt;
            ann.end = snapped_pt;
            ann.text = text_options.text;
            ann.text_font = text_options.font;
            ann.text_size = text_options.size;
            g_app.annotations.push_back(std::move(ann));
        }
        InvalidateRect(hwnd, &preview, FALSE);
        return true;
    }

    // Polyline tool: multi-click to add vertices
    if (g_app.tool_mode == ToolMode::Polyline) {
        if (!g_app.polyline_building) {
            g_app.polyline_building = true;
            g_app.polyline_points.clear();
            g_app.polyline_points.push_back(snapped_pt);
        } else {
            g_app.polyline_points.push_back(snapped_pt);
        }
        InvalidateRect(hwnd, &preview, FALSE);
        return true;
    }
    
    // Arc tool: 3 clicks (start, mid, end)
    if (g_app.tool_mode == ToolMode::Arc) {
        g_app.arc_click_count++;
        if (g_app.arc_click_count == 1) {
            g_app.arc_p1 = snapped_pt;
        } else if (g_app.arc_click_count == 2) {
            g_app.arc_p2 = snapped_pt;
        } else {
            g_app.arc_p3 = snapped_pt;
            push_undo();
            Annotation ann;
            ann.type = Annotation::Type::Arc;
            ann.start = g_app.arc_p1;
            ann.mid = g_app.arc_p2;
            ann.end = g_app.arc_p3;
            g_app.annotations.push_back(std::move(ann));
            g_app.arc_click_count = 0;
        }
        InvalidateRect(hwnd, &preview, FALSE);
        return true;
    }

    // Hit-testing for annotations ONLY if in Select mode
    int hit_index = -1;
    if (g_app.tool_mode == ToolMode::Select) {
        for (int i = static_cast<int>(g_app.annotations.size()) - 1; i >= 0; --i) {
            const auto& ann = g_app.annotations[i];
            if (hit_annotation(ann, sheet_pt, view.scale)) {
                hit_index = i;
                break;
            }
        }

        g_app.selected_annotation_index = hit_index;

        if (hit_index != -1) {
            g_app.dragging_annotation = true;
            push_undo();
            SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
        } else {
            g_app.dragging_annotation = false;
        }
        InvalidateRect(hwnd, &preview, FALSE);
    } else {
        g_app.dragging_annotation = false;
        g_app.selected_annotation_index = -1;
    }

    g_app.preview_interacting = true;
    g_app.preview_panning_view = false;
    g_app.preview_last_mouse = point;
    g_app.preview_down_pos = point;
    SetCapture(hwnd);

    if (g_app.dragging_annotation) {
        return true;
    }

    if (g_app.tool_mode == ToolMode::Select) {
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
    } else if (g_app.tool_mode == ToolMode::Pan) {
        g_app.preview_panning_view = true;
        SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
    } else if (g_app.tool_mode == ToolMode::Measure) {
        SetCursor(LoadCursor(nullptr, IDC_CROSS));
        push_undo();
        Annotation ann;
        ann.type = Annotation::Type::Measure;
        ann.start = snapped_pt;
        ann.end = snapped_pt;
        g_app.annotations.push_back(std::move(ann));
    } else if (g_app.tool_mode == ToolMode::Line) {
        SetCursor(LoadCursor(nullptr, IDC_CROSS));
        push_undo();
        Annotation ann;
        ann.type = Annotation::Type::Line;
        ann.start = snapped_pt;
        ann.end = snapped_pt;
        g_app.annotations.push_back(std::move(ann));
    } else if (g_app.tool_mode == ToolMode::Rectangle) {
        SetCursor(LoadCursor(nullptr, IDC_CROSS));
        push_undo();
        Annotation ann;
        ann.type = Annotation::Type::Rectangle;
        ann.start = snapped_pt;
        ann.end = snapped_pt;
        g_app.annotations.push_back(std::move(ann));
    } else if (g_app.tool_mode == ToolMode::Circle) {
        SetCursor(LoadCursor(nullptr, IDC_CROSS));
        push_undo();
        Annotation ann;
        ann.type = Annotation::Type::Circle;
        ann.start = snapped_pt;
        ann.end = snapped_pt;
        g_app.annotations.push_back(std::move(ann));
    } else if (g_app.tool_mode == ToolMode::Text) {
        SetCursor(LoadCursor(nullptr, IDC_IBEAM));
    }
    
    InvalidateRect(hwnd, &preview, FALSE);
    return true;
}

bool update_preview_interaction(HWND hwnd, LPARAM lparam) {
    if (!g_app.preview_interacting) {
        return false;
    }

    POINT point = point_from_lparam(lparam);
    smn::NestingResult result;
    if (!current_preview_result(result)) {
        return false;
    }
    
    RECT client;
    GetClientRect(hwnd, &client);
    const RECT preview = preview_rect(client);
    const PreviewTransform view = preview_transform(preview, result);
    const double grid_step = nice_grid_step((preview.right - preview.left) > 0 ? 46.0 / view.scale : 100.0);

    if (g_app.preview_panning_view) {
        g_app.preview_pan_x += point.x - g_app.preview_last_mouse.x;
        g_app.preview_pan_y += point.y - g_app.preview_last_mouse.y;
    } else if (g_app.dragging_annotation && g_app.selected_annotation_index != -1) {
        // Calculate delta in sheet coordinates
        smn::Point old_sheet_pt = unproject_point(g_app.preview_last_mouse, result.sheet.height, view.scale, view.origin_x, view.origin_y);
        smn::Point new_sheet_pt = unproject_point(point, result.sheet.height, view.scale, view.origin_x, view.origin_y);
        double dx = new_sheet_pt.x - old_sheet_pt.x;
        double dy = new_sheet_pt.y - old_sheet_pt.y;
        
        auto& ann = g_app.annotations[g_app.selected_annotation_index];
        ann.start.x += dx;
        ann.start.y += dy;
        ann.end.x += dx;
        ann.end.y += dy;
        ann.mid.x += dx;
        ann.mid.y += dy;
        for (auto& p : ann.points) { p.x += dx; p.y += dy; }
    } else if ((g_app.tool_mode == ToolMode::Line ||
                g_app.tool_mode == ToolMode::Rectangle ||
                g_app.tool_mode == ToolMode::Circle) &&
               !g_app.annotations.empty()) {
        smn::Point sheet_pt = unproject_point(point, result.sheet.height, view.scale, view.origin_x, view.origin_y);
        g_app.annotations.back().end = snap_to_grid(sheet_pt, grid_step);
    }

    g_app.preview_last_mouse = point;
    InvalidateRect(hwnd, &preview, FALSE);
    return true;
}

// Helper to ask for text using a proper modal dialog

static std::string g_dialog_result_text;
static std::string g_dialog_initial_text;
static std::string g_dialog_title_text;
static HFONT g_dialog_font = nullptr;
static bool g_input_dialog_class_registered = false;
static bool g_text_dialog_class_registered = false;

struct InputDialogState {
    HWND edit = nullptr;
    std::string initial;
    std::string title;
    std::string result;
    bool done = false;
    bool accepted = false;
    HFONT font = nullptr;
};

struct TextStyleDialogState {
    HWND text_edit = nullptr;
    HWND font_edit = nullptr;
    HWND size_edit = nullptr;
    TextAnnotationOptions options;
    bool done = false;
    HFONT font = nullptr;
};

LRESULT CALLBACK InputDialogWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    auto* state = reinterpret_cast<InputDialogState*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_CREATE: {
            auto* create = reinterpret_cast<CREATESTRUCTA*>(lparam);
            state = reinterpret_cast<InputDialogState*>(create->lpCreateParams);
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
            SetWindowTextA(hwnd, state->title.empty() ? "Modificar valor" : state->title.c_str());

            const bool text_prompt = state->title.find("texto") != std::string::npos ||
                state->title.find("Texto") != std::string::npos;
            const bool key_prompt = state->title.find("Activacion") != std::string::npos;
            std::string prompt_text = "Valor de cota (mm):";
            if (text_prompt) prompt_text = "Texto:";
            if (key_prompt) prompt_text = "Clave de Activacion:";

            HWND static_label = CreateWindowExA(
                0,
                "STATIC",
                prompt_text.c_str(),
                WS_CHILD | WS_VISIBLE,
                14,
                14,
                260,
                20,
                hwnd,
                nullptr,
                nullptr,
                nullptr
            );
            state->edit = CreateWindowExA(
                WS_EX_CLIENTEDGE,
                "EDIT",
                state->initial.c_str(),
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                14,
                38,
                260,
                24,
                hwnd,
                reinterpret_cast<HMENU>(101),
                nullptr,
                nullptr
            );
            HWND ok = CreateWindowExA(
                0,
                "BUTTON",
                "Aceptar",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON | (key_prompt ? BS_OWNERDRAW : 0),
                104,
                76,
                82,
                28,
                hwnd,
                reinterpret_cast<HMENU>(IDOK),
                nullptr,
                nullptr
            );
            HWND cancel = CreateWindowExA(
                0,
                "BUTTON",
                "Cancelar",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | (key_prompt ? BS_OWNERDRAW : 0),
                192,
                76,
                82,
                28,
                hwnd,
                reinterpret_cast<HMENU>(IDCANCEL),
                nullptr,
                nullptr
            );
            if (key_prompt) {
                LOGFONTA lf = {0};
                lf.lfHeight = -16;
                lf.lfWeight = FW_BOLD;
                strcpy_s(lf.lfFaceName, "Roboto");
                state->font = CreateFontIndirectA(&lf);
            }
            if (state->font) {
                SendMessageA(static_label, WM_SETFONT, reinterpret_cast<WPARAM>(state->font), TRUE);
                SendMessageA(state->edit, WM_SETFONT, reinterpret_cast<WPARAM>(state->font), TRUE);
                SendMessageA(ok, WM_SETFONT, reinterpret_cast<WPARAM>(state->font), TRUE);
                SendMessageA(cancel, WM_SETFONT, reinterpret_cast<WPARAM>(state->font), TRUE);
            }
            SendMessageA(state->edit, EM_SETSEL, 0, -1);
            SetFocus(state->edit);
            return 0;
        }
        case WM_ERASEBKGND: {
            if (state && state->title.find("Activacion") != std::string::npos) {
                HDC hdc = reinterpret_cast<HDC>(wparam);
                RECT rect;
                GetClientRect(hwnd, &rect);
                HBRUSH brush = CreateSolidBrush(RGB(31, 37, 35));
                FillRect(hdc, &rect, brush);
                DeleteObject(brush);
                return 1;
            }
            break;
        }
        case WM_DRAWITEM: {
            if (state && state->title.find("Activacion") != std::string::npos) {
                DRAWITEMSTRUCT* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lparam);
                if (dis->CtlType == ODT_BUTTON) {
                    HDC hdc = dis->hDC;
                    RECT rect = dis->rcItem;
                    bool is_pressed = (dis->itemState & ODS_SELECTED);
                    bool is_ok = (dis->CtlID == IDOK);

                    HBRUSH panel_bg = CreateSolidBrush(RGB(31, 37, 35));
                    FillRect(hdc, &rect, panel_bg);
                    DeleteObject(panel_bg);

                    COLORREF bg_color = is_ok ? RGB(232, 91, 46) : RGB(46, 101, 71);
                    if (is_pressed) bg_color = is_ok ? RGB(180, 70, 35) : RGB(36, 80, 56);
                    HBRUSH brush = CreateSolidBrush(bg_color);
                    HPEN pen = CreatePen(PS_SOLID, 1, bg_color);
                    HBRUSH old_brush = (HBRUSH)SelectObject(hdc, brush);
                    HPEN old_pen = (HPEN)SelectObject(hdc, pen);

                    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, 12, 12);

                    SelectObject(hdc, old_brush);
                    SelectObject(hdc, old_pen);
                    DeleteObject(brush);
                    DeleteObject(pen);

                    SetTextColor(hdc, RGB(255, 255, 255));
                    SetBkMode(hdc, TRANSPARENT);
                    char text[64];
                    GetWindowTextA(dis->hwndItem, text, sizeof(text));

                    LOGFONTA lf;
                    HFONT current_font = (HFONT)SendMessageA(dis->hwndItem, WM_GETFONT, 0, 0);
                    if (!current_font) current_font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
                    GetObjectA(current_font, sizeof(LOGFONTA), &lf);
                    lf.lfWeight = FW_BOLD;
                    HFONT bold_font = CreateFontIndirectA(&lf);
                    HFONT old_font = (HFONT)SelectObject(hdc, bold_font);

                    DrawTextA(hdc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                    SelectObject(hdc, old_font);
                    DeleteObject(bold_font);

                    return TRUE;
                }
            }
            break;
        }
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORSTATIC: {
            if (state && state->title.find("Activacion") != std::string::npos) {
                HDC hdc = reinterpret_cast<HDC>(wparam);
                SetTextColor(hdc, RGB(255, 223, 184));
                SetBkColor(hdc, RGB(31, 37, 35));
                static HBRUSH bg_brush = CreateSolidBrush(RGB(31, 37, 35));
                return reinterpret_cast<LRESULT>(bg_brush);
            }
            break;
        }
        case WM_CTLCOLOREDIT: {
            if (state && state->title.find("Activacion") != std::string::npos) {
                HDC hdc = reinterpret_cast<HDC>(wparam);
                SetTextColor(hdc, RGB(255, 223, 184));
                SetBkColor(hdc, RGB(21, 22, 22));
                static HBRUSH edit_brush = CreateSolidBrush(RGB(21, 22, 22));
                return reinterpret_cast<LRESULT>(edit_brush);
            }
            break;
        }
        case WM_COMMAND:
            if (LOWORD(wparam) == IDOK) {
                char buffer[512] = {};
                if (state && state->edit) {
                    GetWindowTextA(state->edit, buffer, 511);
                    state->result = buffer;
                    state->accepted = true;
                }
                if (state) {
                    state->done = true;
                }
                DestroyWindow(hwnd);
                return 0;
            }
            if (LOWORD(wparam) == IDCANCEL) {
                if (state) {
                    state->done = true;
                    state->accepted = false;
                }
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        case WM_CLOSE:
            if (state) {
                state->done = true;
                state->accepted = false;
            }
            DestroyWindow(hwnd);
            return 0;
        case WM_NCDESTROY:
            if (state) {
                state->done = true;
            }
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, 0);
            break;
    }
    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

LRESULT CALLBACK TextStyleDialogWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    auto* state = reinterpret_cast<TextStyleDialogState*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_CREATE: {
            auto* create = reinterpret_cast<CREATESTRUCTA*>(lparam);
            state = reinterpret_cast<TextStyleDialogState*>(create->lpCreateParams);
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
            SetWindowTextA(hwnd, "Texto");

            const auto make_label = [&](const char* text, int y) {
                HWND label = CreateWindowExA(0, "STATIC", text, WS_CHILD | WS_VISIBLE, 14, y, 90, 20, hwnd, nullptr, nullptr, nullptr);
                if (state->font) SendMessageA(label, WM_SETFONT, reinterpret_cast<WPARAM>(state->font), TRUE);
            };
            const auto make_edit = [&](int id, const char* text, int y) {
                HWND edit = CreateWindowExA(
                    WS_EX_CLIENTEDGE,
                    "EDIT",
                    text,
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    106,
                    y,
                    220,
                    24,
                    hwnd,
                    reinterpret_cast<HMENU>(id),
                    nullptr,
                    nullptr
                );
                if (state->font) SendMessageA(edit, WM_SETFONT, reinterpret_cast<WPARAM>(state->font), TRUE);
                return edit;
            };

            make_label("Texto:", 16);
            state->text_edit = make_edit(101, state->options.text.c_str(), 12);
            make_label("Fuente:", 48);
            state->font_edit = make_edit(102, state->options.font.c_str(), 44);
            make_label("Tamano:", 80);
            std::ostringstream size_text;
            size_text << std::fixed << std::setprecision(0) << state->options.size;
            state->size_edit = make_edit(103, size_text.str().c_str(), 76);

            HWND ok = CreateWindowExA(0, "BUTTON", "Aceptar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, 152, 114, 82, 28, hwnd, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
            HWND cancel = CreateWindowExA(0, "BUTTON", "Cancelar", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 244, 114, 82, 28, hwnd, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
            if (state->font) {
                SendMessageA(ok, WM_SETFONT, reinterpret_cast<WPARAM>(state->font), TRUE);
                SendMessageA(cancel, WM_SETFONT, reinterpret_cast<WPARAM>(state->font), TRUE);
            }
            SetFocus(state->text_edit);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wparam) == IDOK) {
                char text[512] = {};
                char font[128] = {};
                char size[64] = {};
                GetWindowTextA(state->text_edit, text, 511);
                GetWindowTextA(state->font_edit, font, 127);
                GetWindowTextA(state->size_edit, size, 63);
                state->options.text = text;
                state->options.font = font[0] ? font : "ISOCPEUR";
                try {
                    state->options.size = std::clamp(std::stod(size), 6.0, 96.0);
                } catch (...) {
                    state->options.size = 16.0;
                }
                state->options.accepted = true;
                state->done = true;
                DestroyWindow(hwnd);
                return 0;
            }
            if (LOWORD(wparam) == IDCANCEL) {
                state->done = true;
                state->options.accepted = false;
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        case WM_CLOSE:
            if (state) {
                state->done = true;
                state->options.accepted = false;
            }
            DestroyWindow(hwnd);
            return 0;
        case WM_NCDESTROY:
            if (state) state->done = true;
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, 0);
            break;
    }
    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

INT_PTR CALLBACK TextDialogProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_INITDIALOG: {
            HWND edit = GetDlgItem(hwnd, 101);
            if (!g_dialog_title_text.empty()) {
                SetWindowTextA(hwnd, g_dialog_title_text.c_str());
            }
            if (!g_dialog_initial_text.empty()) {
                SetDlgItemTextA(hwnd, 101, g_dialog_initial_text.c_str());
                SendMessageA(edit, EM_SETSEL, 0, -1);
            }
            if (g_dialog_font) {
                SendMessageA(edit, WM_SETFONT, reinterpret_cast<WPARAM>(g_dialog_font), TRUE);
            }
            SetFocus(edit);
            // Center dialog on parent
            RECT parent_rect, dlg_rect;
            GetWindowRect(GetParent(hwnd), &parent_rect);
            GetWindowRect(hwnd, &dlg_rect);
            int x = (parent_rect.left + parent_rect.right) / 2 - (dlg_rect.right - dlg_rect.left) / 2;
            int y = (parent_rect.top + parent_rect.bottom) / 2 - (dlg_rect.bottom - dlg_rect.top) / 2;
            SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            return FALSE;
        }
        case WM_COMMAND:
            if (LOWORD(wparam) == IDOK) {
                char buffer[512] = {};
                GetDlgItemTextA(hwnd, 101, buffer, 511);
                g_dialog_result_text = buffer;
                EndDialog(hwnd, IDOK);
                return TRUE;
            }
            if (LOWORD(wparam) == IDCANCEL) {
                g_dialog_result_text.clear();
                EndDialog(hwnd, IDCANCEL);
                return TRUE;
            }
            break;
        case WM_CLOSE:
            g_dialog_result_text.clear();
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
    }
    return FALSE;
}

std::string prompt_for_text(HWND parent, POINT pt) {
    return prompt_for_text_with_default(parent, pt, "", "Ingrese texto");
}

std::string prompt_for_text_with_default(HWND parent, POINT /*pt*/, const std::string& initial, const std::string& title) {
    HINSTANCE instance = GetModuleHandle(nullptr);
    if (!g_input_dialog_class_registered) {
        WNDCLASSA dialog_class = {};
        dialog_class.lpfnWndProc = InputDialogWndProc;
        dialog_class.hInstance = instance;
        dialog_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
        dialog_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        dialog_class.lpszClassName = "FoglarDimensionInputDialog";
        RegisterClassA(&dialog_class);
        g_input_dialog_class_registered = true;
    }

    RECT parent_rect{};
    GetWindowRect(parent, &parent_rect);
    const int width = 310;
    const int height = 150;
    const int x = parent_rect.left + ((parent_rect.right - parent_rect.left) - width) / 2;
    const int y = parent_rect.top + ((parent_rect.bottom - parent_rect.top) - height) / 2;

    InputDialogState state;
    state.initial = initial;
    state.title = title;
    state.font = g_app.normal_font ? g_app.normal_font : reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

    HWND dialog = CreateWindowExA(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        "FoglarDimensionInputDialog",
        title.empty() ? "Modificar valor" : title.c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x,
        y,
        width,
        height,
        parent,
        nullptr,
        instance,
        &state
    );
    if (!dialog) {
        return "";
    }

    EnableWindow(parent, FALSE);
    ShowWindow(dialog, SW_SHOW);
    UpdateWindow(dialog);

    MSG msg;
    while (!state.done && IsWindow(dialog) && GetMessageA(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && (msg.hwnd == dialog || GetParent(msg.hwnd) == dialog)) {
            if (msg.wParam == VK_RETURN) {
                SendMessageA(dialog, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), 0);
                continue;
            }
            if (msg.wParam == VK_ESCAPE) {
                SendMessageA(dialog, WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), 0);
                continue;
            }
        }
        if (!IsDialogMessageA(dialog, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
    SetFocus(parent);
    return state.accepted ? state.result : "";
}

TextAnnotationOptions prompt_for_text_annotation(HWND parent, const TextAnnotationOptions* initial) {
    HINSTANCE instance = GetModuleHandle(nullptr);
    if (!g_text_dialog_class_registered) {
        WNDCLASSA dialog_class = {};
        dialog_class.lpfnWndProc = TextStyleDialogWndProc;
        dialog_class.hInstance = instance;
        dialog_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
        dialog_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        dialog_class.lpszClassName = "FoglestingTextStyleDialog";
        RegisterClassA(&dialog_class);
        g_text_dialog_class_registered = true;
    }

    RECT parent_rect{};
    GetWindowRect(parent, &parent_rect);
    const int width = 360;
    const int height = 190;
    const int x = parent_rect.left + ((parent_rect.right - parent_rect.left) - width) / 2;
    const int y = parent_rect.top + ((parent_rect.bottom - parent_rect.top) - height) / 2;

    TextStyleDialogState state;
    if (initial) {
        state.options = *initial;
    } else {
        state.options.font = g_app.text_tool_font;
        state.options.size = g_app.text_tool_size;
    }
    if (state.options.font.empty()) {
        state.options.font = "ISOCPEUR";
    }
    state.options.size = std::clamp(state.options.size, 6.0, 96.0);
    state.font = g_app.normal_font ? g_app.normal_font : reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

    HWND dialog = CreateWindowExA(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        "FoglestingTextStyleDialog",
        "Texto",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x,
        y,
        width,
        height,
        parent,
        nullptr,
        instance,
        &state
    );
    if (!dialog) {
        return {};
    }

    EnableWindow(parent, FALSE);
    ShowWindow(dialog, SW_SHOW);
    UpdateWindow(dialog);

    MSG msg;
    while (!state.done && IsWindow(dialog) && GetMessageA(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && (msg.hwnd == dialog || GetParent(msg.hwnd) == dialog)) {
            if (msg.wParam == VK_RETURN) {
                SendMessageA(dialog, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), 0);
                continue;
            }
            if (msg.wParam == VK_ESCAPE) {
                SendMessageA(dialog, WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), 0);
                continue;
            }
        }
        if (!IsDialogMessageA(dialog, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
    SetFocus(parent);
    if (state.options.accepted) {
        g_app.text_tool_font = state.options.font;
        g_app.text_tool_size = state.options.size;
    }
    return state.options;
}

bool edit_text_annotation(HWND hwnd, int index) {
    if (index < 0 || index >= static_cast<int>(g_app.annotations.size())) {
        return false;
    }
    Annotation& ann = g_app.annotations[static_cast<size_t>(index)];
    if (ann.type != Annotation::Type::Text) {
        return false;
    }

    TextAnnotationOptions initial;
    initial.text = ann.text;
    initial.font = ann.text_font.empty() ? "ISOCPEUR" : ann.text_font;
    initial.size = std::clamp(ann.text_size, 6.0, 96.0);
    TextAnnotationOptions edited = prompt_for_text_annotation(hwnd, &initial);
    if (!edited.accepted) {
        return false;
    }

    ann.text = edited.text;
    ann.text_font = edited.font.empty() ? "ISOCPEUR" : edited.font;
    ann.text_size = std::clamp(edited.size, 6.0, 96.0);
    ann.end = ann.start;
    return true;
}

bool handle_annotation_double_click(HWND hwnd, LPARAM lparam) {
    RECT client;
    GetClientRect(hwnd, &client);
    const RECT preview = preview_rect(client);
    POINT point = point_from_lparam(lparam);
    if (!point_in_rect(preview, point)) {
        return false;
    }

    smn::NestingResult result;
    if (!current_preview_result(result)) {
        return false;
    }

    const PreviewTransform view = preview_transform(preview, result);
    smn::Point sheet_pt = unproject_point(point, result.sheet.height, view.scale, view.origin_x, view.origin_y);
    for (int i = static_cast<int>(g_app.annotations.size()) - 1; i >= 0; --i) {
        if (g_app.annotations[static_cast<size_t>(i)].type != Annotation::Type::Text) {
            continue;
        }
        if (!hit_annotation(g_app.annotations[static_cast<size_t>(i)], sheet_pt, view.scale)) {
            continue;
        }

        g_app.selected_annotation_index = i;
        push_undo();
        edit_text_annotation(hwnd, i);
        InvalidateRect(hwnd, &preview, FALSE);
        return true;
    }
    return false;
}

bool end_preview_interaction(HWND hwnd) {
    if (!g_app.preview_interacting) {
        return false;
    }
    const bool was_panning = g_app.preview_panning_view;
    const bool was_dragging_annotation = g_app.dragging_annotation;
    g_app.preview_interacting = false;
    g_app.preview_panning_view = false;
    g_app.dragging_annotation = false;
    ReleaseCapture();
    
    if (!was_panning && g_app.selected_annotation_index == -1 && g_app.tool_mode == ToolMode::Text) {
        POINT point = g_app.preview_last_mouse;
        TextAnnotationOptions text_options = prompt_for_text_annotation(hwnd);
        if (text_options.accepted && !text_options.text.empty()) {
            smn::NestingResult result;
            if (current_preview_result(result)) {
                RECT client;
                GetClientRect(hwnd, &client);
                const PreviewTransform view = preview_transform(preview_rect(client), result);
                smn::Point sheet_pt = unproject_point(point, result.sheet.height, view.scale, view.origin_x, view.origin_y);
                push_undo();
                Annotation ann;
                ann.type = Annotation::Type::Text;
                ann.start = sheet_pt;
                ann.end = sheet_pt;
                ann.text = text_options.text;
                ann.text_font = text_options.font;
                ann.text_size = text_options.size;
                g_app.annotations.push_back(std::move(ann));
            }
        }
    }
    if (!was_panning && !was_dragging_annotation && !g_app.annotations.empty()) {
        auto& ann = g_app.annotations.back();
        const bool is_draw_tool =
            g_app.tool_mode == ToolMode::Line ||
            g_app.tool_mode == ToolMode::Rectangle ||
            g_app.tool_mode == ToolMode::Circle;
        if (is_draw_tool && std::hypot(ann.end.x - ann.start.x, ann.end.y - ann.start.y) < 0.01) {
            g_app.annotations.pop_back();
        }
    }

    RECT client;
    GetClientRect(hwnd, &client);
    RECT preview = preview_rect(client);
    InvalidateRect(hwnd, &preview, FALSE);
    return true;
}

bool reset_preview_view(HWND hwnd, LPARAM lparam) {
    RECT client;
    GetClientRect(hwnd, &client);
    RECT preview = preview_rect(client);
    POINT point = point_from_lparam(lparam);
    if (!point_in_rect(preview, point)) {
        return false;
    }
    fit_preview_view(hwnd);
    return true;
}

bool is_active_tool_button(int id) {
    return (id == IdToolSelect && g_app.tool_mode == ToolMode::Select) ||
           (id == IdToolPan && g_app.tool_mode == ToolMode::Pan) ||
           (id == IdToolMeasure && g_app.tool_mode == ToolMode::Measure) ||
           (id == IdToolLine && g_app.tool_mode == ToolMode::Line) ||
           (id == IdToolRectangle && g_app.tool_mode == ToolMode::Rectangle) ||
           (id == IdToolCircle && g_app.tool_mode == ToolMode::Circle) ||
           (id == IdToolArc && g_app.tool_mode == ToolMode::Arc) ||
           (id == IdToolPolyline && g_app.tool_mode == ToolMode::Polyline) ||
           (id == IdToolText && g_app.tool_mode == ToolMode::Text) ||
           (id == IdToolDelete && g_app.tool_mode == ToolMode::Delete) ||
           (id == IdToolSnap && g_app.grid_visible);
}

bool draw_owner_button(LPDRAWITEMSTRUCT item) {
    if (!item || item->CtlType != ODT_BUTTON) {
        return false;
    }

    char text[128] = {};
    GetWindowTextA(item->hwndItem, text, static_cast<int>(sizeof(text)));
    const int id = static_cast<int>(item->CtlID);
    const bool disabled = (item->itemState & ODS_DISABLED) != 0;
    const bool pressed = (item->itemState & ODS_SELECTED) != 0;
    const bool focused = (item->itemState & ODS_FOCUS) != 0;
    const bool primary = id == IdRun || id == IdSaveDxf;
    const bool destructive = id == IdRemoveFile || id == IdToolDelete || id == IdToolClearAnnotations;
    const bool active_tool = is_active_tool_button(id);

    COLORREF fill = kFoglarPanel;
    COLORREF border = RGB(76, 88, 82);
    COLORREF text_color = kFoglarCream;
    if (primary) {
        fill = id == IdRun ? kFoglarOrange : kFoglarGreen;
        border = RGB(255, 196, 151);
        text_color = RGB(255, 248, 232);
    } else if (active_tool) {
        fill = RGB(67, 45, 34);
        border = kFoglarOrange;
    } else if (destructive) {
        border = RGB(162, 82, 72);
        text_color = RGB(255, 204, 190);
    }
    if (disabled) {
        fill = RGB(58, 60, 58);
        border = RGB(84, 88, 84);
        text_color = RGB(135, 140, 136);
    }
    if (pressed && !disabled) {
        fill = primary ? RGB(197, 73, 35) : RGB(43, 48, 45);
    }

    HBRUSH fill_brush = CreateSolidBrush(fill);
    HPEN border_pen = CreatePen(PS_SOLID, focused || active_tool ? 2 : 1, border);
    HGDIOBJ old_brush = SelectObject(item->hDC, fill_brush);
    HGDIOBJ old_pen = SelectObject(item->hDC, border_pen);
    RoundRect(
        item->hDC,
        item->rcItem.left,
        item->rcItem.top,
        item->rcItem.right,
        item->rcItem.bottom,
        6,
        6
    );
    SelectObject(item->hDC, old_pen);
    SelectObject(item->hDC, old_brush);
    DeleteObject(border_pen);
    DeleteObject(fill_brush);

    RECT text_rect = item->rcItem;
    if (pressed && !disabled) {
        OffsetRect(&text_rect, 1, 1);
    }
    SelectObject(item->hDC, g_app.small_font ? g_app.small_font : GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, text_color);
    DrawTextA(item->hDC, text, -1, &text_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    return true;
}

bool handle_cad_hotkey(HWND hwnd, WPARAM key) {
    if (GetKeyState(VK_CONTROL) & 0x8000 || GetKeyState(VK_MENU) & 0x8000) {
        return false;
    }

    switch (key) {
        case VK_ESCAPE:
            cancel_cad_command(hwnd);
            return true;
        case VK_RETURN:
        case VK_SPACE:
            if (finish_dimension(hwnd) || finish_polyline(hwnd) || finish_draft(hwnd)) {
                return true;
            }
            set_tool_mode(hwnd, g_app.last_draw_tool);
            return true;
        case 'V':
            set_tool_mode(hwnd, ToolMode::Select);
            return true;
        case 'L':
            set_tool_mode(hwnd, ToolMode::Line);
            return true;
        case 'R':
            set_tool_mode(hwnd, ToolMode::Rectangle);
            return true;
        case 'C':
            set_tool_mode(hwnd, ToolMode::Circle);
            return true;
        case 'A':
            set_tool_mode(hwnd, ToolMode::Arc);
            return true;
        case 'P':
            set_tool_mode(hwnd, ToolMode::Polyline);
            return true;
        case 'M':
            set_tool_mode(hwnd, ToolMode::Measure);
            return true;
        case 'T':
            set_tool_mode(hwnd, ToolMode::Text);
            return true;
        case 'H':
            set_tool_mode(hwnd, ToolMode::Pan);
            return true;
        case 'F':
            fit_preview_view(hwnd);
            return true;
        case 'S':
        case VK_F9:
            g_app.snap_enabled = !g_app.snap_enabled;
            InvalidateRect(hwnd, nullptr, FALSE);
            return true;
        case 'E':
            if (g_app.selected_annotation_index != -1) {
                push_undo();
                delete_selected_annotation(hwnd);
            } else {
                set_tool_mode(hwnd, ToolMode::Delete);
            }
            return true;
    }
    return false;
}

bool set_preview_cursor_if_needed(HWND hwnd) {
    POINT point;
    GetCursorPos(&point);
    ScreenToClient(hwnd, &point);
    RECT client;
    GetClientRect(hwnd, &client);
    if (!point_in_rect(preview_rect(client), point)) {
        return false;
    }

    if (g_app.tool_mode == ToolMode::Pan || g_app.preview_panning_view) {
        SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
    } else if (g_app.tool_mode == ToolMode::Delete) {
        SetCursor(LoadCursor(nullptr, IDC_CROSS));
    } else if (g_app.tool_mode == ToolMode::Text) {
        SetCursor(LoadCursor(nullptr, IDC_IBEAM));
    } else if (is_two_click_tool(g_app.tool_mode) ||
               g_app.tool_mode == ToolMode::Arc ||
               g_app.tool_mode == ToolMode::Polyline) {
        SetCursor(LoadCursor(nullptr, IDC_CROSS));
    } else {
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
    }
    return true;
}

void update_ui_language(HWND hwnd) {
    SetDlgItemTextA(hwnd, IdAddFolder, T("Agregar carpeta", "Add folder"));
    SetDlgItemTextA(hwnd, IdAddFiles, T("Agregar DXF", "Add DXF"));
    SetDlgItemTextA(hwnd, IdImportCsv, T("Importar CSV", "Import CSV"));
    SetDlgItemTextA(hwnd, IdRemoveFile, T("Quitar seleccionado", "Remove selected"));
    SetDlgItemTextA(hwnd, IdApplyQuantity, T("Aplicar cantidad", "Apply quantity"));
    SetDlgItemTextA(hwnd, IdBrowseOutput, T("Salida", "Output"));
    SetDlgItemTextA(hwnd, IdRun, T("Ejecutar nesting", "Run nesting"));
    SetDlgItemTextA(hwnd, IdPause, T("Pausar", "Pause"));
    SetDlgItemTextA(hwnd, IdSaveDxf, T("Guardar", "Save"));
    SetDlgItemTextA(hwnd, IdFeedback, T("Enviar comentarios / Reportar error", "Send feedback / Report bug"));
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_CREATE:
            g_app.hwnd = hwnd;
            load_brand_icon(hwnd);
            create_controls(hwnd);
            return 0;
        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
            info->ptMinTrackSize.x = 760;
            info->ptMinTrackSize.y = 520;
            return 0;
        }
        case WM_SIZE:
            g_app.file_list_height = clamp_file_list_height({0, 0, LOWORD(lparam), HIWORD(lparam)}, g_app.file_list_height);
            layout_controls(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_SETCURSOR:
            if (LOWORD(lparam) == HTCLIENT && set_resize_cursor_if_needed(hwnd)) {
                return TRUE;
            }
            if (LOWORD(lparam) == HTCLIENT && set_preview_cursor_if_needed(hwnd)) {
                return TRUE;
            }
            return DefWindowProcA(hwnd, message, wparam, lparam);
        case WM_DRAWITEM:
            if (draw_owner_button(reinterpret_cast<LPDRAWITEMSTRUCT>(lparam))) {
                return TRUE;
            }
            return DefWindowProcA(hwnd, message, wparam, lparam);
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            HDC control_dc = reinterpret_cast<HDC>(wparam);
            SetBkMode(control_dc, OPAQUE);
            SetBkColor(control_dc, kFoglarCoal);
            SetTextColor(control_dc, kFoglarCream);
            return reinterpret_cast<LRESULT>(g_app.panel_brush);
        }
        case WM_CTLCOLOREDIT: {
            HDC control_dc = reinterpret_cast<HDC>(wparam);
            SetBkColor(control_dc, kFoglarInput);
            SetTextColor(control_dc, kFoglarCream);
            return reinterpret_cast<LRESULT>(g_app.input_brush);
        }
        case WM_CTLCOLORLISTBOX: {
            HDC control_dc = reinterpret_cast<HDC>(wparam);
            SetBkMode(control_dc, OPAQUE);
            SetBkColor(control_dc, kFoglarDeep);
            SetTextColor(control_dc, RGB(235, 232, 224));
            return reinterpret_cast<LRESULT>(g_app.list_brush);
        }
        case WM_KEYDOWN:
            if ((wparam == VK_DELETE || wparam == VK_BACK) && g_app.selected_annotation_index != -1) {
                push_undo();
                delete_selected_annotation(hwnd);
                return 0;
            }
            // Ctrl+Z for undo
            if (wparam == 'Z' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                perform_undo(hwnd);
                return 0;
            }
            if (handle_cad_hotkey(hwnd, wparam)) {
                return 0;
            }
            return DefWindowProcA(hwnd, message, wparam, lparam);
        case WM_MOUSEWHEEL:
            {
                POINT screen_point{
                    static_cast<LONG>(static_cast<short>(LOWORD(lparam))),
                    static_cast<LONG>(static_cast<short>(HIWORD(lparam)))
                };
                POINT client_point = screen_point;
                ScreenToClient(hwnd, &client_point);
                if (client_point.x >= 0 && client_point.x < kPanelWidth) {
                    const int wheel_delta = GET_WHEEL_DELTA_WPARAM(wparam);
                    const int scroll_delta = wheel_delta < 0 ? 54 : -54;
                    if (scroll_left_panel(hwnd, scroll_delta)) {
                        return 0;
                    }
                }
            }
            if (handle_preview_wheel(hwnd, wparam, lparam)) {
                return 0;
            }
            return DefWindowProcA(hwnd, message, wparam, lparam);
        case WM_TIMER:
            if (wparam == kDimensionDeleteTimer) {
                KillTimer(hwnd, kDimensionDeleteTimer);
                delete_pending_dimension(hwnd);
                return 0;
            }
            return DefWindowProcA(hwnd, message, wparam, lparam);
        case WM_LBUTTONDOWN:
            if (begin_file_list_resize(hwnd, lparam)) {
                return 0;
            }
            if (handle_dimension_click(hwnd, lparam, false)) {
                return 0;
            }
            if (begin_preview_interaction(hwnd, lparam)) {
                return 0;
            }
            return DefWindowProcA(hwnd, message, wparam, lparam);
        case WM_MBUTTONDOWN:
            if (begin_preview_pan(hwnd, lparam)) {
                return 0;
            }
            return DefWindowProcA(hwnd, message, wparam, lparam);
        case WM_RBUTTONDOWN:
            if (finish_dimension(hwnd) || finish_polyline(hwnd) || finish_draft(hwnd)) {
                return 0;
            }
            if (g_app.arc_click_count > 0) {
                g_app.arc_click_count = 0;
                invalidate_preview(hwnd);
                return 0;
            }
            if (g_app.last_draw_tool != ToolMode::Select) {
                set_tool_mode(hwnd, g_app.last_draw_tool);
                return 0;
            }
            return DefWindowProcA(hwnd, message, wparam, lparam);
        case WM_MOUSEMOVE: {
            if (update_file_list_resize(hwnd, lparam)) {
                return 0;
            }
            if (update_preview_interaction(hwnd, lparam)) {
                return 0;
            }
            POINT pt = point_from_lparam(lparam);
            RECT client;
            GetClientRect(hwnd, &client);
            RECT preview = preview_rect(client);
            smn::NestingResult result;
            if (point_in_rect(preview, pt) && current_preview_result(result)) {
                g_app.crosshair_valid = true;
                g_app.crosshair_pos = pt;
                const PreviewTransform view = preview_transform(preview, result);
                smn::Point raw_sheet_pt = unproject_point(pt, result.sheet.height, view.scale, view.origin_x, view.origin_y);
                
                const double snap_threshold = 12.0 / view.scale; // OSNAP radius in mm based on 12 screen pixels
                OSnapResult osnap = find_closest_snap(raw_sheet_pt, result, g_app.annotations, snap_threshold);
                
                if (osnap.valid) {
                    g_app.crosshair_sheet_pos = osnap.point;
                    g_app.osnap_active = true;
                } else {
                    g_app.osnap_active = false;
                    if (g_app.snap_enabled && g_app.grid_visible) {
                        const double grid_step = nice_grid_step((preview.right - preview.left) > 0 ? 46.0 / view.scale : 100.0);
                        g_app.crosshair_sheet_pos = snap_to_grid(raw_sheet_pt, grid_step);
                    } else {
                        g_app.crosshair_sheet_pos = raw_sheet_pt;
                    }
                }
                if (g_app.draft_active && !g_app.annotations.empty()) {
                    g_app.annotations.back().end = g_app.crosshair_sheet_pos;
                }
                if (g_app.dimension_building && g_app.dimension_stage == 2 && !g_app.annotations.empty()) {
                    g_app.annotations.back().mid = g_app.crosshair_sheet_pos;
                }
                InvalidateRect(hwnd, &preview, FALSE);
            } else if (g_app.crosshair_valid) {
                g_app.crosshair_valid = false;
                InvalidateRect(hwnd, &preview, FALSE);
            }
            return DefWindowProcA(hwnd, message, wparam, lparam);
        }
        case WM_LBUTTONUP:
            if (end_file_list_resize(hwnd)) {
                return 0;
            }
            if (end_preview_interaction(hwnd)) {
                return 0;
            }
            return DefWindowProcA(hwnd, message, wparam, lparam);
        case WM_MBUTTONUP:
            if (end_preview_interaction(hwnd)) {
                return 0;
            }
            return DefWindowProcA(hwnd, message, wparam, lparam);
        case WM_CAPTURECHANGED:
            g_app.resizing_file_list = false;
            g_app.preview_interacting = false;
            g_app.preview_panning_view = false;
            return 0;
        case WM_LBUTTONDBLCLK:
            if (handle_dimension_click(hwnd, lparam, true)) {
                return 0;
            }
            if (handle_annotation_double_click(hwnd, lparam)) {
                return 0;
            }
            if (g_app.polyline_building && g_app.polyline_points.size() >= 2) {
                push_undo();
                Annotation ann;
                ann.type = Annotation::Type::Polyline;
                ann.start = g_app.polyline_points.front();
                ann.end = g_app.polyline_points.back();
                ann.points = g_app.polyline_points;
                g_app.annotations.push_back(std::move(ann));
                g_app.polyline_building = false;
                g_app.polyline_points.clear();
                RECT client;
                GetClientRect(hwnd, &client);
                RECT preview = preview_rect(client);
                InvalidateRect(hwnd, &preview, FALSE);
                return 0;
            }
            if (reset_preview_view(hwnd, lparam)) {
                return 0;
            }
            return DefWindowProcA(hwnd, message, wparam, lparam);
        case WM_MBUTTONDBLCLK:
            fit_preview_view(hwnd);
            return 0;
        case WM_NOTIFY: {
            if (!lparam) return DefWindowProcA(hwnd, message, wparam, lparam);
            LPNMHDR nmhdr = reinterpret_cast<LPNMHDR>(lparam);
            if (nmhdr->idFrom == IdFileList) {
                if (nmhdr->code == LVN_KEYDOWN) {
                    LPNMLVKEYDOWN kd = reinterpret_cast<LPNMLVKEYDOWN>(lparam);
                    if (kd->wVKey == VK_DELETE || kd->wVKey == VK_BACK) {
                        SendMessageA(hwnd, WM_COMMAND, MAKEWPARAM(IdRemoveFile, 0), reinterpret_cast<LPARAM>(nmhdr->hwndFrom));
                        return 0;
                    }
                }
                else if (nmhdr->code == LVN_ITEMCHANGED) {
                    LPNMLISTVIEW lv = reinterpret_cast<LPNMLISTVIEW>(lparam);
                    if ((lv->uChanged & LVIF_STATE) && (lv->uNewState & LVIS_SELECTED)) {
                        if (lv->iItem >= 0 && lv->iItem < static_cast<int>(g_app.selections.size())) {
                            set_text(g_app.selected_quantity, std::to_string(g_app.selections[static_cast<size_t>(lv->iItem)].quantity));
                        }
                        RECT client;
                        GetClientRect(hwnd, &client);
                        RECT preview = preview_rect(client);
                        InvalidateRect(hwnd, &preview, FALSE);
                    }
                }
                else if (nmhdr->code == NM_CLICK || nmhdr->code == NM_DBLCLK) {
                    LPNMITEMACTIVATE item = reinterpret_cast<LPNMITEMACTIVATE>(lparam);
                    if (item->iItem >= 0 && item->iSubItem == 1) {
                        begin_file_quantity_edit(item->iItem);
                        return 0;
                    }
                }
            }
            return DefWindowProcA(hwnd, message, wparam, lparam);
        }
        case WM_COMMAND: {
            const int id = LOWORD(wparam);
            const int notification = HIWORD(wparam);

            if (id == 9001) {
                g_lang_en = false;
                update_ui_language(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (id == 9002) {
                g_lang_en = true;
                update_ui_language(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }

            if (id == IdSolutionList && notification == LBN_SELCHANGE) {
                const int index = static_cast<int>(SendMessageA(g_app.solution_list, LB_GETCURSEL, 0, 0));
                if (g_app.running.load()) {
                    {
                        std::lock_guard<std::mutex> lock(g_app.mutex);
                        g_app.selected_solution_index = -1;
                    }
                    SendMessageA(g_app.solution_list, LB_SETCURSEL, -1, 0);
                    append_log("El solver sigue en vivo; detene para revisar una solucion guardada de la lista.\r\n");
                    return 0;
                }

                smn::NestingResult selected_result;
                bool has_selection = false;
                {
                    std::lock_guard<std::mutex> lock(g_app.mutex);
                    if (index >= 0 && index < static_cast<int>(g_app.ranked_solutions.size())) {
                        g_app.selected_solution_index = index;
                        selected_result = g_app.ranked_solutions[static_cast<size_t>(index)].result;
                        g_app.result = selected_result;
                        g_app.has_result = true;
                        has_selection = true;
                    }
                }
                if (has_selection) {
                    append_log("Solucion de la lista cargada en la vista previa. Usa Guardar DXF para exportarla.\r\n");
                }
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (id == IdAddFolder) {
                try {
                    add_folder(hwnd);
                } catch (const std::exception& error) {
                    MessageBoxA(hwnd, error.what(), "No se pudo agregar carpeta", MB_ICONERROR);
                }
                return 0;
            }
            if (id == IdAddFiles) {
                add_files(hwnd);
                return 0;
            }
            if (id == IdImportCsv) {
                IFileOpenDialog* dialog = nullptr;
                if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
                    DWORD options = 0;
                    dialog->GetOptions(&options);
                    dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST);
                    COMDLG_FILTERSPEC filters[] = { {L"CSV files", L"*.csv"}, {L"All files", L"*.*"} };
                    dialog->SetFileTypes(2, filters);
                    dialog->SetTitle(L"Elegir archivo CSV de cantidades");
                    if (SUCCEEDED(dialog->Show(hwnd))) {
                        IShellItem* item = nullptr;
                        if (SUCCEEDED(dialog->GetResult(&item))) {
                            PWSTR raw_path = nullptr;
                            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &raw_path))) {
                                std::filesystem::path csv_path = wide_to_utf8(raw_path);
                                CoTaskMemFree(raw_path);
                                std::ifstream file(csv_path);
                                if (file) {
                                    std::string line;
                                    while (std::getline(file, line)) {
                                        if (line.empty()) continue;
                                        size_t comma = line.find_last_of(',');
                                        if (comma != std::string::npos && comma + 1 < line.size()) {
                                            std::string name = line.substr(0, comma);
                                            std::string qty_str = line.substr(comma + 1);
                                            int qty = 1;
                                            try { qty = std::stoi(qty_str); } catch (...) {}
                                            if (qty > 0) {
                                                std::filesystem::path part_path = csv_path.parent_path() / name;
                                                if (std::filesystem::exists(part_path)) {
                                                    add_selection(part_path, qty);
                                                } else if (std::filesystem::exists(name)) {
                                                    add_selection(name, qty);
                                                }
                                            }
                                        }
                                    }
                                    refresh_file_list();
                                    append_log("CSV importado.\r\n");
                                }
                            }
                            item->Release();
                        }
                    }
                    dialog->Release();
                }
                return 0;
            }
            if (id == IdRemoveFile) {
                std::vector<int> selections;
                int iPos = ListView_GetNextItem(g_app.file_list, -1, LVNI_SELECTED);
                while (iPos != -1) {
                    selections.push_back(iPos);
                    iPos = ListView_GetNextItem(g_app.file_list, iPos, LVNI_SELECTED);
                }
                if (!selections.empty()) {
                    std::sort(selections.rbegin(), selections.rend());
                    for (int index : selections) {
                        if (index >= 0 && index < static_cast<int>(g_app.selections.size())) {
                            g_app.selections.erase(g_app.selections.begin() + index);
                        }
                    }
                    refresh_file_list();
                    start_input_preview(hwnd);
                }
                return 0;
            }
            if (id == IdApplyQuantity) {
                std::vector<int> selections;
                int iPos = ListView_GetNextItem(g_app.file_list, -1, LVNI_SELECTED);
                while (iPos != -1) {
                    selections.push_back(iPos);
                    iPos = ListView_GetNextItem(g_app.file_list, iPos, LVNI_SELECTED);
                }
                if (!selections.empty()) {
                    int qty = std::max(1, std::stoi(get_text(g_app.selected_quantity)));
                    for (int index : selections) {
                        if (index >= 0 && index < static_cast<int>(g_app.selections.size())) {
                            g_app.selections[static_cast<size_t>(index)].quantity = qty;
                        }
                    }
                    refresh_file_list();
                    for (int index : selections) {
                        ListView_SetItemState(g_app.file_list, index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                    }
                    start_input_preview(hwnd);
                }
                return 0;
            }
            if (id == IdBrowseOutput) {
                const auto output = choose_output(hwnd, get_text(g_app.output));
                if (!output.empty()) {
                    set_text(g_app.output, output);
                }
                return 0;
            }
            if (id == IdToolSelect) {
                set_tool_mode(hwnd, ToolMode::Select);
                return 0;
            }
            if (id == IdToolPan) {
                set_tool_mode(hwnd, ToolMode::Pan);
                return 0;
            }
            if (id == IdToolFit) {
                fit_preview_view(hwnd);
                return 0;
            }
            if (id == IdToolMeasure) {
                set_tool_mode(hwnd, ToolMode::Measure);
                return 0;
            }
            if (id == IdToolLine) {
                set_tool_mode(hwnd, ToolMode::Line);
                return 0;
            }
            if (id == IdToolRectangle) {
                set_tool_mode(hwnd, ToolMode::Rectangle);
                return 0;
            }
            if (id == IdToolCircle) {
                set_tool_mode(hwnd, ToolMode::Circle);
                return 0;
            }
            if (id == IdToolText) {
                set_tool_mode(hwnd, ToolMode::Text);
                return 0;
            }
            if (id == IdToolArc) {
                set_tool_mode(hwnd, ToolMode::Arc);
                return 0;
            }
            if (id == IdToolPolyline) {
                set_tool_mode(hwnd, ToolMode::Polyline);
                return 0;
            }
            if (id == IdToolSnap) {
                g_app.grid_visible = !g_app.grid_visible;
                g_app.snap_enabled = g_app.grid_visible;
                SetWindowTextA(g_app.tool_snap, g_app.grid_visible ? "Grilla ON" : "Grilla OFF");
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (id == IdToolUndo) {
                perform_undo(hwnd);
                return 0;
            }
            if (id == IdToolDelete) {
                if (g_app.selected_annotation_index != -1) {
                    push_undo();
                    delete_selected_annotation(hwnd);
                } else {
                    set_tool_mode(hwnd, ToolMode::Delete);
                }
                return 0;
            }
            if (id == IdToolClearAnnotations) {
                clear_annotations(hwnd);
                return 0;
            }
            if (id == IdSaveDxf) {
                try {
                    save_current_result(hwnd);
                } catch (const std::exception& error) {
                    MessageBoxA(hwnd, error.what(), "No se pudo guardar DXF", MB_ICONERROR);
                }
                return 0;
            }
            if (id == IdPause) {
                if (!g_app.running.load()) {
                    return 0;
                }
                const bool paused = !g_app.pause_requested.load();
                g_app.pause_requested.store(paused);
                SetWindowTextA(g_app.pause, paused ? "Continuar" : "Pausar");
                append_log(paused
                    ? "Solver pausado. Podes inspeccionar, guardar y cambiar parametros de busqueda.\r\n"
                    : "Solver reanudado. Aplico los parametros de busqueda editados.\r\n");
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (id == IdRun) {
                if (g_app.running.load()) {
                    g_app.stop_requested.store(true);
                    g_app.pause_requested.store(false);
                    EnableWindow(g_app.run, FALSE);
                    EnableWindow(g_app.pause, FALSE);
                    SetWindowTextA(g_app.run, "Deteniendo...");
                    append_log("Solicitud de detencion recibida. El solver se detiene sin guardar.\r\n");
                    return 0;
                }
                try {
                    run_nesting(hwnd);
                } catch (const std::exception& error) {
                    MessageBoxA(hwnd, error.what(), "Configuracion invalida", MB_ICONERROR);
                }
                return 0;
            }
            return 0;
        }
        case kRunProgress:
            refresh_solution_list();
            InvalidateRect(hwnd, nullptr, FALSE);
            if (g_app.graph_hwnd) InvalidateRect(g_app.graph_hwnd, nullptr, FALSE);
            if (g_app.tree_hwnd) InvalidateRect(g_app.tree_hwnd, nullptr, FALSE);
            return 0;
        case kInputPreviewReady: {
            std::string message_text;
            {
                std::lock_guard<std::mutex> lock(g_app.mutex);
                message_text = g_app.input_preview_message;
            }
            if (!message_text.empty()) {
                append_log(message_text + "\r\n");
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case kRunFinished: {
            std::string message_text;
            bool ok = false;
            {
                std::lock_guard<std::mutex> lock(g_app.mutex);
                message_text = g_app.worker_message;
                ok = g_app.worker_ok;
            }
            append_log(message_text);
            refresh_solution_list();
            g_app.running.store(false);
            g_app.stop_requested.store(false);
            g_app.pause_requested.store(false);
            EnableWindow(g_app.run, TRUE);
            EnableWindow(g_app.pause, FALSE);
            SetWindowTextA(g_app.run, "Ejecutar nesting");
            SetWindowTextA(g_app.pause, "Pausar");
            if (!ok) {
                MessageBoxA(hwnd, message_text.c_str(), "Nesting error", MB_ICONERROR);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            if (g_app.graph_hwnd) InvalidateRect(g_app.graph_hwnd, nullptr, FALSE);
            if (g_app.tree_hwnd) InvalidateRect(g_app.tree_hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            paint_buffered(hwnd, dc, [hwnd](HDC buffer_dc) {
                draw_shell(hwnd, buffer_dc);
            });
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_DESTROY:
            KillTimer(hwnd, kDimensionDeleteTimer);
            g_app.stop_requested.store(true);
            smn::GpuContext::instance().shutdown();
            DeleteObject(g_app.title_font);
            DeleteObject(g_app.normal_font);
            DeleteObject(g_app.small_font);
            DeleteObject(g_app.iso_font);
            delete g_app.brand_logo;
            g_app.brand_logo = nullptr;
            if (g_app.brand_icon) {
                DestroyIcon(g_app.brand_icon);
                g_app.brand_icon = nullptr;
            }
            if (g_app.gdiplus_token != 0) {
                Gdiplus::GdiplusShutdown(g_app.gdiplus_token);
                g_app.gdiplus_token = 0;
            }
            DeleteObject(g_app.panel_brush);
            DeleteObject(g_app.input_brush);
            DeleteObject(g_app.list_brush);
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcA(hwnd, message, wparam, lparam);
    }
}


void draw_evolution_tree(HWND hwnd, HDC dc, const RECT& rect) {
    HBRUSH background = CreateSolidBrush(kFoglarCoal);
    FillRect(dc, &rect, background);
    DeleteObject(background);

    std::vector<smn::IterationStats> history;
    {
        std::lock_guard<std::mutex> lock(g_app.mutex);
        history = g_app.history;
    }

    std::string legend = "Poblacion: " + get_text(g_app.ga_population) +
                         " | Mutacion: " + get_text(g_app.ga_mutation_rate) + "%" +
                         " | Shuffle Prob: " + get_text(g_app.ga_random_shuffle) + "%" +
                         " | Shuffle Int: " + get_text(g_app.ga_shuffle_intensity) + "%";

    if (history.empty()) {
        draw_text(dc, rect.left + 14, rect.top + 10, "Esperando iteraciones...", g_app.normal_font, kFoglarCream);
        draw_text(dc, rect.left + 14, rect.top + 30, legend.c_str(), g_app.small_font, kFoglarSteel);
        return;
    }

    const int node_w = 260;
    const int node_h = 70;
    const int item_h = 18;
    
    int current_x = rect.left + 180 - g_app.tree_scroll_x;
    int current_y = 50 - g_app.tree_scroll_y;
    int total_height = 50;
    int max_x = 180;

    HPEN line_pen = CreatePen(PS_SOLID, 2, kFoglarSteel);
    HPEN discard_pen = CreatePen(PS_SOLID, 1, RGB(180, 70, 70));
    HBRUSH node_brush = CreateSolidBrush(kFoglarDeep);
    HBRUSH highlight_brush = CreateSolidBrush(RGB(30, 90, 50)); 
    HPEN highlight_pen = CreatePen(PS_SOLID, 2, RGB(80, 200, 100)); 
    
    HBRUSH shuffle_brush = CreateSolidBrush(RGB(150, 100, 30)); 
    HPEN shuffle_pen = CreatePen(PS_SOLID, 2, RGB(220, 160, 50));
    
    HBRUSH fail_shuffle_brush = CreateSolidBrush(RGB(180, 50, 50));

    SelectObject(dc, g_app.small_font);
    SetBkMode(dc, TRANSPARENT);

    double current_best_area = std::numeric_limits<double>::max();

    for (size_t i = 0; i < history.size(); ++i) {
        const auto& stats = history[i];
        
        bool is_winner = (i == 0) || (stats.occupied_area < current_best_area - 1e-6);
        if (is_winner) {
            current_best_area = stats.occupied_area;
            
            if (i > 0) {
                if (stats.was_shuffle) {
                    int old_x = current_x;
                    current_x += 350;
                    int absolute_x = current_x - rect.left + g_app.tree_scroll_x;
                    if (absolute_x > max_x) max_x = absolute_x;
                    
                    SelectObject(dc, shuffle_pen);
                    MoveToEx(dc, old_x, current_y, nullptr);
                    LineTo(dc, old_x, current_y + 30);
                    
                    MoveToEx(dc, old_x, current_y + 30, nullptr);
                    LineTo(dc, current_x, current_y + 30);
                    
                    MoveToEx(dc, current_x, current_y + 30, nullptr);
                    LineTo(dc, current_x, current_y + 50);
                    
                    MoveToEx(dc, current_x, current_y + 50, nullptr);
                    LineTo(dc, current_x - 5, current_y + 40);
                    MoveToEx(dc, current_x, current_y + 50, nullptr);
                    LineTo(dc, current_x + 5, current_y + 40);
                    
                    current_y += 50;
                    total_height += 50;
                } else {
                    SelectObject(dc, line_pen);
                    MoveToEx(dc, current_x, current_y, nullptr);
                    LineTo(dc, current_x, current_y + 30);
                    
                    MoveToEx(dc, current_x, current_y + 30, nullptr);
                    LineTo(dc, current_x - 5, current_y + 20);
                    MoveToEx(dc, current_x, current_y + 30, nullptr);
                    LineTo(dc, current_x + 5, current_y + 20);
                    
                    current_y += 30;
                    total_height += 30;
                }
            }

            RECT node_rect = { current_x - node_w / 2, current_y, current_x + node_w / 2, current_y + node_h };
            HBRUSH bg_node = node_brush;
            HPEN pen_node = line_pen;
            if (i > 0) {
                bg_node = stats.was_shuffle ? shuffle_brush : highlight_brush;
                pen_node = stats.was_shuffle ? shuffle_pen : highlight_pen;
            }
            
            FillRect(dc, &node_rect, bg_node);
            SelectObject(dc, pen_node);
            MoveToEx(dc, node_rect.left, node_rect.top, nullptr);
            LineTo(dc, node_rect.right, node_rect.top);
            LineTo(dc, node_rect.right, node_rect.bottom);
            LineTo(dc, node_rect.left, node_rect.bottom);
            LineTo(dc, node_rect.left, node_rect.top);

            SetTextColor(dc, kFoglarCream);
            std::string title = "Iteracion " + std::to_string(stats.iteration);
            if (i == 0) title += " (Configuracion Base)";
            else if (stats.was_shuffle) title += " (Salto Evolutivo / Shuffle)";
            else title += " (Mutacion Ganadora)";
            TextOutA(dc, node_rect.left + 10, node_rect.top + 5, title.c_str(), static_cast<int>(title.size()));

            std::string area_str = "Area ocupada: " + format_area_m2(stats.occupied_area);
            TextOutA(dc, node_rect.left + 10, node_rect.top + 25, area_str.c_str(), static_cast<int>(area_str.size()));

            std::string util_str = "Utilizacion: " + format_percent_axis(stats.utilization * 100.0);
            TextOutA(dc, node_rect.left + 10, node_rect.top + 45, util_str.c_str(), static_cast<int>(util_str.size()));

            current_y += node_h;
            total_height += node_h;

            for (size_t d = 0; d < stats.discarded_mutations.size(); ++d) {
                const auto& d_mut = stats.discarded_mutations[d];
                if (d_mut.was_shuffle) {
                    SelectObject(dc, discard_pen);
                    int d_x = current_x + 350;
                    int absolute_x = d_x - rect.left + g_app.tree_scroll_x;
                    if (absolute_x > max_x) max_x = absolute_x;

                    MoveToEx(dc, current_x, current_y, nullptr);
                    LineTo(dc, current_x, current_y + 30);
                    MoveToEx(dc, current_x, current_y + 30, nullptr);
                    LineTo(dc, d_x, current_y + 30);
                    MoveToEx(dc, d_x, current_y + 30, nullptr);
                    LineTo(dc, d_x, current_y + 50);

                    MoveToEx(dc, d_x, current_y + 50, nullptr);
                    LineTo(dc, d_x - 5, current_y + 40);
                    MoveToEx(dc, d_x, current_y + 50, nullptr);
                    LineTo(dc, d_x + 5, current_y + 40);
                    
                    current_y += 50;
                    total_height += 50;

                    RECT d_node_rect = { d_x - node_w / 2, current_y, d_x + node_w / 2, current_y + node_h };
                    FillRect(dc, &d_node_rect, fail_shuffle_brush);
                    SelectObject(dc, discard_pen);
                    MoveToEx(dc, d_node_rect.left, d_node_rect.top, nullptr);
                    LineTo(dc, d_node_rect.right, d_node_rect.top);
                    LineTo(dc, d_node_rect.right, d_node_rect.bottom);
                    LineTo(dc, d_node_rect.left, d_node_rect.bottom);
                    LineTo(dc, d_node_rect.left, d_node_rect.top);

                    SetTextColor(dc, kFoglarCream);
                    std::string desc = "Shuffle (Descartado)";
                    TextOutA(dc, d_node_rect.left + 10, d_node_rect.top + 5, desc.c_str(), static_cast<int>(desc.size()));
                    std::string area_str2 = "Area: " + format_area_m2(d_mut.area);
                    TextOutA(dc, d_node_rect.left + 10, d_node_rect.top + 25, area_str2.c_str(), static_cast<int>(area_str2.size()));

                    current_y += node_h;
                    total_height += node_h;
                } else {
                    SelectObject(dc, discard_pen);
                    MoveToEx(dc, current_x, current_y, nullptr);
                    LineTo(dc, current_x, current_y + item_h);
                    MoveToEx(dc, current_x, current_y + item_h / 2, nullptr);
                    LineTo(dc, current_x + 20, current_y + item_h / 2);
                    
                    std::string desc = "Mutacion (Descartada): " + format_area_m2(d_mut.area);
                    SetTextColor(dc, RGB(220, 120, 120));
                    TextOutA(dc, current_x + 25, current_y + item_h / 2 - 7, desc.c_str(), static_cast<int>(desc.size()));
                    
                    current_y += item_h;
                    total_height += item_h;
                }
            }
        } else {
            std::vector<smn::MutationInfo> all_failed = stats.discarded_mutations;
            all_failed.push_back({stats.occupied_area, stats.was_shuffle}); 
            
            for (size_t d = 0; d < all_failed.size(); ++d) {
                const auto& d_mut = all_failed[d];
                if (d_mut.was_shuffle) {
                    SelectObject(dc, discard_pen);
                    int d_x = current_x + 350;
                    int absolute_x = d_x - rect.left + g_app.tree_scroll_x;
                    if (absolute_x > max_x) max_x = absolute_x;

                    MoveToEx(dc, current_x, current_y, nullptr);
                    LineTo(dc, current_x, current_y + 30);
                    MoveToEx(dc, current_x, current_y + 30, nullptr);
                    LineTo(dc, d_x, current_y + 30);
                    MoveToEx(dc, d_x, current_y + 30, nullptr);
                    LineTo(dc, d_x, current_y + 50);

                    MoveToEx(dc, d_x, current_y + 50, nullptr);
                    LineTo(dc, d_x - 5, current_y + 40);
                    MoveToEx(dc, d_x, current_y + 50, nullptr);
                    LineTo(dc, d_x + 5, current_y + 40);
                    
                    current_y += 50;
                    total_height += 50;

                    RECT d_node_rect = { d_x - node_w / 2, current_y, d_x + node_w / 2, current_y + node_h };
                    FillRect(dc, &d_node_rect, fail_shuffle_brush);
                    SelectObject(dc, discard_pen);
                    MoveToEx(dc, d_node_rect.left, d_node_rect.top, nullptr);
                    LineTo(dc, d_node_rect.right, d_node_rect.top);
                    LineTo(dc, d_node_rect.right, d_node_rect.bottom);
                    LineTo(dc, d_node_rect.left, d_node_rect.bottom);
                    LineTo(dc, d_node_rect.left, d_node_rect.top);

                    SetTextColor(dc, kFoglarCream);
                    std::string desc = "Shuffle (Descartado)";
                    TextOutA(dc, d_node_rect.left + 10, d_node_rect.top + 5, desc.c_str(), static_cast<int>(desc.size()));
                    std::string area_str2 = "Area: " + format_area_m2(d_mut.area);
                    TextOutA(dc, d_node_rect.left + 10, d_node_rect.top + 25, area_str2.c_str(), static_cast<int>(area_str2.size()));

                    current_y += node_h;
                    total_height += node_h;
                } else {
                    SelectObject(dc, discard_pen);
                    MoveToEx(dc, current_x, current_y, nullptr);
                    LineTo(dc, current_x, current_y + item_h);
                    MoveToEx(dc, current_x, current_y + item_h / 2, nullptr);
                    LineTo(dc, current_x + 20, current_y + item_h / 2);
                    
                    std::string desc = "Mutacion (Descartada): " + format_area_m2(d_mut.area);
                    SetTextColor(dc, RGB(220, 120, 120));
                    TextOutA(dc, current_x + 25, current_y + item_h / 2 - 7, desc.c_str(), static_cast<int>(desc.size()));
                    
                    current_y += item_h;
                    total_height += item_h;
                }
            }
        }
    }

    g_app.tree_max_scroll = std::max<int>(0, total_height - static_cast<int>(rect.bottom - rect.top));
    g_app.tree_max_scroll_x = std::max<int>(0, max_x + 200 - static_cast<int>(rect.right - rect.left));

    DeleteObject(line_pen);
    DeleteObject(highlight_pen);
    DeleteObject(discard_pen);
    DeleteObject(node_brush);
    DeleteObject(highlight_brush);
    DeleteObject(shuffle_pen);
    DeleteObject(shuffle_brush);
    DeleteObject(fail_shuffle_brush);

    RECT legend_bg = { rect.left, rect.top, rect.right, rect.top + 30 };
    HBRUSH bg_brush = CreateSolidBrush(kFoglarCoal);
    FillRect(dc, &legend_bg, bg_brush);
    DeleteObject(bg_brush);
    SetTextColor(dc, RGB(180, 200, 190));
    TextOutA(dc, rect.left + 14, rect.top + 8, legend.c_str(), static_cast<int>(legend.size()));
}
LRESULT CALLBACK tree_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        paint_buffered(hwnd, dc, [hwnd](HDC buffer_dc) {
            RECT rect;
            GetClientRect(hwnd, &rect);
            draw_evolution_tree(hwnd, buffer_dc, rect);
        });
        EndPaint(hwnd, &ps);
        return 0;
    } else if (message == WM_ERASEBKGND) {
        return 1;
    } else if (message == WM_VSCROLL) {
        int action = LOWORD(wparam);
        if (action == SB_LINEUP) g_app.tree_scroll_y -= 40;
        else if (action == SB_LINEDOWN) g_app.tree_scroll_y += 40;
        else if (action == SB_PAGEUP) g_app.tree_scroll_y -= 200;
        else if (action == SB_PAGEDOWN) g_app.tree_scroll_y += 200;
        else if (action == SB_THUMBTRACK) g_app.tree_scroll_y = HIWORD(wparam);
        
        if (g_app.tree_scroll_y < 0) g_app.tree_scroll_y = 0;
        if (g_app.tree_scroll_y > g_app.tree_max_scroll) g_app.tree_scroll_y = g_app.tree_max_scroll;
        
        SetScrollPos(hwnd, SB_VERT, g_app.tree_scroll_y, TRUE);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    } else if (message == WM_MOUSEWHEEL) {
        int zDelta = GET_WHEEL_DELTA_WPARAM(wparam);
        g_app.tree_scroll_y -= (zDelta / WHEEL_DELTA) * 60;
        
        if (g_app.tree_scroll_y < 0) g_app.tree_scroll_y = 0;
        if (g_app.tree_scroll_y > g_app.tree_max_scroll) g_app.tree_scroll_y = g_app.tree_max_scroll;
        
        SetScrollPos(hwnd, SB_VERT, g_app.tree_scroll_y, TRUE);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    } else if (message == WM_HSCROLL) {
        int action = LOWORD(wparam);
        if (action == SB_LINEUP) g_app.tree_scroll_x -= 40;
        else if (action == SB_LINEDOWN) g_app.tree_scroll_x += 40;
        else if (action == SB_PAGEUP) g_app.tree_scroll_x -= 200;
        else if (action == SB_PAGEDOWN) g_app.tree_scroll_x += 200;
        else if (action == SB_THUMBTRACK) g_app.tree_scroll_x = HIWORD(wparam);
        
        if (g_app.tree_scroll_x < 0) g_app.tree_scroll_x = 0;
        if (g_app.tree_scroll_x > g_app.tree_max_scroll_x) g_app.tree_scroll_x = g_app.tree_max_scroll_x;
        
        SetScrollPos(hwnd, SB_HORZ, g_app.tree_scroll_x, TRUE);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    } else if (message == WM_SIZE) {
        SCROLLINFO si = { sizeof(SCROLLINFO), SIF_RANGE | SIF_PAGE | SIF_POS };
        si.nMin = 0;
        si.nMax = g_app.tree_max_scroll + HIWORD(lparam);
        si.nPage = HIWORD(lparam);
        si.nPos = g_app.tree_scroll_y;
        SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
        
        SCROLLINFO siH = { sizeof(SCROLLINFO), SIF_RANGE | SIF_PAGE | SIF_POS };
        siH.nMin = 0;
        siH.nMax = g_app.tree_max_scroll_x + LOWORD(lparam);
        siH.nPage = LOWORD(lparam);
        siH.nPos = g_app.tree_scroll_x;
        SetScrollInfo(hwnd, SB_HORZ, &siH, TRUE);
        return DefWindowProcA(hwnd, message, wparam, lparam);
    } else if (message == WM_DESTROY) {
        g_app.tree_hwnd = nullptr;
        return 0;
    }
    return DefWindowProcA(hwnd, message, wparam, lparam);
}

LRESULT CALLBACK graph_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        paint_buffered(hwnd, dc, [hwnd](HDC buffer_dc) {
            RECT rect;
            GetClientRect(hwnd, &rect);
            draw_graph(buffer_dc, rect);
        });
        EndPaint(hwnd, &ps);
        return 0;
    } else if (message == WM_ERASEBKGND) {
        return 1;
    } else if (message == WM_DESTROY) {
        g_app.graph_hwnd = nullptr;
        return 0;
    }
    return DefWindowProcA(hwnd, message, wparam, lparam);
}


}  // namespace

#include <wininet.h>
#pragma comment(lib, "wininet.lib")

bool check_global_license_requirement() {
    HINTERNET hInternet = InternetOpenA("Foglesting/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return false;
    HINTERNET hConnect = InternetConnectA(hInternet, "firestore.googleapis.com", INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) { InternetCloseHandle(hInternet); return false; }
    std::string path = "/v1/projects/foglesting/databases/(default)/documents/settings/app_config";
    HINTERNET hRequest = HttpOpenRequestA(hConnect, "GET", path.c_str(), NULL, NULL, NULL, INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD, 0);
    if (!hRequest) { InternetCloseHandle(hConnect); InternetCloseHandle(hInternet); return false; }
    
    bool require_license = false;
    if (HttpSendRequestA(hRequest, NULL, 0, NULL, 0)) {
        DWORD statusCode = 0;
        DWORD length = sizeof(statusCode);
        if (HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &length, NULL)) {
            if (statusCode == 200) {
                std::string response;
                char buffer[1024];
                DWORD bytesRead = 0;
                while (InternetReadFile(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
                    response.append(buffer, bytesRead);
                }
                size_t pos1 = response.find("\"requireLicense\"");
                if (pos1 != std::string::npos) {
                    size_t pos2 = response.find("\"booleanValue\"", pos1);
                    if (pos2 != std::string::npos && pos2 < pos1 + 100) {
                        size_t pos3 = response.find("true", pos2);
                        if (pos3 != std::string::npos && pos3 < pos2 + 20) {
                            require_license = true;
                        }
                    }
                }
            }
        }
    }
    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    return require_license;
}

bool validate_license(const std::string& uid) {
    if (uid.empty()) return false;
    HINTERNET hInternet = InternetOpenA("Foglesting/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return false;
    HINTERNET hConnect = InternetConnectA(hInternet, "firestore.googleapis.com", INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) { InternetCloseHandle(hInternet); return false; }
    // Check user document for hasActiveLicense
    std::string path = "/v1/projects/foglesting/databases/(default)/documents/users/" + uid;
    HINTERNET hRequest = HttpOpenRequestA(hConnect, "GET", path.c_str(), NULL, NULL, NULL, INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD, 0);
    if (!hRequest) { InternetCloseHandle(hConnect); InternetCloseHandle(hInternet); return false; }
    
    bool valid = false;
    if (HttpSendRequestA(hRequest, NULL, 0, NULL, 0)) {
        DWORD statusCode = 0;
        DWORD length = sizeof(statusCode);
        if (HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &length, NULL)) {
            if (statusCode == 200) {
                std::string response;
                char buffer[1024];
                DWORD bytesRead = 0;
                while (InternetReadFile(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
                    response.append(buffer, bytesRead);
                }
                size_t pos_smn = response.find("\"Sheet Metal Nesting\"");
                if (pos_smn != std::string::npos) {
                    size_t pos_active = response.find("\"active\"", pos_smn);
                    if (pos_active != std::string::npos && (pos_active - pos_smn) < 200) {
                        valid = true;
                    }
                }
            }
        }
    }
    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    return valid;
}

std::string poll_device_login(const std::string& code) {
    HINTERNET hInternet = InternetOpenA("Foglesting/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return "";
    HINTERNET hConnect = InternetConnectA(hInternet, "firestore.googleapis.com", INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) { InternetCloseHandle(hInternet); return ""; }
    
    std::string path = "/v1/projects/foglesting/databases/(default)/documents/device_logins/" + code;
    std::string uid = "";
    
    // Poll for up to 5 minutes (150 * 2 seconds)
    for (int i = 0; i < 150; ++i) {
        HINTERNET hRequest = HttpOpenRequestA(hConnect, "GET", path.c_str(), NULL, NULL, NULL, INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD, 0);
        if (hRequest) {
            if (HttpSendRequestA(hRequest, NULL, 0, NULL, 0)) {
                DWORD statusCode = 0;
                DWORD length = sizeof(statusCode);
                if (HttpQueryInfoA(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &statusCode, &length, NULL)) {
                    if (statusCode == 200) {
                        std::string response;
                        char buffer[1024];
                        DWORD bytesRead = 0;
                        while (InternetReadFile(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
                            response.append(buffer, bytesRead);
                        }
                        
                        // Extract UID from JSON: "uid": { "stringValue": "THE_UID" }
                        size_t uid_pos = response.find("\"uid\"");
                        if (uid_pos != std::string::npos) {
                            size_t stringValPos = response.find("\"stringValue\":", uid_pos);
                            if (stringValPos != std::string::npos) {
                                size_t startQuote = response.find("\"", stringValPos + 14);
                                if (startQuote != std::string::npos) {
                                    size_t endQuote = response.find("\"", startQuote + 1);
                                    if (endQuote != std::string::npos) {
                                        uid = response.substr(startQuote + 1, endQuote - startQuote - 1);
                                        InternetCloseHandle(hRequest);
                                        
                                        // Try to delete the device login doc so it's not reused
                                        HINTERNET hDel = HttpOpenRequestA(hConnect, "DELETE", path.c_str(), NULL, NULL, NULL, INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD, 0);
                                        if (hDel) {
                                            HttpSendRequestA(hDel, NULL, 0, NULL, 0);
                                            InternetCloseHandle(hDel);
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            InternetCloseHandle(hRequest);
        }
        Sleep(2000);
    }
    
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    return uid;
}

constexpr const char* kFoglestingCurrentVersion = "5.1.0";
constexpr const char* kFoglestingUpdateHost = "foglesting-web.vercel.app";
constexpr const char* kFoglestingUpdatePath = "/version.json";
constexpr const char* kFoglestingDownloadPage = "https://foglesting-web.vercel.app/#descargar";

bool http_get_https(const char* host, const char* path, std::string& response) {
    HINTERNET internet = InternetOpenA("Foglesting Update Checker", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!internet) {
        return false;
    }
    HINTERNET connection = InternetConnectA(internet, host, INTERNET_DEFAULT_HTTPS_PORT, nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
    if (!connection) {
        InternetCloseHandle(internet);
        return false;
    }
    HINTERNET request = HttpOpenRequestA(
        connection,
        "GET",
        path,
        nullptr,
        nullptr,
        nullptr,
        INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE,
        0
    );
    if (!request) {
        InternetCloseHandle(connection);
        InternetCloseHandle(internet);
        return false;
    }

    bool ok = false;
    if (HttpSendRequestA(request, nullptr, 0, nullptr, 0)) {
        DWORD status_code = 0;
        DWORD length = sizeof(status_code);
        if (HttpQueryInfoA(request, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status_code, &length, nullptr) &&
            status_code == 200) {
            char buffer[4096];
            DWORD bytes_read = 0;
            while (InternetReadFile(request, buffer, sizeof(buffer), &bytes_read) && bytes_read > 0) {
                response.append(buffer, bytes_read);
            }
            ok = !response.empty();
        }
    }

    InternetCloseHandle(request);
    InternetCloseHandle(connection);
    InternetCloseHandle(internet);
    return ok;
}

std::string json_string_value(const std::string& json, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) {
        return "";
    }
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) {
        return "";
    }
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) {
        return "";
    }
    const size_t start = pos + 1;
    std::string value;
    bool escaped = false;
    for (size_t i = start; i < json.size(); ++i) {
        const char ch = json[i];
        if (escaped) {
            value.push_back(ch);
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') {
            return value;
        }
        value.push_back(ch);
    }
    return "";
}

std::vector<int> version_parts(const std::string& version) {
    std::vector<int> parts;
    int current = 0;
    bool has_digits = false;
    for (unsigned char ch : version) {
        if (std::isdigit(ch)) {
            current = current * 10 + static_cast<int>(ch - '0');
            has_digits = true;
        } else if (has_digits) {
            parts.push_back(current);
            current = 0;
            has_digits = false;
        }
    }
    if (has_digits) {
        parts.push_back(current);
    }
    while (parts.size() < 3) {
        parts.push_back(0);
    }
    return parts;
}

bool is_newer_version(const std::string& remote, const std::string& current) {
    const auto remote_parts = version_parts(remote);
    const auto current_parts = version_parts(current);
    const size_t count = std::max(remote_parts.size(), current_parts.size());
    for (size_t i = 0; i < count; ++i) {
        const int r = i < remote_parts.size() ? remote_parts[i] : 0;
        const int c = i < current_parts.size() ? current_parts[i] : 0;
        if (r != c) {
            return r > c;
        }
    }
    return false;
}

void check_for_updates_async(HWND hwnd) {
    std::thread([hwnd]() {
        std::string manifest;
        if (!http_get_https(kFoglestingUpdateHost, kFoglestingUpdatePath, manifest)) {
            return;
        }

        const std::string latest = json_string_value(manifest, "version");
        if (latest.empty() || !is_newer_version(latest, kFoglestingCurrentVersion)) {
            return;
        }

        std::string download_url = json_string_value(manifest, "download_url");
        if (download_url.empty()) {
            download_url = kFoglestingDownloadPage;
        }
        const std::string notes = json_string_value(manifest, "notes");

        std::ostringstream message;
        message << "Hay una nueva version de FOGLESTING disponible.\r\n\r\n"
                << "Version instalada: " << kFoglestingCurrentVersion << "\r\n"
                << "Nueva version: " << latest << "\r\n";
        if (!notes.empty()) {
            message << "\r\n" << notes << "\r\n";
        }
        message << "\r\nDesea abrir la pagina de descarga para actualizar?";

        const int answer = MessageBoxA(
            hwnd,
            message.str().c_str(),
            "Actualizacion disponible",
            MB_ICONINFORMATION | MB_YESNO | MB_DEFBUTTON1
        );
        if (answer == IDYES) {
            ShellExecuteA(hwnd, "open", download_url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
    }).detach();
}

#include <ctime>
#include <sys/stat.h>

bool check_free_trial(int& days_left) {
    char appdata[MAX_PATH];
    GetEnvironmentVariableA("APPDATA", appdata, MAX_PATH);
    std::string trial_dir = std::string(appdata) + "\\Foglesting";
    CreateDirectoryA(trial_dir.c_str(), NULL);
    std::string trial_file = trial_dir + "\\t_lic.dat";

    struct stat buffer;
    if (stat(trial_file.c_str(), &buffer) != 0) {
        days_left = 10;
        return false;
    }

    std::ifstream in(trial_file);
    if (!in.is_open()) { days_left = 0; return false; }
    time_t start_time;
    in >> start_time;
    in.close();

    time_t now = time(nullptr);
    double seconds = difftime(now, start_time);
    int days_passed = static_cast<int>(seconds / (60 * 60 * 24));
    days_left = 10 - days_passed;

    if (days_left <= 0) {
        days_left = 0;
        return true;
    }
    return true;
}

void start_free_trial() {
    char appdata[MAX_PATH];
    GetEnvironmentVariableA("APPDATA", appdata, MAX_PATH);
    std::string trial_dir = std::string(appdata) + "\\Foglesting";
    CreateDirectoryA(trial_dir.c_str(), NULL);
    std::string trial_file = trial_dir + "\\t_lic.dat";
    std::ofstream out(trial_file);
    out << time(nullptr);
    out.close();
}

std::string foglesting_appdata_file(const char* filename) {
    char appdata[MAX_PATH] = {};
    DWORD len = GetEnvironmentVariableA("APPDATA", appdata, MAX_PATH);
    std::string dir;
    if (len > 0 && len < MAX_PATH) {
        dir = std::string(appdata) + "\\Foglesting";
    } else {
        dir = ".";
    }
    CreateDirectoryA(dir.c_str(), NULL);
    return dir + "\\" + filename;
}

int g_auth_action = -1;
std::string g_auth_code = "";
int g_auth_trial_days = 10;
ULONG_PTR g_auth_gdiplus_token = 0;
HFONT g_hFontNormal = NULL;
HFONT g_hFontBold = NULL;
HBRUSH g_hbrDark = NULL;

LRESULT CALLBACK AuthDialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        g_hbrDark = CreateSolidBrush(RGB(26, 26, 26));
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        {
            int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
            float scale = dpi / 96.0f;

            Gdiplus::Graphics graphics(hdc);
            graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            graphics.Clear(Gdiplus::Color(255, 26, 26, 26));

            Gdiplus::FontFamily fontFamily(L"Arial");
            Gdiplus::Font fontLogo(&fontFamily, 36 * scale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
            Gdiplus::SolidBrush orangeBrush(Gdiplus::Color(255, 232, 91, 46));
            Gdiplus::SolidBrush greenBrush(Gdiplus::Color(255, 61, 135, 96));
            Gdiplus::SolidBrush whiteBrush(Gdiplus::Color(255, 255, 255, 255));
            Gdiplus::SolidBrush grayBrush(Gdiplus::Color(255, 170, 170, 170));
            Gdiplus::StringFormat format;
            format.SetAlignment(Gdiplus::StringAlignmentCenter);

            graphics.DrawString(L"Fogl", -1, &fontLogo, Gdiplus::PointF(205.0f * scale, 40.0f * scale), &format, &orangeBrush);
            graphics.DrawString(L"esting", -1, &fontLogo, Gdiplus::PointF(295.0f * scale, 40.0f * scale), &format, &greenBrush);

            Gdiplus::Font fontTitle(&fontFamily, 22 * scale, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
            graphics.DrawString(L"Inicio de Sesión Requerido", -1, &fontTitle, Gdiplus::PointF(250.0f * scale, 100.0f * scale), &format, &whiteBrush);

            Gdiplus::Font fontBody(&fontFamily, 16 * scale, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
            graphics.DrawString(L"Para usar el software, necesitás vincular tu cuenta.", -1, &fontBody, Gdiplus::PointF(250.0f * scale, 150.0f * scale), &format, &grayBrush);
            
            std::wstring codeW(g_auth_code.begin(), g_auth_code.end());
            std::wstring codeText = L"Código de dispositivo: " + codeW;
            Gdiplus::Font fontCode(&fontFamily, 22 * scale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
            graphics.DrawString(codeText.c_str(), -1, &fontCode, Gdiplus::PointF(250.0f * scale, 200.0f * scale), &format, &orangeBrush);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC:
        SetBkMode((HDC)wParam, TRANSPARENT);
        SetTextColor((HDC)wParam, RGB(255, 255, 255));
        return (LRESULT)g_hbrDark;
    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        if (wmId == 101) { g_auth_action = 1; DestroyWindow(hwnd); }
        if (wmId == 102) { g_auth_action = 2; DestroyWindow(hwnd); }
        return 0;
    }
    case WM_CLOSE:
    case WM_DESTROY:
        if (g_hbrDark) DeleteObject(g_hbrDark);
        if (g_hFontNormal) DeleteObject(g_hFontNormal);
        if (g_hFontBold) DeleteObject(g_hFontBold);
        g_auth_action = (g_auth_action == -1) ? 0 : g_auth_action;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

void ShowCustomAuthDialog(HINSTANCE hInstance, const std::string& code, int trial_days, int& out_action) {
    g_auth_code = code;
    g_auth_trial_days = trial_days;
    g_auth_action = -1;

    Gdiplus::GdiplusStartupInput gdiSI;
    Gdiplus::GdiplusStartup(&g_auth_gdiplus_token, &gdiSI, NULL);

    WNDCLASSA wc = {};
    wc.lpfnWndProc = AuthDialogProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "FoglestingAuthDialog";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassA(&wc);

    HDC hdcScreen = GetDC(NULL);
    int dpi = GetDeviceCaps(hdcScreen, LOGPIXELSX);
    ReleaseDC(NULL, hdcScreen);
    float scale = dpi / 96.0f;

    int w = (int)(500 * scale);
    int h = (int)(420 * scale);
    int sx = GetSystemMetrics(SM_CXSCREEN);
    int sy = GetSystemMetrics(SM_CYSCREEN);
    HWND hwnd = CreateWindowExA(0, "FoglestingAuthDialog", "Foglesting - Validar Licencia", WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        (sx - w) / 2, (sy - h) / 2, w, h, NULL, NULL, hInstance, NULL);

    g_hFontBold = CreateFontA((int)(18 * scale), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
    g_hFontNormal = CreateFontA((int)(16 * scale), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");

    HWND btn1 = CreateWindowA("BUTTON", "Vincular Cuenta en la Web", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        (int)(100 * scale), (int)(260 * scale), (int)(300 * scale), (int)(45 * scale), hwnd, (HMENU)101, hInstance, NULL);
    SendMessage(btn1, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);
    
    std::string trial_text;
    if (trial_days > 0) trial_text = "Continuar Gratis (" + std::to_string(trial_days) + " dias restantes)";
    else trial_text = "Prueba Gratuita Expirada";

    HWND btn2 = CreateWindowA("BUTTON", trial_text.c_str(), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
        (int)(100 * scale), (int)(320 * scale), (int)(300 * scale), (int)(40 * scale), hwnd, (HMENU)102, hInstance, NULL);
    SendMessage(btn2, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    if (trial_days <= 0) EnableWindow(btn2, FALSE);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    out_action = g_auth_action;
    Gdiplus::GdiplusShutdown(g_auth_gdiplus_token);
}

void enable_dpi_awareness() {
    HMODULE user32 = GetModuleHandleA("user32.dll");
    using SetProcessDpiAwarenessContextFn = BOOL (WINAPI*)(HANDLE);
    auto* set_context = user32
        ? reinterpret_cast<SetProcessDpiAwarenessContextFn>(GetProcAddress(user32, "SetProcessDpiAwarenessContext"))
        : nullptr;
    // Let Windows scale this fixed-pixel Win32/GDI UI on high-DPI displays.
    // Per-monitor DPI aware made the 4K layout crisp but physically tiny because
    // most controls and CAD overlay text are still sized in raw pixels.
    if (set_context && set_context(reinterpret_cast<HANDLE>(-5))) { // DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED
        return;
    }
    // On older Windows versions, staying DPI-unaware is preferable to forcing
    // SetProcessDPIAware(), which makes text too small on 4K notebooks.
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show_command) {
    enable_dpi_awareness();
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);

    const char* class_name = "SheetMetalNestingCppWindow";
    WNDCLASSA window_class = {};
    window_class.style = CS_DBLCLKS;
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = class_name;
    window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassA(&window_class);

    const char* graph_class_name = "SheetMetalNestingGraphWindow";
    WNDCLASSA graph_window_class = {};
    graph_window_class.style = CS_DBLCLKS;
    graph_window_class.lpfnWndProc = graph_window_proc;
    graph_window_class.hInstance = instance;
    graph_window_class.lpszClassName = graph_class_name;
    graph_window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    graph_window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassA(&graph_window_class);

    const char* tree_class_name = "SheetMetalNestingTreeWindow";
    WNDCLASSA tree_window_class = {};
    tree_window_class.style = CS_DBLCLKS;
    tree_window_class.lpfnWndProc = tree_window_proc;
    tree_window_class.hInstance = instance;
    tree_window_class.lpszClassName = tree_class_name;
    tree_window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    tree_window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassA(&tree_window_class);

    RECT work_area{0, 0, 1320, 1010};
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &work_area, 0);
    const int available_w = std::max(760, static_cast<int>(work_area.right - work_area.left - 40));
    const int available_h = std::max(520, static_cast<int>(work_area.bottom - work_area.top - 40));
    const int window_w = std::min(1320, available_w);
    const int window_h = std::min(1010, available_h);
    const int window_x = static_cast<int>(work_area.left) +
        std::max(0, static_cast<int>((work_area.right - work_area.left - window_w) / 2));
    const int window_y = static_cast<int>(work_area.top) +
        std::max(0, static_cast<int>((work_area.bottom - work_area.top - window_h) / 2));

    HWND hwnd = CreateWindowExA(
        0,
        class_name,
        "FOGLESTING V5.1",
        WS_OVERLAPPEDWINDOW,
        window_x,
        window_y,
        window_w,
        window_h,
        nullptr,
        nullptr,
        instance,
        nullptr
    );

    if (!hwnd) {
        CoUninitialize();
        return 1;
    }

    HMENU hMenu = CreateMenu();
    HMENU hSubMenuLang = CreatePopupMenu();
    AppendMenuA(hSubMenuLang, MF_STRING, 9001, "Español");
    AppendMenuA(hSubMenuLang, MF_STRING, 9002, "English");
    AppendMenuA(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hSubMenuLang), "Idioma / Language");
    SetMenu(hwnd, hMenu);

    if (check_global_license_requirement()) {
        std::string uid;
        const std::string license_path = foglesting_appdata_file("license.key");
        std::ifstream uid_file(license_path);
        if (uid_file.is_open()) {
            std::getline(uid_file, uid);
            uid_file.close();
        }
        
        while (true) {
            if (!uid.empty() && validate_license(uid)) {
                std::ofstream out(license_path);
                out << uid;
                out.close();
                break;
            }
            if (!uid.empty()) {
                MessageBoxA(NULL, "Su cuenta vinculada no posee una licencia activa.\nPor favor, contactese con el administrador.", "Licencia Inactiva", MB_ICONERROR);
                uid = ""; 
            }
            
            srand(GetTickCount());
            int rand_code = 100000 + (rand() % 900000);
            std::string code_str = std::to_string(rand_code);
            
            int days_left = 10;
            bool trial_started = check_free_trial(days_left);
            
            int action = 0;
            ShowCustomAuthDialog(instance, code_str, days_left, action);
            
            if (action == 0) {
                CoUninitialize();
                return 0;
            } else if (action == 1) {
                std::string url = "https://foglesting.com/auth.html?code=" + code_str;
                ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
                
                uid = poll_device_login(code_str);
                if (uid.empty()) {
                    if (MessageBoxA(NULL, "No se detecto inicio de sesion a tiempo. ¿Desea reintentar?", "Tiempo Agotado", MB_YESNO | MB_ICONWARNING) == IDNO) {
                        CoUninitialize();
                        return 0;
                    }
                } else {
                    if (validate_license(uid)) {
                        MessageBoxA(NULL, "Licencia confirmada con exito. Bienvenido.", "Exito", MB_ICONINFORMATION);
                        std::ofstream out(license_path);
                        out << uid;
                        out.close();
                        break;
                    } else {
                        MessageBoxA(NULL, "Su cuenta no posee una licencia activa.\nPor favor, contactese con el administrador.", "Sin Licencia", MB_ICONERROR);
                        uid = ""; 
                    }
                }
            } else if (action == 2) {
                if (!trial_started) {
                    start_free_trial();
                }
                break; 
            }
        }
    }

    g_app.graph_hwnd = CreateWindowExA(
        WS_EX_TOOLWINDOW,
        graph_class_name,
        "Ahorro por iteracion",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        600,
        400,
        nullptr,
        nullptr,
        instance,
        nullptr
    );

    if (g_app.graph_hwnd) {
        ShowWindow(g_app.graph_hwnd, show_command);
        UpdateWindow(g_app.graph_hwnd);
    }

    g_app.tree_hwnd = CreateWindowExA(
        WS_EX_TOOLWINDOW,
        tree_class_name,
        "Arbol Evolutivo de Mutaciones",
        WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_HSCROLL,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        600,
        800,
        nullptr,
        nullptr,
        instance,
        nullptr
    );

    if (g_app.tree_hwnd) {
        ShowWindow(g_app.tree_hwnd, show_command);
        UpdateWindow(g_app.tree_hwnd);
    }

    ShowWindow(hwnd, show_command);
    UpdateWindow(hwnd);
    check_for_updates_async(hwnd);

    MSG message;
    while (GetMessageA(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
    CoUninitialize();
    return 0;
}
