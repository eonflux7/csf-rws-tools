#include "rws/decoded.hpp"

#include <bit>
#include <limits>
#include <string_view>

namespace rws {
namespace {

constexpr std::uint32_t struct_chunk = 0x01;
constexpr std::uint32_t material_list_chunk = 0x08;
constexpr std::uint32_t geometry_prelit = 0x08;
constexpr std::uint32_t geometry_normals = 0x10;
constexpr std::uint32_t geometry_textured = 0x04;
constexpr std::uint32_t geometry_textured2 = 0x80;
constexpr std::uint32_t geometry_native = 0x01000000;

class Reader {
public:
    Reader(const std::span<const std::byte> bytes, const Chunk& chunk)
        : bytes_(bytes), begin_(chunk.payload_offset), end_(chunk.payload_offset + chunk.available_size), cursor_(begin_) {}

    [[nodiscard]] bool can_read(const std::uint64_t count) const noexcept {
        return cursor_ <= end_ && count <= end_ - cursor_ && end_ <= bytes_.size();
    }
    [[nodiscard]] std::uint64_t position() const noexcept { return cursor_; }
    [[nodiscard]] std::uint64_t consumed() const noexcept { return cursor_ - begin_; }
    bool skip(const std::uint64_t count) noexcept {
        if (!can_read(count)) return false;
        cursor_ += count;
        return true;
    }
    bool u32(std::uint32_t& value) noexcept {
        if (!can_read(4)) return false;
        const auto i = static_cast<std::size_t>(cursor_);
        value = std::to_integer<std::uint32_t>(bytes_[i]) |
            (std::to_integer<std::uint32_t>(bytes_[i + 1]) << 8U) |
            (std::to_integer<std::uint32_t>(bytes_[i + 2]) << 16U) |
            (std::to_integer<std::uint32_t>(bytes_[i + 3]) << 24U);
        cursor_ += 4;
        return true;
    }
    bool u8(std::uint8_t& value) noexcept {
        if (!can_read(1)) return false;
        value = std::to_integer<std::uint8_t>(bytes_[static_cast<std::size_t>(cursor_++)]);
        return true;
    }
    bool u16(std::uint16_t& value) noexcept {
        if (!can_read(2)) return false;
        const auto i = static_cast<std::size_t>(cursor_);
        value = static_cast<std::uint16_t>(std::to_integer<std::uint16_t>(bytes_[i]) |
            (std::to_integer<std::uint16_t>(bytes_[i + 1]) << 8U));
        cursor_ += 2;
        return true;
    }
    bool i32(std::int32_t& value) noexcept {
        std::uint32_t raw{};
        if (!u32(raw)) return false;
        value = std::bit_cast<std::int32_t>(raw);
        return true;
    }
    bool f32(float& value) noexcept {
        std::uint32_t raw{};
        if (!u32(raw)) return false;
        value = std::bit_cast<float>(raw);
        return true;
    }
    bool vec3(Vec3& value) noexcept { return f32(value.x) && f32(value.y) && f32(value.z); }
    bool rgba(std::array<std::uint8_t, 4>& value) noexcept {
        if (!can_read(4)) return false;
        for (auto& component : value) component = std::to_integer<std::uint8_t>(bytes_[cursor_++]);
        return true;
    }
    bool string(const std::uint32_t count, std::string& value) {
        if (!can_read(count)) return false;
        value.clear();
        value.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            const auto character = static_cast<char>(std::to_integer<unsigned char>(
                bytes_[static_cast<std::size_t>(cursor_++)]));
            if (character != '\0') value.push_back(character);
        }
        return true;
    }
    [[nodiscard]] std::uint64_t remaining() const noexcept { return end_ - cursor_; }

private:
    std::span<const std::byte> bytes_;
    std::uint64_t begin_{}, end_{}, cursor_{};
};

template <typename T>
DecodeResult<T> failure(std::string message) { return {std::nullopt, std::move(message)}; }

template <typename T>
DecodeResult<T> success(T value) { return {std::move(value), {}}; }

const Chunk* payload_struct(const Chunk& parent) noexcept {
    return find_child(parent, struct_chunk);
}

bool sane_count(const std::int32_t value) noexcept { return value >= 0 && value <= 100'000'000; }

bool checked_array(Reader& reader, const std::int32_t count, const std::uint64_t stride) noexcept {
    if (!sane_count(count)) return false;
    const auto unsigned_count = static_cast<std::uint64_t>(count);
    if (unsigned_count > std::numeric_limits<std::uint64_t>::max() / stride) return false;
    return reader.skip(unsigned_count * stride);
}

bool length_prefixed_string(Reader& reader, std::string& value) {
    std::uint32_t length{};
    return reader.u32(length) && length <= 16U * 1024U * 1024U && reader.string(length, value);
}

bool physics_tag(Reader& reader, const std::uint16_t expected, std::uint16_t* version = nullptr) noexcept {
    std::uint32_t tag{};
    if (!reader.u32(tag) || (tag & 0xFFFFU) != expected) return false;
    if (version) *version = static_cast<std::uint16_t>(tag >> 16U);
    return true;
}

bool physics_float(Reader& reader, float& value) noexcept {
    return physics_tag(reader, 5) && reader.f32(value);
}

bool physics_vec3(Reader& reader, Vec3& value) noexcept {
    return physics_tag(reader, 6) && reader.vec3(value);
}

bool physics_quaternion(Reader& reader, std::array<float, 4>& value) noexcept {
    if (!physics_tag(reader, 7)) return false;
    for (auto& component : value) if (!reader.f32(component)) return false;
    return true;
}

bool physics_matrix(Reader& reader, std::array<float, 12>& value) noexcept {
    if (!physics_tag(reader, 8)) return false;
    for (auto& component : value) if (!reader.f32(component)) return false;
    return true;
}

bool decode_physics_volume_data(Reader& reader, PhysicsVolumeInfo& result, std::string& error,
                                const unsigned depth = 0) {
    if (depth > 64) { error = "Physics volume nesting exceeds 64 levels"; return false; }
    if (!physics_tag(reader, 0x17, &result.version)) {
        error = "Physics volume is missing its versioned 0x17 record"; return false;
    }
    if (!physics_tag(reader, 0x0B) || !physics_tag(reader, 3) || !reader.u32(result.kind)) {
        error = "Physics volume kind header is truncated or invalid"; return false;
    }
    if (result.kind == 0x0E) {
        if (!physics_tag(reader, 0x0E)) {
            error = "Physics sphere volume is truncated"; return false;
        }
        result.shape_scalars.push_back(1.0F); // reader constructs the sphere with this fixed radius
    } else if (result.kind == 0x0F) {
        result.shape_scalars.resize(1);
        if (!physics_tag(reader, 0x0F) || !physics_float(reader, result.shape_scalars[0])) {
            error = "Physics capsule volume is truncated"; return false;
        }
    } else if (result.kind == 0x10) {
        Vec3 half_extents;
        if (!physics_tag(reader, 0x10) || !physics_vec3(reader, half_extents)) {
            error = "Physics box volume is truncated"; return false;
        }
        result.shape_vector = half_extents;
    } else if (result.kind == 0x11) {
        result.shape_scalars.resize(2);
        if (!physics_tag(reader, 0x11) || !physics_float(reader, result.shape_scalars[0]) ||
            !physics_float(reader, result.shape_scalars[1])) {
            error = "Physics cylinder volume is truncated"; return false;
        }
    } else if (result.kind == 0x13) {
        std::uint16_t list_version{};
        std::uint32_t child_count{};
        if (!physics_tag(reader, 0x13) || !physics_tag(reader, 0x15, &list_version) ||
            list_version > 1 || !physics_tag(reader, 3) || !reader.u32(child_count) || child_count > 1'000'000U) {
            error = "Physics triangle-list volume header is truncated or invalid"; return false;
        }
        result.children.reserve(child_count);
        for (std::uint32_t i = 0; i < child_count; ++i) {
            PhysicsVolumeInfo child;
            if (!decode_physics_volume_data(reader, child, error, depth + 1)) return false;
            result.children.push_back(std::move(child));
        }
        if (list_version != 0) {
            float list_scalar{};
            Vec3 list_vector, transform_position;
            std::array<float, 4> transform_orientation{};
            if (!physics_float(reader, list_scalar) || !physics_vec3(reader, list_vector) ||
                !physics_tag(reader, 9) || !physics_vec3(reader, transform_position) ||
                !physics_quaternion(reader, transform_orientation)) {
                error = "Physics triangle-list version-1 fields are truncated"; return false;
            }
            result.shape_scalars.push_back(list_scalar);
            result.shape_vector = list_vector;
        }
    } else {
        error = "Unsupported Physics volume kind " + std::to_string(result.kind) +
            " near file offset " + std::to_string(reader.position());
        return false;
    }
    if (!physics_matrix(reader, result.matrix)) {
        error = "Physics volume matrix is truncated"; return false;
    }
    if (!physics_float(reader, result.size_or_fatness)) {
        error = "Physics volume size/fatness field is truncated"; return false;
    }
    for (auto& value : result.material_coefficients) {
        if (!physics_float(reader, value)) { error = "Physics volume common scalar fields are truncated"; return false; }
    }
    if (result.version != 0 && (!physics_tag(reader, 1) || !reader.u16(result.group) ||
        !physics_tag(reader, 1) || !reader.u16(result.flags))) {
        error = "Physics volume group/flags fields are truncated"; return false;
    }
    return true;
}

bool decode_physics_body_data(Reader& reader, PhysicsBodyDefInfo& result, std::string& error) {
    if (!physics_tag(reader, 0x18)) { error = "Physics Body Definition is missing tag 0x18"; return false; }
    if (!decode_physics_volume_data(reader, result.volume, error)) return false;
    if (!physics_float(reader, result.mass) || !physics_tag(reader, 9) ||
        !physics_vec3(reader, result.principal_inertia) ||
        !physics_quaternion(reader, result.inertia_orientation)) {
        error = "Physics Body Definition mass properties are truncated"; return false;
    }
    if (!physics_float(reader, result.scalar_inertia)) {
        error = "Physics Body Definition scalar inertia is truncated"; return false;
    }
    for (auto& value : result.unknown_scalars) {
        if (!physics_float(reader, value)) { error = "Physics Body Definition scalar fields are truncated"; return false; }
    }
    if (!physics_vec3(reader, result.unknown_vector) || !physics_tag(reader, 3) ||
        !reader.u32(result.flags) || !physics_vec3(reader, result.center_of_mass)) {
        error = "Physics Body Definition final fields are truncated"; return false;
    }
    return true;
}

bool decode_physics_joint_data(Reader& reader, std::string& error) {
    std::uint32_t integer{};
    Vec3 vector;
    std::array<float, 4> quaternion{};
    auto tagged_integer = [&] { return physics_tag(reader, 3) && reader.u32(integer); };
    auto four_floats = [&] {
        float value{};
        for (unsigned i = 0; i < 4; ++i) if (!physics_float(reader, value)) return false;
        return true;
    };
    auto triple = [&] {
        float value{};
        if (!physics_tag(reader, 0x1A)) return false;
        for (unsigned i = 0; i < 3; ++i) if (!physics_float(reader, value)) return false;
        return true;
    };
    if (!physics_tag(reader, 0x1B) || !tagged_integer() || !physics_vec3(reader, vector) ||
        !physics_quaternion(reader, quaternion) || !physics_vec3(reader, vector) ||
        !physics_quaternion(reader, quaternion) || !tagged_integer() ||
        !physics_tag(reader, 0x19) || !four_floats() || !tagged_integer() ||
        !physics_tag(reader, 0x19) || !four_floats() || !triple() || !triple() || !triple() ||
        !tagged_integer() || !triple() || !triple() || !triple()) {
        error = "Physics ragdoll joint record is truncated or has unexpected tags";
        return false;
    }
    return true;
}

std::uint16_t read_u16(const std::span<const std::byte> bytes, const std::uint64_t offset) noexcept {
    const auto i = static_cast<std::size_t>(offset);
    return static_cast<std::uint16_t>(std::to_integer<std::uint16_t>(bytes[i]) |
        (std::to_integer<std::uint16_t>(bytes[i + 1]) << 8U));
}

std::string read_rw_string(const Chunk& chunk, const std::span<const std::byte> bytes) {
    if (chunk.payload_offset > bytes.size() || chunk.available_size > bytes.size() - chunk.payload_offset) return {};
    std::string result;
    result.reserve(static_cast<std::size_t>(chunk.available_size));
    for (std::uint64_t i = 0; i < chunk.available_size; ++i) {
        const auto value = std::to_integer<unsigned char>(bytes[static_cast<std::size_t>(chunk.payload_offset + i)]);
        if (value == 0) break;
        result.push_back(static_cast<char>(value));
    }
    return result;
}

} // namespace

