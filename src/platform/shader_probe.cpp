#include "shaders.h"
#include "platform/shader_probe.h"
#include "platform/engine_hooks.h"
#include "platform/graphics/vita_compiler.h"
#include <cstdio>
namespace port {
bool probe_vita_shader_compiler(std::string& error) {
#ifdef __vita__
    graphics::VitaCompiler compiler;
    std::vector<uint32_t> vertex, fragment;
    if (!compiler.initialize(error) ||
        !compiler.compile("geometry_vertex.cg", graphics::shaders::gxm_geometry_vert_cg, SCE_SHACCCG_PROFILE_VP, vertex, error) ||
        !compiler.compile("geometry_fragment.cg", graphics::shaders::gxm_probe_frag_cg, SCE_SHACCCG_PROFILE_FP, fragment, error))
        return false;
    char message[160];
    std::snprintf(message, sizeof(message), "GPU_SHADER_PROBE_OK vertex_bytes=%u fragment_bytes=%u GXM_program_check=pass",
                  static_cast<unsigned>(vertex.size() * 4), static_cast<unsigned>(fragment.size() * 4));
    log_stage(message); return true;
#else
    error = "--probe-gpu checks the native Vita shader compiler; run it on Vita/Vita3K, not Linux.";
    return false;
#endif
}
}
