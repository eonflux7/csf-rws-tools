# Repository guide

- This is a C++20/CMake project targeting Visual Studio Build Tools 2026 (18.9.2), x64.
- From the repository root, use `./build.ps1` and `./test.ps1`; both default to Release. Use `-Config Debug` when needed.
- `./build.ps1 -Target rws-man` builds only the GUI and its dependencies. `-CoreOnly` uses `build-core` without GLFW, ImGui, or OpenGL.
- CMake presets own generator, build-directory, and test configuration. Keep scripts, presets, and README commands consistent when changing the build.
- Do not edit or commit generated content under `build*`, `_deps`, or Python `__pycache__` directories.
- `rws_core` contains parsing/export logic; `rws-man` is the GUI; `rws-info` and `rws-corpus` are console tools.
- Tests belong in `tests/document_tests.cpp`. Run `./test.ps1` after changes to parser, decoder, or export behavior.
- Treat any locally available unpacked game resources as read-only reference data and do not copy them into the repository.
- Preserve unknown/truncated RWS data and write modified assets to new files unless the user explicitly requests an overwrite.
- The worktree may contain ongoing user changes; preserve unrelated modifications.
