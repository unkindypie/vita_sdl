#include "shaders.h"
#ifdef __vita__
#include "platform/graphics/device.h"
#include "platform/graphics/vita_compiler.h"
#include <psp2/gxm.h>
#include <psp2/display.h>
#include <psp2/kernel/sysmem.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace port::graphics {
namespace {
bool checked(int status, const char *operation, std::string &error) {
    if (status >= 0)
        return true;
    char message[160];
    std::snprintf(message, sizeof(message), "%s: 0x%08X", operation, static_cast<unsigned>(status));
    error = message;
    return false;
}
enum class Mapping { Data, VertexCode, FragmentCode };
struct Memory {
    SceUID uid = -1;
    void *data = nullptr;
    uint32_t offset = 0;
    Mapping mapping = Mapping::Data;
    bool mapped = false;
    Memory() = default;
    Memory(const Memory &) = delete;
    Memory &operator=(const Memory &) = delete;
    ~Memory() {
        reset();
    }
    void swap(Memory &other) noexcept {
        std::swap(uid, other.uid);
        std::swap(data, other.data);
        std::swap(offset, other.offset);
        std::swap(mapping, other.mapping);
        std::swap(mapped, other.mapped);
    }
    void reset() {
        if (mapped) {
            if (mapping == Mapping::VertexCode)
                sceGxmUnmapVertexUsseMemory(data);
            else if (mapping == Mapping::FragmentCode)
                sceGxmUnmapFragmentUsseMemory(data);
            else
                sceGxmUnmapMemory(data);
        }
        if (uid >= 0)
            sceKernelFreeMemBlock(uid);
        uid = -1;
        data = nullptr;
        mapped = false;
    }
    bool allocate(uint32_t bytes, Mapping kind, std::string &error, bool cdram = false) {
        reset();
        mapping = kind;
        const uint32_t alignment = cdram ? 256 * 1024 : 4096;
        const uint32_t aligned = (bytes + alignment - 1) & ~(alignment - 1);
        uid = sceKernelAllocMemBlock("engine_gpu",
                                     cdram ? SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW
                                           : SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
                                     aligned, nullptr);
        if (!checked(uid, "GPU memory allocation", error) ||
            !checked(sceKernelGetMemBlockBase(uid, &data), "GPU memory base", error))
            return false;
        int status;
        if (kind == Mapping::VertexCode)
            status = sceGxmMapVertexUsseMemory(data, aligned, &offset);
        else if (kind == Mapping::FragmentCode)
            status = sceGxmMapFragmentUsseMemory(data, aligned, &offset);
        else
            status = sceGxmMapMemory(data, aligned,
                                     static_cast<SceGxmMemoryAttribFlags>(SCE_GXM_MEMORY_ATTRIB_READ |
                                                                          SCE_GXM_MEMORY_ATTRIB_WRITE));
        mapped = status >= 0;
        return checked(status, "GPU memory mapping", error);
    }
};
void *host_alloc(void *, uint32_t size) {
    return std::malloc(size);
}
void host_free(void *, void *memory) {
    std::free(memory);
}
struct DisplayFrame {
    void *address;
    bool paced;
};
void display_callback(const void *data) {
    const auto *frame = static_cast<const DisplayFrame *>(data);
    SceDisplayFrameBuf buffer{};
    buffer.size = sizeof(buffer);
    buffer.base = frame->address;
    buffer.pitch = 960;
    buffer.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
    buffer.width = 960;
    buffer.height = 544;
    sceDisplaySetFrameBuf(&buffer, SCE_DISPLAY_SETBUF_NEXTFRAME);
    if (frame->paced)
        sceDisplayWaitVblankStart();
}
struct Runtime {
    bool initialized = false;
    std::vector<uint32_t> vertex_code, fragment_code, blit_code, ascii_code, style_code;
    ~Runtime() {
        if (initialized)
            sceGxmTerminate();
    }
    bool initialize(std::string &error) {
        // Compile once per graphics runtime, not per target/device recreation.
        // Immutable owned GXP storage outlives every patcher using this runtime.
        {
            VitaCompiler compiler;
            if (!compiler.initialize(error) ||
                !compiler.compile("geometry_vertex.cg", shaders::gxm_geometry_vert_cg, SCE_SHACCCG_PROFILE_VP,
                                  vertex_code, error) ||
                !compiler.compile("geometry_fragment.cg", shaders::gxm_geometry_frag_cg,
                                  SCE_SHACCCG_PROFILE_FP, fragment_code, error) ||
                !compiler.compile("present_fragment.cg", shaders::gxm_present_frag_cg, SCE_SHACCCG_PROFILE_FP,
                                  blit_code, error) ||
                !compiler.compile("ascii_fragment.cg", shaders::gxm_ascii_frag_cg, SCE_SHACCCG_PROFILE_FP,
                                  ascii_code, error) ||
                !compiler.compile("style_fragment.cg", shaders::gxm_style_frag_cg, SCE_SHACCCG_PROFILE_FP,
                                  style_code, error))
                return false;
        }
        SceGxmInitializeParams init{};
        init.displayQueueMaxPendingCount = 2;
        init.displayQueueCallback = display_callback;
        init.displayQueueCallbackDataSize = sizeof(DisplayFrame);
        init.parameterBufferSize = SCE_GXM_DEFAULT_PARAMETER_BUFFER_SIZE;
        initialized = checked(sceGxmInitialize(&init), "GXM initialize", error);
        return initialized;
    }
};
class GxmDevice final : public Device {
    std::shared_ptr<Runtime> runtime_;
    SceGxmContext *context_ = nullptr;
    SceGxmRenderTarget *target_ = nullptr;
    SceGxmRenderTarget *display_target_ = nullptr;
    SceGxmShaderPatcher *patcher_ = nullptr;
    SceGxmShaderPatcherId vertex_id_ = nullptr, fragment_id_ = nullptr;
    SceGxmShaderPatcherId blit_id_ = nullptr;
    SceGxmShaderPatcherId ascii_id_ = nullptr;
    SceGxmFragmentProgram *ascii_program_ = nullptr;
    SceGxmShaderPatcherId style_id_ = nullptr;
    SceGxmFragmentProgram *style_program_ = nullptr;
    SceGxmRenderTarget *style_target_ = nullptr;
    Memory style_memory_;
    SceGxmColorSurface style_color_{};
    SceGxmTexture style_texture_{};
    const SceGxmProgramParameter *style_layout_ = nullptr;
    const SceGxmProgramParameter *style_clock_ = nullptr;
    unsigned styles_slot_ = 6;
    std::array<unsigned, 5> style_slots_{};
    const SceGxmProgramParameter *ascii_layout_ = nullptr;
    const SceGxmProgramParameter *ascii_clock_ = nullptr;
    Memory material_memory_, font_memory_;
    SceGxmTexture material_texture_{}, font_texture_{};
    bool ascii_enabled_ = false;
    Memory surface_memory_;
    SceGxmTexture surface_texture_{};
    bool surface_enabled_ = false;
    int surface_width_ = 0, surface_height_ = 0, surface_stride_ = 0;
    unsigned scene_slot_ = 0, material_slot_ = 1, font_slot_ = 2;
    unsigned overlay_slot_ = 3, underlay_slot_ = 4, reflection_slot_ = 5;
    Memory overlay_memory_, underlay_memory_, reflection_memory_, dynamic_vertices_, dynamic_indices_;
    SceGxmTexture overlay_texture_{}, underlay_texture_{}, reflection_texture_{};
    uint32_t dynamic_count_ = 0;
    std::vector<uint8_t> overlay_pixels_;
    SceGxmVertexProgram *vertex_program_ = nullptr;
    SceGxmFragmentProgram *fragment_program_ = nullptr;
    SceGxmFragmentProgram *blit_program_ = nullptr;
    const SceGxmProgramParameter *matrix_ = nullptr;
    const SceGxmProgramParameter *normals_ = nullptr;
    const SceGxmProgramParameter *plane_ = nullptr;
    const SceGxmProgramParameter *light_ = nullptr;
    std::unique_ptr<void, decltype(&std::free)> host_{nullptr, std::free};
    Memory vdm_, vertex_ring_, fragment_ring_, fragment_usse_;
    Memory patch_data_, patch_vertex_, patch_fragment_;
    Memory color_memory_, depth_memory_, vertices_, indices_, clear_vertices_, clear_indices_, readback_;
    Memory blit_vertices_, blit_indices_;
    Memory display_readback_;
    std::array<Memory, 3> display_memory_;
    std::array<SceGxmColorSurface, 3> display_color_{};
    std::array<SceGxmSyncObject *, 3> display_sync_{};
    SceGxmTexture color_texture_{};
    unsigned front_ = 0, back_ = 1;
    SceGxmColorSurface color_{};
    SceGxmDepthStencilSurface depth_{};
    int width_, height_, stride_;
    uint32_t count_ = 0;
    std::vector<engine::GeometryRange> ranges_;
    bool draw(const void *vertices, const void *indices, uint32_t count, const engine::Matrix &matrix,
              std::string &error, const std::array<float, 4> &plane = {},
              const engine::Matrix &normals = engine::identity_matrix()) {
        void *uniforms = nullptr;
        if (!checked(sceGxmReserveVertexDefaultUniformBuffer(context_, &uniforms), "GXM uniforms", error) ||
            !checked(sceGxmSetUniformDataF(uniforms, matrix_, 0, 16, matrix.data()), "GXM matrix", error) ||
            !checked(sceGxmSetUniformDataF(uniforms, normals_, 0, 16, normals.data()), "GXM normals",
                     error) ||
            !checked(sceGxmSetUniformDataF(uniforms, plane_, 0, 4, plane.data()), "GXM clip plane", error) ||
            !checked(sceGxmSetUniformDataF(uniforms, light_, 0, 4, pass_.light.data()), "GXM lighting",
                     error) ||
            !checked(sceGxmSetVertexStream(context_, 0, vertices), "GXM vertex stream", error))
            return false;
        return checked(
            sceGxmDraw(context_, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U16, indices, count),
            "GXM draw", error);
    }

