#include "rws/document.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace rws {
namespace {

constexpr std::uint64_t header_size = 12;
constexpr unsigned max_depth = 64;

std::uint32_t read_u32(const std::span<const std::byte> data, const std::uint64_t offset) {
    const auto i = static_cast<std::size_t>(offset);
    return std::to_integer<std::uint32_t>(data[i]) |
           (std::to_integer<std::uint32_t>(data[i + 1]) << 8U) |
           (std::to_integer<std::uint32_t>(data[i + 2]) << 16U) |
           (std::to_integer<std::uint32_t>(data[i + 3]) << 24U);
}

std::string at_offset(const std::string_view text, const std::uint64_t offset) {
    std::ostringstream out;
    out << text << " at 0x" << std::hex << offset;
    return out.str();
}

} // namespace

Document Document::load(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("Cannot open RWS file: " + path.string());
    }
    const auto end = input.tellg();
    if (end < 0) {
        throw std::runtime_error("Cannot determine RWS file size: " + path.string());
    }
    const auto size = static_cast<std::uint64_t>(end);
    if (size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error("RWS file is too large for this process");
    }
    Document result;
    result.source_path_ = path;
    result.bytes_.resize(static_cast<std::size_t>(size));
    input.seekg(0);
    input.read(reinterpret_cast<char*>(result.bytes_.data()), static_cast<std::streamsize>(size));
    if (!input && size != 0) {
        throw std::runtime_error("Cannot read complete RWS file: " + path.string());
    }
    result.parse();
    return result;
}

Document Document::from_bytes(std::vector<std::byte> bytes) {
    Document result;
    result.bytes_ = std::move(bytes);
    result.parse();
    return result;
}

void Document::save_as(const std::filesystem::path& path) const {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Cannot create output file: " + path.string());
    }
    output.write(reinterpret_cast<const char*>(bytes_.data()),
                 static_cast<std::streamsize>(bytes_.size()));
    if (!output) {
        throw std::runtime_error("Cannot write complete output file: " + path.string());
    }
}

void Document::set_byte(const std::uint64_t offset, const std::byte value) {
    if (offset >= bytes_.size()) {
        throw std::out_of_range("Byte offset is outside the document");
    }
    auto& current = bytes_[static_cast<std::size_t>(offset)];
    if (current != value) {
        current = value;
        dirty_ = true;
        parse();
    }
}

void Document::parse() {
    chunks_.clear();
    diagnostics_.clear();
    stream_library_id_.reset();
    if (bytes_.size() >= header_size) {
        stream_library_id_ = read_u32(bytes_, 8);
    }
    parse_range(0, bytes_.size(), chunks_, 0, true);

    // CSF map streams may place a short game-specific instance table between
    // the ordinary top-level Clumps and an embedded RenderWare World. Recover
    // only a strongly identifiable World suffix: matching library stamp,
    // leading Struct child, and a declared end at (or just beyond) physical EOF.
    // The small overrun is retained as truncation instead of rewriting the file.
    if (stream_library_id_ && bytes_.size() >= header_size * 2) {
        std::uint64_t scan_begin{};
        if (!chunks_.empty())
            scan_begin = chunks_.back().payload_offset + chunks_.back().available_size;
        for (std::uint64_t candidate = scan_begin;
             candidate + header_size * 2 <= bytes_.size(); ++candidate) {
            if (read_u32(bytes_, candidate) != 0x0B ||
                read_u32(bytes_, candidate + 8) != *stream_library_id_ ||
                read_u32(bytes_, candidate + header_size) != 0x01 ||
                read_u32(bytes_, candidate + header_size + 8) != *stream_library_id_)
                continue;
            const auto declared = static_cast<std::uint64_t>(read_u32(bytes_, candidate + 4));
            const auto physical = bytes_.size() - candidate - header_size;
            if (declared < physical || declared - physical > 4096) continue;
            std::vector<Chunk> recovered;
            parse_range(candidate, bytes_.size(), recovered, 0, false);
            if (recovered.size() == 1 && recovered.front().type == 0x0B) {
                chunks_.push_back(std::move(recovered.front()));
                diagnostics_.push_back({Diagnostic::Severity::warning, candidate,
                    "Recovered RenderWare World after CSF-specific instance records"});
            }
            break;
        }
    }
}

void Document::parse_range(const std::uint64_t begin, const std::uint64_t end,
                           std::vector<Chunk>& output, const unsigned depth,
                           const bool require_complete) {
    if (depth > max_depth) {
        diagnostics_.push_back({Diagnostic::Severity::error, begin, "Maximum chunk nesting exceeded"});
        return;
    }

    std::uint64_t cursor = begin;
    while (cursor < end) {
        const auto remaining = end - cursor;
        if (remaining < header_size) {
            if (require_complete && remaining != 0) {
                diagnostics_.push_back({Diagnostic::Severity::warning, cursor,
                    "Trailing bytes do not form a complete chunk header"});
            }
            return;
        }

        Chunk chunk;
        chunk.offset = cursor;
        chunk.payload_offset = cursor + header_size;
        chunk.type = read_u32(bytes_, cursor);
        chunk.declared_size = read_u32(bytes_, cursor + 4);
        chunk.library_id = read_u32(bytes_, cursor + 8);

        // Chunks produced by one RenderWare stream use one library stamp. Treat a
        // mismatch as opaque payload/trailing data rather than inventing a tree
        // from coincidental integer patterns inside game-specific binary data.
        if (stream_library_id_ && chunk.library_id != *stream_library_id_) {
            if (require_complete) {
                diagnostics_.push_back({Diagnostic::Severity::warning, cursor,
                    "Remaining bytes are not a chunk sequence with the stream library ID"});
            }
            return;
        }

        const auto available = end - chunk.payload_offset;
        chunk.available_size = std::min<std::uint64_t>(chunk.declared_size, available);
        chunk.truncated = chunk.declared_size > available;
        if (chunk.truncated) {
            diagnostics_.push_back({Diagnostic::Severity::warning, cursor,
                at_offset("Chunk payload is truncated", cursor)});
        }

        if (is_container_chunk(chunk.type) && chunk.available_size >= header_size) {
            parse_range(chunk.payload_offset, chunk.payload_offset + chunk.available_size,
                        chunk.children, depth + 1, false);
        }
        output.push_back(std::move(chunk));

        if (output.back().truncated) {
            return;
        }
        cursor = output.back().payload_offset + output.back().declared_size;
    }
}

} // namespace rws