LibraryVersion decode_library_id(const std::uint32_t stamp) noexcept {
    LibraryVersion result;
    result.build = static_cast<std::uint16_t>(stamp & 0xFFFFU);
    if ((stamp & 0xFFFF0000U) != 0) {
        result.encoded_version = ((stamp >> 14U) & 0x3FF00U) + 0x30000U + ((stamp >> 16U) & 0x3FU);
    } else {
        result.encoded_version = stamp << 8U;
    }
    result.major = (result.encoded_version >> 16U) & 0x7U;
    result.minor = (result.encoded_version >> 12U) & 0xFU;
    result.revision = (result.encoded_version >> 8U) & 0xFU;
    result.binary = result.encoded_version & 0x3FU;
    return result;
}

const Chunk* find_child(const Chunk& parent, const std::uint32_t type) noexcept {
    for (const auto& child : parent.children) if (child.type == type) return &child;
    return nullptr;
}

DecodeResult<ClumpInfo> decode_clump(const Chunk& parent, const std::span<const std::byte> bytes) {
    const auto* data = payload_struct(parent);
    if (!data) return failure<ClumpInfo>("Clump has no Struct child");
    Reader reader(bytes, *data);
    ClumpInfo result;
    if (!reader.i32(result.atomics)) return failure<ClumpInfo>("Clump Struct is shorter than 4 bytes");
    if (data->available_size >= 12 && (!reader.i32(result.lights) || !reader.i32(result.cameras)))
        return failure<ClumpInfo>("Clump Struct is truncated");
    if (!sane_count(result.atomics) || !sane_count(result.lights) || !sane_count(result.cameras))
        return failure<ClumpInfo>("Clump contains implausible object counts");
    return success(result);
}

