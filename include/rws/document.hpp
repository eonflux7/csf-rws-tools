#pragma once

#include "rws/chunk.hpp"
#include "rws/decoded.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <optional>
#include <vector>

namespace rws {

struct Diagnostic {
    enum class Severity { warning, error };
    Severity severity{Severity::warning};
    std::uint64_t offset{};
    std::string message;
};

// Commandos: Strike Force inserts these records between the streamed Clump
// prototypes and the map World. They are not ordinary RenderWare chunks even
// though each record embeds a standard Matrix/Struct pair.
struct SceneInstance {
    std::uint64_t offset{};
    std::uint32_t declared_size{};
    std::uint32_t prototype_id{};
    std::uint32_t instance_id{};
    std::array<float, 3> atomic_parameters{};
    std::uint32_t flags{};
    std::array<float, 9> rotation{};
    Vec3 position;
    std::uint32_t matrix_flags{};
    std::string prototype_name;
    std::uint64_t physical_size{};
};

class Document {
public:
    [[nodiscard]] static Document load(const std::filesystem::path& path);
    [[nodiscard]] static Document from_bytes(std::vector<std::byte> bytes);

    void save_as(const std::filesystem::path& path) const;
    void set_byte(std::uint64_t offset, std::byte value);

    [[nodiscard]] const std::filesystem::path& source_path() const noexcept { return source_path_; }
    [[nodiscard]] std::span<const std::byte> bytes() const noexcept { return bytes_; }
    [[nodiscard]] const std::vector<Chunk>& chunks() const noexcept { return chunks_; }
    [[nodiscard]] const std::vector<SceneInstance>& scene_instances() const noexcept {
        return scene_instances_;
    }
    [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const noexcept { return diagnostics_; }
    [[nodiscard]] bool dirty() const noexcept { return dirty_; }

private:
    void parse();
    [[nodiscard]] std::uint64_t parse_scene_instances(std::uint64_t begin);
    void parse_range(std::uint64_t begin, std::uint64_t end, std::vector<Chunk>& output,
                     unsigned depth, bool require_complete);

    std::filesystem::path source_path_;
    std::vector<std::byte> bytes_;
    std::vector<Chunk> chunks_;
    std::vector<SceneInstance> scene_instances_;
    std::vector<Diagnostic> diagnostics_;
    std::optional<std::uint32_t> stream_library_id_;
    bool dirty_{};
};

} // namespace rws