  public:
    GxmDevice(std::shared_ptr<Runtime> runtime, int width, int height)
        : runtime_(std::move(runtime)), width_(width), height_(height), stride_((width + 7) & ~7) {}
    ~GxmDevice() override {
        if (context_)
            sceGxmFinish(context_);
        if (runtime_->initialized)
            sceGxmDisplayQueueFinish();
        if (patcher_) {
            if (ascii_program_)
                sceGxmShaderPatcherReleaseFragmentProgram(patcher_, ascii_program_);
            if (ascii_id_)
                sceGxmShaderPatcherUnregisterProgram(patcher_, ascii_id_);
            if (style_program_)
                sceGxmShaderPatcherReleaseFragmentProgram(patcher_, style_program_);
            if (style_id_)
                sceGxmShaderPatcherUnregisterProgram(patcher_, style_id_);
            if (blit_program_)
                sceGxmShaderPatcherReleaseFragmentProgram(patcher_, blit_program_);
            if (fragment_program_)
                sceGxmShaderPatcherReleaseFragmentProgram(patcher_, fragment_program_);
            if (vertex_program_)
                sceGxmShaderPatcherReleaseVertexProgram(patcher_, vertex_program_);
            if (fragment_id_)
                sceGxmShaderPatcherUnregisterProgram(patcher_, fragment_id_);
            if (vertex_id_)
                sceGxmShaderPatcherUnregisterProgram(patcher_, vertex_id_);
            if (blit_id_)
                sceGxmShaderPatcherUnregisterProgram(patcher_, blit_id_);
            sceGxmShaderPatcherDestroy(patcher_);
        }
        if (target_)
            sceGxmDestroyRenderTarget(target_);
        if (display_target_)
            sceGxmDestroyRenderTarget(display_target_);
        if (style_target_)
            sceGxmDestroyRenderTarget(style_target_);
        if (context_)
            sceGxmDestroyContext(context_);
        // Unmap every allocation before terminating GXM, including partial init.
        indices_.reset();
        vertices_.reset();
        clear_indices_.reset();
        clear_vertices_.reset();
        display_readback_.reset();
        readback_.reset();
        depth_memory_.reset();
        color_memory_.reset();
        blit_vertices_.reset();
        blit_indices_.reset();
        material_memory_.reset();
        font_memory_.reset();
        surface_memory_.reset();
        overlay_memory_.reset();
        underlay_memory_.reset();
        dynamic_vertices_.reset();
        dynamic_indices_.reset();
        reflection_memory_.reset();
        style_memory_.reset();
        for (size_t i = 0; i < display_memory_.size(); ++i) {
            if (display_sync_[i])
                sceGxmSyncObjectDestroy(display_sync_[i]);
            display_memory_[i].reset();
        }
        patch_fragment_.reset();
        patch_vertex_.reset();
        patch_data_.reset();
        fragment_usse_.reset();
        fragment_ring_.reset();
        vertex_ring_.reset();
        vdm_.reset();
    }
    const char *name() const override {
        return "GXM";
    }
    bool capture_reflection(std::string &error) override {
        // Preserve the rendered allocation rather than copying it through the
        // transfer engine. The next primary pass renders into the spare target.
        // Synchronization before target reuse is retained in render/set_dynamic.
        color_memory_.swap(reflection_memory_);
        std::swap(color_texture_, reflection_texture_);
        return checked(sceGxmColorSurfaceSetData(&color_, color_memory_.data), "reflection target rotation",
                       error);
    }
    bool set_dynamic(const engine::Geometry &geometry, std::string &error) override {
        if (geometry.vertices.empty() && geometry.indices.empty()) {
            dynamic_count_ = 0;
            return true;
        }
        if (!engine::valid_geometry(geometry, error))
            return false;
        sceGxmFinish(context_);
        if (!dynamic_vertices_.data &&
            !dynamic_vertices_.allocate(65536 * sizeof(engine::GeometryVertex), Mapping::Data, error))
            return false;
        if (!dynamic_indices_.data &&
            !dynamic_indices_.allocate(3 * 65536 * sizeof(uint16_t), Mapping::Data, error))
            return false;
        std::memcpy(dynamic_vertices_.data, geometry.vertices.data(),
                    geometry.vertices.size() * sizeof(engine::GeometryVertex));
        std::memcpy(dynamic_indices_.data, geometry.indices.data(),
                    geometry.indices.size() * sizeof(uint16_t));
        dynamic_count_ = geometry.indices.size();
        return true;
    }
    bool set_overlay(const engine::Cell *cells, int columns, int rows, std::string &error) override {
        if (!engine::pack_overlay(cells, columns, rows, overlay_pixels_)) {
            error = "Invalid overlay cells";
            return false;
        }
        sceGxmFinish(context_);
        std::memcpy(overlay_memory_.data, overlay_pixels_.data(), overlay_pixels_.size());
        return true;
    }
    bool set_surface(const engine::SurfaceTexture &surface, std::string &error) override {
        if (!surface.valid()) {
            error = "Invalid surface texture";
            return false;
        }
        sceGxmFinish(context_);
        // GXM linear textures round each row to eight pixels, even for a
        // smaller logical width. Source atlases remain tightly packed.
        const int stride = (surface.width + 7) & ~7;
        if (!surface_memory_.allocate(static_cast<size_t>(stride) * surface.height * 4, Mapping::Data, error, true))
            return false;
        for (int row = 0; row < surface.height; ++row)
            std::memcpy(static_cast<uint8_t *>(surface_memory_.data) + row * stride * 4,
                        surface.rgba.data() + row * surface.width * 4, surface.width * 4);
        if (!checked(sceGxmTextureInitLinear(&surface_texture_, surface_memory_.data,
                                             SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR, surface.width,
                                             surface.height, 0),
                     "Terrain surface", error))
            return false;
        sceGxmTextureSetMinFilter(&surface_texture_, SCE_GXM_TEXTURE_FILTER_POINT);
        sceGxmTextureSetMagFilter(&surface_texture_, SCE_GXM_TEXTURE_FILTER_POINT);
        sceGxmTextureSetUAddrMode(&surface_texture_, SCE_GXM_TEXTURE_ADDR_CLAMP);
        sceGxmTextureSetVAddrMode(&surface_texture_, SCE_GXM_TEXTURE_ADDR_CLAMP);
        surface_enabled_ = true;
        surface_width_ = surface.width;
        surface_height_ = surface.height;
        surface_stride_ = stride;
        return true;
    }
    bool update_surface(const engine::SurfaceTexture &source, int x, int y, int width, int height,
                        std::string &error) override {
        if (!source.valid() || source.width != surface_width_ || source.height != surface_height_ || x < 0 ||
            y < 0 || width < 1 || height < 1 || width > source.width - x || height > source.height - y) {
            error = "Invalid GPU surface update region";
            return false;
        }
        sceGxmFinish(context_);
        for (int row = y; row < y + height; ++row) {
            const size_t source_offset = (row * source.width + x) * 4;
            const size_t target_offset = (row * surface_stride_ + x) * 4;
            std::memcpy(static_cast<uint8_t *>(surface_memory_.data) + target_offset,
                        source.rgba.data() + source_offset, width * 4);
        }
        return true;
    }
    bool set_ascii(const engine::AsciiResources &resources, std::string &error) override {
        if ((width_ - 4) % 2 || (height_ - 4) % 2) {
            error = "ASCII target requires an even bordered grid";
            return false;
        }
        sceGxmFinish(context_);
        if (!ascii_program_) {
            const auto *code = reinterpret_cast<const SceGxmProgram *>(runtime_->ascii_code.data());
            if (!checked(sceGxmShaderPatcherRegisterProgram(patcher_, code, &ascii_id_), "ASCII register",
                         error) ||
                !checked(sceGxmShaderPatcherCreateFragmentProgram(
                             patcher_, ascii_id_, SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
                             SCE_GXM_MULTISAMPLE_NONE, nullptr,
                             reinterpret_cast<const SceGxmProgram *>(runtime_->vertex_code.data()),
                             &ascii_program_),
                         "ASCII fragment program", error))
                return false;
            ascii_layout_ = sceGxmProgramFindParameterByName(code, "layout");
            if (!ascii_layout_) {
                error = "Missing ASCII layout uniform";
                return false;
            }
            const char *names[] = {"scene", "font", "overlay", "styles"};
            unsigned *slots[] = {&scene_slot_, &font_slot_, &overlay_slot_, &styles_slot_};
            for (int i = 0; i < 4; ++i) {
                const auto *parameter = sceGxmProgramFindParameterByName(code, names[i]);
                if (!parameter) {
                    error = "Missing glyph sampler";
                    return false;
                }
                *slots[i] = sceGxmProgramParameterGetResourceIndex(parameter);
            }
            const auto *style_code = reinterpret_cast<const SceGxmProgram *>(runtime_->style_code.data());
            if (!checked(sceGxmShaderPatcherRegisterProgram(patcher_, style_code, &style_id_),
                         "Cell style register", error) ||
                !checked(sceGxmShaderPatcherCreateFragmentProgram(
                             patcher_, style_id_, SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
                             SCE_GXM_MULTISAMPLE_NONE, nullptr,
                             reinterpret_cast<const SceGxmProgram *>(runtime_->vertex_code.data()),
                             &style_program_),
                         "Cell style program", error))
                return false;
            style_layout_ = sceGxmProgramFindParameterByName(style_code, "layout");
            style_clock_ = sceGxmProgramFindParameterByName(style_code, "clock");
            const char *style_names[] = {"scene", "materials", "underlay", "reflection", "font"};
            for (unsigned i = 0; i < 5; ++i) {
                const auto *parameter = sceGxmProgramFindParameterByName(style_code, style_names[i]);
                if (!parameter) {
                    error = "Missing cell style sampler";
                    return false;
                }
                style_slots_[i] = sceGxmProgramParameterGetResourceIndex(parameter);
            }
            if (!style_layout_ || !style_clock_) {
                error = "Missing cell style uniforms";
                return false;
            }
            const int width = width_ - 4, height = (height_ - 4) / 2;
            if (width % 8) {
                error = "Native ASCII style width must be aligned to eight texels";
                return false;
            }
            if (!style_memory_.allocate(width * height * 4, Mapping::Data, error, true))
                return false;
            SceGxmRenderTargetParams target{};
            target.width = width;
            target.height = height;
            target.scenesPerFrame = 1;
            target.multisampleMode = SCE_GXM_MULTISAMPLE_NONE;
            target.driverMemBlock = -1;
            if (!checked(sceGxmCreateRenderTarget(&target, &style_target_), "Cell style target", error) ||
                !checked(sceGxmColorSurfaceInit(
                             &style_color_, SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR, SCE_GXM_COLOR_SURFACE_LINEAR,
                             SCE_GXM_COLOR_SURFACE_SCALE_NONE, SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT, width,
                             height, width, style_memory_.data),
                         "Cell style color", error) ||
                !checked(sceGxmTextureInitLinear(&style_texture_, style_memory_.data,
                                                 SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR, width, height, 0),
                         "Cell style texture", error))
                return false;
            sceGxmTextureSetMinFilter(&style_texture_, SCE_GXM_TEXTURE_FILTER_POINT);
            sceGxmTextureSetMagFilter(&style_texture_, SCE_GXM_TEXTURE_FILTER_POINT);
            sceGxmTextureSetUAddrMode(&style_texture_, SCE_GXM_TEXTURE_ADDR_CLAMP);
            sceGxmTextureSetVAddrMode(&style_texture_, SCE_GXM_TEXTURE_ADDR_CLAMP);
        }
        if (!material_memory_.allocate(resources.materials.size(), Mapping::Data, error, true) ||
            !font_memory_.allocate(resources.font.size(), Mapping::Data, error, true))
            return false;
        std::memcpy(material_memory_.data, resources.materials.data(), resources.materials.size());
        std::memcpy(font_memory_.data, resources.font.data(), resources.font.size());
        if (!checked(sceGxmTextureInitLinear(&material_texture_, material_memory_.data,
                                             SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR, 128, 256, 0),
                     "ASCII materials", error) ||
            !checked(sceGxmTextureInitLinear(&font_texture_, font_memory_.data,
                                             SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR, 128, 128, 0),
                     "ASCII font", error))
            return false;
        for (auto *texture : {&material_texture_, &font_texture_}) {
            sceGxmTextureSetMinFilter(texture, SCE_GXM_TEXTURE_FILTER_POINT);
            sceGxmTextureSetMagFilter(texture, SCE_GXM_TEXTURE_FILTER_POINT);
            sceGxmTextureSetUAddrMode(texture, SCE_GXM_TEXTURE_ADDR_CLAMP);
            sceGxmTextureSetVAddrMode(texture, SCE_GXM_TEXTURE_ADDR_CLAMP);
        }
        // Exact full-screen normalized coordinates for cell/pixel addressing.
        const engine::GeometryVertex quad[] = {{{-1, -1, 0}, {0, 1, 0, 1}},
                                               {{1, -1, 0}, {1, 1, 0, 1}},
                                               {{-1, 1, 0}, {0, 0, 0, 1}},
                                               {{1, 1, 0}, {1, 0, 0, 1}}};
        std::memcpy(blit_vertices_.data, quad, sizeof(quad));
        ascii_enabled_ = true;
        return true;
    }
    bool initialize(std::string &error) {
        if (!vdm_.allocate(SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE, Mapping::Data, error) ||
            !vertex_ring_.allocate(SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE, Mapping::Data, error) ||
            !fragment_ring_.allocate(SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE, Mapping::Data, error) ||
            !fragment_usse_.allocate(SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE, Mapping::FragmentCode,
                                     error))
            return false;
        host_.reset(std::malloc(SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE));
        if (!host_) {
            error = "GXM context host allocation failed";
            return false;
        }
        SceGxmContextParams context{};
        context.hostMem = host_.get();
        context.hostMemSize = SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE;
        context.vdmRingBufferMem = vdm_.data;
        context.vdmRingBufferMemSize = SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE;
        context.vertexRingBufferMem = vertex_ring_.data;
        context.vertexRingBufferMemSize = SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE;
        context.fragmentRingBufferMem = fragment_ring_.data;
        context.fragmentRingBufferMemSize = SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE;
        context.fragmentUsseRingBufferMem = fragment_usse_.data;
        context.fragmentUsseRingBufferMemSize = SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE;
        context.fragmentUsseRingBufferOffset = fragment_usse_.offset;
        if (!checked(sceGxmCreateContext(&context, &context_), "GXM context", error))
            return false;
        SceGxmRenderTargetParams target{};
        target.width = width_;
        target.height = height_;
        target.scenesPerFrame = 1;
        target.multisampleMode = SCE_GXM_MULTISAMPLE_NONE;
        target.driverMemBlock = -1;
        if (!checked(sceGxmCreateRenderTarget(&target, &target_), "GXM target", error))
            return false;
        target.width = 960;
        target.height = 544;
        if (!checked(sceGxmCreateRenderTarget(&target, &display_target_), "GXM display target", error))
            return false;
        const uint32_t depth_width = (width_ + SCE_GXM_TILE_SIZEX - 1) & ~(SCE_GXM_TILE_SIZEX - 1);
        const uint32_t depth_height = (height_ + SCE_GXM_TILE_SIZEY - 1) & ~(SCE_GXM_TILE_SIZEY - 1);
        if (!color_memory_.allocate(stride_ * height_ * 4, Mapping::Data, error, true) ||
            !depth_memory_.allocate(depth_width * depth_height * 4, Mapping::Data, error, true))
            return false;
        if (!checked(sceGxmColorSurfaceInit(&color_, SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR,
                                            SCE_GXM_COLOR_SURFACE_LINEAR, SCE_GXM_COLOR_SURFACE_SCALE_NONE,
                                            SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT, width_, height_, stride_,
                                            color_memory_.data),
                     "GXM color surface", error) ||
            !checked(sceGxmDepthStencilSurfaceInit(&depth_, SCE_GXM_DEPTH_STENCIL_FORMAT_S8D24,
                                                   SCE_GXM_DEPTH_STENCIL_SURFACE_TILED, depth_width,
                                                   depth_memory_.data, nullptr),
                     "GXM depth surface", error))
            return false;
        sceGxmDepthStencilSurfaceSetBackgroundDepth(&depth_, 1.0f);
        // Sprites use a second scene and must depth-test against terrain.
        // Tile-local depth is not guaranteed to reach backing memory unless
        // explicitly stored; emulators can conceal this hardware requirement.
        sceGxmDepthStencilSurfaceSetForceStoreMode(&depth_, SCE_GXM_DEPTH_STENCIL_FORCE_STORE_ENABLED);
        if (!checked(sceGxmTextureInitLinear(&color_texture_, color_memory_.data,
                                             SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR, stride_, height_, 0),
                     "GXM target texture", error))
            return false;
        sceGxmTextureSetMinFilter(&color_texture_, SCE_GXM_TEXTURE_FILTER_POINT);
        sceGxmTextureSetMagFilter(&color_texture_, SCE_GXM_TEXTURE_FILTER_POINT);
        sceGxmTextureSetUAddrMode(&color_texture_, SCE_GXM_TEXTURE_ADDR_CLAMP);
        sceGxmTextureSetVAddrMode(&color_texture_, SCE_GXM_TEXTURE_ADDR_CLAMP);
        if (!overlay_memory_.allocate(128 * 128 * 4, Mapping::Data, error, true) ||
            !underlay_memory_.allocate(stride_ * height_ * 4, Mapping::Data, error, true) ||
            !reflection_memory_.allocate(stride_ * height_ * 4, Mapping::Data, error, true))
            return false;
        std::memset(overlay_memory_.data, 0, 128 * 128 * 4);
        std::memset(underlay_memory_.data, 0, stride_ * height_ * 4);
        std::memset(reflection_memory_.data, 0, stride_ * height_ * 4);
        if (!checked(sceGxmTextureInitLinear(&overlay_texture_, overlay_memory_.data,
                                             SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR, 128, 128, 0),
                     "overlay texture", error) ||
            !checked(sceGxmTextureInitLinear(&underlay_texture_, underlay_memory_.data,
                                             SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR, stride_, height_, 0),
                     "underlay texture", error) ||
            !checked(sceGxmTextureInitLinear(&reflection_texture_, reflection_memory_.data,
                                             SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR, stride_, height_, 0),
                     "reflection texture", error))
            return false;
        for (auto *texture : {&overlay_texture_, &underlay_texture_, &reflection_texture_}) {
            sceGxmTextureSetMinFilter(texture, SCE_GXM_TEXTURE_FILTER_POINT);
            sceGxmTextureSetMagFilter(texture, SCE_GXM_TEXTURE_FILTER_POINT);
            sceGxmTextureSetUAddrMode(texture, SCE_GXM_TEXTURE_ADDR_CLAMP);
            sceGxmTextureSetVAddrMode(texture, SCE_GXM_TEXTURE_ADDR_CLAMP);
        }
        for (size_t i = 0; i < display_memory_.size(); ++i) {
            if (!display_memory_[i].allocate(960 * 544 * 4, Mapping::Data, error, true) ||
                !checked(sceGxmColorSurfaceInit(
                             &display_color_[i], SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR,
                             SCE_GXM_COLOR_SURFACE_LINEAR, SCE_GXM_COLOR_SURFACE_SCALE_NONE,
                             SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT, 960, 544, 960, display_memory_[i].data),
                         "GXM display surface", error) ||
                !checked(sceGxmSyncObjectCreate(&display_sync_[i]), "GXM display sync", error))
                return false;
        }
        constexpr uint32_t kPatchBytes = 64 * 1024;
        if (!patch_data_.allocate(kPatchBytes, Mapping::Data, error) ||
            !patch_vertex_.allocate(kPatchBytes, Mapping::VertexCode, error) ||
            !patch_fragment_.allocate(kPatchBytes, Mapping::FragmentCode, error))
            return false;
        SceGxmShaderPatcherParams patch{};
        patch.hostAllocCallback = host_alloc;
        patch.hostFreeCallback = host_free;
        patch.bufferMem = patch_data_.data;
        patch.bufferMemSize = kPatchBytes;
        patch.vertexUsseMem = patch_vertex_.data;
        patch.vertexUsseMemSize = kPatchBytes;
        patch.vertexUsseOffset = patch_vertex_.offset;
        patch.fragmentUsseMem = patch_fragment_.data;
        patch.fragmentUsseMemSize = kPatchBytes;
        patch.fragmentUsseOffset = patch_fragment_.offset;
        const auto *vp = reinterpret_cast<const SceGxmProgram *>(runtime_->vertex_code.data());
        const auto *fp = reinterpret_cast<const SceGxmProgram *>(runtime_->fragment_code.data());
        if (!checked(sceGxmShaderPatcherCreate(&patch, &patcher_), "GXM patcher", error) ||
            !checked(sceGxmShaderPatcherRegisterProgram(patcher_, vp, &vertex_id_), "GXM vertex registration",
                     error) ||
            !checked(sceGxmShaderPatcherRegisterProgram(patcher_, fp, &fragment_id_),
                     "GXM fragment registration", error) ||
            !checked(
                sceGxmShaderPatcherRegisterProgram(
                    patcher_, reinterpret_cast<const SceGxmProgram *>(runtime_->blit_code.data()), &blit_id_),
                "GXM present registration", error))
            return false;
        const auto *position = sceGxmProgramFindParameterByName(vp, "position");
        const auto *color = sceGxmProgramFindParameterByName(vp, "color");
        const auto *surface = sceGxmProgramFindParameterByName(vp, "surface");
        matrix_ = sceGxmProgramFindParameterByName(vp, "mvp");
        normals_ = sceGxmProgramFindParameterByName(vp, "normals");
        plane_ = sceGxmProgramFindParameterByName(vp, "plane");
        light_ = sceGxmProgramFindParameterByName(vp, "light");
        if (!position || !color || !matrix_ || !surface || !plane_ || !normals_ || !light_) {
            error = "GXM missing geometry parameters";
            return false;
        }
        SceGxmVertexAttribute attributes[3]{};
        attributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
        attributes[0].componentCount = 3;
        attributes[0].regIndex = sceGxmProgramParameterGetResourceIndex(position);
        attributes[1].offset = offsetof(engine::GeometryVertex, color);
        attributes[1].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
        attributes[1].componentCount = 4;
        attributes[1].regIndex = sceGxmProgramParameterGetResourceIndex(color);
        attributes[2].offset = offsetof(engine::GeometryVertex, surface);
        attributes[2].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
        attributes[2].componentCount = 4;
        attributes[2].regIndex = sceGxmProgramParameterGetResourceIndex(surface);
        SceGxmVertexStream stream{};
        stream.stride = sizeof(engine::GeometryVertex);
        stream.indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;
        if (!checked(sceGxmShaderPatcherCreateVertexProgram(patcher_, vertex_id_, attributes, 3, &stream, 1,
                                                            &vertex_program_),
                     "GXM vertex program", error) ||
            !checked(sceGxmShaderPatcherCreateFragmentProgram(
                         patcher_, fragment_id_, SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
                         SCE_GXM_MULTISAMPLE_NONE, nullptr, vp, &fragment_program_),
                     "GXM fragment program", error) ||
            !checked(sceGxmShaderPatcherCreateFragmentProgram(
                         patcher_, blit_id_, SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4, SCE_GXM_MULTISAMPLE_NONE,
                         nullptr, vp, &blit_program_),
                     "GXM present program", error))
            return false;
        if (!clear_vertices_.allocate(3 * sizeof(engine::GeometryVertex), Mapping::Data, error) ||
            !clear_indices_.allocate(3 * sizeof(uint16_t), Mapping::Data, error))
            return false;
        const engine::GeometryVertex clear[] = {{{-1, -1, 1}, {0.12f, 0.16f, 0.22f, 1}},
                                                {{3, -1, 1}, {0.12f, 0.16f, 0.22f, 1}},
                                                {{-1, 3, 1}, {0.12f, 0.16f, 0.22f, 1}}};
        const uint16_t indices[] = {0, 1, 2};
        std::memcpy(clear_vertices_.data, clear, sizeof(clear));
        std::memcpy(clear_indices_.data, indices, sizeof(indices));
        if (!blit_vertices_.allocate(4 * sizeof(engine::GeometryVertex), Mapping::Data, error) ||
            !blit_indices_.allocate(6 * sizeof(uint16_t), Mapping::Data, error))
            return false;
        // Keep samples inside the valid image, away from padded row storage.
        const float u0 = 0.5f / stride_, u1 = (width_ - 0.5f) / stride_;
        const float v0 = 0.5f / height_, v1 = (height_ - 0.5f) / height_;
        const engine::GeometryVertex quad[] = {{{-1, -1, 0}, {u0, v1, 0, 1}},
                                               {{1, -1, 0}, {u1, v1, 0, 1}},
                                               {{-1, 1, 0}, {u0, v0, 0, 1}},
                                               {{1, 1, 0}, {u1, v0, 0, 1}}};
        const uint16_t quad_indices[] = {0, 1, 2, 2, 1, 3};
        std::memcpy(blit_vertices_.data, quad, sizeof(quad));
        std::memcpy(blit_indices_.data, quad_indices, sizeof(quad_indices));
        // Shader compilers may hoist sampling out of the surface-kind branch.
        // Every declared sampler must have valid storage, even for plain mesh
        // fixtures that never select a terrain material.
        return set_surface(engine::SurfaceTexture{1, 1, {0, 0, 0, 255}}, error);
    }
    bool upload(const std::vector<engine::Geometry> &geometry, std::string &error) override {
        engine::PackedGeometry packed;
        if (!engine::pack_geometry(geometry, packed, error))
            return false;
        sceGxmFinish(context_);
        count_ = 0;
        if (!vertices_.allocate(packed.vertices.size() * sizeof(engine::GeometryVertex), Mapping::Data, error,
                                true) ||
            !indices_.allocate(packed.indices.size() * sizeof(uint16_t), Mapping::Data, error, true))
            return false;
        std::memcpy(vertices_.data, packed.vertices.data(),
                    packed.vertices.size() * sizeof(engine::GeometryVertex));
        std::memcpy(indices_.data, packed.indices.data(), packed.indices.size() * sizeof(uint16_t));
        count_ = packed.indices.size();
        ranges_ = std::move(packed.ranges);
        set_resources(geometry);
        return true;
    }
    bool render(const engine::Matrix &matrix, const std::vector<engine::GeometryInstance> &instances,
                std::string &error) override {
        if (!count_) {
            error = "Render before geometry upload";
            return false;
        }
        if (!engine::gather_visible_geometry(matrix, instances, bounds_, visible_, error))
            return false;
        // Single offscreen target: serialize reuse for the diagnostic. Production
        // presentation will use explicit sync objects and multiple frame slots.
        sceGxmFinish(context_);
        if (!checked(sceGxmBeginScene(context_, 0, target_, nullptr, nullptr, nullptr, &color_, &depth_),
                     "GXM begin", error))
            return false;
        sceGxmSetViewport(context_, width_ * 0.5f, width_ * 0.5f, height_ * 0.5f, -height_ * 0.5f, 0, 1);
        sceGxmSetCullMode(context_, SCE_GXM_CULL_NONE);
        sceGxmSetVertexProgram(context_, vertex_program_);
        sceGxmSetFragmentProgram(context_, fragment_program_);
        if (surface_enabled_ && !checked(sceGxmSetFragmentTexture(context_, 0, &surface_texture_),
                                         "Terrain surface bind", error)) {
            sceGxmEndScene(context_, nullptr, nullptr);
            return false;
        }
        sceGxmSetFrontDepthWriteEnable(context_, SCE_GXM_DEPTH_WRITE_ENABLED);
        sceGxmSetBackDepthWriteEnable(context_, SCE_GXM_DEPTH_WRITE_ENABLED);
        sceGxmSetFrontDepthFunc(context_, SCE_GXM_DEPTH_FUNC_ALWAYS);
        sceGxmSetBackDepthFunc(context_, SCE_GXM_DEPTH_FUNC_ALWAYS);
        bool ok = draw(clear_vertices_.data, clear_indices_.data, 3, engine::identity_matrix(), error);
        sceGxmSetFrontDepthFunc(context_, SCE_GXM_DEPTH_FUNC_LESS);
        sceGxmSetBackDepthFunc(context_, SCE_GXM_DEPTH_FUNC_LESS);
        for (const auto &item : visible_) {
            if (!ok)
                break;
            const auto &range = ranges_[item.geometry];
            ok = draw(static_cast<const engine::GeometryVertex *>(vertices_.data) + range.vertex_offset,
                      static_cast<const uint16_t *>(indices_.data) + range.index_offset, range.index_count,
                      item.mvp, error, clip_plane(item.model), engine::normal_matrix(item.model));
        }
        const int ended = sceGxmEndScene(context_, nullptr, nullptr);
        if (!ok || !checked(ended, "GXM end", error))
            return false;
        if (dynamic_count_ && pass_.dynamic) {
            sceGxmFinish(context_);
            if (!checked(sceGxmTransferCopy(width_, height_, 0, 0, SCE_GXM_TRANSFER_COLORKEY_NONE,
                                            SCE_GXM_TRANSFER_FORMAT_U8U8U8U8_ABGR, SCE_GXM_TRANSFER_LINEAR,
                                            color_memory_.data, 0, 0, stride_ * 4,
                                            SCE_GXM_TRANSFER_FORMAT_U8U8U8U8_ABGR, SCE_GXM_TRANSFER_LINEAR,
                                            underlay_memory_.data, 0, 0, stride_ * 4, nullptr, 0, nullptr),
                         "sprite underlay copy", error) ||
                !checked(sceGxmTransferFinish(), "sprite underlay finish", error))
                return false;
            sceGxmDepthStencilSurfaceSetForceLoadMode(&depth_, SCE_GXM_DEPTH_STENCIL_FORCE_LOAD_ENABLED);
            const int begin =
                sceGxmBeginScene(context_, 0, target_, nullptr, nullptr, nullptr, &color_, &depth_);
            sceGxmDepthStencilSurfaceSetForceLoadMode(&depth_, SCE_GXM_DEPTH_STENCIL_FORCE_LOAD_DISABLED);
            if (!checked(begin, "sprite scene begin", error))
                return false;
            sceGxmSetViewport(context_, width_ * 0.5f, width_ * 0.5f, height_ * 0.5f, -height_ * 0.5f, 0, 1);
            sceGxmSetVertexProgram(context_, vertex_program_);
            sceGxmSetFragmentProgram(context_, fragment_program_);
            sceGxmSetFrontDepthFunc(context_, SCE_GXM_DEPTH_FUNC_LESS_EQUAL);
            sceGxmSetBackDepthFunc(context_, SCE_GXM_DEPTH_FUNC_LESS_EQUAL);
            ok = draw(dynamic_vertices_.data, dynamic_indices_.data, dynamic_count_,
                      engine::identity_matrix(), error);
            const int finish = sceGxmEndScene(context_, nullptr, nullptr);
            if (!ok || !checked(finish, "sprite scene end", error))
                return false;
        }
        return true;
    }
    bool copy_pixels(void *source, Memory &destination, int width, int height, int stride,
                     std::vector<uint8_t> &rgba, std::string &error) {
        sceGxmFinish(context_);
        // Explicit GPU transfer makes readback observable even to emulators
        // that do not track ordinary CPU reads after a mapped address is reused.
        if (!destination.data && !destination.allocate(stride * height * 4, Mapping::Data, error))
            return false;
        if (!checked(sceGxmTransferCopy(width, height, 0, 0, SCE_GXM_TRANSFER_COLORKEY_NONE,
                                        SCE_GXM_TRANSFER_FORMAT_U8U8U8U8_ABGR, SCE_GXM_TRANSFER_LINEAR,
                                        source, 0, 0, stride * 4, SCE_GXM_TRANSFER_FORMAT_U8U8U8U8_ABGR,
                                        SCE_GXM_TRANSFER_LINEAR, destination.data, 0, 0, stride * 4, nullptr,
                                        0, nullptr),
                     "GXM diagnostic transfer", error) ||
            !checked(sceGxmTransferFinish(), "GXM diagnostic transfer finish", error))
            return false;
        rgba.resize(static_cast<size_t>(width) * height * 4);
        for (int y = 0; y < height; ++y)
            std::memcpy(rgba.data() + y * width * 4,
                        static_cast<const uint8_t *>(destination.data) + y * stride * 4, width * 4);
        return true;
    }
    bool readback(std::vector<uint8_t> &rgba, std::string &error) override {
        return copy_pixels(color_memory_.data, readback_, width_, height_, stride_, rgba, error);
    }
    bool readback_display(std::vector<uint8_t> &rgba, int &width, int &height, std::string &error) override {
        sceGxmDisplayQueueFinish();
        width = 960;
        height = 544;
        return copy_pixels(display_memory_[front_].data, display_readback_, width, height, width, rgba,
                           error);
    }
    bool present(std::string &error) override {
        const float layout[] = {static_cast<float>((width_ - 4) / 2), static_cast<float>((height_ - 4) / 2),
                                static_cast<float>(stride_), static_cast<float>(height_)};
        if (ascii_enabled_) {
            sceGxmFinish(context_);
            if (!checked(sceGxmBeginScene(context_, 0, style_target_, nullptr, nullptr, nullptr,
                                          &style_color_, nullptr),
                         "Cell style begin", error))
                return false;
            sceGxmSetViewport(context_, (width_ - 4) * 0.5F, (width_ - 4) * 0.5F, (height_ - 4) * 0.25F,
                              -(height_ - 4) * 0.25F, 0, 1);
            sceGxmSetCullMode(context_, SCE_GXM_CULL_NONE);
            sceGxmSetFrontDepthFunc(context_, SCE_GXM_DEPTH_FUNC_ALWAYS);
            sceGxmSetBackDepthFunc(context_, SCE_GXM_DEPTH_FUNC_ALWAYS);
            sceGxmSetFrontDepthWriteEnable(context_, SCE_GXM_DEPTH_WRITE_DISABLED);
            sceGxmSetBackDepthWriteEnable(context_, SCE_GXM_DEPTH_WRITE_DISABLED);
            sceGxmSetVertexProgram(context_, vertex_program_);
            sceGxmSetFragmentProgram(context_, style_program_);
            void *uniforms = nullptr;
            bool ok = checked(sceGxmReserveFragmentDefaultUniformBuffer(context_, &uniforms),
                              "Cell style uniforms", error) &&
                      checked(sceGxmSetUniformDataF(uniforms, style_layout_, 0, 4, layout),
                              "Cell style layout", error) &&
                      checked(sceGxmSetUniformDataF(uniforms, style_clock_, 0, 1, &pass_.time_seconds),
                              "Cell style clock", error);
            const SceGxmTexture *textures[] = {&color_texture_, &material_texture_, &underlay_texture_,
                                               &reflection_texture_, &font_texture_};
            for (unsigned i = 0; i < 5 && ok; ++i)
                ok = checked(sceGxmSetFragmentTexture(context_, style_slots_[i], textures[i]),
                             "Cell style texture", error);
            if (ok)
                ok = draw(blit_vertices_.data, blit_indices_.data, 6, engine::identity_matrix(), error);
            const int ended = sceGxmEndScene(context_, nullptr, nullptr);
            if (!ok || !checked(ended, "Cell style end", error))
                return false;
            sceGxmFinish(context_);
        }
        if (!checked(sceGxmBeginScene(context_, 0, display_target_, nullptr, nullptr, display_sync_[back_],
                                      &display_color_[back_], nullptr),
                     "GXM display begin", error))
            return false;
        sceGxmSetViewport(context_, 480, 480, 272, -272, 0, 1);
        sceGxmSetCullMode(context_, SCE_GXM_CULL_NONE);
        sceGxmSetFrontDepthFunc(context_, SCE_GXM_DEPTH_FUNC_ALWAYS);
        sceGxmSetBackDepthFunc(context_, SCE_GXM_DEPTH_FUNC_ALWAYS);
        sceGxmSetFrontDepthWriteEnable(context_, SCE_GXM_DEPTH_WRITE_DISABLED);
        sceGxmSetBackDepthWriteEnable(context_, SCE_GXM_DEPTH_WRITE_DISABLED);
        sceGxmSetVertexProgram(context_, vertex_program_);
        sceGxmSetFragmentProgram(context_, ascii_enabled_ ? ascii_program_ : blit_program_);
        if (ascii_enabled_) {
            void *uniforms = nullptr;
            if (!checked(sceGxmReserveFragmentDefaultUniformBuffer(context_, &uniforms), "ASCII uniforms",
                         error) ||
                !checked(sceGxmSetUniformDataF(uniforms, ascii_layout_, 0, 4, layout), "ASCII layout",
                         error) ||
                !checked(sceGxmSetFragmentTexture(context_, styles_slot_, &style_texture_), "Cell style bind",
                         error) ||
                !checked(sceGxmSetFragmentTexture(context_, font_slot_, &font_texture_), "ASCII font bind",
                         error) ||
                !checked(sceGxmSetFragmentTexture(context_, overlay_slot_, &overlay_texture_),
                         "ASCII overlay bind", error)) {
                sceGxmEndScene(context_, nullptr, nullptr);
                return false;
            }
        }
        bool ok =
            checked(sceGxmSetFragmentTexture(context_, ascii_enabled_ ? scene_slot_ : 0, &color_texture_),
                    "GXM present texture", error) &&
            draw(blit_vertices_.data, blit_indices_.data, 6, engine::identity_matrix(), error);
        const int ended = sceGxmEndScene(context_, nullptr, nullptr);
        if (!ok || !checked(ended, "GXM display end", error))
            return false;
        const DisplayFrame frame{display_memory_[back_].data, display_pacing_};
        if (!checked(sceGxmPadHeartbeat(&display_color_[back_], display_sync_[back_]), "GXM heartbeat",
                     error) ||
            !checked(sceGxmDisplayQueueAddEntry(display_sync_[front_], display_sync_[back_], &frame),
                     "GXM display queue", error))
            return false;
        front_ = back_;
        back_ = (back_ + 1) % display_memory_.size();
        return true;
    }
};
} // namespace
class GxmFactory final : public DeviceFactory {
    std::shared_ptr<Runtime> runtime_ = std::make_shared<Runtime>();

  public:
    bool initialize(std::string &error) {
        return runtime_->initialize(error);
    }
    std::unique_ptr<Device> create(int width, int height, std::string &error) override {
        if (width < 16 || height < 16 || width > 2048 || height > 2048) {
            error = "Invalid GPU target dimensions (16..2048)";
            return nullptr;
        }
        auto device = std::make_unique<GxmDevice>(runtime_, width, height);
        if (!device->initialize(error))
            return nullptr;
        return device;
    }
};
std::unique_ptr<DeviceFactory> create_device_factory(std::string &error) {
    auto factory = std::make_unique<GxmFactory>();
    if (!factory->initialize(error))
        return nullptr;
    return factory;
}
} // namespace port::graphics
#endif