DecodeResult<FrameListInfo> decode_frame_list(const Chunk& parent, const std::span<const std::byte> bytes) {
    const auto* data = payload_struct(parent);
    if (!data) return failure<FrameListInfo>("Frame List has no Struct child");
    Reader reader(bytes, *data);
    std::int32_t count{};
    if (!reader.i32(count) || !sane_count(count)) return failure<FrameListInfo>("Invalid frame count");
    FrameListInfo result;
    result.frames.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count; ++i) {
        FrameInfo frame;
        for (auto& component : frame.rotation) if (!reader.f32(component)) return failure<FrameListInfo>("Frame matrix array is truncated");
        if (!reader.vec3(frame.position) || !reader.i32(frame.parent) || !reader.u32(frame.flags))
            return failure<FrameListInfo>("Frame array is truncated");
        if (frame.parent < -1 || frame.parent >= count) return failure<FrameListInfo>("Frame parent index is out of range");
        result.frames.push_back(frame);
    }
    if (reader.consumed() != data->available_size) return failure<FrameListInfo>("Frame Struct has unexpected trailing bytes");
    return success(std::move(result));
}

DecodeResult<AtomicInfo> decode_atomic(const Chunk& parent, const std::span<const std::byte> bytes) {
    const auto* data = payload_struct(parent);
    if (!data) return failure<AtomicInfo>("Atomic has no Struct child");
    Reader reader(bytes, *data);
    AtomicInfo result;
    if (!reader.i32(result.frame_index) || !reader.i32(result.geometry_index) ||
        !reader.u32(result.flags) || !reader.u32(result.unused))
        return failure<AtomicInfo>("Atomic Struct is shorter than 16 bytes");
    return success(result);
}

