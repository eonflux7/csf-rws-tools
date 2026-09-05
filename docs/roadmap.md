# Implementation roadmap

## Phase 1 — safe stream inventory (implemented)

- C++20 core library with bounds-checked chunk parsing.
- Known chunk names and conservative container traversal.
- Unknown chunk preservation and truncation diagnostics.
- `rws-info` tree/summary CLI.
- Dear ImGui + GLFW + OpenGL 3 shell with drag/drop, chunk tree, hex payload editing,
  and save-to-copy.
- Synthetic parser tests and validation against both ST05 samples.

## Phase 2 — typed standard RenderWare views (in progress)

- [x] Decode library stamps in the UI and CLI.
- [x] Frame List transforms and hierarchy data.
- [x] Geometry flags, morph targets, vertices, normals, prelights, UV sets, triangles,
  bounding spheres, and material indices.
- [x] Materials and Material List remaps.
- [x] Typed texture sampler state and texture/mask strings.
- [x] Atomic frame/geometry bindings.
- [x] World header, Plane Section, and Atomic Section layouts.
- [x] Array-bound/count invariants and synthetic tests.
- [ ] Golden-file fixtures small enough to commit.

Deliverable: render standard Clump geometry and the collision BSP while preserving
all extension payloads byte-for-byte.

## Phase 3 — CSF-specific schemas

- [x] Recursive corpus scanner grouped by root type, library stamp, chunk ID, size,
  truncation, and source path.
- [x] Decode standard Skin payloads using the owning Geometry vertex count.
- [x] Decode standard User Data arrays (integer, real, and string formats).
- [x] Decode the standard dual-pass Material Effects payloads used by CSF,
  including their embedded Texture stream, blend modes, and object pipeline flag.
- [x] Decode HAnim, Anisotropy, Right To Render, and Bin Mesh plugin payloads.
- [x] Cluster and decode `0xFFFFFF00` as owner-specific Pyro Atomic, Material,
  Frame, World Sector, and Light metadata.
- [x] Decode RenderWare Physics `0x907` Body Definitions and `0x909` Ragdoll
  Definitions using the readers recovered from `CommXPC.exe`.
- Recover final semantic names for the remaining conservative Body/Joint scalar
  fields and support `0x90B` Generic Definitions if samples are found.
- Decode the scene/instance tail beginning at `0x00DD9095` in the ST05 sample.
- Correlate length-prefixed names and record IDs with rendered clumps and in-game
  entities.
- Compare intact collision files to determine whether ST05's 1,944-byte deficit is
  file-specific or systematic.

Deliverable: named, evidence-tagged field schemas in this directory and structured
views in the GUI.

## Phase 4 — graphical preview (in progress)

- [x] Add an embedded, depth-tested OpenGL Geometry viewport with orbit/pan/zoom,
  automatic framing, material-index colors, solid/wireframe modes, and a display
  budget for large meshes.
- [x] Resolve Material texture references and preview external DXT1/DXT3 DDS files,
  with Textured, Material Index, Material Color, UV Checker, and Wireframe styles.
- [x] Add selectable geometry UV channels and a Lightmap UV checker that defaults
  to the conventional second UV set.
- [x] Resolve CSF's `*_Lm` textures from MatFX dual-pass material payloads and add
  lightmap-only and base-times-lightmap preview modes.
- Assemble Clump previews from Atomic geometry/frame indices and hierarchical frame
  transforms.
- Add selectable Physics primitive and ragdoll overlays.
- Decode embedded/native rasters for assets that do not use external DDS files.
- Assemble World Sector geometry after its vertex/triangle arrays are decoded.

## Phase 5 — extraction, rendering, and guarded editing

- Material/texture binding and vertex-normal visualization in the preview.
- World/collision visualization and object selection.
- [x] Export local-space geometry to OBJ as an initial interchange path.
- [ ] Export complete transformed scenes to glTF with materials and textures.
- Typed edits with range/count propagation and transactional undo/redo.
- Rebuild chunk sizes bottom-up, write to a new file, reopen it, and verify all
  boundaries before accepting the save.
- Optional batch CLI for extract/replace/validate operations.

## Reverse-engineering discipline

Each proposed field should record its byte offset, type, parent chunk, supporting
files, and confidence. A schema becomes editable only after at least two independent
samples agree and a serialize/reparse byte comparison passes. Unknown padding and
extensions remain verbatim by default.
