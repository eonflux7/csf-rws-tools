#include "geometry_preview.hpp"

#include <imgui.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <unordered_map>

namespace rwsman {
namespace {

using GlSizePtr = std::ptrdiff_t;
using CreateShaderProc = GLuint (APIENTRY*)(GLenum);
using ShaderSourceProc = void (APIENTRY*)(GLuint, GLsizei, const char* const*, const GLint*);
using CompileShaderProc = void (APIENTRY*)(GLuint);
using GetShaderIvProc = void (APIENTRY*)(GLuint, GLenum, GLint*);
using GetShaderInfoLogProc = void (APIENTRY*)(GLuint, GLsizei, GLsizei*, char*);
using DeleteShaderProc = void (APIENTRY*)(GLuint);
using CreateProgramProc = GLuint (APIENTRY*)();
using AttachShaderProc = void (APIENTRY*)(GLuint, GLuint);
using LinkProgramProc = void (APIENTRY*)(GLuint);
using GetProgramIvProc = void (APIENTRY*)(GLuint, GLenum, GLint*);
using GetProgramInfoLogProc = void (APIENTRY*)(GLuint, GLsizei, GLsizei*, char*);
using DeleteProgramProc = void (APIENTRY*)(GLuint);
using UseProgramProc = void (APIENTRY*)(GLuint);
using GenVertexArraysProc = void (APIENTRY*)(GLsizei, GLuint*);
using BindVertexArrayProc = void (APIENTRY*)(GLuint);
using DeleteVertexArraysProc = void (APIENTRY*)(GLsizei, const GLuint*);
using GenBuffersProc = void (APIENTRY*)(GLsizei, GLuint*);
using BindBufferProc = void (APIENTRY*)(GLenum, GLuint);
using BufferDataProc = void (APIENTRY*)(GLenum, GlSizePtr, const void*, GLenum);
using DeleteBuffersProc = void (APIENTRY*)(GLsizei, const GLuint*);
using EnableVertexAttribArrayProc = void (APIENTRY*)(GLuint);
using VertexAttribPointerProc = void (APIENTRY*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
using GetUniformLocationProc = GLint (APIENTRY*)(GLuint, const char*);
using Uniform1iProc = void (APIENTRY*)(GLint, GLint);
using Uniform1fProc = void (APIENTRY*)(GLint, GLfloat);
using Uniform2fProc = void (APIENTRY*)(GLint, GLfloat, GLfloat);
using Uniform3fProc = void (APIENTRY*)(GLint, GLfloat, GLfloat, GLfloat);
using Uniform4fProc = void (APIENTRY*)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
using ActiveTextureProc = void (APIENTRY*)(GLenum);

constexpr GLenum gl_vertex_shader = 0x8B31;
constexpr GLenum gl_fragment_shader = 0x8B30;
constexpr GLenum gl_compile_status = 0x8B81;
constexpr GLenum gl_link_status = 0x8B82;
constexpr GLenum gl_array_buffer = 0x8892;
constexpr GLenum gl_static_draw = 0x88E4;
constexpr GLenum gl_texture0 = 0x84C0;

struct GlApi {
    CreateShaderProc create_shader{};
    ShaderSourceProc shader_source{};
    CompileShaderProc compile_shader{};
    GetShaderIvProc get_shader_iv{};
    GetShaderInfoLogProc get_shader_log{};
    DeleteShaderProc delete_shader{};
    CreateProgramProc create_program{};
    AttachShaderProc attach_shader{};
    LinkProgramProc link_program{};
    GetProgramIvProc get_program_iv{};
    GetProgramInfoLogProc get_program_log{};
    DeleteProgramProc delete_program{};
    UseProgramProc use_program{};
    GenVertexArraysProc gen_vertex_arrays{};
    BindVertexArrayProc bind_vertex_array{};
    DeleteVertexArraysProc delete_vertex_arrays{};
    GenBuffersProc gen_buffers{};
    BindBufferProc bind_buffer{};
    BufferDataProc buffer_data{};
    DeleteBuffersProc delete_buffers{};
    EnableVertexAttribArrayProc enable_vertex_attrib_array{};
    VertexAttribPointerProc vertex_attrib_pointer{};
    GetUniformLocationProc get_uniform_location{};
    Uniform1iProc uniform_1i{};
    Uniform1fProc uniform_1f{};
    Uniform2fProc uniform_2f{};
    Uniform3fProc uniform_3f{};
    Uniform4fProc uniform_4f{};
    ActiveTextureProc active_texture{};

    bool load() {
#define LOAD_GL(member, name) member = reinterpret_cast<decltype(member)>(glfwGetProcAddress(name)); if (!member) return false
        LOAD_GL(create_shader, "glCreateShader");
        LOAD_GL(shader_source, "glShaderSource");
        LOAD_GL(compile_shader, "glCompileShader");
        LOAD_GL(get_shader_iv, "glGetShaderiv");
        LOAD_GL(get_shader_log, "glGetShaderInfoLog");
        LOAD_GL(delete_shader, "glDeleteShader");
        LOAD_GL(create_program, "glCreateProgram");
        LOAD_GL(attach_shader, "glAttachShader");
        LOAD_GL(link_program, "glLinkProgram");
        LOAD_GL(get_program_iv, "glGetProgramiv");
        LOAD_GL(get_program_log, "glGetProgramInfoLog");
        LOAD_GL(delete_program, "glDeleteProgram");
        LOAD_GL(use_program, "glUseProgram");
        LOAD_GL(gen_vertex_arrays, "glGenVertexArrays");
        LOAD_GL(bind_vertex_array, "glBindVertexArray");
        LOAD_GL(delete_vertex_arrays, "glDeleteVertexArrays");
        LOAD_GL(gen_buffers, "glGenBuffers");
        LOAD_GL(bind_buffer, "glBindBuffer");
        LOAD_GL(buffer_data, "glBufferData");
        LOAD_GL(delete_buffers, "glDeleteBuffers");
        LOAD_GL(enable_vertex_attrib_array, "glEnableVertexAttribArray");
        LOAD_GL(vertex_attrib_pointer, "glVertexAttribPointer");
        LOAD_GL(get_uniform_location, "glGetUniformLocation");
        LOAD_GL(uniform_1i, "glUniform1i");
        LOAD_GL(uniform_1f, "glUniform1f");
        LOAD_GL(uniform_2f, "glUniform2f");
        LOAD_GL(uniform_3f, "glUniform3f");
        LOAD_GL(uniform_4f, "glUniform4f");
        LOAD_GL(active_texture, "glActiveTexture");
#undef LOAD_GL
        return true;
    }
};

GlApi& gl_api() {
    static GlApi api;
    static const bool loaded = api.load();
    (void)loaded;
    return api;
}

float read_f32(const std::span<const std::byte> bytes, const std::uint64_t offset) {
    std::uint32_t raw{};
    for (unsigned i = 0; i < 4; ++i)
        raw |= std::to_integer<std::uint32_t>(bytes[static_cast<std::size_t>(offset + i)]) << (i * 8U);
    return std::bit_cast<float>(raw);
}

ImU32 material_color(const std::uint16_t material, const float shade) {
    constexpr std::array<std::array<float, 3>, 12> colors{{
        {0.36F, 0.67F, 0.91F}, {0.91F, 0.48F, 0.35F}, {0.48F, 0.82F, 0.49F},
        {0.84F, 0.67F, 0.31F}, {0.68F, 0.49F, 0.86F}, {0.32F, 0.78F, 0.76F},
        {0.91F, 0.43F, 0.67F}, {0.62F, 0.70F, 0.35F}, {0.42F, 0.55F, 0.83F},
        {0.86F, 0.58F, 0.39F}, {0.48F, 0.76F, 0.65F}, {0.74F, 0.50F, 0.64F}
    }};
    const auto& color = colors[material % colors.size()];
    return IM_COL32(static_cast<int>(255.0F * color[0] * shade),
                    static_cast<int>(255.0F * color[1] * shade),
                    static_cast<int>(255.0F * color[2] * shade), 255);
}

std::uint16_t read_u16(const std::vector<std::byte>& bytes, const std::size_t offset) {
    return static_cast<std::uint16_t>(std::to_integer<std::uint16_t>(bytes[offset]) |
        (std::to_integer<std::uint16_t>(bytes[offset + 1]) << 8U));
}

std::uint32_t read_u32(const std::vector<std::byte>& bytes, const std::size_t offset) {
    return std::to_integer<std::uint32_t>(bytes[offset]) |
        (std::to_integer<std::uint32_t>(bytes[offset + 1]) << 8U) |
        (std::to_integer<std::uint32_t>(bytes[offset + 2]) << 16U) |
        (std::to_integer<std::uint32_t>(bytes[offset + 3]) << 24U);
}

std::array<std::uint8_t, 4> color_565(const std::uint16_t value) {
    const auto r = static_cast<std::uint8_t>((value >> 11U) & 31U);
    const auto g = static_cast<std::uint8_t>((value >> 5U) & 63U);
    const auto b = static_cast<std::uint8_t>(value & 31U);
    return {static_cast<std::uint8_t>((r << 3U) | (r >> 2U)),
            static_cast<std::uint8_t>((g << 2U) | (g >> 4U)),
            static_cast<std::uint8_t>((b << 3U) | (b >> 2U)), 255};
}

bool decode_dds(const std::filesystem::path& path, int& width, int& height,
                std::vector<std::uint8_t>& rgba, std::string& error) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) { error = "Cannot open " + path.string(); return false; }
    stream.seekg(0, std::ios::end);
    const auto file_size = stream.tellg();
    if (file_size < 0) { error = "Cannot determine DDS size for " + path.string(); return false; }
    std::vector<std::byte> bytes(static_cast<std::size_t>(file_size));
    stream.seekg(0, std::ios::beg);
    if (!bytes.empty() && !stream.read(reinterpret_cast<char*>(bytes.data()), file_size)) {
        error = "Cannot read " + path.string(); return false;
    }
    if (bytes.size() < 128 || std::to_integer<char>(bytes[0]) != 'D' ||
        std::to_integer<char>(bytes[1]) != 'D' || std::to_integer<char>(bytes[2]) != 'S' ||
        std::to_integer<char>(bytes[3]) != ' ') {
        error = "Invalid DDS header in " + path.string(); return false;
    }
    width = static_cast<int>(read_u32(bytes, 16));
    height = static_cast<int>(read_u32(bytes, 12));
    const std::string fourcc{std::to_integer<char>(bytes[84]), std::to_integer<char>(bytes[85]),
                             std::to_integer<char>(bytes[86]), std::to_integer<char>(bytes[87])};
    const bool dxt1 = fourcc == "DXT1", dxt3 = fourcc == "DXT3";
    if (width <= 0 || height <= 0 || width > 16384 || height > 16384 || (!dxt1 && !dxt3)) {
        error = "Unsupported DDS format/dimensions in " + path.string(); return false;
    }
    const std::size_t block_size = dxt1 ? 8U : 16U;
    const auto blocks_x = static_cast<std::size_t>((width + 3) / 4);
    const auto blocks_y = static_cast<std::size_t>((height + 3) / 4);
    if (blocks_x > std::numeric_limits<std::size_t>::max() / blocks_y ||
        blocks_x * blocks_y > (bytes.size() - 128) / block_size) {
        error = "Truncated DDS image in " + path.string(); return false;
    }
    rgba.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U, 0);
    std::size_t cursor = 128;
    for (std::size_t by = 0; by < blocks_y; ++by) for (std::size_t bx = 0; bx < blocks_x; ++bx) {
        std::uint64_t alpha = ~std::uint64_t{};
        if (dxt3) {
            alpha = static_cast<std::uint64_t>(read_u32(bytes, cursor)) |
                    (static_cast<std::uint64_t>(read_u32(bytes, cursor + 4)) << 32U);
            cursor += 8;
        }
        const auto c0_raw = read_u16(bytes, cursor), c1_raw = read_u16(bytes, cursor + 2);
        std::array<std::array<std::uint8_t, 4>, 4> colors{};
        colors[0] = color_565(c0_raw); colors[1] = color_565(c1_raw);
        if (dxt3 || c0_raw > c1_raw) {
            for (unsigned channel = 0; channel < 3; ++channel) {
                colors[2][channel] = static_cast<std::uint8_t>((2U * colors[0][channel] + colors[1][channel]) / 3U);
                colors[3][channel] = static_cast<std::uint8_t>((colors[0][channel] + 2U * colors[1][channel]) / 3U);
            }
            colors[2][3] = colors[3][3] = 255;
        } else {
            for (unsigned channel = 0; channel < 3; ++channel)
                colors[2][channel] = static_cast<std::uint8_t>((colors[0][channel] + colors[1][channel]) / 2U);
            colors[2][3] = 255; colors[3] = {0, 0, 0, 0};
        }
        const auto indices = read_u32(bytes, cursor + 4);
        cursor += 8;
        for (unsigned py = 0; py < 4; ++py) for (unsigned px = 0; px < 4; ++px) {
            const auto x = bx * 4U + px, y = by * 4U + py;
            if (x >= static_cast<std::size_t>(width) || y >= static_cast<std::size_t>(height)) continue;
            const auto pixel = py * 4U + px;
            auto color = colors[(indices >> (pixel * 2U)) & 3U];
            if (dxt3) color[3] = static_cast<std::uint8_t>(((alpha >> (pixel * 4U)) & 15U) * 17U);
            const auto output = (y * static_cast<std::size_t>(width) + x) * 4U;
            std::copy(color.begin(), color.end(), rgba.begin() + static_cast<std::ptrdiff_t>(output));
        }
    }
    return true;
}

