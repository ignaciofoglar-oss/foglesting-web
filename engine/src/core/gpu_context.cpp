#include "core/gpu_context.hpp"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// ---- Minimal OpenCL type definitions (no SDK needed) ----
using cl_int = int32_t;
using cl_uint = uint32_t;
using cl_ulong = uint64_t;
using cl_bitfield = uint64_t;
using cl_platform_id = void*;
using cl_device_id = void*;
using cl_context = void*;
using cl_command_queue = void*;
using cl_program = void*;
using cl_kernel = void*;
using cl_mem = void*;
using cl_device_type = cl_bitfield;
using cl_device_info = cl_uint;
using cl_platform_info = cl_uint;
using cl_context_properties = intptr_t;
using cl_command_queue_properties = cl_bitfield;
using cl_program_build_info = cl_uint;
using cl_kernel_work_group_info = cl_uint;
using cl_mem_flags = cl_bitfield;

constexpr cl_int CL_SUCCESS = 0;
constexpr cl_device_type CL_DEVICE_TYPE_GPU = (1 << 2);
constexpr cl_device_info CL_DEVICE_NAME = 0x102B;
constexpr cl_device_info CL_DEVICE_VENDOR = 0x102C;
constexpr cl_device_info CL_DEVICE_GLOBAL_MEM_SIZE = 0x101F;
constexpr cl_device_info CL_DEVICE_MAX_COMPUTE_UNITS = 0x1002;
constexpr cl_device_info CL_DEVICE_MAX_WORK_GROUP_SIZE = 0x1004;
constexpr cl_platform_info CL_PLATFORM_NAME = 0x0902;
constexpr cl_context_properties CL_CONTEXT_PLATFORM = 0x1084;
constexpr cl_program_build_info CL_PROGRAM_BUILD_LOG = 0x1183;
constexpr cl_mem_flags CL_MEM_READ_ONLY = (1 << 2);
constexpr cl_mem_flags CL_MEM_WRITE_ONLY = (1 << 1);
constexpr cl_mem_flags CL_MEM_COPY_HOST_PTR = (1 << 5);
constexpr cl_kernel_work_group_info CL_KERNEL_WORK_GROUP_SIZE = 0x11B0;

// ---- OpenCL function pointer types ----
using pfn_clGetPlatformIDs = cl_int (*)(cl_uint, cl_platform_id*, cl_uint*);
using pfn_clGetPlatformInfo = cl_int (*)(cl_platform_id, cl_platform_info, size_t, void*, size_t*);
using pfn_clGetDeviceIDs = cl_int (*)(cl_platform_id, cl_device_type, cl_uint, cl_device_id*, cl_uint*);
using pfn_clGetDeviceInfo = cl_int (*)(cl_device_id, cl_device_info, size_t, void*, size_t*);
using pfn_clCreateContext = cl_context (*)(const cl_context_properties*, cl_uint, const cl_device_id*,
    void (*)(const char*, const void*, size_t, void*), void*, cl_int*);
using pfn_clCreateCommandQueue = cl_command_queue (*)(cl_context, cl_device_id, cl_command_queue_properties, cl_int*);
using pfn_clCreateProgramWithSource = cl_program (*)(cl_context, cl_uint, const char**, const size_t*, cl_int*);
using pfn_clBuildProgram = cl_int (*)(cl_program, cl_uint, const cl_device_id*, const char*,
    void (*)(cl_program, void*), void*);
using pfn_clGetProgramBuildInfo = cl_int (*)(cl_program, cl_device_id, cl_program_build_info, size_t, void*, size_t*);
using pfn_clCreateKernel = cl_kernel (*)(cl_program, const char*, cl_int*);
using pfn_clCreateBuffer = cl_mem (*)(cl_context, cl_mem_flags, size_t, void*, cl_int*);
using pfn_clSetKernelArg = cl_int (*)(cl_kernel, cl_uint, size_t, const void*);
using pfn_clEnqueueNDRangeKernel = cl_int (*)(cl_command_queue, cl_kernel, cl_uint, const size_t*,
    const size_t*, const size_t*, cl_uint, const void*, void*);
