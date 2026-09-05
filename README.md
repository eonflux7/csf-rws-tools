# rws-man

`rws-man` is an early-stage C++20 viewer/editor and reverse-engineering workspace for
RenderWare Binary Stream (`.rws`) assets from *Commandos: Strike Force*.

Current capabilities:

- bounds-checked parsing of little-endian 12-byte RenderWare chunk headers;
- schema-aware trees for common RenderWare container chunks;
- preservation of unknown/custom chunks and malformed/truncated inputs;
- CLI tree and type summaries;
- Dear ImGui chunk browser, drag-and-drop loading, payload hex editing, and safe
  save-to-copy behavior;
- embedded depth-tested OpenGL Geometry preview with orbit/pan/zoom, external DXT1/DXT3 DDS
  textures, material debug colors, UV checker, lit-solid and wireframe modes,
  optional backface culling, and automatic framing;
- typed views for Clump, Frame List, Geometry, Material List/Material, Atomic,
  World, Plane Section, Atomic Section, Skin, HAnim, User Data, Bin Mesh, and
  other standard plugin structures;
- owner-aware Pyro Studios `0xFFFFFF00` metadata and RenderWare Physics Body/
  Ragdoll Definition decoding recovered from the game executable;
- Wavefront OBJ extraction with positions, UVs, normals, faces, and material groups.

The project intentionally does **not** claim full format support yet. See
[`docs/rws-format.md`](docs/rws-format.md) for confirmed findings and
[`docs/corpus-findings.md`](docs/corpus-findings.md) for whole-archive results. The
remaining work is tracked in [`docs/roadmap.md`](docs/roadmap.md).

## Build

Requirements are CMake 3.24+, a C++20 compiler, Git, and OpenGL. CMake fetches
pinned GLFW 3.4 and Dear ImGui 1.91.9b sources when the GUI is enabled.

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

To build only the dependency-free parser, CLI, and tests:

```powershell
cmake -S . -B build-core -DRWSMAN_BUILD_GUI=OFF
cmake --build build-core --config Release
```

## Run

```powershell
.\build\Release\rws-man.exe C:\path\to\asset.rws
.\build\Release\rws-info.exe C:\path\to\asset.rws --summary
.\build\Release\rws-info.exe C:\path\to\asset.rws --validate-types
.\build\Release\rws-info.exe C:\path\to\asset.rws --export-obj C:\output\folder
.\build\Release\rws-corpus.exe C:\path\to\extracted-game
```

An asset can also be dropped onto the GUI. The first Geometry is selected
automatically; selecting a Geometry or one of its child chunks opens the `3D Preview`
tab. Left-drag orbits, middle/right-drag pans, the wheel zooms, and double-clicking
frames the mesh. The view selector switches between Textured, Material index,
Material color, UV checker, and Wireframe modes. Texture references are resolved
against a sibling `Textures` directory and loaded from DXT1/DXT3 DDS files.
Selecting a Geometry also enables local-space OBJ export. `Save copy` writes
`<original>.edited.rws`; it never overwrites the source file.

## Safety model

Raw byte editing can still produce a game-invalid asset. Unknown bytes are retained
verbatim, ranges are checked before reads, and short chunks are clamped to physical
EOF. Work on copies and validate edited assets in a disposable game installation.