DecodeResult<MaterialInfo> decode_material(const Chunk& parent, const std::span<const std::byte> bytes) {
    const auto* data = payload_struct(parent);
    if (!data) return failure<MaterialInfo>("Material has no Struct child");
    Reader reader(bytes, *data);
    MaterialInfo result;
    std::int32_t unused{}, textured{};
    if (!reader.u32(result.flags) || !reader.rgba(result.color) || !reader.i32(unused) || !reader.i32(textured))
        return failure<MaterialInfo>("Material Struct is shorter than 16 bytes");
    result.textured = textured != 0;
    if (data->available_size >= 28 && (!reader.f32(result.ambient) || !reader.f32(result.specular) || !reader.f32(result.diffuse)))
        return failure<MaterialInfo>("Material surface properties are truncated");
    return success(result);
}

DecodeResult<MaterialListInfo> decode_material_list(const Chunk& parent, const std::span<const std::byte> bytes) {
    const auto* data = payload_struct(parent);
    if (!data) return failure<MaterialListInfo>("Material List has no Struct child");
    Reader reader(bytes, *data);
    MaterialListInfo result;
    if (!reader.i32(result.material_count) || !sane_count(result.material_count))
        return failure<MaterialListInfo>("Invalid material count");
    result.remap.reserve(static_cast<std::size_t>(result.material_count));
    for (std::int32_t i = 0; i < result.material_count; ++i) {
        std::int32_t item{};
        if (!reader.i32(item)) return failure<MaterialListInfo>("Material remap table is truncated");
        result.remap.push_back(item);
    }
    if (reader.consumed() != data->available_size)
        return failure<MaterialListInfo>("Material List Struct has unexpected trailing bytes");
    return success(std::move(result));
}

DecodeResult<TextureInfo> decode_texture(const Chunk& parent, const std::span<const std::byte> bytes) {
    const auto* data = payload_struct(parent);
    if (!data) return failure<TextureInfo>("Texture has no Struct child");
    Reader reader(bytes, *data);
    TextureInfo result;
    if (!reader.u32(result.filter_addressing)) return failure<TextureInfo>("Texture Struct is shorter than 4 bytes");
    result.filter_mode = static_cast<std::uint8_t>(result.filter_addressing & 0xFFU);
    result.address_u = static_cast<std::uint8_t>((result.filter_addressing >> 8U) & 0xFU);
    result.address_v = static_cast<std::uint8_t>((result.filter_addressing >> 12U) & 0xFU);
    unsigned string_index{};
    for (const auto& child : parent.children) {
        if (child.type != 0x02) continue;
        if (string_index++ == 0) result.name = read_rw_string(child, bytes);
        else if (string_index == 2) result.mask_name = read_rw_string(child, bytes);
    }
    if (string_index < 2) return failure<TextureInfo>("Texture is missing name or mask String chunks");
    return success(std::move(result));
}

DecodeResult<HAnimInfo> decode_hanim(const Chunk& chunk, const std::span<const std::byte> bytes) {
    Reader reader(bytes, chunk);
    HAnimInfo result;
    std::int32_t count{};
    if (!reader.u32(result.version) || !reader.i32(result.hierarchy_id) || !reader.i32(count) || !sane_count(count))
        return failure<HAnimInfo>("HAnim header is truncated or invalid");
    if (count > 0 && (!reader.u32(result.flags) || !reader.u32(result.keyframe_size)))
        return failure<HAnimInfo>("HAnim hierarchy header is truncated");
    result.nodes.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count; ++i) {
        HAnimNodeInfo node;
        if (!reader.i32(node.node_id) || !reader.i32(node.node_index) || !reader.u32(node.flags))
            return failure<HAnimInfo>("HAnim node array is truncated");
        result.nodes.push_back(node);
    }
    if (reader.consumed() != chunk.available_size) return failure<HAnimInfo>("HAnim payload has unexpected trailing bytes");
    return success(std::move(result));
}

DecodeResult<BinMeshInfo> decode_bin_mesh(const Chunk& chunk, const std::span<const std::byte> bytes) {
    Reader reader(bytes, chunk);
    BinMeshInfo result;
    std::uint32_t mesh_count{};
    if (!reader.u32(result.flags) || !reader.u32(mesh_count) || !reader.u32(result.total_indices) || mesh_count > 10'000'000U)
        return failure<BinMeshInfo>("Bin Mesh header is truncated or invalid");
    result.meshes.reserve(mesh_count);
    std::uint64_t actual_indices{};
    for (std::uint32_t i = 0; i < mesh_count; ++i) {
        BinMeshEntry entry;
        std::uint32_t count{};
        if (!reader.u32(count) || !reader.u32(entry.material_index)) return failure<BinMeshInfo>("Bin Mesh entry is truncated");
        if (count > (chunk.available_size - reader.consumed()) / 4U) return failure<BinMeshInfo>("Bin Mesh index array is truncated");
        entry.indices.reserve(count);
        for (std::uint32_t j = 0; j < count; ++j) {
            std::uint32_t index{}; reader.u32(index); entry.indices.push_back(index);
        }
        actual_indices += count;
        result.meshes.push_back(std::move(entry));
    }
    if (actual_indices != result.total_indices) return failure<BinMeshInfo>("Bin Mesh total index count disagrees with entries");
    if (reader.consumed() != chunk.available_size) return failure<BinMeshInfo>("Bin Mesh payload has unexpected trailing bytes");
    return success(std::move(result));
}

DecodeResult<RightToRenderInfo> decode_right_to_render(const Chunk& chunk, const std::span<const std::byte> bytes) {
    Reader reader(bytes, chunk);
    RightToRenderInfo result;
    if (!reader.u32(result.plugin_id) || !reader.u32(result.extra_data) || reader.consumed() != chunk.available_size)
        return failure<RightToRenderInfo>("Right To Render payload is not exactly 8 bytes");
    return success(result);
}