using pfn_clEnqueueReadBuffer = cl_int (*)(cl_command_queue, cl_mem, cl_uint, size_t, size_t,
    void*, cl_uint, const void*, void*);
using pfn_clFinish = cl_int (*)(cl_command_queue);
using pfn_clReleaseMemObject = cl_int (*)(cl_mem);
using pfn_clReleaseKernel = cl_int (*)(cl_kernel);
using pfn_clReleaseProgram = cl_int (*)(cl_program);
using pfn_clReleaseCommandQueue = cl_int (*)(cl_command_queue);
using pfn_clReleaseContext = cl_int (*)(cl_context);
using pfn_clGetKernelWorkGroupInfo = cl_int (*)(cl_kernel, cl_device_id, cl_kernel_work_group_info,
    size_t, void*, size_t*);

// ---- Global function pointers ----
static pfn_clGetPlatformIDs          g_clGetPlatformIDs = nullptr;
static pfn_clGetPlatformInfo         g_clGetPlatformInfo = nullptr;
static pfn_clGetDeviceIDs            g_clGetDeviceIDs = nullptr;
static pfn_clGetDeviceInfo           g_clGetDeviceInfo = nullptr;
static pfn_clCreateContext           g_clCreateContext = nullptr;
static pfn_clCreateCommandQueue      g_clCreateCommandQueue = nullptr;
static pfn_clCreateProgramWithSource g_clCreateProgramWithSource = nullptr;
static pfn_clBuildProgram            g_clBuildProgram = nullptr;
static pfn_clGetProgramBuildInfo     g_clGetProgramBuildInfo = nullptr;
static pfn_clCreateKernel            g_clCreateKernel = nullptr;
static pfn_clCreateBuffer            g_clCreateBuffer = nullptr;
static pfn_clSetKernelArg            g_clSetKernelArg = nullptr;
static pfn_clEnqueueNDRangeKernel    g_clEnqueueNDRangeKernel = nullptr;
static pfn_clEnqueueReadBuffer       g_clEnqueueReadBuffer = nullptr;
static pfn_clFinish                  g_clFinish = nullptr;
static pfn_clReleaseMemObject        g_clReleaseMemObject = nullptr;
static pfn_clReleaseKernel           g_clReleaseKernel = nullptr;
static pfn_clReleaseProgram          g_clReleaseProgram = nullptr;
static pfn_clReleaseCommandQueue     g_clReleaseCommandQueue = nullptr;
static pfn_clReleaseContext          g_clReleaseContext = nullptr;
static pfn_clGetKernelWorkGroupInfo  g_clGetKernelWorkGroupInfo = nullptr;

// ---- Embedded kernel source ----
static const char* const kKernelSource = R"CL(
// Segment intersection test
float cross2d(float2 a, float2 b, float2 c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

int on_segment(float2 a, float2 b, float2 p, float eps) {
    if (fabs(cross2d(a, b, p)) > eps) return 0;
    return (p.x >= fmin(a.x, b.x) - eps &&
            p.x <= fmax(a.x, b.x) + eps &&
            p.y >= fmin(a.y, b.y) - eps &&
            p.y <= fmax(a.y, b.y) + eps) ? 1 : 0;
}

int segments_intersect_k(float2 a, float2 b, float2 c, float2 d, float eps) {
    float c1 = cross2d(a, b, c);
    float c2 = cross2d(a, b, d);
    float c3 = cross2d(c, d, a);
    float c4 = cross2d(c, d, b);
    if (((c1 > eps && c2 < -eps) || (c1 < -eps && c2 > eps)) &&
        ((c3 > eps && c4 < -eps) || (c3 < -eps && c4 > eps))) {
        return 1;
    }
    return on_segment(a, b, c, eps) || on_segment(a, b, d, eps) ||
           on_segment(c, d, a, eps) || on_segment(c, d, b, eps);
}

float point_seg_dist_k(float2 p, float2 a, float2 b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float len2 = dx * dx + dy * dy;
    if (len2 <= 1e-12f) {
        float ex = p.x - a.x; float ey = p.y - a.y;
        return sqrt(ex * ex + ey * ey);
    }
    float t = clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / len2, 0.0f, 1.0f);
    float cx = a.x + t * dx - p.x;
    float cy = a.y + t * dy - p.y;
    return sqrt(cx * cx + cy * cy);
}

