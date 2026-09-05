#include "rws/document.hpp"
#include "rws/decoded.hpp"

#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

void append_u32(std::vector<std::byte>& bytes, const std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
        bytes.push_back(static_cast<std::byte>((value >> shift) & 0xFFU));
    }
}

void append_header(std::vector<std::byte>& bytes, const std::uint32_t type,
                   const std::uint32_t size, const std::uint32_t stamp = 0x1C020037) {
    append_u32(bytes, type);
    append_u32(bytes, size);
    append_u32(bytes, stamp);
}

void append_f32(std::vector<std::byte>& bytes, const float value) {
    append_u32(bytes, std::bit_cast<std::uint32_t>(value));
}

} // namespace

int main() {
    {
        const auto version = rws::decode_library_id(0x1C020037);
        assert(version.encoded_version == 0x37002);
        assert(version.major == 3 && version.minor == 7 && version.revision == 0 && version.binary == 2);
        assert(version.build == 55);
    }
    {
        std::vector<std::byte> bytes;
        append_header(bytes, 0x10, 16);
        append_header(bytes, 0x01, 4);
        append_u32(bytes, 7);
        const auto document = rws::Document::from_bytes(std::move(bytes));
        assert(document.chunks().size() == 1);
        assert(document.chunks()[0].children.size() == 1);
        assert(document.chunks()[0].children[0].type == 0x01);
        assert(document.diagnostics().empty());
        const auto clump = rws::decode_clump(document.chunks()[0], document.bytes());
        assert(clump && clump.value->atomics == 7);
    }
    {
        std::vector<std::byte> bytes;
        append_header(bytes, 0x0B, 100);
        append_u32(bytes, 0);
        const auto document = rws::Document::from_bytes(std::move(bytes));
        assert(document.chunks().size() == 1);
        assert(document.chunks()[0].truncated);
        assert(!document.diagnostics().empty());
    }
    {
        constexpr std::uint32_t struct_size = 120;
        std::vector<std::byte> bytes;
        append_header(bytes, 0x0F, 12 + struct_size);
        append_header(bytes, 0x01, struct_size);
        append_u32(bytes, 0x12); // positions + normals
        append_u32(bytes, 1);    // triangles
        append_u32(bytes, 3);    // vertices
        append_u32(bytes, 1);    // morph targets
        for (int i = 0; i < 4; ++i) append_u32(bytes, 0); // one triangle
        append_f32(bytes, 0); append_f32(bytes, 0); append_f32(bytes, 0); append_f32(bytes, 1);
        append_u32(bytes, 1); append_u32(bytes, 1);
        for (int i = 0; i < 18; ++i) append_f32(bytes, 0); // positions + normals
        const auto document = rws::Document::from_bytes(std::move(bytes));
        const auto geometry = rws::decode_geometry(document.chunks()[0], document.bytes());
        assert(geometry);
        assert(geometry.value->vertex_count == 3);
        assert(geometry.value->triangle_count == 1);
        assert(geometry.value->computed_size == struct_size);
        assert(geometry.value->morph_targets[0].has_vertices);
        assert(geometry.value->morph_targets[0].has_normals);
    }
    {
        constexpr std::uint32_t struct_size = 68;
        std::vector<std::byte> bytes;
        append_header(bytes, 0x0F, 12 + struct_size);
        append_header(bytes, 0x01, struct_size);
        append_u32(bytes, 0x00020002); // positions + two explicit UV sets
        append_u32(bytes, 0);         // triangles
        append_u32(bytes, 1);         // vertices
        append_u32(bytes, 1);         // morph targets
        append_f32(bytes, 0.1F); append_f32(bytes, 0.2F); // UV1
        append_f32(bytes, 0.3F); append_f32(bytes, 0.4F); // UV2
        append_f32(bytes, 0); append_f32(bytes, 0); append_f32(bytes, 0); append_f32(bytes, 1);
        append_u32(bytes, 1); append_u32(bytes, 0);
        append_f32(bytes, 0); append_f32(bytes, 0); append_f32(bytes, 0);
        const auto document = rws::Document::from_bytes(std::move(bytes));
        const auto geometry = rws::decode_geometry(document.chunks()[0], document.bytes());
        assert(geometry && geometry.value->texcoord_sets == 2);
        assert(geometry.value->texcoord_offsets.size() == 2);
        assert(geometry.value->texcoord_offsets[1] - geometry.value->texcoord_offsets[0] == 8);
    }
    {
        std::vector<std::byte> bytes;
        append_header(bytes, 0x050E, 32);
        append_u32(bytes, 0); // flags: triangle list
        append_u32(bytes, 1); // mesh count
        append_u32(bytes, 3); // total indices
        append_u32(bytes, 3); // entry indices
        append_u32(bytes, 2); // material
        append_u32(bytes, 0); append_u32(bytes, 1); append_u32(bytes, 2);
        const auto document = rws::Document::from_bytes(std::move(bytes));
        const auto mesh = rws::decode_bin_mesh(document.chunks()[0], document.bytes());
        assert(mesh && mesh.value->meshes.size() == 1);
        assert(mesh.value->total_indices == 3);
        assert(mesh.value->meshes[0].material_index == 2);
    }
    {
        std::vector<std::byte> bytes;
        append_header(bytes, 0x011E, 32);
        append_u32(bytes, 0x100); append_u32(bytes, 42); append_u32(bytes, 1);
        append_u32(bytes, 3); append_u32(bytes, 36);
        append_u32(bytes, 7); append_u32(bytes, 0); append_u32(bytes, 3);
        const auto document = rws::Document::from_bytes(std::move(bytes));
        const auto hierarchy = rws::decode_hanim(document.chunks()[0], document.bytes());
        assert(hierarchy && hierarchy.value->nodes.size() == 1);
        assert(hierarchy.value->hierarchy_id == 42);
        assert(hierarchy.value->nodes[0].node_id == 7);
    }
    {
        std::vector<std::byte> bytes;
        append_header(bytes, 0x0116, 186);
        bytes.push_back(std::byte{2}); // bones
        bytes.push_back(std::byte{2}); // used bones
        bytes.push_back(std::byte{2}); // max weights
        bytes.push_back(std::byte{0});
        bytes.push_back(std::byte{0}); bytes.push_back(std::byte{1});
        for (int i = 0; i < 2; ++i) append_u32(bytes, 0x00000100U); // four packed indices per vertex
        for (int i = 0; i < 8; ++i) append_f32(bytes, i % 4 == 0 ? 1.0F : 0.0F);
        for (int i = 0; i < 32; ++i) append_f32(bytes, i % 5 == 0 ? 1.0F : 0.0F);
        append_u32(bytes, 1); append_u32(bytes, 0); append_u32(bytes, 0);
        const auto document = rws::Document::from_bytes(std::move(bytes));
        const auto skin = rws::decode_skin(document.chunks()[0], 2, document.bytes());
        assert(skin && skin.value->bone_count == 2 && skin.value->used_bones.size() == 2);
        assert(skin.value->vertex_count == 2 && skin.value->trailing_split_bytes == 0);
    }
    {
        std::vector<std::byte> bytes;
        append_header(bytes, 0x011F, 51);
        append_u32(bytes, 2);
        append_u32(bytes, 4); bytes.push_back(std::byte{'i'}); bytes.push_back(std::byte{'d'});
        bytes.push_back(std::byte{'\0'}); bytes.push_back(std::byte{'\0'});
        append_u32(bytes, 1); append_u32(bytes, 1); append_u32(bytes, 42);
        append_u32(bytes, 6); bytes.push_back(std::byte{'l'}); bytes.push_back(std::byte{'a'});
        bytes.push_back(std::byte{'b'}); bytes.push_back(std::byte{'e'}); bytes.push_back(std::byte{'l'});
        bytes.push_back(std::byte{'\0'});
        append_u32(bytes, 3); append_u32(bytes, 1);
        append_u32(bytes, 5); bytes.push_back(std::byte{'t'}); bytes.push_back(std::byte{'e'});
        bytes.push_back(std::byte{'s'}); bytes.push_back(std::byte{'t'}); bytes.push_back(std::byte{'\0'});
        const auto document = rws::Document::from_bytes(std::move(bytes));
        const auto data = rws::decode_user_data(document.chunks()[0], document.bytes());
        assert(data && data.value->arrays.size() == 2);
        assert(data.value->arrays[0].integers[0] == 42);
        assert(data.value->arrays[1].strings[0] == "test");
    }
    {
        std::vector<std::byte> payload;
        append_u32(payload, 2); // material schema version
        append_u32(payload, 1); // optional metadata record present
        for (const auto value : {1U, 0U, 4U, 7U, 2U}) append_u32(payload, value);
        append_u32(payload, 7);
        for (const char character : std::string("Cemento"))
            payload.push_back(static_cast<std::byte>(character));
        std::vector<std::byte> bytes;
        append_header(bytes, 0xFFFFFF00U, static_cast<std::uint32_t>(payload.size()));
        bytes.insert(bytes.end(), payload.begin(), payload.end());
        const auto document = rws::Document::from_bytes(std::move(bytes));
        const auto metadata = rws::decode_pyro_extension(document.chunks()[0], 0x07, document.bytes());
        assert(metadata && metadata.value->version == 2 && metadata.value->present);
        assert(metadata.value->words.size() == 6 && metadata.value->strings[0] == "Cemento");
    }
    {
        std::vector<std::byte> payload;
        append_u32(payload, 0x18); append_u32(payload, 0x00010017);
        append_u32(payload, 0x0B); append_u32(payload, 3); append_u32(payload, 0x11);
        append_u32(payload, 0x11); append_u32(payload, 5); append_f32(payload, 3.0F);
        append_u32(payload, 5); append_f32(payload, 5.75F);
        append_u32(payload, 8); for (int i = 0; i < 12; ++i) append_f32(payload, i % 5 == 0 ? 1.0F : 0.0F);
        for (float value : {0.0F, 0.2F, 0.2F}) { append_u32(payload, 5); append_f32(payload, value); }
        append_u32(payload, 1); payload.push_back(std::byte{0}); payload.push_back(std::byte{0});
        append_u32(payload, 1); payload.push_back(std::byte{0}); payload.push_back(std::byte{0});
        append_u32(payload, 5); append_f32(payload, 3.0F);
        append_u32(payload, 9); append_u32(payload, 6);
        append_f32(payload, 1); append_f32(payload, 2); append_f32(payload, 3);
        append_u32(payload, 7); append_f32(payload, 0); append_f32(payload, 0); append_f32(payload, 0); append_f32(payload, 1);
        for (float value : {1.0F, 0.0F, 0.0F}) { append_u32(payload, 5); append_f32(payload, value); }
        append_u32(payload, 6); append_f32(payload, 1); append_f32(payload, 0); append_f32(payload, 0);
        append_u32(payload, 3); append_u32(payload, 2);
        append_u32(payload, 6); append_f32(payload, -1); append_f32(payload, 0.25F); append_f32(payload, 0.5F);
        std::vector<std::byte> bytes;
        append_header(bytes, 0x907, static_cast<std::uint32_t>(12 + payload.size()), 0x1C020018);
        append_header(bytes, 1, static_cast<std::uint32_t>(payload.size()), 0x1C020018);
        bytes.insert(bytes.end(), payload.begin(), payload.end());
        const auto document = rws::Document::from_bytes(std::move(bytes));
        const auto body = rws::decode_physics_body_def(document.chunks()[0], document.bytes());
        assert(body && body.value->volume.kind == 0x11 && body.value->mass == 3.0F);
        assert(body.value->principal_inertia.x == 1.0F && body.value->flags == 2);
        assert(body.value->center_of_mass.x == -1.0F);
    }
    return 0;
}