DecodeResult<AnisotropyInfo> decode_anisotropy(const Chunk& chunk, const std::span<const std::byte> bytes) {
    Reader reader(bytes, chunk);
    AnisotropyInfo result;
    if (!reader.f32(result.coefficient) || reader.consumed() != chunk.available_size)
        return failure<AnisotropyInfo>("Anisotropy payload is not exactly 4 bytes");
    return success(result);
}

DecodeResult<SkinInfo> decode_skin(const Chunk& chunk, const std::int32_t vertex_count,
                                   const std::span<const std::byte> bytes) {
    if (!sane_count(vertex_count)) return failure<SkinInfo>("Skin requires a valid owning Geometry vertex count");
    Reader reader(bytes, chunk);
    SkinInfo result;
    result.vertex_count = vertex_count;
    if (!reader.u8(result.bone_count) || !reader.u8(result.used_bone_count) ||
        !reader.u8(result.max_weights_per_vertex) || !reader.u8(result.padding))
        return failure<SkinInfo>("Skin header is shorter than 4 bytes");
    if (result.used_bone_count > result.bone_count)
        return failure<SkinInfo>("Skin used-bone count exceeds total bone count");
    if (result.max_weights_per_vertex > 4)
        return failure<SkinInfo>("Skin has more than four weights per vertex");
    result.used_bones.reserve(result.used_bone_count);
    for (std::uint8_t i = 0; i < result.used_bone_count; ++i) {
        std::uint8_t bone{};
        if (!reader.u8(bone)) return failure<SkinInfo>("Skin used-bone table is truncated");
        if (bone >= result.bone_count) return failure<SkinInfo>("Skin used-bone index is out of range");
        result.used_bones.push_back(bone);
    }
    result.vertex_indices_offset = reader.position();
    if (!checked_array(reader, vertex_count, 4)) return failure<SkinInfo>("Skin vertex bone-index array is truncated");
    result.vertex_weights_offset = reader.position();
    if (!checked_array(reader, vertex_count, 16)) return failure<SkinInfo>("Skin vertex weight array is truncated");
    result.inverse_matrices_offset = reader.position();
    if (!reader.skip(static_cast<std::uint64_t>(result.bone_count) * 64U))
        return failure<SkinInfo>("Skin inverse bone-matrix array is truncated");
    if (!reader.u32(result.bone_limit) || !reader.u32(result.mesh_count) || !reader.u32(result.rle_count))
        return failure<SkinInfo>("Skin split-data header is truncated");
    result.trailing_split_bytes = chunk.available_size - reader.consumed();
    return success(std::move(result));
}

DecodeResult<UserDataInfo> decode_user_data(const Chunk& chunk, const std::span<const std::byte> bytes) {
    Reader reader(bytes, chunk);
    std::int32_t array_count{};
    if (!reader.i32(array_count) || !sane_count(array_count))
        return failure<UserDataInfo>("User Data array count is truncated or invalid");
    UserDataInfo result;
    result.arrays.reserve(static_cast<std::size_t>(array_count));
    for (std::int32_t array_index = 0; array_index < array_count; ++array_index) {
        std::uint32_t name_size{}, raw_format{};
        std::int32_t element_count{};
        UserDataArrayInfo array;
        if (!reader.u32(name_size) || name_size > chunk.available_size || !reader.string(name_size, array.name))
            return failure<UserDataInfo>("User Data array name is truncated or invalid");
        if (!reader.u32(raw_format) || !reader.i32(element_count) || !sane_count(element_count) ||
            raw_format < 1 || raw_format > 3)
            return failure<UserDataInfo>("User Data array format/count is truncated or invalid");
        array.format = static_cast<UserDataFormat>(raw_format);
        if (array.format == UserDataFormat::integer) {
            array.integers.reserve(static_cast<std::size_t>(element_count));
            for (std::int32_t i = 0; i < element_count; ++i) {
                std::int32_t value{};
                if (!reader.i32(value)) return failure<UserDataInfo>("User Data integer array is truncated");
                array.integers.push_back(value);
            }
        } else if (array.format == UserDataFormat::real) {
            array.reals.reserve(static_cast<std::size_t>(element_count));
            for (std::int32_t i = 0; i < element_count; ++i) {
                float value{};
                if (!reader.f32(value)) return failure<UserDataInfo>("User Data real array is truncated");
                array.reals.push_back(value);
            }
        } else {
            array.strings.reserve(static_cast<std::size_t>(element_count));
            for (std::int32_t i = 0; i < element_count; ++i) {
                std::uint32_t string_size{};
                std::string value;
                if (!reader.u32(string_size) || string_size > chunk.available_size || !reader.string(string_size, value))
                    return failure<UserDataInfo>("User Data string array is truncated");
                array.strings.push_back(std::move(value));
            }
        }
        result.arrays.push_back(std::move(array));
    }
    if (reader.consumed() != chunk.available_size)
        return failure<UserDataInfo>("User Data payload has unexpected trailing bytes");
    return success(std::move(result));
}

DecodeResult<PhysicsBodyDefInfo> decode_physics_body_def(const Chunk& parent,
                                                         const std::span<const std::byte> bytes) {
    const auto* data = payload_struct(parent);
    if (!data) return failure<PhysicsBodyDefInfo>("Physics Body Definition has no Struct child");
    Reader reader(bytes, *data);
    PhysicsBodyDefInfo result;
    std::string error;
    if (!decode_physics_body_data(reader, result, error)) return failure<PhysicsBodyDefInfo>(std::move(error));
    if (reader.consumed() != data->available_size)
        return failure<PhysicsBodyDefInfo>("Physics Body Definition has unexpected trailing bytes");
    return success(std::move(result));
}