float seg_seg_dist_k(float2 a, float2 b, float2 c, float2 d, float eps) {
    if (segments_intersect_k(a, b, c, d, eps)) return 0.0f;
    float d1 = point_seg_dist_k(a, c, d);
    float d2 = point_seg_dist_k(b, c, d);
    float d3 = point_seg_dist_k(c, a, b);
    float d4 = point_seg_dist_k(d, a, b);
    return fmin(fmin(d1, d2), fmin(d3, d4));
}

__kernel void evaluate_positions(
    __global const float* existing_segments,
    __global const float* existing_bounds,
    __global const int*   existing_seg_offsets,
    int existing_part_count,
    __global const float* candidate_segments,
    int candidate_segment_count,
    __global const int*   candidate_seg_offsets,
    __global const float* candidate_bounds_buf,
    __global const float* positions_buf,
    __global const int*   position_rotations,
    float sheet_width,
    float sheet_height,
    float spacing,
    __global const float* global_existing_bounds_buf,
    int has_existing,
    __global float* results
) {
    int gid = get_global_id(0);

    float px = positions_buf[gid * 2];
    float py = positions_buf[gid * 2 + 1];
    int rot_idx = position_rotations[gid];
    float cand_w = candidate_bounds_buf[rot_idx * 4 + 0];
    float cand_h = candidate_bounds_buf[rot_idx * 4 + 1];
    float cand_min_x = candidate_bounds_buf[rot_idx * 4 + 2];
    float cand_min_y = candidate_bounds_buf[rot_idx * 4 + 3];
    float tc_min_x = cand_min_x + px;
    float tc_min_y = cand_min_y + py;
    float tc_max_x = tc_min_x + cand_w;
    float tc_max_y = tc_min_y + cand_h;
    float eps = 1e-6f;

    if (tc_min_x < -eps || tc_min_y < -eps ||
        tc_max_x > sheet_width + eps || tc_max_y > sheet_height + eps) {
        results[gid * 6] = 0.0f;
        return;
    }

    int collides = 0;
    for (int part = 0; part < existing_part_count && !collides; ++part) {
        float eb_min_x = existing_bounds[part * 4];
        float eb_min_y = existing_bounds[part * 4 + 1];
        float eb_max_x = existing_bounds[part * 4 + 2];
        float eb_max_y = existing_bounds[part * 4 + 3];

        if (tc_min_x >= eb_max_x + spacing || tc_max_x + spacing <= eb_min_x ||
            tc_min_y >= eb_max_y + spacing || tc_max_y + spacing <= eb_min_y) {
            continue;
        }

        int seg_start = existing_seg_offsets[part];
        int seg_end = existing_seg_offsets[part + 1];

        int c_seg_start = candidate_seg_offsets[rot_idx];
        int c_seg_end = candidate_seg_offsets[rot_idx + 1];

        for (int es = seg_start; es < seg_end && !collides; ++es) {
            float2 ea = (float2)(existing_segments[es * 4],
                                 existing_segments[es * 4 + 1]);
            float2 eb2 = (float2)(existing_segments[es * 4 + 2],
                                  existing_segments[es * 4 + 3]);

            for (int cs = c_seg_start; cs < c_seg_end && !collides; ++cs) {
                float2 ca = (float2)(candidate_segments[cs * 4] + px,
                                     candidate_segments[cs * 4 + 1] + py);
                float2 cb = (float2)(candidate_segments[cs * 4 + 2] + px,
                                     candidate_segments[cs * 4 + 3] + py);

                if (spacing <= eps) {
                    if (segments_intersect_k(ea, eb2, ca, cb, 1e-7f)) collides = 1;
                } else {
                    if (seg_seg_dist_k(ea, eb2, ca, cb, 1e-7f) < spacing - eps) collides = 1;
                }
            }
        }
    }

    if (collides) {
        results[gid * 6] = 0.0f;
        return;
    }

    float bb_min_x = tc_min_x, bb_min_y = tc_min_y;
    float bb_max_x = tc_max_x, bb_max_y = tc_max_y;
    if (has_existing) {
        bb_min_x = fmin(bb_min_x, global_existing_bounds_buf[0]);
        bb_min_y = fmin(bb_min_y, global_existing_bounds_buf[1]);
        bb_max_x = fmax(bb_max_x, global_existing_bounds_buf[2]);
        bb_max_y = fmax(bb_max_y, global_existing_bounds_buf[3]);
    }
    float used_w = bb_max_x - bb_min_x;
    float used_h = bb_max_y - bb_min_y;

    results[gid * 6 + 0] = 1.0f;
    results[gid * 6 + 1] = used_w * used_h;
    results[gid * 6 + 2] = used_h;
    results[gid * 6 + 3] = used_w;
    results[gid * 6 + 4] = tc_min_y;
    results[gid * 6 + 5] = tc_min_x;
}
)CL";

