#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace smn {

struct GpuDevice {
    std::string name;
    std::string vendor;
    unsigned long long global_mem = 0;
    unsigned int compute_units = 0;
    void* device_id = nullptr;  // cl_device_id
    void* context = nullptr;    // cl_context
    void* queue = nullptr;      // cl_command_queue
    void* program = nullptr;    // cl_program
    void* kernel_evaluate = nullptr;  // cl_kernel
};

class GpuContext {
public:
    ~GpuContext();

    bool init();
    void shutdown();

    bool available() const { return !devices_.empty(); }
    int device_count() const { return static_cast<int>(devices_.size()); }
    const GpuDevice& device(int index) const { return devices_[static_cast<size_t>(index)]; }
    const std::vector<GpuDevice>& devices() const { return devices_; }

    // Batch evaluate candidate positions for collision and scoring.
    // Returns indices of valid (non-colliding) positions sorted by score.
    // existing_segments: flat array of (x1,y1,x2,y2) per segment
    // existing_bounds: flat array of (min_x,min_y,max_x,max_y) per part
    // existing_seg_offsets: start index into existing_segments per part (+ final count)
    // candidate_segments: (x1,y1,x2,y2) per segment of rotated candidate at origin
    // positions: flat (x,y) per candidate position
    // sheet_width, sheet_height, spacing: nesting params
    // Returns: for each position, float4 {valid, bounding_area, used_height, used_width}
    struct BatchResult {
        bool valid = false;
        double bounding_area = 0.0;
        double used_height = 0.0;
        double used_width = 0.0;
        double pos_y = 0.0;
        double pos_x = 0.0;
    };

    std::vector<BatchResult> evaluate_positions_gpu(
        int device_index,
        const float* existing_segments,
        int existing_segment_count,
        const float* existing_bounds,
        const int* existing_seg_offsets,
        int existing_part_count,
        const float* candidate_segments,
        int candidate_segment_count,
        const int* candidate_seg_offsets,
        const float* candidate_bounds,  // {width, height, min_x, min_y} per rotation
        int rotation_count,
        const float* positions,
        const int* position_rotations,
        int position_count,
        float sheet_width,
        float sheet_height,
        float spacing,
        const float* global_existing_bounds,  // {min_x, min_y, max_x, max_y} or null
        bool has_existing_bounds
    );

    static GpuContext& instance();

private:
    std::vector<GpuDevice> devices_;
    std::vector<std::unique_ptr<std::mutex>> device_mutexes_;
    void* opencl_lib_ = nullptr;
    bool initialized_ = false;

    bool load_opencl_library();
    bool build_kernels();
};

}  // namespace smn
