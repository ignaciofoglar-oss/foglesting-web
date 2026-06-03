// Sheet Metal Nesting - OpenCL batch collision + scoring kernel
// Each work-item evaluates ONE candidate position against all existing parts.

// Segment intersection test (same algorithm as geometry.cpp)
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

int segments_intersect(float2 a, float2 b, float2 c, float2 d, float eps) {
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

float point_seg_dist(float2 p, float2 a, float2 b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float len2 = dx * dx + dy * dy;
    if (len2 <= 1e-12f) {
        float ex = p.x - a.x;
        float ey = p.y - a.y;
        return sqrt(ex * ex + ey * ey);
    }
    float t = clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / len2, 0.0f, 1.0f);
    float cx = a.x + t * dx - p.x;
    float cy = a.y + t * dy - p.y;
    return sqrt(cx * cx + cy * cy);
}

float seg_seg_dist(float2 a, float2 b, float2 c, float2 d, float eps) {
    if (segments_intersect(a, b, c, d, eps)) return 0.0f;
    float d1 = point_seg_dist(a, c, d);
    float d2 = point_seg_dist(b, c, d);
    float d3 = point_seg_dist(c, a, b);
    float d4 = point_seg_dist(d, a, b);
    return fmin(fmin(d1, d2), fmin(d3, d4));
}

__kernel void evaluate_positions(
    __global const float* existing_segments,    // (x1,y1,x2,y2) * existing_segment_count
    __global const float* existing_bounds,      // (min_x,min_y,max_x,max_y) * existing_part_count
    __global const int*   existing_seg_offsets,  // start index per part + sentinel
    int existing_part_count,
    __global const float* candidate_segments,   // (x1,y1,x2,y2) * candidate_segment_count
    int candidate_segment_count,
    __global const int*   candidate_seg_offsets, // start index per rotation + sentinel
    __global const float* candidate_bounds_buf,  // {width, height, min_x, min_y} * rotation_count
    __global const float* positions,             // (x, y) * position_count
    __global const int*   position_rotations,    // rotation index per position
    float sheet_width,
    float sheet_height,
    float spacing,
    __global const float* global_existing_bounds_buf, // {min_x,min_y,max_x,max_y} or zeros
    int has_existing,
    __global float* results                     // (valid, bounding_area, used_height, used_width, pos_y, pos_x) * position_count
) {
    int gid = get_global_id(0);

    float px = positions[gid * 2];
    float py = positions[gid * 2 + 1];
    int rot_idx = position_rotations[gid];

    float cand_w = candidate_bounds_buf[rot_idx * 4 + 0];
    float cand_h = candidate_bounds_buf[rot_idx * 4 + 1];
    float cand_min_x = candidate_bounds_buf[rot_idx * 4 + 2];
    float cand_min_y = candidate_bounds_buf[rot_idx * 4 + 3];

    // Translated candidate bounds
    float tc_min_x = cand_min_x + px;
    float tc_min_y = cand_min_y + py;
    float tc_max_x = tc_min_x + cand_w;
    float tc_max_y = tc_min_y + cand_h;

    float eps = 1e-6f;

    // Check fits on sheet
    if (tc_min_x < -eps || tc_min_y < -eps ||
        tc_max_x > sheet_width + eps || tc_max_y > sheet_height + eps) {
        results[gid * 6 + 0] = 0.0f;
        return;
    }

    // Check collision against each existing part
    int collides = 0;
    for (int part = 0; part < existing_part_count && !collides; ++part) {
        float eb_min_x = existing_bounds[part * 4 + 0];
        float eb_min_y = existing_bounds[part * 4 + 1];
        float eb_max_x = existing_bounds[part * 4 + 2];
        float eb_max_y = existing_bounds[part * 4 + 3];

        // Bounds overlap test with spacing
        if (tc_min_x >= eb_max_x + spacing || tc_max_x + spacing <= eb_min_x ||
            tc_min_y >= eb_max_y + spacing || tc_max_y + spacing <= eb_min_y) {
            continue;
        }

        // Segment-level collision test
        int seg_start = existing_seg_offsets[part];
        int seg_end = existing_seg_offsets[part + 1];

        int c_seg_start = candidate_seg_offsets[rot_idx];
        int c_seg_end = candidate_seg_offsets[rot_idx + 1];

        for (int es = seg_start; es < seg_end && !collides; ++es) {
            float2 ea = (float2)(existing_segments[es * 4 + 0],
                                 existing_segments[es * 4 + 1]);
            float2 eb = (float2)(existing_segments[es * 4 + 2],
                                 existing_segments[es * 4 + 3]);

            for (int cs = c_seg_start; cs < c_seg_end && !collides; ++cs) {
                float2 ca = (float2)(candidate_segments[cs * 4 + 0] + px,
                                     candidate_segments[cs * 4 + 1] + py);
                float2 cb = (float2)(candidate_segments[cs * 4 + 2] + px,
                                     candidate_segments[cs * 4 + 3] + py);

                if (spacing <= eps) {
                    if (segments_intersect(ea, eb, ca, cb, 1e-7f)) {
                        collides = 1;
                    }
                } else {
                    float d = seg_seg_dist(ea, eb, ca, cb, 1e-7f);
                    if (d < spacing - eps) {
                        collides = 1;
                    }
                }
            }
        }
    }

    if (collides) {
        results[gid * 6 + 0] = 0.0f;
        return;
    }

    // Compute score: bounding box of existing + this candidate
    float bb_min_x = tc_min_x;
    float bb_min_y = tc_min_y;
    float bb_max_x = tc_max_x;
    float bb_max_y = tc_max_y;

    if (has_existing) {
        bb_min_x = fmin(bb_min_x, global_existing_bounds_buf[0]);
        bb_min_y = fmin(bb_min_y, global_existing_bounds_buf[1]);
        bb_max_x = fmax(bb_max_x, global_existing_bounds_buf[2]);
        bb_max_y = fmax(bb_max_y, global_existing_bounds_buf[3]);
    }

    float used_w = bb_max_x - bb_min_x;
    float used_h = bb_max_y - bb_min_y;
    float bounding_area = used_w * used_h;

    results[gid * 6 + 0] = 1.0f;           // valid
    results[gid * 6 + 1] = bounding_area;
    results[gid * 6 + 2] = used_h;
    results[gid * 6 + 3] = used_w;
    results[gid * 6 + 4] = tc_min_y;       // pos_y for sorting
    results[gid * 6 + 5] = tc_min_x;       // pos_x for sorting
}