namespace smn {

namespace {

template<typename T>
T load_func(void* lib, const char* name) {
#ifdef _WIN32
    return reinterpret_cast<T>(GetProcAddress(static_cast<HMODULE>(lib), name));
#else
    return reinterpret_cast<T>(dlsym(lib, name));
#endif
}

std::string get_device_string(cl_device_id device, cl_device_info info) {
    size_t len = 0;
    g_clGetDeviceInfo(device, info, 0, nullptr, &len);
    std::string value(len, '\0');
    g_clGetDeviceInfo(device, info, len, value.data(), nullptr);
    while (!value.empty() && value.back() == '\0') value.pop_back();
    return value;
}

std::string get_platform_string(cl_platform_id platform, cl_platform_info info) {
    size_t len = 0;
    g_clGetPlatformInfo(platform, info, 0, nullptr, &len);
    std::string value(len, '\0');
    g_clGetPlatformInfo(platform, info, len, value.data(), nullptr);
    while (!value.empty() && value.back() == '\0') value.pop_back();
    return value;
}

}  // namespace

GpuContext& GpuContext::instance() {
    static GpuContext ctx;
    return ctx;
}

GpuContext::~GpuContext() {
    shutdown();
}

bool GpuContext::load_opencl_library() {
#ifdef _WIN32
    opencl_lib_ = LoadLibraryA("OpenCL.dll");
#else
    opencl_lib_ = dlopen("libOpenCL.so", RTLD_NOW);
    if (!opencl_lib_) opencl_lib_ = dlopen("libOpenCL.so.1", RTLD_NOW);
#endif
    if (!opencl_lib_) return false;

    g_clGetPlatformIDs          = load_func<pfn_clGetPlatformIDs>(opencl_lib_, "clGetPlatformIDs");
    g_clGetPlatformInfo         = load_func<pfn_clGetPlatformInfo>(opencl_lib_, "clGetPlatformInfo");
    g_clGetDeviceIDs            = load_func<pfn_clGetDeviceIDs>(opencl_lib_, "clGetDeviceIDs");
    g_clGetDeviceInfo           = load_func<pfn_clGetDeviceInfo>(opencl_lib_, "clGetDeviceInfo");
    g_clCreateContext           = load_func<pfn_clCreateContext>(opencl_lib_, "clCreateContext");
    g_clCreateCommandQueue      = load_func<pfn_clCreateCommandQueue>(opencl_lib_, "clCreateCommandQueue");
    g_clCreateProgramWithSource = load_func<pfn_clCreateProgramWithSource>(opencl_lib_, "clCreateProgramWithSource");
    g_clBuildProgram            = load_func<pfn_clBuildProgram>(opencl_lib_, "clBuildProgram");
    g_clGetProgramBuildInfo     = load_func<pfn_clGetProgramBuildInfo>(opencl_lib_, "clGetProgramBuildInfo");
    g_clCreateKernel            = load_func<pfn_clCreateKernel>(opencl_lib_, "clCreateKernel");
    g_clCreateBuffer            = load_func<pfn_clCreateBuffer>(opencl_lib_, "clCreateBuffer");
    g_clSetKernelArg            = load_func<pfn_clSetKernelArg>(opencl_lib_, "clSetKernelArg");
    g_clEnqueueNDRangeKernel    = load_func<pfn_clEnqueueNDRangeKernel>(opencl_lib_, "clEnqueueNDRangeKernel");
    g_clEnqueueReadBuffer       = load_func<pfn_clEnqueueReadBuffer>(opencl_lib_, "clEnqueueReadBuffer");
    g_clFinish                  = load_func<pfn_clFinish>(opencl_lib_, "clFinish");
    g_clReleaseMemObject        = load_func<pfn_clReleaseMemObject>(opencl_lib_, "clReleaseMemObject");
    g_clReleaseKernel           = load_func<pfn_clReleaseKernel>(opencl_lib_, "clReleaseKernel");
    g_clReleaseProgram          = load_func<pfn_clReleaseProgram>(opencl_lib_, "clReleaseProgram");
    g_clReleaseCommandQueue     = load_func<pfn_clReleaseCommandQueue>(opencl_lib_, "clReleaseCommandQueue");
    g_clReleaseContext          = load_func<pfn_clReleaseContext>(opencl_lib_, "clReleaseContext");
    g_clGetKernelWorkGroupInfo  = load_func<pfn_clGetKernelWorkGroupInfo>(opencl_lib_, "clGetKernelWorkGroupInfo");

    return g_clGetPlatformIDs && g_clGetDeviceIDs && g_clCreateContext &&
           g_clCreateCommandQueue && g_clCreateProgramWithSource &&
           g_clBuildProgram && g_clCreateKernel && g_clCreateBuffer &&
           g_clSetKernelArg && g_clEnqueueNDRangeKernel &&
           g_clEnqueueReadBuffer && g_clFinish && g_clReleaseMemObject;
}

bool GpuContext::build_kernels() {
    for (auto& dev : devices_) {
        cl_int err = 0;
        const char* src = kKernelSource;
        size_t src_len = std::strlen(src);
        dev.program = g_clCreateProgramWithSource(
            static_cast<cl_context>(dev.context), 1, &src, &src_len, &err);
        if (err != CL_SUCCESS || !dev.program) return false;

        cl_device_id did = static_cast<cl_device_id>(dev.device_id);
        err = g_clBuildProgram(static_cast<cl_program>(dev.program), 1, &did,
                               "-cl-fast-relaxed-math", nullptr, nullptr);
        if (err != CL_SUCCESS) {
            size_t log_len = 0;
            g_clGetProgramBuildInfo(static_cast<cl_program>(dev.program), did,
                                   CL_PROGRAM_BUILD_LOG, 0, nullptr, &log_len);
            std::string build_log(log_len, '\0');
            g_clGetProgramBuildInfo(static_cast<cl_program>(dev.program), did,
                                   CL_PROGRAM_BUILD_LOG, log_len, build_log.data(), nullptr);
            std::cerr << "[GPU] Kernel build error on " << dev.name << ":\n" << build_log << "\n";
            return false;
        }

        dev.kernel_evaluate = g_clCreateKernel(
            static_cast<cl_program>(dev.program), "evaluate_positions", &err);
        if (err != CL_SUCCESS) return false;
    }
    return true;
}

bool GpuContext::init() {
    if (initialized_) return available();
    initialized_ = true;

    if (!load_opencl_library()) {
        std::cerr << "[GPU] OpenCL.dll not found. GPU acceleration disabled.\n";
        return false;
    }

    cl_uint num_platforms = 0;
    g_clGetPlatformIDs(0, nullptr, &num_platforms);
    if (num_platforms == 0) {
        std::cerr << "[GPU] No OpenCL platforms found.\n";
        return false;
    }

    std::vector<cl_platform_id> platforms(num_platforms);
    g_clGetPlatformIDs(num_platforms, platforms.data(), nullptr);

    for (cl_platform_id platform : platforms) {
        std::string plat_name = get_platform_string(platform, CL_PLATFORM_NAME);
        std::cerr << "[GPU] Found platform: " << plat_name << "\n";

        cl_uint num_devices = 0;
        if (g_clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 0, nullptr, &num_devices) != CL_SUCCESS) {
            continue;
        }

        std::vector<cl_device_id> devs(num_devices);
        g_clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, num_devices, devs.data(), nullptr);

