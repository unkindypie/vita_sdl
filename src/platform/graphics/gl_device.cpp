#include "shaders.h"
#ifndef __vita__
#define GL_GLEXT_PROTOTYPES
#include <SDL.h>
#include <SDL_opengl.h>
#include "platform/graphics/device.h"
#include "engine/render/camera.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace port::graphics {
namespace {
#define GL_FUNCTIONS(X)                                                                                      \
    X(glGetError)                                                                                            \
    X(glGetString)                                                                                           \
    X(glGetIntegerv)                                                                                         \
    X(glGenBuffers)                                                                                          \
    X(glDeleteBuffers) X(glBindBuffer) X(glBufferData) X(glCreateShader) X(glShaderSource) X(                \
        glCompileShader) X(glGetShaderiv) X(glGetShaderInfoLog) X(glDeleteShader) X(glCreateProgram)         \
        X(glAttachShader) X(glBindAttribLocation) X(glLinkProgram) X(glGetProgramiv) X(glGetProgramInfoLog)  \
            X(glDeleteProgram) X(glUseProgram) X(glGetUniformLocation) X(glUniformMatrix4fv) X(glUniform1i)  \
                X(glUniform1f) X(glUniform4f) X(glActiveTexture) X(glGenFramebuffers) X(glBindFramebuffer)   \
                    X(glFramebufferTexture2D) X(glCheckFramebufferStatus) X(glDeleteFramebuffers)            \
                        X(glBlitFramebuffer) X(glGenRenderbuffers) X(glBindRenderbuffer)                     \
                            X(glRenderbufferStorage) X(glFramebufferRenderbuffer) X(glDeleteRenderbuffers)   \
                                X(glGenTextures) X(glBindTexture) X(glTexImage2D) X(glTexSubImage2D)         \
                                    X(glTexParameteri) X(glDeleteTextures) X(glViewport) X(glEnable)         \
                                        X(glDisable) X(glDepthFunc) X(glDepthMask) X(glClearDepth)           \
                                            X(glClearColor) X(glClear) X(glEnableVertexAttribArray)          \
                                                X(glDisableVertexAttribArray) X(glVertexAttribPointer)       \
                                                    X(glDrawElements) X(glReadPixels) X(glReadBuffer)        \
                                                        X(glPixelStorei) X(glFinish) X(glCopyTexSubImage2D)
struct Gl {
#define DECLARE(fn) decltype(&::fn) fn = nullptr;
    GL_FUNCTIONS(DECLARE)
#undef DECLARE
    bool load(std::string &error) {
#define LOAD(fn)                                                                                             \
    fn = reinterpret_cast<decltype(fn)>(SDL_GL_GetProcAddress(#fn));                                         \
    if (!fn) {                                                                                               \
        error = std::string("Missing OpenGL entry point: ") + #fn;                                           \
        return false;                                                                                        \
    }
        GL_FUNCTIONS(LOAD)
#undef LOAD
        return true;
    }
};
class GlDevice final : public Device {
    SDL_Window *window_ = nullptr;
    SDL_GLContext context_ = nullptr;
    Gl gl_;
    bool gl_ready_ = false;
    GLuint program_ = 0, vertex_ = 0, index_ = 0, target_ = 0, color_ = 0, depth_ = 0;
    GLint matrix_ = -1, plane_ = -1;
    GLuint ascii_program_ = 0, material_texture_ = 0, font_texture_ = 0, quad_ = 0;
    GLuint style_program_ = 0, style_target_ = 0, style_texture_ = 0;
    GLuint surface_texture_ = 0;
    int surface_width_ = 0, surface_height_ = 0;
    GLuint overlay_texture_ = 0, underlay_texture_ = 0, reflection_texture_ = 0, dynamic_vertices_ = 0,
           dynamic_indices_ = 0;
    GLsizei dynamic_count_ = 0;
    std::vector<uint8_t> overlay_pixels_;
    int width_, height_;
    GLsizei count_ = 0;
    std::vector<engine::GeometryRange> ranges_;
    bool current(std::string &error) {
        if (SDL_GL_MakeCurrent(window_, context_) == 0)
            return true;
        error = std::string("SDL_GL_MakeCurrent: ") + SDL_GetError();
        return false;
    }
    bool check(const char *operation, std::string &error) {
        const GLenum status = gl_.glGetError();
        if (status == GL_NO_ERROR)
            return true;
        error = std::string(operation) + " OpenGL error " + std::to_string(status);
        return false;
    }
    GLuint shader(GLenum type, const char *source, std::string &error) {
        const GLuint shader = gl_.glCreateShader(type);
        gl_.glShaderSource(shader, 1, &source, nullptr);
        gl_.glCompileShader(shader);
        GLint ok = 0;
        gl_.glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (ok)
            return shader;
        char log[2048]{};
        gl_.glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        error = std::string("OpenGL shader compile: ") + log;
        gl_.glDeleteShader(shader);
        return 0;
    }

  public:
    GlDevice(int width, int height) : width_(width), height_(height) {}
    ~GlDevice() override {
        if (context_) {
            if (SDL_GL_MakeCurrent(window_, context_) == 0 && gl_ready_) {
                gl_.glFinish();
                if (program_)
                    gl_.glDeleteProgram(program_);
                if (ascii_program_)
                    gl_.glDeleteProgram(ascii_program_);
                if (style_program_)
                    gl_.glDeleteProgram(style_program_);
                if (style_target_)
                    gl_.glDeleteFramebuffers(1, &style_target_);
                if (style_texture_)
                    gl_.glDeleteTextures(1, &style_texture_);
                if (material_texture_)
                    gl_.glDeleteTextures(1, &material_texture_);
                if (font_texture_)
                    gl_.glDeleteTextures(1, &font_texture_);
                if (quad_)
                    gl_.glDeleteBuffers(1, &quad_);
                if (surface_texture_)
                    gl_.glDeleteTextures(1, &surface_texture_);
                if (overlay_texture_)
                    gl_.glDeleteTextures(1, &overlay_texture_);
                if (reflection_texture_)
                    gl_.glDeleteTextures(1, &reflection_texture_);
                if (underlay_texture_)
                    gl_.glDeleteTextures(1, &underlay_texture_);
                if (dynamic_vertices_)
                    gl_.glDeleteBuffers(1, &dynamic_vertices_);
                if (dynamic_indices_)
                    gl_.glDeleteBuffers(1, &dynamic_indices_);
                if (vertex_)
                    gl_.glDeleteBuffers(1, &vertex_);
                if (index_)
                    gl_.glDeleteBuffers(1, &index_);
                if (target_)
                    gl_.glDeleteFramebuffers(1, &target_);
                if (color_)
                    gl_.glDeleteTextures(1, &color_);
                if (depth_)
                    gl_.glDeleteRenderbuffers(1, &depth_);
            }
            SDL_GL_DeleteContext(context_);
        }
        if (window_)
            SDL_DestroyWindow(window_);
    }
    bool initialize(std::string &error) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        window_ = SDL_CreateWindow("Asciicker | GPU ASCII", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                   960, 544, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        if (!window_) {
            error = std::string("SDL_CreateWindow(GL): ") + SDL_GetError();
            return false;
        }
        context_ = SDL_GL_CreateContext(window_);
        if (!context_) {
            error = std::string("SDL_GL_CreateContext: ") + SDL_GetError();
            return false;
        }
        if (!gl_.load(error))
            return false;
        gl_ready_ = true;
        const GLuint vp = shader(GL_VERTEX_SHADER, shaders::gl_geometry_vert_glsl, error);
        if (!vp)
            return false;
        const GLuint fp = shader(GL_FRAGMENT_SHADER, shaders::gl_geometry_frag_glsl, error);
        if (!fp) {
            gl_.glDeleteShader(vp);
            return false;
        }
        program_ = gl_.glCreateProgram();
        gl_.glAttachShader(program_, vp);
        gl_.glAttachShader(program_, fp);
        gl_.glBindAttribLocation(program_, 0, "position");
        gl_.glBindAttribLocation(program_, 1, "color");
        gl_.glBindAttribLocation(program_, 2, "surface");
        gl_.glLinkProgram(program_);
        gl_.glDeleteShader(vp);
        gl_.glDeleteShader(fp);
        GLint ok = 0;
        gl_.glGetProgramiv(program_, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[2048]{};
            gl_.glGetProgramInfoLog(program_, sizeof(log), nullptr, log);
            error = std::string("OpenGL program link: ") + log;
            return false;
        }
        matrix_ = gl_.glGetUniformLocation(program_, "mvp");
        plane_ = gl_.glGetUniformLocation(program_, "plane");
        if (matrix_ < 0) {
            error = "Missing OpenGL mvp uniform";
            return false;
        }
        gl_.glGenBuffers(1, &vertex_);
        gl_.glGenBuffers(1, &index_);
        gl_.glGenFramebuffers(1, &target_);
        gl_.glBindFramebuffer(GL_FRAMEBUFFER, target_);
        gl_.glGenTextures(1, &color_);
        gl_.glBindTexture(GL_TEXTURE_2D, color_);
        gl_.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        gl_.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_, 0);
        gl_.glGenRenderbuffers(1, &depth_);
        gl_.glBindRenderbuffer(GL_RENDERBUFFER, depth_);
        gl_.glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width_, height_);
        gl_.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_);
        if (gl_.glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            error = "Incomplete OpenGL color/depth target";
            return false;
        }
        SDL_Log("GPU_DEVICE backend=OpenGL renderer=%s", gl_.glGetString(GL_RENDERER));
        gl_.glGenTextures(1, &overlay_texture_);
        gl_.glGenTextures(1, &underlay_texture_);
        gl_.glGenTextures(1, &reflection_texture_);
        overlay_pixels_.assign(128 * 128 * 4, 0);
        for (GLuint texture : {overlay_texture_, underlay_texture_, reflection_texture_}) {
            gl_.glBindTexture(GL_TEXTURE_2D, texture);
            const bool ui = texture == overlay_texture_;
            gl_.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ui ? 128 : width_, ui ? 128 : height_, 0, GL_RGBA,
                             GL_UNSIGNED_BYTE, ui ? overlay_pixels_.data() : nullptr);
            gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        gl_.glGenBuffers(1, &dynamic_vertices_);
        gl_.glGenBuffers(1, &dynamic_indices_);
        return check("initialize", error) && set_surface(engine::SurfaceTexture{1, 1, {0, 0, 0, 255}}, error);
    }
    const char *name() const override {
        return "OpenGL";
    }
    bool capture_reflection(std::string &error) override {
        if (!current(error))
            return false;
        gl_.glBindFramebuffer(GL_FRAMEBUFFER, target_);
        gl_.glActiveTexture(GL_TEXTURE0);
        gl_.glBindTexture(GL_TEXTURE_2D, reflection_texture_);
        gl_.glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width_, height_);
        return check("reflection copy", error);
    }
    bool set_dynamic(const engine::Geometry &geometry, std::string &error) override {
        if (geometry.vertices.empty() && geometry.indices.empty()) {
            dynamic_count_ = 0;
            return true;
        }
        if (!engine::valid_geometry(geometry, error) || !current(error))
            return false;
        gl_.glBindBuffer(GL_ARRAY_BUFFER, dynamic_vertices_);
        gl_.glBufferData(GL_ARRAY_BUFFER, geometry.vertices.size() * sizeof(engine::GeometryVertex),
                         geometry.vertices.data(), GL_STREAM_DRAW);
        gl_.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dynamic_indices_);
        gl_.glBufferData(GL_ELEMENT_ARRAY_BUFFER, geometry.indices.size() * sizeof(uint16_t),
                         geometry.indices.data(), GL_STREAM_DRAW);
        dynamic_count_ = static_cast<GLsizei>(geometry.indices.size());
        return check("dynamic geometry", error);
    }
    bool set_overlay(const engine::Cell *cells, int columns, int rows, std::string &error) override {
        if (!engine::pack_overlay(cells, columns, rows, overlay_pixels_)) {
            error = "Invalid overlay cells";
            return false;
        }
        if (!current(error))
            return false;
        gl_.glActiveTexture(GL_TEXTURE0);
        gl_.glBindTexture(GL_TEXTURE_2D, overlay_texture_);
        gl_.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         overlay_pixels_.data());
        return check("overlay upload", error);
    }
    bool set_surface(const engine::SurfaceTexture &surface, std::string &error) override {
        if (!surface.valid()) {
            error = "Invalid surface texture";
            return false;
        }
        if (!current(error))
            return false;
        if (!surface_texture_)
            gl_.glGenTextures(1, &surface_texture_);
        gl_.glActiveTexture(GL_TEXTURE0);
        gl_.glBindTexture(GL_TEXTURE_2D, surface_texture_);
        gl_.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, surface.width, surface.height, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, surface.rgba.data());
        surface_width_ = surface.width;
        surface_height_ = surface.height;
        gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        return check("surface texture", error);
    }
    bool update_surface(const engine::SurfaceTexture &source, int x, int y, int width, int height,
                        std::string &error) override {
        if (!source.valid() || source.width != surface_width_ || source.height != surface_height_ || x < 0 ||
            y < 0 || width < 1 || height < 1 || width > source.width - x || height > source.height - y) {
            error = "Invalid GPU surface update region";
            return false;
        }
        if (!current(error))
            return false;
        gl_.glActiveTexture(GL_TEXTURE0);
        gl_.glBindTexture(GL_TEXTURE_2D, surface_texture_);
        gl_.glPixelStorei(GL_UNPACK_ROW_LENGTH, source.width);
        gl_.glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                            source.rgba.data() + (y * source.width + x) * 4);
        gl_.glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        return check("surface region upload", error);
    }
    bool set_ascii(const engine::AsciiResources &resources, std::string &error) override {
        if ((width_ - 4) % 2 || (height_ - 4) % 2 || !current(error)) {
            error = "ASCII target requires an even two-sample bordered grid";
            return false;
        }
        if (!ascii_program_) {
            GLuint vp = shader(GL_VERTEX_SHADER, shaders::gl_present_vert_glsl, error);
            if (!vp)
                return false;
            GLuint fp = shader(GL_FRAGMENT_SHADER, shaders::gl_ascii_frag_glsl, error);
            if (!fp) {
                gl_.glDeleteShader(vp);
                return false;
            }
            ascii_program_ = gl_.glCreateProgram();
            gl_.glAttachShader(ascii_program_, vp);
            gl_.glAttachShader(ascii_program_, fp);
            gl_.glBindAttribLocation(ascii_program_, 0, "position");
            gl_.glLinkProgram(ascii_program_);
            gl_.glDeleteShader(vp);
            gl_.glDeleteShader(fp);
            GLint ok = 0;
            gl_.glGetProgramiv(ascii_program_, GL_LINK_STATUS, &ok);
            if (!ok) {
                error = "ASCII resolve program link failed";
                return false;
            }
            vp = shader(GL_VERTEX_SHADER, shaders::gl_present_vert_glsl, error);
            fp = shader(GL_FRAGMENT_SHADER, shaders::gl_style_frag_glsl, error);
            if (!vp || !fp) {
                if (vp)
                    gl_.glDeleteShader(vp);
                if (fp)
                    gl_.glDeleteShader(fp);
                return false;
            }
            style_program_ = gl_.glCreateProgram();
            gl_.glAttachShader(style_program_, vp);
            gl_.glAttachShader(style_program_, fp);
            gl_.glBindAttribLocation(style_program_, 0, "position");
            gl_.glLinkProgram(style_program_);
            gl_.glDeleteShader(vp);
            gl_.glDeleteShader(fp);
            gl_.glGetProgramiv(style_program_, GL_LINK_STATUS, &ok);
            if (!ok) {
                error = "ASCII cell-style link failed";
                return false;
            }
            gl_.glGenTextures(1, &style_texture_);
            gl_.glBindTexture(GL_TEXTURE_2D, style_texture_);
            gl_.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_ - 4, (height_ - 4) / 2, 0, GL_RGBA,
                             GL_UNSIGNED_BYTE, nullptr);
            gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            gl_.glGenFramebuffers(1, &style_target_);
            gl_.glBindFramebuffer(GL_FRAMEBUFFER, style_target_);
            gl_.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, style_texture_,
                                       0);
            if (gl_.glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                error = "Incomplete ASCII cell-style target";
                return false;
            }
            const float quad[] = {-1, -1, 1, -1, -1, 1, 1, 1};
            gl_.glGenBuffers(1, &quad_);
            gl_.glBindBuffer(GL_ARRAY_BUFFER, quad_);
            gl_.glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
            gl_.glGenTextures(1, &material_texture_);
            gl_.glGenTextures(1, &font_texture_);
        }
        const GLuint textures[] = {material_texture_, font_texture_};
        const uint8_t *pixels[] = {resources.materials.data(), resources.font.data()};
        for (int i = 0; i < 2; ++i) {
            gl_.glActiveTexture(GL_TEXTURE0);
            gl_.glBindTexture(GL_TEXTURE_2D, textures[i]);
            gl_.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 128, i ? 128 : 256, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                             pixels[i]);
            gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        return check("ASCII resources", error);
    }
    bool upload(const std::vector<engine::Geometry> &geometry, std::string &error) override {
        engine::PackedGeometry packed;
        if (!engine::pack_geometry(geometry, packed, error) || !current(error))
            return false;
        gl_.glBindBuffer(GL_ARRAY_BUFFER, vertex_);
        gl_.glBufferData(GL_ARRAY_BUFFER, packed.vertices.size() * sizeof(engine::GeometryVertex),
                         packed.vertices.data(), GL_STATIC_DRAW);
        gl_.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_);
        gl_.glBufferData(GL_ELEMENT_ARRAY_BUFFER, packed.indices.size() * sizeof(uint16_t),
                         packed.indices.data(), GL_STATIC_DRAW);
        count_ = static_cast<GLsizei>(packed.indices.size());
        ranges_ = std::move(packed.ranges);
        set_resources(geometry);
        return check("upload", error);
    }
    bool render(const engine::Matrix &matrix, const std::vector<engine::GeometryInstance> &instances,
                std::string &error) override {
        if (!count_) {
            error = "Render before geometry upload";
            return false;
        }
        if (!engine::gather_visible_geometry(matrix, instances, bounds_, visible_, error))
            return false;
        if (!current(error))
            return false;
        gl_.glBindFramebuffer(GL_FRAMEBUFFER, target_);
        gl_.glViewport(0, 0, width_, height_);
        gl_.glDisable(GL_BLEND);
        gl_.glDisable(GL_CULL_FACE);
        gl_.glDisable(GL_SCISSOR_TEST);
        gl_.glDisable(GL_DITHER);
        gl_.glEnable(GL_DEPTH_TEST);
        gl_.glDepthFunc(GL_LESS);
        gl_.glDepthMask(GL_TRUE);
        gl_.glClearDepth(1);
        gl_.glClearColor(0.12f, 0.16f, 0.22f, 1);
        gl_.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        gl_.glUseProgram(program_);
        gl_.glUniform4f(gl_.glGetUniformLocation(program_, "light"), pass_.light[0], pass_.light[1],
                        pass_.light[2], pass_.light[3]);
        gl_.glActiveTexture(GL_TEXTURE0);
        gl_.glBindTexture(GL_TEXTURE_2D, surface_texture_);
        gl_.glUniform1i(gl_.glGetUniformLocation(program_, "terrain"), 0);
        gl_.glBindBuffer(GL_ARRAY_BUFFER, vertex_);
        gl_.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_);
        gl_.glEnableVertexAttribArray(0);
        gl_.glEnableVertexAttribArray(1);
        gl_.glEnableVertexAttribArray(2);
        for (const auto &draw : visible_) {
            const auto plane = clip_plane(draw.model);
            gl_.glUniform4f(plane_, plane[0], plane[1], plane[2], plane[3]);
            const auto &range = ranges_[draw.geometry];
            gl_.glUniformMatrix4fv(matrix_, 1, GL_FALSE, draw.mvp.data());
            const size_t base = range.vertex_offset * sizeof(engine::GeometryVertex);
            gl_.glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(engine::GeometryVertex),
                                      reinterpret_cast<const void *>(base));
            gl_.glVertexAttribPointer(
                1, 4, GL_FLOAT, GL_FALSE, sizeof(engine::GeometryVertex),
                reinterpret_cast<const void *>(base + offsetof(engine::GeometryVertex, color)));
            gl_.glVertexAttribPointer(
                2, 4, GL_FLOAT, GL_FALSE, sizeof(engine::GeometryVertex),
                reinterpret_cast<const void *>(base + offsetof(engine::GeometryVertex, surface)));
            gl_.glDrawElements(GL_TRIANGLES, range.index_count, GL_UNSIGNED_SHORT,
                               reinterpret_cast<const void *>(range.index_offset * sizeof(uint16_t)));
        }
        if (dynamic_count_ && pass_.dynamic) {
            gl_.glUniform4f(plane_, 0, 0, 0, 0);
            gl_.glDepthFunc(GL_LEQUAL);
            gl_.glActiveTexture(GL_TEXTURE0);
            gl_.glBindTexture(GL_TEXTURE_2D, underlay_texture_);
            gl_.glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width_, height_);
            gl_.glBindTexture(GL_TEXTURE_2D, surface_texture_);
            gl_.glBindBuffer(GL_ARRAY_BUFFER, dynamic_vertices_);
            gl_.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dynamic_indices_);
            const auto identity = engine::identity_matrix();
            gl_.glUniformMatrix4fv(matrix_, 1, GL_FALSE, identity.data());
            gl_.glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(engine::GeometryVertex), nullptr);
            gl_.glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(engine::GeometryVertex),
                                      reinterpret_cast<void *>(offsetof(engine::GeometryVertex, color)));
            gl_.glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(engine::GeometryVertex),
                                      reinterpret_cast<void *>(offsetof(engine::GeometryVertex, surface)));
            gl_.glDrawElements(GL_TRIANGLES, dynamic_count_, GL_UNSIGNED_SHORT, nullptr);
        }
        return check("render", error);
    }
    bool readback(std::vector<uint8_t> &rgba, std::string &error) override {
        if (!current(error))
            return false;
        rgba.resize(static_cast<size_t>(width_) * height_ * 4);
        gl_.glBindFramebuffer(GL_FRAMEBUFFER, target_);
        gl_.glPixelStorei(GL_PACK_ALIGNMENT, 1);
        gl_.glReadPixels(0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        const size_t stride = static_cast<size_t>(width_) * 4;
        for (int y = 0; y < height_ / 2; ++y)
            std::swap_ranges(rgba.begin() + y * stride, rgba.begin() + (y + 1) * stride,
                             rgba.begin() + (height_ - 1 - y) * stride);
        return check("readback", error);
    }
    bool blit_to_window(int &width, int &height, std::string &error) {
        if (!current(error))
            return false;
        SDL_GL_GetDrawableSize(window_, &width, &height);
        const auto viewport = engine::fit_viewport({width, height});
        const int gl_y = height - viewport.y - viewport.height;
        gl_.glBindFramebuffer(GL_READ_FRAMEBUFFER, target_);
        gl_.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        gl_.glDisable(GL_SCISSOR_TEST);
        gl_.glClearColor(0, 0, 0, 1);
        gl_.glClear(GL_COLOR_BUFFER_BIT);
        if (ascii_program_) {
            gl_.glDisable(GL_DEPTH_TEST);
            gl_.glDisable(GL_BLEND);
            gl_.glBindBuffer(GL_ARRAY_BUFFER, quad_);
            gl_.glEnableVertexAttribArray(0);
            gl_.glDisableVertexAttribArray(1);
            gl_.glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
            gl_.glDisableVertexAttribArray(2);
            const uint16_t indices[] = {0, 1, 2, 2, 1, 3};
            gl_.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            for (int pass = 0; pass < 2; ++pass) {
                const GLuint program = pass ? ascii_program_ : style_program_;
                gl_.glBindFramebuffer(GL_FRAMEBUFFER, pass ? 0 : style_target_);
                if (pass)
                    gl_.glViewport(viewport.x, gl_y, viewport.width, viewport.height);
                else
                    gl_.glViewport(0, 0, width_ - 4, (height_ - 4) / 2);
                gl_.glUseProgram(program);
                gl_.glUniform1f(gl_.glGetUniformLocation(program, "clock"), pass_.time_seconds);
                const GLuint textures[] = {color_,           material_texture_, font_texture_,
                                           overlay_texture_, underlay_texture_, reflection_texture_,
                                           style_texture_};
                const char *names[] = {"scene",    "materials",  "font",  "overlay",
                                       "underlay", "reflection", "styles"};
                for (int i = 0; i < 7; ++i) {
                    gl_.glActiveTexture(GL_TEXTURE0 + i);
                    // Never bind the current color attachment for sampling.
                    gl_.glBindTexture(GL_TEXTURE_2D, i == 6 && !pass ? 0 : textures[i]);
                    gl_.glUniform1i(gl_.glGetUniformLocation(program, names[i]), i);
                }
                gl_.glUniform4f(gl_.glGetUniformLocation(program, "layout"), (width_ - 4) / 2,
                                (height_ - 4) / 2, width_, -height_);
                gl_.glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices);
            }
            gl_.glActiveTexture(GL_TEXTURE0);
            return check("ASCII resolve", error);
        }
        gl_.glBlitFramebuffer(0, 0, width_, height_, viewport.x, gl_y, viewport.x + viewport.width,
                              gl_y + viewport.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        return check("present", error);
    }
    bool present(std::string &error) override {
        int width = 0, height = 0;
        if (!blit_to_window(width, height, error))
            return false;
        SDL_GL_SwapWindow(window_);
        return true;
    }
    bool readback_display(std::vector<uint8_t> &rgba, int &width, int &height, std::string &error) override {
        // Wayland front-buffer contents are not retained/readable after swap.
        // Replay only the GPU resolve, capture its window buffer before swap,
        // then present that exact buffer. Never rerender simulation or geometry.
        if (!blit_to_window(width, height, error))
            return false;
        const auto viewport = engine::fit_viewport({width, height});
        const int gl_y = height - viewport.y - viewport.height;
        width = viewport.width;
        height = viewport.height;
        rgba.resize(static_cast<size_t>(width) * height * 4);
        gl_.glBindFramebuffer(GL_FRAMEBUFFER, 0);
        GLint buffer = 0;
        gl_.glGetIntegerv(GL_DRAW_BUFFER, &buffer);
        gl_.glReadBuffer(static_cast<GLenum>(buffer));
        gl_.glPixelStorei(GL_PACK_ALIGNMENT, 1);
        gl_.glReadPixels(viewport.x, gl_y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        const size_t stride = static_cast<size_t>(width) * 4;
        for (int y = 0; y < height / 2; ++y)
            std::swap_ranges(rgba.begin() + y * stride, rgba.begin() + (y + 1) * stride,
                             rgba.begin() + (height - 1 - y) * stride);
        const bool ok = check("display readback", error);
        SDL_GL_SwapWindow(window_);
        return ok;
    }
};
} // namespace
class GlFactory final : public DeviceFactory {
  public:
    std::unique_ptr<Device> create(int width, int height, std::string &error) override {
        if (width < 16 || height < 16 || width > 2048 || height > 2048) {
            error = "Invalid GPU target dimensions (16..2048)";
            return nullptr;
        }
        auto device = std::make_unique<GlDevice>(width, height);
        if (!device->initialize(error))
            return nullptr;
        return device;
    }
};
std::unique_ptr<DeviceFactory> create_device_factory(std::string &) {
    return std::make_unique<GlFactory>();
}
} // namespace port::graphics
#endif