DecodeResult<PhysicsRagdollDefInfo> decode_physics_ragdoll_def(const Chunk& parent,
                                                                const std::span<const std::byte> bytes) {
    const auto* data = payload_struct(parent);
    if (!data) return failure<PhysicsRagdollDefInfo>("Physics Ragdoll Definition has no Struct child");
    Reader reader(bytes, *data);
    PhysicsRagdollDefInfo result;
    if (!physics_tag(reader, 1) || !reader.u16(result.type_0) ||
        !physics_tag(reader, 1) || !reader.u16(result.type_1) ||
        !physics_tag(reader, 1) || !reader.u16(result.body_count) ||
        !physics_tag(reader, 1) || !reader.u16(result.joint_count))
        return failure<PhysicsRagdollDefInfo>("Physics Ragdoll Definition header is truncated");
    if (result.body_count > 4096 || result.joint_count > 4096)
        return failure<PhysicsRagdollDefInfo>("Physics Ragdoll Definition counts are implausible");
    std::string error;
    result.bodies.reserve(result.body_count);
    for (std::uint16_t i = 0; i < result.body_count; ++i) {
        PhysicsBodyDefInfo body;
        if (!decode_physics_body_data(reader, body, error)) return failure<PhysicsRagdollDefInfo>(std::move(error));
        result.bodies.push_back(std::move(body));
    }
    for (std::uint16_t i = 0; i < result.joint_count; ++i) {
        if (!decode_physics_joint_data(reader, error)) return failure<PhysicsRagdollDefInfo>(std::move(error));
    }
    result.joint_pairs.reserve(result.joint_count);
    for (std::uint16_t i = 0; i < result.joint_count; ++i) {
        std::array<std::uint16_t, 2> pair{};
        if (!physics_tag(reader, 1) || !reader.u16(pair[0]) ||
            !physics_tag(reader, 1) || !reader.u16(pair[1]))
            return failure<PhysicsRagdollDefInfo>("Physics ragdoll joint-pair table is truncated");
        result.joint_pairs.push_back(pair);
    }
    std::uint32_t table_count{};
    if (!physics_tag(reader, 0x1C) || !physics_tag(reader, 3) || !reader.u32(result.table_rows) ||
        !physics_tag(reader, 3) || !reader.u32(result.table_columns) ||
        !physics_tag(reader, 4) || !reader.u32(table_count) || table_count > 10'000'000U)
        return failure<PhysicsRagdollDefInfo>("Physics ragdoll lookup table header is truncated or invalid");
    result.table_values.reserve(table_count);
    for (std::uint32_t i = 0; i < table_count; ++i) {
        std::uint32_t value{};
        if (!reader.u32(value)) return failure<PhysicsRagdollDefInfo>("Physics ragdoll lookup table is truncated");
        result.table_values.push_back(value);
    }
    std::uint32_t body_id_count{};
    if (!physics_tag(reader, 3) || !reader.u32(result.integer_field) ||
        !physics_tag(reader, 4) || !reader.u32(body_id_count) || body_id_count != result.body_count)
        return failure<PhysicsRagdollDefInfo>("Physics ragdoll body-ID table header is invalid");
    result.body_ids.reserve(body_id_count);
    for (std::uint32_t i = 0; i < body_id_count; ++i) {
        std::uint32_t value{};
        if (!reader.u32(value)) return failure<PhysicsRagdollDefInfo>("Physics ragdoll body-ID table is truncated");
        result.body_ids.push_back(value);
    }
    if (reader.consumed() != data->available_size)
        return failure<PhysicsRagdollDefInfo>("Physics Ragdoll Definition has unexpected trailing bytes");
    return success(std::move(result));
}