        for (cl_device_id did : devs) {
            GpuDevice gpu;
            gpu.name = get_device_string(did, CL_DEVICE_NAME);
            gpu.vendor = get_device_string(did, CL_DEVICE_VENDOR);
            
            // Skip Intel integrated graphics if we want to force AMD
            if (gpu.vendor.find("Intel") != std::string::npos || gpu.name.find("Intel") != std::string::npos) {
                std::cerr << "[GPU] Skipping Intel device: " << gpu.name << "\n";
                continue;
            }

            g_clGetDeviceInfo(did, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(gpu.global_mem), &gpu.global_mem, nullptr);
            g_clGetDeviceInfo(did, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(gpu.compute_units), &gpu.compute_units, nullptr);
            gpu.device_id = did;

            cl_context_properties props[] = {
                CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(platform), 0
            };
            cl_int err = 0;
            gpu.context = g_clCreateContext(props, 1, &did, nullptr, nullptr, &err);
            if (err != CL_SUCCESS) continue;

            gpu.queue = g_clCreateCommandQueue(
                static_cast<cl_context>(gpu.context), did, 0, &err);
            if (err != CL_SUCCESS) {
                g_clReleaseContext(static_cast<cl_context>(gpu.context));
                continue;
            }

            std::cerr << "[GPU] Device: " << gpu.name
                      << " | CUs: " << gpu.compute_units
                      << " | VRAM: " << (gpu.global_mem / (1024 * 1024)) << " MB\n";
            devices_.push_back(std::move(gpu));
            device_mutexes_.push_back(std::make_unique<std::mutex>());
        }
    }

    if (devices_.empty()) {
        std::cerr << "[GPU] No GPU devices found.\n";
        return false;
    }

    if (!build_kernels()) {
        std::cerr << "[GPU] Failed to build OpenCL kernels.\n";
        shutdown();
        return false;
    }

    std::cerr << "[GPU] Ready. " << devices_.size() << " GPU(s) available.\n";
    return true;
}

