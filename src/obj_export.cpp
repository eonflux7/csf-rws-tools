#include "rws/obj_export.hpp"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace rws {
namespace {

std::uint32_t read_u32(const std::span<const std::byte> bytes, const std::uint64_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) throw std::runtime_error("OBJ source array is truncated");
    const auto i = static_cast<std::size_t>(offset);
    return std::to_integer<std::uint32_t>(bytes[i]) |
        (std::to_integer<std::uint32_t>(bytes[i + 1]) << 8U) |
        (std::to_integer<std::uint32_t>(bytes[i + 2]) << 16U) |
        (std::to_integer<std::uint32_t>(bytes[i + 3]) << 24U);
}

float read_f32(const std::span<const std::byte> bytes, const std::uint64_t offset) {
    return std::bit_cast<float>(read_u32(bytes, offset));
}

} // namespace


void export_geometry_obj(const GeometryInfo& geometry, const std::span<const std::byte> bytes,
                         const std::filesystem::path& output_path) {
    const MorphTargetInfo* morph = nullptr;
    for (const auto& candidate : geometry.morph_targets) {
        if (candidate.has_vertices) { morph = &candidate; break; }
    }
    if (!morph) throw std::runtime_error("Geometry has no exportable vertex morph target");
    if (geometry.triangle_layout == TriangleLayout::unknown)
        throw std::runtime_error("Geometry triangle word order is ambiguous");

    std::ofstream output(output_path, std::ios::trunc);
    if (!output) throw std::runtime_error("Cannot create OBJ: " + output_path.string());
    output << "# Exported by rws-man\n" << std::setprecision(9);
    for (std::int32_t i = 0; i < geometry.vertex_count; ++i) {
        const auto offset = morph->vertices_offset + static_cast<std::uint64_t>(i) * 12;
        output << "v " << read_f32(bytes, offset) << ' ' << read_f32(bytes, offset + 4) << ' '
               << read_f32(bytes, offset + 8) << '\n';
    }
    const bool has_uv = !geometry.texcoord_offsets.empty();
    if (has_uv) {
        for (std::int32_t i = 0; i < geometry.vertex_count; ++i) {
            const auto offset = geometry.texcoord_offsets.front() + static_cast<std::uint64_t>(i) * 8;
            output << "vt " << read_f32(bytes, offset) << ' ' << (1.0F - read_f32(bytes, offset + 4)) << '\n';
        }
    }
    if (morph->has_normals) {
        for (std::int32_t i = 0; i < geometry.vertex_count; ++i) {
            const auto offset = morph->normals_offset + static_cast<std::uint64_t>(i) * 12;
            output << "vn " << read_f32(bytes, offset) << ' ' << read_f32(bytes, offset + 4) << ' '
                   << read_f32(bytes, offset + 8) << '\n';
        }
    }

    std::uint16_t active_material = 0xFFFFU;
    for (std::int32_t i = 0; i < geometry.triangle_count; ++i) {
        const auto triangle = decode_triangle(geometry, i, bytes);
        if (!triangle) throw std::runtime_error(triangle.error);
        if (triangle.value->material != active_material) {
            active_material = triangle.value->material;
            output << "usemtl material_" << active_material << '\n';
        }
        output << "f";
        for (const auto index : triangle.value->vertices) {
            const auto obj_index = static_cast<std::uint32_t>(index) + 1U;
            output << ' ' << obj_index;
            if (has_uv || morph->has_normals) {
                output << '/';
                if (has_uv) output << obj_index;
                if (morph->has_normals) output << '/' << obj_index;
            }
        }
        output << '\n';
    }
    if (!output) throw std::runtime_error("Failed while writing OBJ: " + output_path.string());
}

} // namespace rws