unsigned int upload_texture(const int width, const int height, const std::uint8_t* rgba) {
    GLuint texture{};
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    return texture;
}

std::filesystem::path find_texture(const std::filesystem::path& source_path, const std::string& name) {
    if (name.empty()) return {};
    const std::array candidates{
        source_path.parent_path() / "Textures" / (name + ".dds"),
        source_path.parent_path() / (name + ".dds"),
        source_path.parent_path().parent_path() / "Textures" / (name + ".dds")
    };
    std::error_code error;
    for (const auto& candidate : candidates)
        if (std::filesystem::is_regular_file(candidate, error)) return candidate;

    return {};
}

} // namespace

void GeometryPreview::clear() {
    destroy_gpu_resources();
    if (!owned_texture_ids_.empty())
        glDeleteTextures(static_cast<GLsizei>(owned_texture_ids_.size()), owned_texture_ids_.data());
    if (checker_texture_ != 0) glDeleteTextures(1, &checker_texture_);
    chunk_offset_ = ~std::uint64_t{};
    vertices_.clear();
    uvs_.clear();
    faces_.clear();
    gpu_vertices_.clear();
    draw_batches_.clear();
    material_colors_.clear();
    material_textures_.clear();
    material_texture_names_.clear();
    owned_texture_ids_.clear();
    checker_texture_ = 0;
    loaded_texture_count_ = missing_texture_count_ = 0;
    texture_status_.clear();
    error_.clear();
}