void GpuContext::shutdown() {
    for (auto& dev : devices_) {
        if (dev.kernel_evaluate && g_clReleaseKernel)
            g_clReleaseKernel(static_cast<cl_kernel>(dev.kernel_evaluate));
        if (dev.program && g_clReleaseProgram)
            g_clReleaseProgram(static_cast<cl_program>(dev.program));
        if (dev.queue && g_clReleaseCommandQueue)
            g_clReleaseCommandQueue(static_cast<cl_command_queue>(dev.queue));
        if (dev.context && g_clReleaseContext)
            g_clReleaseContext(static_cast<cl_context>(dev.context));
    }
    devices_.clear();
    device_mutexes_.clear();

#ifdef _WIN32
    if (opencl_lib_) FreeLibrary(static_cast<HMODULE>(opencl_lib_));
#else
    if (opencl_lib_) dlclose(opencl_lib_);
#endif
    opencl_lib_ = nullptr;
    initialized_ = false;
}

std::vector<GpuContext::BatchResult> GpuContext::evaluate_positions_gpu(
    int device_index,
    const float* existing_segments,
    int existing_segment_count,
    const float* existing_bounds,
    const int* existing_seg_offsets,
    int existing_part_count,
    const float* candidate_segments,
    int candidate_segment_count,
    const int* candidate_seg_offsets,
    const float* candidate_bounds,
    int rotation_count,
    const float* positions,
    const int* position_rotations,
    int position_count,
    float sheet_width,
    float sheet_height,
    float spacing,
    const float* global_existing_bounds,
    bool has_existing_bounds
) {
    std::vector<BatchResult> output(static_cast<size_t>(position_count));
    if (device_index < 0 || device_index >= static_cast<int>(devices_.size())) {
        return output;
    }
    if (position_count <= 0) return output;

    std::lock_guard<std::mutex> lock(*device_mutexes_[static_cast<size_t>(device_index)]);

    const auto& dev = devices_[static_cast<size_t>(device_index)];
    auto ctx = static_cast<cl_context>(dev.context);
    auto queue = static_cast<cl_command_queue>(dev.queue);
    auto kernel = static_cast<cl_kernel>(dev.kernel_evaluate);

    cl_int err = 0;

    // Create buffers
    size_t es_size = std::max(size_t(4), static_cast<size_t>(existing_segment_count) * 4 * sizeof(float));
    size_t eb_size = std::max(size_t(4), static_cast<size_t>(existing_part_count) * 4 * sizeof(float));
    size_t eo_size = std::max(size_t(4), static_cast<size_t>(existing_part_count + 1) * sizeof(int));
    size_t cs_size = std::max(size_t(4), static_cast<size_t>(candidate_segment_count) * 4 * sizeof(float));
    size_t cso_size = std::max(size_t(4), static_cast<size_t>(rotation_count + 1) * sizeof(int));
    size_t cb_size = static_cast<size_t>(rotation_count) * 4 * sizeof(float);
    size_t pos_size = static_cast<size_t>(position_count) * 2 * sizeof(float);
    size_t pr_size = static_cast<size_t>(position_count) * sizeof(int);
    size_t geb_size = 4 * sizeof(float);
    size_t res_size = static_cast<size_t>(position_count) * 6 * sizeof(float);

    // Dummy data for empty existing
    float dummy_seg[4] = {0, 0, 0, 0};
    float dummy_bounds[4] = {0, 0, 0, 0};
    int dummy_offsets[2] = {0, 0};
    float dummy_geb[4] = {0, 0, 0, 0};

    const float* es_ptr = existing_segment_count > 0 ? existing_segments : dummy_seg;
    const float* eb_ptr = existing_part_count > 0 ? existing_bounds : dummy_bounds;
    const int* eo_ptr = existing_part_count > 0 ? existing_seg_offsets : dummy_offsets;
    const float* geb_ptr = has_existing_bounds ? global_existing_bounds : dummy_geb;

    cl_mem buf_es  = g_clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, es_size, const_cast<float*>(es_ptr), &err);
    cl_mem buf_eb  = g_clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, eb_size, const_cast<float*>(eb_ptr), &err);
    cl_mem buf_eo  = g_clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, eo_size, const_cast<int*>(eo_ptr), &err);
    cl_mem buf_cs  = g_clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, cs_size, const_cast<float*>(candidate_segments), &err);
    cl_mem buf_cso = g_clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, cso_size, const_cast<int*>(candidate_seg_offsets), &err);
    cl_mem buf_cb  = g_clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, cb_size, const_cast<float*>(candidate_bounds), &err);
    cl_mem buf_pos = g_clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, pos_size, const_cast<float*>(positions), &err);
    cl_mem buf_pr  = g_clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, pr_size, const_cast<int*>(position_rotations), &err);
    cl_mem buf_geb = g_clCreateBuffer(ctx, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, geb_size, const_cast<float*>(geb_ptr), &err);
    cl_mem buf_res = g_clCreateBuffer(ctx, CL_MEM_WRITE_ONLY, res_size, nullptr, &err);

    if (!buf_es || !buf_eb || !buf_eo || !buf_cs || !buf_cso || !buf_cb || !buf_pos || !buf_pr || !buf_geb || !buf_res) {
        if (buf_es) g_clReleaseMemObject(buf_es);
        if (buf_eb) g_clReleaseMemObject(buf_eb);
        if (buf_eo) g_clReleaseMemObject(buf_eo);
        if (buf_cs) g_clReleaseMemObject(buf_cs);
        if (buf_cso) g_clReleaseMemObject(buf_cso);
        if (buf_cb) g_clReleaseMemObject(buf_cb);
        if (buf_pos) g_clReleaseMemObject(buf_pos);
        if (buf_pr) g_clReleaseMemObject(buf_pr);
        if (buf_geb) g_clReleaseMemObject(buf_geb);
        if (buf_res) g_clReleaseMemObject(buf_res);
        return output;
    }

    // Set kernel arguments
    int has_existing_int = has_existing_bounds ? 1 : 0;
    cl_uint arg = 0;
    g_clSetKernelArg(kernel, arg++, sizeof(cl_mem), &buf_es);
    g_clSetKernelArg(kernel, arg++, sizeof(cl_mem), &buf_eb);
    g_clSetKernelArg(kernel, arg++, sizeof(cl_mem), &buf_eo);
    g_clSetKernelArg(kernel, arg++, sizeof(int), &existing_part_count);
    g_clSetKernelArg(kernel, arg++, sizeof(cl_mem), &buf_cs);
    g_clSetKernelArg(kernel, arg++, sizeof(int), &candidate_segment_count);
    g_clSetKernelArg(kernel, arg++, sizeof(cl_mem), &buf_cso);
    g_clSetKernelArg(kernel, arg++, sizeof(cl_mem), &buf_cb);
    g_clSetKernelArg(kernel, arg++, sizeof(cl_mem), &buf_pos);
    g_clSetKernelArg(kernel, arg++, sizeof(cl_mem), &buf_pr);
    g_clSetKernelArg(kernel, arg++, sizeof(float), &sheet_width);
    g_clSetKernelArg(kernel, arg++, sizeof(float), &sheet_height);
    g_clSetKernelArg(kernel, arg++, sizeof(float), &spacing);
    g_clSetKernelArg(kernel, arg++, sizeof(cl_mem), &buf_geb);
    g_clSetKernelArg(kernel, arg++, sizeof(int), &has_existing_int);
    g_clSetKernelArg(kernel, arg++, sizeof(cl_mem), &buf_res);

    // Get max work group size for this kernel
    size_t max_wg = 256;
    if (g_clGetKernelWorkGroupInfo) {
        g_clGetKernelWorkGroupInfo(kernel, static_cast<cl_device_id>(dev.device_id),
                                   CL_KERNEL_WORK_GROUP_SIZE, sizeof(max_wg), &max_wg, nullptr);
    }

    size_t global_size = static_cast<size_t>(position_count);
    // Round up to multiple of work group size
    size_t local_size = std::min(max_wg, size_t(256));
    global_size = ((global_size + local_size - 1) / local_size) * local_size;

    err = g_clEnqueueNDRangeKernel(queue, kernel, 1, nullptr, &global_size, &local_size, 0, nullptr, nullptr);
    if (err != CL_SUCCESS) {
        // Fallback: try without local size constraint
        err = g_clEnqueueNDRangeKernel(queue, kernel, 1, nullptr, &global_size, nullptr, 0, nullptr, nullptr);
    }

    g_clFinish(queue);

    // Read results
    std::vector<float> raw_results(static_cast<size_t>(position_count) * 6);
    g_clEnqueueReadBuffer(queue, buf_res, 1 /*blocking*/, 0, res_size, raw_results.data(), 0, nullptr, nullptr);

    for (int i = 0; i < position_count; ++i) {
        output[static_cast<size_t>(i)].valid = raw_results[static_cast<size_t>(i) * 6 + 0] > 0.5f;
        output[static_cast<size_t>(i)].bounding_area = static_cast<double>(raw_results[static_cast<size_t>(i) * 6 + 1]);
        output[static_cast<size_t>(i)].used_height = static_cast<double>(raw_results[static_cast<size_t>(i) * 6 + 2]);
        output[static_cast<size_t>(i)].used_width = static_cast<double>(raw_results[static_cast<size_t>(i) * 6 + 3]);
        output[static_cast<size_t>(i)].pos_y = static_cast<double>(raw_results[static_cast<size_t>(i) * 6 + 4]);
        output[static_cast<size_t>(i)].pos_x = static_cast<double>(raw_results[static_cast<size_t>(i) * 6 + 5]);
    }

    // Cleanup
    g_clReleaseMemObject(buf_es);
    g_clReleaseMemObject(buf_eb);
    g_clReleaseMemObject(buf_eo);
    g_clReleaseMemObject(buf_cs);
    g_clReleaseMemObject(buf_cso);
    g_clReleaseMemObject(buf_cb);
    g_clReleaseMemObject(buf_pos);
    g_clReleaseMemObject(buf_pr);
    g_clReleaseMemObject(buf_geb);
    g_clReleaseMemObject(buf_res);

    return output;
}

}  // namespace smn

