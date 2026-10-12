#ifdef __vita__
#include "platform/graphics/vita_compiler.h"
#include <psp2/gxm.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/modulemgr.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
namespace port::graphics {
namespace {
// Public callback ABI has no cookie: serialized startup compilation on game
// thread only. No filesystem or shader includes exposed.
SceShaccCgSourceFile current_source{};
SceShaccCgSourceFile* open_source(const char* name, const SceShaccCgSourceLocation*,
                                const SceShaccCgCompileOptions*, const char** error) {
    if (name && std::strcmp(name, current_source.fileName) == 0) return &current_source;
    if (error) *error = "Shader includes are unsupported";
    return nullptr;
}
}
VitaCompiler::~VitaCompiler() {
    if (initialized_) sceShaccCgReleaseCompiler();
    if (module_ >= 0) sceKernelStopUnloadModule(module_, 0, nullptr, 0, nullptr, nullptr);
}
bool VitaCompiler::initialize(std::string& error) {
    if (module_ >= 0) { error = "Compiler already initialized"; return false; }
    constexpr const char* kPath = "ur0:/data/libshacccg.suprx";
    SceIoStat info{};
    if (sceIoGetstat(kPath, &info) < 0 || info.st_size <= 0) {
        error = "Missing ur0:data/libshacccg.suprx. Install the Vita runtime shader compiler "
                "on your device, then retry. It is not bundled in this VPK."; return false;
    }
    module_ = sceKernelLoadStartModule(kPath, 0, nullptr, 0, nullptr, nullptr);
    if (module_ < 0) {
        char message[160];
        std::snprintf(message, sizeof(message), "Cannot load %s: 0x%08X (requires a valid decrypted module)",
                      kPath, static_cast<unsigned>(module_)); error = message; return false;
    }
    if (sceShaccCgSetDefaultAllocator(std::malloc, std::free) < 0) {
        error = "Cannot initialize SceShaccCg allocator"; return false;
    }
    initialized_ = true; return true;
}
bool VitaCompiler::compile(const char* name, const char* source, SceShaccCgTargetProfile profile,
                           std::vector<uint32_t>& program, std::string& error) {
    if (!initialized_) { error = "Compiler not initialized"; return false; }
    current_source = {name, source, static_cast<SceUInt32>(std::strlen(source))};
    struct Reset { ~Reset() { current_source = {}; } } reset;
    SceShaccCgCompileOptions options{};
    if (sceShaccCgInitializeCompileOptions(&options) < 0) {
        error = "Cannot initialize SceShaccCg compile options"; return false;
    }
    options.mainSourceFile = name; options.entryFunctionName = "main";
    options.targetProfile = profile; options.optimizationLevel = 2; options.useFx = 1;
    options.useFastmath = options.useFastint = options.useFastprecision = 0;
    SceShaccCgCallbackList callbacks{};
    sceShaccCgInitializeCallbackList(&callbacks, SCE_SHACCCG_TRIVIAL); callbacks.openFile = open_source;
    using Output = std::unique_ptr<const SceShaccCgCompileOutput, decltype(&sceShaccCgDestroyCompileOutput)>;
    Output output(sceShaccCgCompileProgram(&options, &callbacks, 0), sceShaccCgDestroyCompileOutput);
    if (!output || !output->programData || !output->programSize) {
        error = std::string("Cannot compile ") + name;
        if (output && output->diagnostics) {
            for (int i = 0; i < output->diagnosticCount && i < 8 && error.size() < 1800; ++i)
                if (output->diagnostics[i].message)
                    error += "\n" + std::string(output->diagnostics[i].message).substr(0, 200);
        }
        return false;
    }
    if (sceGxmProgramCheck(reinterpret_cast<const SceGxmProgram*>(output->programData)) < 0) {
        error = std::string("GXM rejected ") + name; return false;
    }
    program.assign((output->programSize + 3) / 4, 0);
    std::memcpy(program.data(), output->programData, output->programSize);
    return true;
}
}
#endif