void GeometryPreview::reset_view() {
    yaw_ = -0.65F;
    pitch_ = -0.35F;
    distance_ = std::max(radius_ * 3.0F, 0.01F);
    pan_x_ = pan_y_ = 0.0F;
}

bool GeometryPreview::load(const rws::Chunk& geometry_chunk,
                           const std::span<const std::byte> bytes,
                           const std::filesystem::path& source_path) {
    clear();
    chunk_offset_ = geometry_chunk.offset;
    const auto geometry = rws::decode_geometry(geometry_chunk, bytes);
    if (!geometry) { error_ = geometry.error; return false; }
    if (geometry.value->triangle_layout == rws::TriangleLayout::unknown) {
        error_ = "Triangle word order is ambiguous";
        return false;
    }
    const auto morph = std::find_if(geometry.value->morph_targets.begin(),
        geometry.value->morph_targets.end(), [](const rws::MorphTargetInfo& value) {
            return value.has_vertices;
        });
    if (morph == geometry.value->morph_targets.end()) {
        error_ = "Geometry has no non-native vertex array";
        return false;
    }
    if (!geometry.value->texcoord_offsets.empty()) {
        const auto uv_offset = geometry.value->texcoord_offsets.front();
        const auto uv_bytes = static_cast<std::uint64_t>(geometry.value->vertex_count) * 8U;
        if (uv_offset <= bytes.size() && uv_bytes <= bytes.size() - uv_offset) {
            uvs_.reserve(static_cast<std::size_t>(geometry.value->vertex_count));
            for (std::int32_t i = 0; i < geometry.value->vertex_count; ++i) {
                const auto offset = uv_offset + static_cast<std::uint64_t>(i) * 8U;
                uvs_.push_back({read_f32(bytes, offset), 1.0F - read_f32(bytes, offset + 4)});
            }
        }
    }

    if (const auto* material_list_chunk = rws::find_child(geometry_chunk, 0x08)) {
        const auto material_list = rws::decode_material_list(*material_list_chunk, bytes);
        if (material_list) {
            std::vector<const rws::Chunk*> material_chunks;
            for (const auto& child : material_list_chunk->children)
                if (child.type == 0x07) material_chunks.push_back(&child);
            material_colors_.resize(static_cast<std::size_t>(material_list.value->material_count), {190, 190, 190, 255});
            material_textures_.resize(material_colors_.size());
            material_texture_names_.resize(material_colors_.size());
            std::unordered_map<std::string, unsigned int> texture_cache;
            std::size_t next_material{};
            for (std::size_t i = 0; i < material_colors_.size(); ++i) {
                const auto remap = i < material_list.value->remap.size() ? material_list.value->remap[i] : -1;
                if (remap >= 0 && static_cast<std::size_t>(remap) < i) {
                    material_colors_[i] = material_colors_[static_cast<std::size_t>(remap)];
                    material_textures_[i] = material_textures_[static_cast<std::size_t>(remap)];
                    material_texture_names_[i] = material_texture_names_[static_cast<std::size_t>(remap)];
                    continue;
                }
                if (next_material >= material_chunks.size()) continue;
                const auto& material_chunk = *material_chunks[next_material++];
                const auto material = rws::decode_material(material_chunk, bytes);
                if (material) material_colors_[i] = material.value->color;
                const auto* texture_chunk = rws::find_child(material_chunk, 0x06);
                if (!texture_chunk) continue;
                const auto texture = rws::decode_texture(*texture_chunk, bytes);
                if (!texture || texture.value->name.empty()) continue;
                material_texture_names_[i] = texture.value->name;
                if (const auto cached = texture_cache.find(texture.value->name); cached != texture_cache.end()) {
                    material_textures_[i] = cached->second;
                    continue;
                }
                const auto path = find_texture(source_path, texture.value->name);
                if (path.empty()) {
                    ++missing_texture_count_;
                    if (texture_status_.empty())
                        texture_status_ = "Could not locate " + texture.value->name + ".dds from " +
                            source_path.parent_path().string();
                    continue;
                }
                int width{}, height{};
                std::vector<std::uint8_t> rgba;
                std::string texture_error;
                if (!decode_dds(path, width, height, rgba, texture_error)) {
                    ++missing_texture_count_;
                    if (texture_status_.empty()) texture_status_ = std::move(texture_error);
                    continue;
                }
                const auto id = upload_texture(width, height, rgba.data());
                owned_texture_ids_.push_back(id);
                texture_cache.emplace(texture.value->name, id);
                material_textures_[i] = id;
                ++loaded_texture_count_;
            }
        }
    }
    std::array<std::uint8_t, 64 * 64 * 4> checker{};
    for (int y = 0; y < 64; ++y) for (int x = 0; x < 64; ++x) {
        const bool bright = ((x / 8) ^ (y / 8)) & 1;
        const auto offset = static_cast<std::size_t>(y * 64 + x) * 4U;
        checker[offset] = checker[offset + 1] = checker[offset + 2] = bright ? 210 : 65;
        checker[offset + 3] = 255;
    }
    checker_texture_ = upload_texture(64, 64, checker.data());
    const auto vertex_bytes = static_cast<std::uint64_t>(geometry.value->vertex_count) * 12U;
    if (morph->vertices_offset > bytes.size() || vertex_bytes > bytes.size() - morph->vertices_offset) {
        error_ = "Geometry vertex array is outside the file";
        return false;
    }
    vertices_.reserve(static_cast<std::size_t>(geometry.value->vertex_count));
    rws::Vec3 minimum{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                      std::numeric_limits<float>::max()};
    rws::Vec3 maximum{-minimum.x, -minimum.y, -minimum.z};
    for (std::int32_t i = 0; i < geometry.value->vertex_count; ++i) {
        const auto offset = morph->vertices_offset + static_cast<std::uint64_t>(i) * 12U;
        rws::Vec3 vertex{read_f32(bytes, offset), read_f32(bytes, offset + 4), read_f32(bytes, offset + 8)};
        if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.z)) {
            vertices_.clear(); faces_.clear();
            error_ = "Geometry contains a non-finite vertex";
            return false;
        }
        vertices_.push_back(vertex);
        minimum.x = std::min(minimum.x, vertex.x); minimum.y = std::min(minimum.y, vertex.y);
        minimum.z = std::min(minimum.z, vertex.z); maximum.x = std::max(maximum.x, vertex.x);
        maximum.y = std::max(maximum.y, vertex.y); maximum.z = std::max(maximum.z, vertex.z);
    }
    center_ = {(minimum.x + maximum.x) * 0.5F, (minimum.y + maximum.y) * 0.5F,
               (minimum.z + maximum.z) * 0.5F};
    radius_ = 0.0F;
    for (const auto& vertex : vertices_) {
        const auto x = vertex.x - center_.x, y = vertex.y - center_.y, z = vertex.z - center_.z;
        radius_ = std::max(radius_, std::sqrt(x * x + y * y + z * z));
    }
    radius_ = std::max(radius_, 0.001F);
    faces_.reserve(static_cast<std::size_t>(geometry.value->triangle_count));
    for (std::int32_t i = 0; i < geometry.value->triangle_count; ++i) {
        const auto triangle = rws::decode_triangle(*geometry.value, i, bytes);
        if (!triangle) { error_ = triangle.error; faces_.clear(); return false; }
        if (triangle.value->vertices[0] >= vertices_.size() ||
            triangle.value->vertices[1] >= vertices_.size() ||
            triangle.value->vertices[2] >= vertices_.size()) {
            error_ = "Triangle references a vertex outside the geometry";
            faces_.clear(); return false;
        }
        faces_.push_back({triangle.value->vertices[0], triangle.value->vertices[1],
                          triangle.value->vertices[2], triangle.value->material});
    }
    std::vector<std::size_t> face_order(faces_.size());
    for (std::size_t i = 0; i < face_order.size(); ++i) face_order[i] = i;
    std::stable_sort(face_order.begin(), face_order.end(), [&](const std::size_t a, const std::size_t b) {
        return faces_[a].material < faces_[b].material;
    });
    gpu_vertices_.reserve(faces_.size() * 3U);
    for (const auto face_index : face_order) {
        const auto& face = faces_[face_index];
        if (draw_batches_.empty() || draw_batches_.back().material != face.material)
            draw_batches_.push_back({face.material, static_cast<std::uint32_t>(gpu_vertices_.size()), 0});
        const auto& a = vertices_[face.a];
        const auto& b = vertices_[face.b];
        const auto& c = vertices_[face.c];
        const float ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
        const float vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
        float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        const float normal_length = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (normal_length > 0.0F) { nx /= normal_length; ny /= normal_length; nz /= normal_length; }
        const std::array indices{face.a, face.b, face.c};
        for (const auto index : indices) {
            const auto uv = index < uvs_.size() ? uvs_[index] : Uv{};
            const auto& vertex = vertices_[index];
            gpu_vertices_.push_back({vertex.x, vertex.y, vertex.z, uv.u, uv.v, nx, ny, nz});
        }
        draw_batches_.back().count += 3;
    }
    if (!create_gpu_resources()) return false;
    reset_view();
    return true;
}

