#pragma once
#include <algorithm>
#include <cstdint>
#ifdef __vita__
#include <psp2/display.h>
#include <psp2/kernel/sysmem.h>
#endif

namespace port {
// Native boot feedback precedes SDL, asset I/O and shader compilation. This
// bounded startup-only framebuffer is not a software scene renderer.
class BootScreen {
#ifdef __vita__
    SceUID memory_ = -1;
    uint32_t *pixels_ = nullptr;
    void rectangle(int x, int y, int width, int height, uint32_t color) {
        for (int row = y; row < y + height; ++row)
            std::fill_n(pixels_ + row * 960 + x, width, color);
    }
#endif
  public:
    BootScreen() {
#ifdef __vita__
        memory_ = sceKernelAllocMemBlock("boot_screen", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
                                         9 * 256 * 1024, nullptr);
        void *base = nullptr;
        if (memory_ >= 0 && sceKernelGetMemBlockBase(memory_, &base) >= 0)
            pixels_ = static_cast<uint32_t *>(base);
#endif
        progress(0);
    }
    BootScreen(const BootScreen &) = delete;
    BootScreen &operator=(const BootScreen &) = delete;
    ~BootScreen() {
        close();
    }
    void progress(int completed) {
#ifdef __vita__
        if (!pixels_)
            return;
        std::fill_n(pixels_, 960 * 544, 0xff302a28);
        // Five-by-seven LOADING, independent of external font/assets.
        constexpr uint8_t letters[7][7] = {{16, 16, 16, 16, 16, 16, 31}, {14, 17, 17, 17, 17, 17, 14},
                                           {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 17, 17, 17, 30},
                                           {31, 4, 4, 4, 4, 4, 31},      {17, 25, 25, 21, 19, 19, 17},
                                           {14, 17, 16, 23, 17, 17, 14}};
        for (int letter = 0; letter < 7; ++letter)
            for (int y = 0; y < 7; ++y)
                for (int x = 0; x < 5; ++x)
                    if (letters[letter][y] & (16 >> x))
                        rectangle(318 + letter * 48 + x * 7, 220 + y * 7, 6, 6, 0xffe9d8ce);
        rectangle(240, 306, 480, 12, 0xff675957);
        rectangle(240, 306, std::clamp(completed, 0, 100) * 480 / 100, 12, 0xffc5b4e5);
        SceDisplayFrameBuf frame{};
        frame.size = sizeof(frame);
        frame.base = pixels_;
        frame.pitch = 960;
        frame.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
        frame.width = 960;
        frame.height = 544;
        sceDisplaySetFrameBuf(&frame, SCE_DISPLAY_SETBUF_NEXTFRAME);
        sceDisplayWaitVblankStart();
#else
        (void)completed;
#endif
    }
    void close() {
#ifdef __vita__
        if (memory_ < 0)
            return;
        // Never free an actively scanned-out buffer, including startup errors.
        SceDisplayFrameBuf frame{};
        frame.size = sizeof(frame);
        sceDisplayGetFrameBuf(&frame, SCE_DISPLAY_SETBUF_IMMEDIATE);
        if (frame.base == pixels_)
            sceDisplaySetFrameBuf(nullptr, SCE_DISPLAY_SETBUF_NEXTFRAME);
        sceDisplayWaitVblankStart();
        sceKernelFreeMemBlock(memory_);
        memory_ = -1;
        pixels_ = nullptr;
#endif
    }
};
} // namespace port
