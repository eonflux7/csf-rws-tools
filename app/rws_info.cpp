#include "rws/document.hpp"
#include "rws/decoded.hpp"
#include "rws/obj_export.hpp"

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

namespace {

void print_chunks(const std::vector<rws::Chunk>& chunks, const unsigned depth = 0) {
    for (const auto& chunk : chunks) {
        std::cout << std::string(depth * 2, ' ') << "0x" << std::hex << std::setw(8)
                  << std::setfill('0') << chunk.offset << std::dec << std::setfill(' ')
                  << "  " << rws::chunk_name(chunk.type) << " [0x" << std::hex << chunk.type
                  << std::dec << "] size=" << chunk.declared_size;
        if (chunk.truncated) {
            std::cout << " available=" << chunk.available_size << " TRUNCATED";
        }
        std::cout << " stamp=0x" << std::hex << chunk.library_id << std::dec << '\n';
        print_chunks(chunk.children, depth + 1);
    }
}

struct TypeStats { std::uint64_t count{}; std::uint64_t bytes{}; std::uint64_t truncated{}; };

void collect_stats(const std::vector<rws::Chunk>& chunks, std::map<std::uint32_t, TypeStats>& stats) {
    for (const auto& chunk : chunks) {
        auto& item = stats[chunk.type];
        ++item.count;
        item.bytes += chunk.available_size;
        item.truncated += chunk.truncated ? 1U : 0U;
        collect_stats(chunk.children, stats);
    }
}

struct ValidationStats {
    std::uint64_t decoded{}, failed{};
    std::uint64_t triangles_stream{}, triangles_memory{}, triangles_ambiguous{};
};

void validate_types(const std::vector<rws::Chunk>& chunks, const std::span<const std::byte> bytes,
                    ValidationStats& stats, std::optional<std::int32_t> geometry_vertices = std::nullopt,
                    const std::uint32_t enclosing_object_type = 0) {
    for (const auto& chunk : chunks) {
        std::string error;
        bool handled = true;
        auto child_geometry_vertices = geometry_vertices;
        switch (chunk.type) {
        case 0x06: { auto value = rws::decode_texture(chunk, bytes); error = value.error; break; }
        case 0x08: { auto value = rws::decode_material_list(chunk, bytes); error = value.error; break; }
        case 0x09: { auto value = rws::decode_world_sector(chunk, bytes); error = value.error; break; }
        case 0x0A: { auto value = rws::decode_plane_sector(chunk, bytes); error = value.error; break; }
        case 0x0B: { auto value = rws::decode_world(chunk, bytes); error = value.error; break; }
        case 0x0E: { auto value = rws::decode_frame_list(chunk, bytes); error = value.error; break; }
        case 0x0F: {
            auto value = rws::decode_geometry(chunk, bytes);
            error = value.error;
            if (value) {
                child_geometry_vertices = value.value->vertex_count;
                if (value.value->triangle_layout == rws::TriangleLayout::stream_order) ++stats.triangles_stream;
                else if (value.value->triangle_layout == rws::TriangleLayout::memory_order) ++stats.triangles_memory;
                else ++stats.triangles_ambiguous;
            }
            break;
        }
        case 0x10: { auto value = rws::decode_clump(chunk, bytes); error = value.error; break; }
        case 0x14: { auto value = rws::decode_atomic(chunk, bytes); error = value.error; break; }
        case 0x1F: { auto value = rws::decode_right_to_render(chunk, bytes); error = value.error; break; }
        case 0x11E: { auto value = rws::decode_hanim(chunk, bytes); error = value.error; break; }
        case 0x116: {
            if (!geometry_vertices) error = "Skin is not inside a decoded Geometry";
            else { auto value = rws::decode_skin(chunk, *geometry_vertices, bytes); error = value.error; }
            break;
        }
        case 0x11F: { auto value = rws::decode_user_data(chunk, bytes); error = value.error; break; }
        case 0x127: { auto value = rws::decode_anisotropy(chunk, bytes); error = value.error; break; }
        case 0x50E: { auto value = rws::decode_bin_mesh(chunk, bytes); error = value.error; break; }
        case 0x907: { auto value = rws::decode_physics_body_def(chunk, bytes); error = value.error; break; }
        case 0x909: { auto value = rws::decode_physics_ragdoll_def(chunk, bytes); error = value.error; break; }
        case 0xFFFFFF00U: {
            auto value = rws::decode_pyro_extension(chunk, enclosing_object_type, bytes); error = value.error; break;
        }
        case 0x07: { auto value = rws::decode_material(chunk, bytes); error = value.error; break; }
        default: handled = false; break;
        }
        if (handled) {
            if (error.empty()) ++stats.decoded;
            else {
                ++stats.failed;
                std::cerr << "typed error at 0x" << std::hex << chunk.offset << std::dec
                          << " (" << rws::chunk_name(chunk.type) << "): " << error << '\n';
            }
        }
        const auto child_owner = chunk.type == 0x03 ? enclosing_object_type : chunk.type;
        validate_types(chunk.children, bytes, stats, child_geometry_vertices, child_owner);
    }
}

void export_geometries(const std::vector<rws::Chunk>& chunks, const std::span<const std::byte> bytes,
                       const std::filesystem::path& directory, std::uint64_t& exported) {
    for (const auto& chunk : chunks) {
        if (chunk.type == 0x0F) {
            const auto geometry = rws::decode_geometry(chunk, bytes);
            if (!geometry) throw std::runtime_error("Cannot decode Geometry at offset " + std::to_string(chunk.offset) + ": " + geometry.error);
            std::ostringstream name;
            name << "geometry_" << std::hex << std::setw(8) << std::setfill('0') << chunk.offset << ".obj";
            rws::export_geometry_obj(*geometry.value, bytes, directory / name.str());
            ++exported;
        }
        export_geometries(chunk.children, bytes, directory, exported);
    }
}

} // namespace