void GeometryPreview::render_callback(const ImDrawList*, const ImDrawCmd* command) {
    static_cast<GeometryPreview*>(command->UserCallbackData)->render_gpu();
}

bool GeometryPreview::create_gpu_resources() {
    auto& gl = gl_api();
    if (!gl.create_shader || gpu_vertices_.empty()) {
        error_ = "OpenGL 3.3 functions are unavailable";
        return false;
    }
    constexpr const char* vertex_source = R"GLSL(#version 330 core
layout(location=0) in vec3 aPosition;
layout(location=1) in vec2 aUv;
layout(location=2) in vec3 aNormal;
uniform vec3 uCenter;
uniform float uYaw, uPitch, uDistance;
uniform vec2 uPan;
uniform float uAspect, uTanHalfFov, uNear, uFar;
out vec2 vUv;
out vec3 vNormal;
void main() {
    vec3 p = aPosition - uCenter;
    float cy=cos(uYaw), sy=sin(uYaw), cp=cos(uPitch), sp=sin(uPitch);
    float rx=cy*p.x-sy*p.y, ry=sy*p.x+cy*p.y;
    vec3 view=vec3(rx+uPan.x, cp*p.z-sp*ry+uPan.y, sp*p.z+cp*ry-uDistance);
    float nrx=cy*aNormal.x-sy*aNormal.y, nry=sy*aNormal.x+cy*aNormal.y;
    vNormal=vec3(nrx, cp*aNormal.z-sp*nry, sp*aNormal.z+cp*nry);
    float f=1.0/uTanHalfFov;
    gl_Position=vec4(view.x*f/uAspect, view.y*f,
        ((uFar+uNear)/(uNear-uFar))*view.z+(2.0*uFar*uNear)/(uNear-uFar), -view.z);
    vUv=aUv;
})GLSL";
    constexpr const char* fragment_source = R"GLSL(#version 330 core