DecodeResult<PyroExtensionInfo> decode_pyro_extension(const Chunk& chunk,
                                                       const std::uint32_t owner_type,
                                                       const std::span<const std::byte> bytes) {
    if (chunk.type != 0xFFFFFF00U)
        return failure<PyroExtensionInfo>("Chunk is not the Pyro 0xFFFFFF00 extension");
    Reader reader(bytes, chunk);
    PyroExtensionInfo result;
    result.owner_type = owner_type;
    if (!reader.u32(result.version))
        return failure<PyroExtensionInfo>("Pyro extension has no version field");
    auto read_word = [&]() {
        std::uint32_t value{};
        if (!reader.u32(value)) return false;
        result.words.push_back(value);
        return true;
    };
    auto finish = [&]() -> DecodeResult<PyroExtensionInfo> {
        if (reader.remaining() != 0)
            return failure<PyroExtensionInfo>("Pyro extension has unexpected trailing bytes");
        return success(std::move(result));
    };

    if (owner_type == 0x14) { // RpAtomic, reader 0x006C1BB0
        if (result.version < 1 || result.version > 11)
            return failure<PyroExtensionInfo>("Unsupported Pyro Atomic version");
        const std::uint32_t version_words[] = {0, 4, 5, 6, 7, 8, 9, 10, 10, 12, 13, 13};
        for (std::uint32_t i = 0; i < version_words[result.version]; ++i)
            if (!read_word()) return failure<PyroExtensionInfo>("Pyro Atomic fixed fields are truncated");
        if (!read_word()) return failure<PyroExtensionInfo>("Pyro Atomic object index is truncated");
        if (result.version > 7 && (!read_word() || !read_word()))
            return failure<PyroExtensionInfo>("Pyro Atomic version-8 fields are truncated");
        if (result.version > 10 && (!read_word() || !read_word()))
            return failure<PyroExtensionInfo>("Pyro Atomic version-11 fields are truncated");
        result.strings.resize(result.version > 4 ? 2 : 1);
        for (auto& string : result.strings)
            if (!length_prefixed_string(reader, string))
                return failure<PyroExtensionInfo>("Pyro Atomic string is truncated or implausibly large");
        if (result.version > 5) {
            std::uint32_t has_bounds{};
            if (!reader.u32(has_bounds))
                return failure<PyroExtensionInfo>("Pyro Atomic bounds flag is truncated");
            result.present = has_bounds != 0;
            if (result.present) {
                std::array<float, 6> bounds{};
                for (auto& value : bounds)
                    if (!reader.f32(value)) return failure<PyroExtensionInfo>("Pyro Atomic bounds are truncated");
                result.bounds = bounds;
            }
        }
        return finish();
    }
    if (owner_type == 0x07) { // RpMaterial, reader 0x006C1FD0
        if (result.version < 1 || result.version > 2 || !read_word())
            return failure<PyroExtensionInfo>("Pyro Material header is invalid or truncated");
        result.present = result.words.back() != 0;
        if (result.present) {
            const auto count = result.version > 1 ? 5U : 4U;
            for (std::uint32_t i = 0; i < count; ++i)
                if (!read_word()) return failure<PyroExtensionInfo>("Pyro Material fields are truncated");
            result.strings.resize(1);
            if (!length_prefixed_string(reader, result.strings[0]))
                return failure<PyroExtensionInfo>("Pyro Material name is truncated or implausibly large");
        }
        return finish();
    }
    if (owner_type == 0x0E) { // RwFrame, reader 0x006C2130
        if (result.version < 1 || result.version > 2 || !read_word())
            return failure<PyroExtensionInfo>("Pyro Frame header is invalid or truncated");
        result.present = result.words.back() != 0;
        if (result.present) {
            if (!read_word()) return failure<PyroExtensionInfo>("Pyro Frame field is truncated");
            result.strings.resize(1);
            if (!length_prefixed_string(reader, result.strings[0]))
                return failure<PyroExtensionInfo>("Pyro Frame name is truncated or implausibly large");
        }
        return finish();
    }
    if (owner_type == 0x09) { // RpWorldSector, reader family at 0x006BF6F0
        if (!read_word() || !read_word())
            return failure<PyroExtensionInfo>("Pyro World Sector payload is truncated");
        result.present = result.words.front() != 0;
        while (reader.remaining() >= 4)
            if (!read_word()) return failure<PyroExtensionInfo>("Pyro World Sector payload is truncated");
        return finish();
    }
    if (owner_type == 0x12) { // RpLight, reader 0x006BF7F0
        if (result.version == 1) {
            if (!read_word() || !read_word())
                return failure<PyroExtensionInfo>("Pyro Light version-1 fields are truncated");
        } else if (!read_word()) {
            return failure<PyroExtensionInfo>("Pyro Light mode is truncated");
        }
        return finish();
    }
    return failure<PyroExtensionInfo>("Unsupported owner type for Pyro 0xFFFFFF00 extension");
}

DecodeResult<GeometryInfo> decode_geometry(const Chunk& parent, const std::span<const std::byte> bytes) {
    const auto* data = payload_struct(parent);
    if (!data) return failure<GeometryInfo>("Geometry has no Struct child");
    Reader reader(bytes, *data);
    GeometryInfo result;
    if (!reader.u32(result.format) || !reader.i32(result.triangle_count) ||
        !reader.i32(result.vertex_count) || !reader.i32(result.morph_target_count))
        return failure<GeometryInfo>("Geometry Struct is shorter than 16 bytes");
    if (!sane_count(result.triangle_count) || !sane_count(result.vertex_count) || !sane_count(result.morph_target_count))
        return failure<GeometryInfo>("Geometry contains implausible counts");
    result.stored_size = data->available_size;
    result.texcoord_sets = (result.format >> 16U) & 0xFFU;
    if (result.texcoord_sets == 0) {
        if (result.format & geometry_textured2) result.texcoord_sets = 2;
        else if (result.format & geometry_textured) result.texcoord_sets = 1;
    }
    if (result.texcoord_sets > 8) return failure<GeometryInfo>("Geometry has more than 8 texture coordinate sets");
    if (result.format & geometry_native) {
        result.computed_size = reader.consumed();
        return success(std::move(result));
    }
    if (result.format & geometry_prelit) {
        result.prelight_offset = reader.position();
        if (!checked_array(reader, result.vertex_count, 4)) return failure<GeometryInfo>("Prelight array is truncated");
    }
    for (std::uint32_t i = 0; i < result.texcoord_sets; ++i) {
        result.texcoord_offsets.push_back(reader.position());
        if (!checked_array(reader, result.vertex_count, 8)) return failure<GeometryInfo>("Texture coordinate array is truncated");
    }
    result.triangles_offset = reader.position();
    if (!checked_array(reader, result.triangle_count, 8)) return failure<GeometryInfo>("Triangle array is truncated");
    result.morph_targets.reserve(static_cast<std::size_t>(result.morph_target_count));
    for (std::int32_t i = 0; i < result.morph_target_count; ++i) {
        MorphTargetInfo morph;
        std::uint32_t vertices{}, normals{};
        if (!reader.vec3(morph.sphere.center) || !reader.f32(morph.sphere.radius) ||
            !reader.u32(vertices) || !reader.u32(normals)) return failure<GeometryInfo>("Morph target header is truncated");
        morph.has_vertices = vertices != 0;
        morph.has_normals = normals != 0;
        if (morph.has_vertices) {
            morph.vertices_offset = reader.position();
            if (!checked_array(reader, result.vertex_count, 12)) return failure<GeometryInfo>("Morph vertex array is truncated");
        }
        if (morph.has_normals) {
            morph.normals_offset = reader.position();
            if (!checked_array(reader, result.vertex_count, 12)) return failure<GeometryInfo>("Morph normal array is truncated");
        }
        result.morph_targets.push_back(morph);
    }
    result.computed_size = reader.consumed();
    if (result.computed_size != result.stored_size) return failure<GeometryInfo>("Computed Geometry arrays do not consume the complete Struct");

    const auto* material_list = find_child(parent, material_list_chunk);
    if (material_list) {
        const auto decoded_materials = decode_material_list(*material_list, bytes);
        if (!decoded_materials) return failure<GeometryInfo>(decoded_materials.error);
        result.material_count = decoded_materials.value->material_count;
        bool memory_order = true;
        bool stream_order = true;
        for (std::int32_t i = 0; i < result.triangle_count; ++i) {
            const auto offset = result.triangles_offset + static_cast<std::uint64_t>(i) * 8;
            const std::array<std::uint16_t, 4> item = {read_u16(bytes, offset), read_u16(bytes, offset + 2),
                read_u16(bytes, offset + 4), read_u16(bytes, offset + 6)};
            memory_order = memory_order && item[0] < result.vertex_count && item[1] < result.vertex_count &&
                item[2] < result.vertex_count && item[3] < result.material_count;
            stream_order = stream_order && item[0] < result.vertex_count && item[1] < result.vertex_count &&
                item[3] < result.vertex_count && item[2] < result.material_count;
        }
        if (stream_order && !memory_order) result.triangle_layout = TriangleLayout::stream_order;
        else if (memory_order && !stream_order) result.triangle_layout = TriangleLayout::memory_order;
        else if (result.triangle_count == 0) result.triangle_layout = TriangleLayout::stream_order;
    }
    return success(std::move(result));
}

