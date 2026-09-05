#pragma once

#include "rws/chunk.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace rws {

struct LibraryVersion {
    std::uint32_t encoded_version{};
    std::uint16_t build{};
    unsigned major{};
    unsigned minor{};
    unsigned revision{};
    unsigned binary{};
};

struct Vec3 { float x{}, y{}, z{}; };
struct Sphere { Vec3 center; float radius{}; };

struct ClumpInfo {
    std::int32_t atomics{};
    std::int32_t lights{};
    std::int32_t cameras{};
};

struct FrameInfo {
    std::array<float, 9> rotation{};
    Vec3 position;
    std::int32_t parent{};
    std::uint32_t flags{};
};

struct FrameListInfo { std::vector<FrameInfo> frames; };

struct AtomicInfo {
    std::int32_t frame_index{};
    std::int32_t geometry_index{};
    std::uint32_t flags{};
    std::uint32_t unused{};
};

struct MaterialInfo {
    std::uint32_t flags{};
    std::array<std::uint8_t, 4> color{};
    bool textured{};
    float ambient{}, specular{}, diffuse{};
};

struct MaterialListInfo {
    std::int32_t material_count{};
    std::vector<std::int32_t> remap;
};

struct TextureInfo {
    std::uint32_t filter_addressing{};
    std::uint8_t filter_mode{}, address_u{}, address_v{};
    std::string name;
    std::string mask_name;
};

struct MaterialEffectsInfo {
    bool pipeline_enabled{};
    std::uint32_t effect_type{};
    std::uint32_t slot_type{};
    bool has_dual_texture{};
    std::uint32_t source_blend{}, destination_blend{};
    TextureInfo dual_texture;
    std::uint32_t trailing_slot_type{};
};

struct HAnimNodeInfo { std::int32_t node_id{}, node_index{}; std::uint32_t flags{}; };
struct HAnimInfo {
    std::uint32_t version{};
    std::int32_t hierarchy_id{};
    std::uint32_t flags{}, keyframe_size{};
    std::vector<HAnimNodeInfo> nodes;
};

struct BinMeshEntry { std::uint32_t material_index{}; std::vector<std::uint32_t> indices; };
struct BinMeshInfo {
    std::uint32_t flags{}, total_indices{};
    std::vector<BinMeshEntry> meshes;
};

struct RightToRenderInfo { std::uint32_t plugin_id{}, extra_data{}; };
struct AnisotropyInfo { float coefficient{}; };

struct SkinInfo {
    std::uint8_t bone_count{}, used_bone_count{}, max_weights_per_vertex{}, padding{};
    std::vector<std::uint8_t> used_bones;
    std::int32_t vertex_count{};
    std::uint64_t vertex_indices_offset{}, vertex_weights_offset{}, inverse_matrices_offset{};
    std::uint32_t bone_limit{}, mesh_count{}, rle_count{};
    std::uint64_t trailing_split_bytes{};
};

enum class UserDataFormat : std::uint32_t { integer = 1, real = 2, string = 3 };
struct UserDataArrayInfo {
    std::string name;
    UserDataFormat format{};
    std::vector<std::int32_t> integers;
    std::vector<float> reals;
    std::vector<std::string> strings;
};
struct UserDataInfo { std::vector<UserDataArrayInfo> arrays; };

struct PhysicsVolumeInfo {
    std::uint16_t version{};
    std::uint32_t kind{};
    std::vector<float> shape_scalars;
    std::optional<Vec3> shape_vector;
    std::vector<PhysicsVolumeInfo> children;
    std::array<float, 12> matrix{};
    float size_or_fatness{};
    std::array<float, 2> material_coefficients{};
    std::uint16_t group{}, flags{};
};

struct PhysicsBodyDefInfo {
    PhysicsVolumeInfo volume;
    float mass{};
    Vec3 principal_inertia;
    std::array<float, 4> inertia_orientation{};
    float scalar_inertia{};
    std::array<float, 2> unknown_scalars{};
    Vec3 unknown_vector;
    std::uint32_t flags{};
    Vec3 center_of_mass;
};

struct PhysicsRagdollDefInfo {
    std::uint16_t type_0{}, type_1{}, body_count{}, joint_count{};
    std::vector<PhysicsBodyDefInfo> bodies;
    std::vector<std::array<std::uint16_t, 2>> joint_pairs;
    std::uint32_t table_rows{}, table_columns{};
    std::vector<std::uint32_t> table_values;
    std::uint32_t integer_field{};
    std::vector<std::uint32_t> body_ids;
};

// Pyro Studios registers chunk 0xFFFFFF00 on several RenderWare object classes.
// Its payload schema is selected by the type of the object owning the Extension.
struct PyroExtensionInfo {
    std::uint32_t owner_type{};
    std::uint32_t version{};
    bool present{};
    std::vector<std::uint32_t> words;
    std::vector<std::string> strings;
    std::optional<std::array<float, 6>> bounds;
    [[nodiscard]] std::optional<std::uint32_t> material_flags() const noexcept {
        return owner_type == 0x07 && words.size() >= 5 ? std::optional(words[3]) : std::nullopt;
    }
    [[nodiscard]] std::optional<std::uint32_t> material_surface_type() const noexcept {
        return owner_type == 0x07 && words.size() >= 5 ? std::optional(words[4]) : std::nullopt;
    }
    [[nodiscard]] std::string_view object_name() const noexcept {
        return (owner_type == 0x07 || owner_type == 0x0E || owner_type == 0x14) && !strings.empty()
            ? std::string_view(strings.front()) : std::string_view{};
    }
};