in vec2 vUv;
in vec3 vNormal;
uniform sampler2D uTexture;
uniform bool uUseTexture;
uniform vec4 uBaseColor;
out vec4 FragColor;
void main() {
    float light=0.42+0.58*abs(dot(normalize(vNormal), normalize(vec3(0.35,0.55,0.75))));
    vec4 color=uUseTexture ? texture(uTexture,vUv) : uBaseColor;
    if (color.a < 0.08) discard;
    FragColor=vec4(color.rgb*light,color.a);
})GLSL";
    auto compile = [&](const GLenum type, const char* source) -> GLuint {
        const GLuint shader = gl.create_shader(type);
        gl.shader_source(shader, 1, &source, nullptr);
        gl.compile_shader(shader);
        GLint okay{};
        gl.get_shader_iv(shader, gl_compile_status, &okay);
        if (!okay) {
            std::array<char, 1024> log{};
            gl.get_shader_log(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
            error_ = "OpenGL shader compile failed: " + std::string(log.data());
            gl.delete_shader(shader);
            return 0;
        }
        return shader;
    };
    const GLuint vertex_shader = compile(gl_vertex_shader, vertex_source);
    if (!vertex_shader) return false;
    const GLuint fragment_shader = compile(gl_fragment_shader, fragment_source);
    if (!fragment_shader) { gl.delete_shader(vertex_shader); return false; }
    shader_program_ = gl.create_program();
    gl.attach_shader(shader_program_, vertex_shader);
    gl.attach_shader(shader_program_, fragment_shader);
    gl.link_program(shader_program_);
    gl.delete_shader(vertex_shader);
    gl.delete_shader(fragment_shader);
    GLint linked{};
    gl.get_program_iv(shader_program_, gl_link_status, &linked);
    if (!linked) {
        std::array<char, 1024> log{};
        gl.get_program_log(shader_program_, static_cast<GLsizei>(log.size()), nullptr, log.data());
        error_ = "OpenGL shader link failed: " + std::string(log.data());
        destroy_gpu_resources();
        return false;
    }
    gl.gen_vertex_arrays(1, &vertex_array_);
    gl.gen_buffers(1, &vertex_buffer_);
    gl.bind_vertex_array(vertex_array_);
    gl.bind_buffer(gl_array_buffer, vertex_buffer_);
    gl.buffer_data(gl_array_buffer, static_cast<GlSizePtr>(gpu_vertices_.size() * sizeof(GpuVertex)),
                   gpu_vertices_.data(), gl_static_draw);
    gl.enable_vertex_attrib_array(0);
    gl.vertex_attrib_pointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), reinterpret_cast<void*>(offsetof(GpuVertex, x)));
    gl.enable_vertex_attrib_array(1);
    gl.vertex_attrib_pointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), reinterpret_cast<void*>(offsetof(GpuVertex, u)));
    gl.enable_vertex_attrib_array(2);
    gl.vertex_attrib_pointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), reinterpret_cast<void*>(offsetof(GpuVertex, nx)));
    gl.bind_vertex_array(0);
    return true;
}