DecodeResult<TriangleInfo> decode_triangle(const GeometryInfo& geometry, const std::int32_t index,
                                           const std::span<const std::byte> bytes) {
    if (index < 0 || index >= geometry.triangle_count) return failure<TriangleInfo>("Triangle index is out of range");
    if (geometry.triangle_layout == TriangleLayout::unknown) return failure<TriangleInfo>("Triangle word order is ambiguous");
    const auto offset = geometry.triangles_offset + static_cast<std::uint64_t>(index) * 8;
    if (offset > bytes.size() || bytes.size() - offset < 8) return failure<TriangleInfo>("Triangle is outside the file");
    const std::array<std::uint16_t, 4> item = {read_u16(bytes, offset), read_u16(bytes, offset + 2),
        read_u16(bytes, offset + 4), read_u16(bytes, offset + 6)};
    TriangleInfo result;
    if (geometry.triangle_layout == TriangleLayout::stream_order) {
        result.vertices = {item[1], item[0], item[3]};
        result.material = item[2];
    } else {
        result.vertices = {item[0], item[1], item[2]};
        result.material = item[3];
    }
    return success(result);
}

DecodeResult<WorldInfo> decode_world(const Chunk& parent, const std::span<const std::byte> bytes) {
    const auto* data = payload_struct(parent);
    if (!data) return failure<WorldInfo>("World has no Struct child");
    Reader reader(bytes, *data);
    WorldInfo result;
    std::uint32_t root{};
    if (!reader.u32(root) || !reader.vec3(result.inverse_origin) || !reader.i32(result.triangle_count) ||
        !reader.i32(result.vertex_count) || !reader.i32(result.plane_sector_count) ||
        !reader.i32(result.world_sector_count) || !reader.i32(result.collision_sector_size) ||
        !reader.u32(result.format) || !reader.vec3(result.bounding_box_sup) || !reader.vec3(result.bounding_box_inf))
        return failure<WorldInfo>("World Struct is shorter than the 3.7 layout");
    result.root_is_world_sector = root != 0;
    return success(result);
}

DecodeResult<PlaneSectorInfo> decode_plane_sector(const Chunk& parent, const std::span<const std::byte> bytes) {
    const auto* data = payload_struct(parent);
    if (!data) return failure<PlaneSectorInfo>("Plane Section has no Struct child");
    Reader reader(bytes, *data);
    PlaneSectorInfo result;
    std::uint32_t left{}, right{};
    if (!reader.i32(result.axis) || !reader.f32(result.split) || !reader.u32(left) || !reader.u32(right) ||
        !reader.f32(result.left_value) || !reader.f32(result.right_value))
        return failure<PlaneSectorInfo>("Plane Section Struct is shorter than 24 bytes");
    result.left_is_world_sector = left != 0;
    result.right_is_world_sector = right != 0;
    return success(result);
}

DecodeResult<WorldSectorInfo> decode_world_sector(const Chunk& parent, const std::span<const std::byte> bytes) {
    const auto* data = payload_struct(parent);
    if (!data) return failure<WorldSectorInfo>("Atomic Section has no Struct child");
    Reader reader(bytes, *data);
    WorldSectorInfo result;
    std::uint32_t collision{}, unused{};
    if (!reader.i32(result.material_window_base) || !reader.i32(result.triangle_count) ||
        !reader.i32(result.vertex_count) || !reader.vec3(result.bounding_box_inf) ||
        !reader.vec3(result.bounding_box_sup) || !reader.u32(collision) || !reader.u32(unused))
        return failure<WorldSectorInfo>("Atomic Section Struct is shorter than 44 bytes");
    result.collision_sector_present = collision != 0;
    return success(result);
}

} // namespace rws