int main(const int argc, char** argv) {
    if (argc < 2 || argc > 4) {
        std::cerr << "Usage: rws-info <file.rws> [--summary|--validate-types|--export-obj <directory>]\n";
        return 2;
    }
    try {
        const auto document = rws::Document::load(argv[1]);
        std::cout << argv[1] << ": " << document.bytes().size() << " bytes, "
                  << document.chunks().size() << " top-level chunks\n";
        if (!document.chunks().empty()) {
            const auto version = rws::decode_library_id(document.chunks().front().library_id);
            std::cout << "RenderWare " << version.major << '.' << version.minor << '.'
                      << version.revision << '.' << version.binary << " build " << version.build
                      << " (stamp 0x" << std::hex << document.chunks().front().library_id << std::dec << ")\n";
        }
        const auto mode = argc >= 3 ? std::string_view(argv[2]) : std::string_view{};
        if (mode == "--summary") {
            std::map<std::uint32_t, TypeStats> stats;
            collect_stats(document.chunks(), stats);
            std::cout << "type        name                                  count       payload bytes  truncated\n";
            for (const auto& [type, item] : stats) {
                std::cout << "0x" << std::hex << std::setw(8) << std::setfill('0') << type << std::dec
                          << std::setfill(' ') << "  " << std::left << std::setw(36) << rws::chunk_name(type)
                          << std::right << std::setw(9) << item.count << std::setw(20) << item.bytes
                          << std::setw(11) << item.truncated << '\n';
            }
        } else if (mode == "--validate-types") {
            ValidationStats stats;
            validate_types(document.chunks(), document.bytes(), stats);
            std::cout << "Typed structures decoded: " << stats.decoded << ", failed: " << stats.failed << '\n';
            std::cout << "Geometry triangle layouts: stream=" << stats.triangles_stream
                      << ", memory=" << stats.triangles_memory << ", ambiguous=" << stats.triangles_ambiguous << '\n';
            if (stats.failed != 0) return 3;
        } else if (mode == "--export-obj" && argc == 4) {
            const std::filesystem::path directory(argv[3]);
            std::filesystem::create_directories(directory);
            std::uint64_t exported{};
            export_geometries(document.chunks(), document.bytes(), directory, exported);
            std::cout << "Exported " << exported << " geometries to " << directory.string() << '\n';
        } else {
            print_chunks(document.chunks());
        }
        for (const auto& diagnostic : document.diagnostics()) {
            std::cerr << (diagnostic.severity == rws::Diagnostic::Severity::error ? "error" : "warning")
                      << " at 0x" << std::hex << diagnostic.offset << std::dec << ": "
                      << diagnostic.message << '\n';
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "rws-info: " << error.what() << '\n';
        return 1;
    }
}
