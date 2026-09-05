#pragma once

#include "rws/chunk.hpp"
#include "rws/decoded.hpp"

#include <imgui.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace rwsman {

class GeometryPreview {
public:
    void clear();
    void draw(const rws::Chunk& geometry_chunk, std::span<const std::byte> bytes,
              const std::filesystem::path& source_path);

private:
    struct Face {
        std::uint32_t a{}, b{}, c{};
        std::uint16_t material{};
    };
    struct Uv { float u{}, v{}; };
    struct GpuVertex {
        float x{}, y{}, z{};
        float base_u{}, base_v{}, debug_u{}, debug_v{};
        float nx{}, ny{}, nz{};
        std::uint32_t source_index{};
    };
    struct DrawBatch {
        std::uint16_t material{};
        std::uint32_t first{}, count{};
    };

    bool load(const rws::Chunk& geometry_chunk, std::span<const std::byte> bytes,
              const std::filesystem::path& source_path);
    void select_uv_set(std::size_t index);
    void reset_view();
    static void render_callback(const ImDrawList*, const ImDrawCmd* command);
    void render_gpu();
    bool create_gpu_resources();
    void destroy_gpu_resources();

    std::uint64_t chunk_offset_{~std::uint64_t{}};
    std::vector<rws::Vec3> vertices_;
    std::vector<std::vector<Uv>> uv_sets_;
    std::vector<Face> faces_;
    std::vector<GpuVertex> gpu_vertices_;
    std::vector<DrawBatch> draw_batches_;
    std::vector<std::array<std::uint8_t, 4>> material_colors_;
    std::vector<unsigned int> material_textures_;
    std::vector<unsigned int> material_lightmap_textures_;
    std::vector<std::string> material_texture_names_;
    std::vector<std::string> material_lightmap_texture_names_;
    std::vector<unsigned int> owned_texture_ids_;
    unsigned int checker_texture_{};
    unsigned int vertex_array_{}, vertex_buffer_{}, shader_program_{};
    std::size_t loaded_texture_count_{}, missing_texture_count_{};
    std::string texture_status_;
    rws::Vec3 center_{};
    float radius_{1.0F};
    float yaw_{-0.65F};
    float pitch_{-0.35F};
    float distance_{3.0F};
    float pan_x_{}, pan_y_{};
    float canvas_x_{}, canvas_y_{}, canvas_width_{}, canvas_height_{};
    int view_style_{};
    std::size_t selected_uv_set_{};
    bool wireframe_{true};
    bool cull_backfaces_{};
    std::string error_;
};

} // namespace rwsman