void GeometryPreview::destroy_gpu_resources() {
    auto& gl = gl_api();
    if (vertex_buffer_ && gl.delete_buffers) gl.delete_buffers(1, &vertex_buffer_);
    if (vertex_array_ && gl.delete_vertex_arrays) gl.delete_vertex_arrays(1, &vertex_array_);
    if (shader_program_ && gl.delete_program) gl.delete_program(shader_program_);
    vertex_buffer_ = vertex_array_ = shader_program_ = 0;
}

void GeometryPreview::render_gpu() {
    if (!shader_program_ || !vertex_array_ || draw_batches_.empty() ||
        canvas_width_ < 1.0F || canvas_height_ < 1.0F) return;
    auto& gl = gl_api();
    const auto& io = ImGui::GetIO();
    const int viewport_x = static_cast<int>(canvas_x_ * io.DisplayFramebufferScale.x);
    const int viewport_y = static_cast<int>((io.DisplaySize.y - canvas_y_ - canvas_height_) *
                                             io.DisplayFramebufferScale.y);
    const int viewport_width = std::max(1, static_cast<int>(canvas_width_ * io.DisplayFramebufferScale.x));
    const int viewport_height = std::max(1, static_cast<int>(canvas_height_ * io.DisplayFramebufferScale.y));

    glViewport(viewport_x, viewport_y, viewport_width, viewport_height);
    glScissor(viewport_x, viewport_y, viewport_width, viewport_height);
    glEnable(GL_SCISSOR_TEST);
    glClearDepth(1.0);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    if (cull_backfaces_) { glEnable(GL_CULL_FACE); glCullFace(GL_BACK); }
    else glDisable(GL_CULL_FACE);
    gl.use_program(shader_program_);
    gl.bind_vertex_array(vertex_array_);
    gl.active_texture(gl_texture0);

    const float focal = std::min(canvas_width_, canvas_height_) * 0.78F;
    const float pan_world_x = pan_x_ * distance_ / std::max(focal, 1.0F);
    const float pan_world_z = -pan_y_ * distance_ / std::max(focal, 1.0F);
    const float near_plane = std::max(radius_ * 0.01F, 0.00001F);
    const float far_plane = std::max(distance_ + radius_ * 3.0F, near_plane + 1.0F);
    gl.uniform_3f(gl.get_uniform_location(shader_program_, "uCenter"), center_.x, center_.y, center_.z);
    gl.uniform_1f(gl.get_uniform_location(shader_program_, "uYaw"), yaw_);
    gl.uniform_1f(gl.get_uniform_location(shader_program_, "uPitch"), pitch_);
    gl.uniform_1f(gl.get_uniform_location(shader_program_, "uDistance"), distance_);
    gl.uniform_2f(gl.get_uniform_location(shader_program_, "uPan"), pan_world_x, pan_world_z);
    gl.uniform_1f(gl.get_uniform_location(shader_program_, "uAspect"),
                  static_cast<float>(viewport_width) / static_cast<float>(viewport_height));
    gl.uniform_1f(gl.get_uniform_location(shader_program_, "uTanHalfFov"),
                  std::tan(25.0F * 3.14159265358979323846F / 180.0F));
    gl.uniform_1f(gl.get_uniform_location(shader_program_, "uNear"), near_plane);
    gl.uniform_1f(gl.get_uniform_location(shader_program_, "uFar"), far_plane);
    gl.uniform_1i(gl.get_uniform_location(shader_program_, "uTexture"), 0);
    const GLint use_texture_location = gl.get_uniform_location(shader_program_, "uUseTexture");
    const GLint base_color_location = gl.get_uniform_location(shader_program_, "uBaseColor");

    auto set_color = [&](const std::uint16_t material) {
        if (view_style_ == 1) {
            const auto packed = material_color(material, 1.0F);
            gl.uniform_4f(base_color_location,
                static_cast<float>((packed >> IM_COL32_R_SHIFT) & 0xFFU) / 255.0F,
                static_cast<float>((packed >> IM_COL32_G_SHIFT) & 0xFFU) / 255.0F,
                static_cast<float>((packed >> IM_COL32_B_SHIFT) & 0xFFU) / 255.0F, 1.0F);
            return;
        }
        const auto index = static_cast<std::size_t>(material);
        const auto rgba = index < material_colors_.size() ? material_colors_[index] :
            std::array<std::uint8_t, 4>{190, 190, 190, 255};
        gl.uniform_4f(base_color_location, rgba[0] / 255.0F, rgba[1] / 255.0F,
                      rgba[2] / 255.0F, rgba[3] / 255.0F);
    };

    if (view_style_ != 4) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        for (const auto& batch : draw_batches_) {
            const auto material = static_cast<std::size_t>(batch.material);
            GLuint texture{};
            if (view_style_ == 0 && uvs_.size() == vertices_.size() && material < material_textures_.size())
                texture = material_textures_[material];
            else if (view_style_ == 3 && uvs_.size() == vertices_.size()) texture = checker_texture_;
            gl.uniform_1i(use_texture_location, texture != 0);
            if (texture) glBindTexture(GL_TEXTURE_2D, texture);
            else set_color(batch.material);
            glDrawArrays(GL_TRIANGLES, static_cast<GLint>(batch.first), static_cast<GLsizei>(batch.count));
        }
    }
    if (view_style_ == 4 || wireframe_) {
        gl.uniform_1i(use_texture_location, 0);
        const float color = view_style_ == 4 ? 0.84F : 0.09F;
        gl.uniform_4f(base_color_location, color, view_style_ == 4 ? 0.88F : 0.10F,
                      view_style_ == 4 ? 0.95F : 0.13F, 1.0F);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(-1.0F, -1.0F);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        for (const auto& batch : draw_batches_)
            glDrawArrays(GL_TRIANGLES, static_cast<GLint>(batch.first), static_cast<GLsizei>(batch.count));
        glDisable(GL_POLYGON_OFFSET_LINE);
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    gl.bind_vertex_array(0);
    gl.use_program(0);
}