enum class TriangleLayout { unknown, memory_order, stream_order };

struct TriangleInfo {
    std::array<std::uint16_t, 3> vertices{};
    std::uint16_t material{};
};

struct MorphTargetInfo {
    Sphere sphere;
    bool has_vertices{};
    bool has_normals{};
    std::uint64_t vertices_offset{};
    std::uint64_t normals_offset{};
};

struct GeometryInfo {
    std::uint32_t format{};
    std::int32_t triangle_count{};
    std::int32_t vertex_count{};
    std::int32_t morph_target_count{};
    std::uint32_t texcoord_sets{};
    std::uint64_t prelight_offset{};
    std::vector<std::uint64_t> texcoord_offsets;
    std::uint64_t triangles_offset{};
    std::int32_t material_count{};
    TriangleLayout triangle_layout{TriangleLayout::unknown};
    std::vector<MorphTargetInfo> morph_targets;
    std::uint64_t computed_size{};
    std::uint64_t stored_size{};
};

struct WorldInfo {
    bool root_is_world_sector{};
    Vec3 inverse_origin;
    std::int32_t triangle_count{}, vertex_count{}, plane_sector_count{}, world_sector_count{};
    std::int32_t collision_sector_size{};
    std::uint32_t format{};
    Vec3 bounding_box_sup;
    Vec3 bounding_box_inf;
};

struct PlaneSectorInfo {
    std::int32_t axis{};
    float split{};
    bool left_is_world_sector{}, right_is_world_sector{};
    float left_value{}, right_value{};
};

struct WorldSectorInfo {
    std::int32_t material_window_base{}, triangle_count{}, vertex_count{};
    Vec3 bounding_box_inf;
    Vec3 bounding_box_sup;
    bool collision_sector_present{};
};

template <typename T>
struct DecodeResult {
    std::optional<T> value;
    std::string error;
    explicit operator bool() const noexcept { return value.has_value(); }
};

[[nodiscard]] LibraryVersion decode_library_id(std::uint32_t stamp) noexcept;
[[nodiscard]] const Chunk* find_child(const Chunk& parent, std::uint32_t type) noexcept;
[[nodiscard]] DecodeResult<ClumpInfo> decode_clump(const Chunk&, std::span<const std::byte>);
[[nodiscard]] DecodeResult<FrameListInfo> decode_frame_list(const Chunk&, std::span<const std::byte>);
[[nodiscard]] DecodeResult<AtomicInfo> decode_atomic(const Chunk&, std::span<const std::byte>);
[[nodiscard]] DecodeResult<MaterialInfo> decode_material(const Chunk&, std::span<const std::byte>);
[[nodiscard]] DecodeResult<MaterialListInfo> decode_material_list(const Chunk&, std::span<const std::byte>);
[[nodiscard]] DecodeResult<TextureInfo> decode_texture(const Chunk&, std::span<const std::byte>);
[[nodiscard]] DecodeResult<MaterialEffectsInfo> decode_material_effects(
    const Chunk&, std::uint32_t owner_type, std::span<const std::byte>);
[[nodiscard]] DecodeResult<HAnimInfo> decode_hanim(const Chunk&, std::span<const std::byte>);
[[nodiscard]] DecodeResult<BinMeshInfo> decode_bin_mesh(const Chunk&, std::span<const std::byte>);
[[nodiscard]] DecodeResult<RightToRenderInfo> decode_right_to_render(const Chunk&, std::span<const std::byte>);
[[nodiscard]] DecodeResult<AnisotropyInfo> decode_anisotropy(const Chunk&, std::span<const std::byte>);
[[nodiscard]] DecodeResult<SkinInfo> decode_skin(const Chunk&, std::int32_t vertex_count,
                                                 std::span<const std::byte>);
[[nodiscard]] DecodeResult<UserDataInfo> decode_user_data(const Chunk&, std::span<const std::byte>);
[[nodiscard]] DecodeResult<PhysicsBodyDefInfo> decode_physics_body_def(const Chunk&,
                                                                       std::span<const std::byte>);
[[nodiscard]] DecodeResult<PhysicsRagdollDefInfo> decode_physics_ragdoll_def(const Chunk&,
                                                                             std::span<const std::byte>);
[[nodiscard]] DecodeResult<PyroExtensionInfo> decode_pyro_extension(const Chunk&,
                                                                    std::uint32_t owner_type,
                                                                    std::span<const std::byte>);
[[nodiscard]] DecodeResult<GeometryInfo> decode_geometry(const Chunk&, std::span<const std::byte>);
[[nodiscard]] DecodeResult<TriangleInfo> decode_triangle(const GeometryInfo&, std::int32_t index,
                                                         std::span<const std::byte>);
[[nodiscard]] DecodeResult<WorldInfo> decode_world(const Chunk&, std::span<const std::byte>);
[[nodiscard]] DecodeResult<PlaneSectorInfo> decode_plane_sector(const Chunk&, std::span<const std::byte>);
[[nodiscard]] DecodeResult<WorldSectorInfo> decode_world_sector(const Chunk&, std::span<const std::byte>);

} // namespace rws
