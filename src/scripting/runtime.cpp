#include "scripting/runtime.h"
#include "game_api.h"
#include "platform/engine_hooks.h"
extern "C" {
#include "quickjs.h"
}
#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <cmath>

namespace {
JSRuntime* runtime = nullptr;
JSContext* context = nullptr;
alignas(8) std::array<uint8_t, AKAPI_BUF_SIZE> buffer{};
port::ScriptStats stats;
using Clock = std::chrono::steady_clock;
Clock::time_point deadline;
Clock::time_point bootstrap_deadline;
bool bootstrapping = false;
int interrupt(JSRuntime*, void*) { return Clock::now() > deadline; }
void no_free(JSRuntime*, void*, void*) {}
std::string exception_text() {
    JSValue exception = JS_GetException(context);
    const char* text = JS_ToCString(context, exception);
    std::string error = text ? text : "JavaScript exception (no message)";
    if (text) JS_FreeCString(context, text);
    JS_FreeValue(context, exception);
    ++stats.errors;
    return error;
}
JSValue native_call(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    int id = -1;
    if (argc != 1 || JS_ToInt32(ctx, &id, argv[0]) < 0) return JS_EXCEPTION;
    if (!((id >= 0 && id <= 17) || id == 100 || id == 101 || id == 200))
        return JS_ThrowRangeError(ctx, "invalid ak operation");
    const int floats = id == 1 || id == 13 ? 3 : (id == 17 ? 4 : (id == 3 || id == 5 || id == 15 ? 1 : 0));
    for (int i = 0; i < floats; ++i) {
        float value = 0; std::memcpy(&value, buffer.data() + i * sizeof(float), sizeof(value));
        if (!std::isfinite(value) || std::fabs(value) > 1000000)
            return JS_ThrowRangeError(ctx, "non-finite/out-of-range engine value");
    }
    int32_t integer = 0; std::memcpy(&integer, buffer.data(), sizeof(integer));
    if ((id == 9 && (integer < 0 || integer > 2)) || (id == 11 && (integer < 0 || integer > 4)))
        return JS_ThrowRangeError(ctx, "invalid mount/action");
    if (id == 7 || id == 100) buffer[65535] = 0;
    akAPI_Call(id);
    return JS_UNDEFINED;
}
JSValue print(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    std::string output = "JS ";
    for (int i = 0; i < argc; ++i) {
        const char* text = JS_ToCString(ctx, argv[i]);
        if (!text) return JS_EXCEPTION;
        if (output.size() < 2048) output.append(text, std::min<std::size_t>(std::strlen(text), 2048 - output.size()));
        JS_FreeCString(ctx, text);
        if (output.size() < 2048) output += ' ';
    }
    port::log_stage(output.c_str());
    return JS_UNDEFINED;
}
JSValue read_string(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    int pos = 0, length = 0;
    if (argc != 2 || JS_ToInt32(ctx, &pos, argv[0]) < 0 || JS_ToInt32(ctx, &length, argv[1]) < 0) return JS_EXCEPTION;
    if (pos < 0 || pos >= 65536 || length < 0 || length > 65536 - pos)
        return JS_ThrowRangeError(ctx, "string read outside API buffer");
    const char* begin = reinterpret_cast<const char*>(buffer.data()) + pos;
    std::size_t count = 0;
    while (count < static_cast<std::size_t>(length) && begin[count]) ++count;
    return JS_NewStringLen(ctx, begin, count);
}
JSValue write_string(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv) {
    int pos = 0, length = 0;
    if (argc != 3 || JS_ToInt32(ctx, &pos, argv[1]) < 0 || JS_ToInt32(ctx, &length, argv[2]) < 0) return JS_EXCEPTION;
    if (pos < 0 || pos >= 65536 || length < 1 || length > 65536 - pos)
        return JS_ThrowRangeError(ctx, "string write outside API buffer");
    std::size_t size = 0;
    const char* text = JS_ToCStringLen(ctx, &size, argv[0]);
    if (!text) return JS_EXCEPTION;
    std::size_t count = std::min(size, static_cast<std::size_t>(length - 1));
    // Do not leave a partial UTF-8 codepoint at the truncation boundary.
    if (count < size) while (count && (static_cast<unsigned char>(text[count]) & 0xC0) == 0x80) --count;
    std::memcpy(buffer.data() + pos, text, count);
    buffer[pos + count] = 0;
    JS_FreeCString(ctx, text);
    return JS_NewInt32(ctx, static_cast<int>(count));
}
}
namespace port {
bool eval_script(const std::string& source, bool root, std::string& result) {
    if (!context) { result = "JavaScript runtime is not initialized"; return false; }
    if (source.size() > 65536) { result = "Script exceeds 64 KiB limit"; return false; }
    const std::string code = root ? source : "(function(ak,akPrint){" + source + "\n}).apply(akAPI_This,[ak,akPrint])";
    const auto start = Clock::now();
    // Trusted bundled API initialization shares one bounded startup budget.
    // Cold ARM/JIT startup must not consume the interactive console's budget.
    deadline = bootstrapping ? bootstrap_deadline : start + std::chrono::milliseconds(50);
    JS_UpdateStackTop(runtime);
    JSValue value = JS_Eval(context, code.c_str(), code.size(), root ? "bootstrap.js" : "console.js", JS_EVAL_TYPE_GLOBAL);
    bool ok = !JS_IsException(value);
    if (!ok) result = exception_text();
    else {
        const char* text = JS_ToCString(context, value);
        ok = text != nullptr;
        result = text ? text : exception_text();
        if (text) JS_FreeCString(context, text);
    }
    JS_FreeValue(context, value);
    ++stats.calls;
    stats.microseconds += std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start).count();
    return ok;
}
bool start_scripting(std::string& error) {
    stop_scripting();
    bootstrapping = true;
    bootstrap_deadline = Clock::now() + std::chrono::seconds(1);
    buffer.fill(0);
    stats = {};
    runtime = JS_NewRuntime();
    if (!runtime) { error = "QuickJS allocation failed"; bootstrapping = false; return false; }
    JS_SetMemoryLimit(runtime, 16 * 1024 * 1024);
    JS_SetMaxStackSize(runtime, 256 * 1024);
    JS_SetInterruptHandler(runtime, interrupt, nullptr);
    deadline = bootstrap_deadline;
    context = JS_NewContext(runtime);
    if (!context) { error = "QuickJS context failed"; stop_scripting(); return false; }
    akAPI_Buff = buffer.data();
    JSValue global = JS_GetGlobalObject(context);
    JS_SetPropertyStr(context, global, "akAPI_V8AB", JS_NewArrayBuffer(context, buffer.data(), buffer.size(), no_free, nullptr, false));
    JS_SetPropertyStr(context, global, "akAPI_Call", JS_NewCFunction(context, native_call, "akAPI_Call", 1));
    JS_SetPropertyStr(context, global, "akPrint", JS_NewCFunction(context, print, "akPrint", 1));
    JS_SetPropertyStr(context, global, "UTF8ToString", JS_NewCFunction(context, read_string, "UTF8ToString", 2));
    JS_SetPropertyStr(context, global, "stringToUTF8", JS_NewCFunction(context, write_string, "stringToUTF8", 3));
    JS_FreeValue(context, global);
    if (!eval_script("delete this.SharedArrayBuffer;this.akAPI_Buff=0;this.akAPI_This={};this.Module={HEAPF32:new Float32Array(akAPI_V8AB),HEAP32:new Int32Array(akAPI_V8AB),HEAPU8:new Uint8Array(akAPI_V8AB)};", true, error)) {
        stop_scripting(); return false;
    }
    akAPI_Init();
    if (stats.errors) { error = "Upstream ak bootstrap failed"; stop_scripting(); return false; }
    bootstrapping = false;
    log_stage("SCRIPT_READY QuickJS 2026-06-04 memory_limit=16MiB bootstrap_limit=1000ms eval_limit=50ms callback_limit=5ms");
    return true;
}
void stop_scripting() {
    bootstrapping = false;
    if (context) JS_FreeContext(context);
    if (runtime) JS_FreeRuntime(runtime);
    context = nullptr; runtime = nullptr; akAPI_Buff = nullptr;
}
bool load_script(const std::string& path, std::string& error) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file || file.tellg() < 0 || file.tellg() > 65536) { error = "Cannot read bounded script: " + path; return false; }
    file.seekg(0);
    const std::string code{std::istreambuf_iterator<char>(file), {}};
    return eval_script(code, false, error);
}
ScriptStats script_stats() {
    if (runtime) {
        JSMemoryUsage usage{};
        JS_ComputeMemoryUsage(runtime, &usage);
        stats.memory_bytes = usage.memory_used_size;
    }
    return stats;
}
uint64_t script_time_us() { return stats.microseconds; }
}
void akAPI_Exec(const char* code, int length, bool root) {
    std::string result;
    if (!code || !port::eval_script(length < 0 ? std::string(code) : std::string(code, length), root, result)) {
        port::log_stage(("SCRIPT_ERROR " + result).c_str());
    }
}
void akAPI_CB(int id) {
    if (!context || id < 0 || id > 2) return;
    const auto start = Clock::now();
    deadline = start + std::chrono::milliseconds(5);
    JS_UpdateStackTop(runtime);
    JSValue global = JS_GetGlobalObject(context);
    JSValue function = JS_GetPropertyStr(context, global, "akAPI_CB");
    JSValue argument = JS_NewInt32(context, id);
    JSValue value = JS_Call(context, function, global, 1, &argument);
    if (JS_IsException(value)) {
        port::log_stage(("SCRIPT_CALLBACK_DISABLED " + exception_text()).c_str());
        buffer[65536 + (id >> 3)] &= ~(1 << (id & 7));
        // Fail closed: disallow an interrupted say/item operation.
        if (id < 2) std::memset(buffer.data(), 0, sizeof(int32_t));
    }
    JS_FreeValue(context, value);
    JS_FreeValue(context, function);
    JS_FreeValue(context, global);
    ++stats.calls;
    stats.microseconds += std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start).count();
}