void GeometryPreview::draw(const rws::Chunk& geometry_chunk,
                           const std::span<const std::byte> bytes,
                           const std::filesystem::path& source_path) {
    if (chunk_offset_ != geometry_chunk.offset) load(geometry_chunk, bytes, source_path);

    constexpr const char* styles[] = {"Textured", "Material index", "Material color", "UV checker", "Wireframe"};
    ImGui::SetNextItemWidth(150.0F);
    ImGui::Combo("View style", &view_style_, styles, static_cast<int>(std::size(styles))); ImGui::SameLine();
    if (view_style_ != 4) { ImGui::Checkbox("Wire overlay", &wireframe_); ImGui::SameLine(); }
    ImGui::Checkbox("Cull backfaces", &cull_backfaces_); ImGui::SameLine();
    if (ImGui::Button("Frame geometry")) reset_view(); ImGui::SameLine();
    if (ImGui::Button("Reload edited bytes")) load(geometry_chunk, bytes, source_path);
    if (view_style_ == 0) {
        ImGui::Text("DDS files: %zu loaded | material slots unresolved: %zu | UV set: %s", loaded_texture_count_,
            missing_texture_count_, uvs_.size() == vertices_.size() ? "present" : "missing");
        if (!texture_status_.empty()) { ImGui::SameLine(); ImGui::TextDisabled("%s", texture_status_.c_str()); }
    }
    ImGui::TextDisabled("Left drag: orbit | Middle/right drag: pan | Wheel: zoom | Double-click: frame");

    const auto available = ImGui::GetContentRegionAvail();
    const ImVec2 size{std::max(available.x, 64.0F), std::max(available.y, 160.0F)};
    const auto origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("geometry_canvas", size,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle | ImGuiButtonFlags_MouseButtonRight);
    auto* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y}, IM_COL32(22, 25, 31, 255));
    draw_list->AddRect(origin, {origin.x + size.x, origin.y + size.y}, IM_COL32(70, 76, 88, 255));
    if (ImGui::IsItemHovered()) {
        const auto& io = ImGui::GetIO();
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) reset_view();
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            yaw_ += io.MouseDelta.x * 0.01F;
            pitch_ = std::clamp(pitch_ + io.MouseDelta.y * 0.01F, -1.5F, 1.5F);
        }
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) || ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            pan_x_ += io.MouseDelta.x;
            pan_y_ += io.MouseDelta.y;
        }
        if (io.MouseWheel != 0.0F)
            distance_ = std::clamp(distance_ * std::exp(-io.MouseWheel * 0.16F), radius_ * 1.05F,
                                   radius_ * 100.0F);
    }
    if (!error_.empty()) {
        draw_list->AddText({origin.x + 12, origin.y + 12}, IM_COL32(255, 120, 90, 255), error_.c_str());
        return;
    }

    canvas_x_ = origin.x; canvas_y_ = origin.y;
    canvas_width_ = size.x; canvas_height_ = size.y;
    draw_list->AddCallback(render_callback, this);
    draw_list->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
    const std::string statistics = std::to_string(vertices_.size()) + " vertices | " +
        std::to_string(faces_.size()) + " triangles | GPU depth test";
    draw_list->AddText({origin.x + 10, origin.y + 9}, IM_COL32(225, 229, 238, 255), statistics.c_str());
}

} // namespace rwsman
