#pragma once

#include "rws/decoded.hpp"

#include <cstddef>
#include <filesystem>
#include <span>

namespace rws {

// Exports one geometry in its local frame. Materials are emitted as symbolic
// `material_N` groups; texture image extraction is a separate concern.
void export_geometry_obj(const GeometryInfo& geometry, std::span<const std::byte> bytes,
                         const std::filesystem::path& output_path);

} // namespace rws

