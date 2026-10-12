#pragma once
#ifdef __vita__
#include <psp2/shacccg.h>
#include <string>
#include <vector>
#include <cstdint>
namespace port::graphics {
// Returned aligned storage must outlive shader-patcher registration.
class VitaCompiler {
    int module_ = -1;
    bool initialized_ = false;

  public:
    VitaCompiler() = default;
    VitaCompiler(const VitaCompiler &) = delete;
    VitaCompiler &operator=(const VitaCompiler &) = delete;
    ~VitaCompiler();
    bool initialize(std::string &error);
    bool compile(const char *name, const char *source, SceShaccCgTargetProfile,
                 std::vector<uint32_t> &program, std::string &error);
};
} // namespace port::graphics
#endif
