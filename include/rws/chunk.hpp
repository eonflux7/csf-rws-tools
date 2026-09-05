#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace rws {

struct Chunk {
    std::uint32_t type{};
    std::uint32_t declared_size{};
    std::uint32_t library_id{};
    std::uint64_t offset{};
    std::uint64_t payload_offset{};
    std::uint64_t available_size{};
    bool truncated{};
    std::vector<Chunk> children;
};

[[nodiscard]] std::string_view chunk_name(std::uint32_t type) noexcept;
[[nodiscard]] bool is_container_chunk(std::uint32_t type) noexcept;
[[nodiscard]] constexpr std::uint32_t chunk_vendor_id(const std::uint32_t type) noexcept {
    return (type >> 8U) & 0x00FFFFFFU;
}
[[nodiscard]] constexpr std::uint8_t chunk_object_id(const std::uint32_t type) noexcept {
    return static_cast<std::uint8_t>(type & 0xFFU);
}
[[nodiscard]] std::string_view chunk_vendor_name(std::uint32_t vendor) noexcept;

} // namespace rws
