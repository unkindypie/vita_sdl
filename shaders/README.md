# Shader sources

- `gl/*.glsl`: OpenGL 2.1 / GLSL 1.20 entry points.
- `gxm/*.cg`: native Vita GXM entry points compiled once at graphics startup.
- `common/*.cg`: shared material/ASCII functions, also included by GLSL wrappers
  with type/function aliases. Keep addressing and palette behavior identical.

`cmake/embed_shaders.cmake` expands local includes and produces the ignored
`build/*/generated/shaders.h`. Sources are embedded in the executable: the VPK
does not depend on an external shader directory. CMake tracks shader changes as
configure dependencies. Edit these files, never the generated C++ header.

Vita still requires its separately installed shader compiler module; embedding
source does not bundle that module. Validate changes with `make test`,
`make gpu-test`, `make vita`, and the native Vita3K GPU fixtures.
