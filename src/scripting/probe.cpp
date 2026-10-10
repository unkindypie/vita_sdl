// Foundation compatibility probe only. No engine bindings until Phase 3.
#include "scripting/probe.h"
extern "C" {
#include "quickjs.h"
}
#include <array>
#include <cstring>
namespace {
JSValue native_increment(JSContext* context, JSValueConst, int argc, JSValueConst* argv) {
    int32_t value = 0;
    if (argc != 1 || JS_ToInt32(context, &value, argv[0]) < 0) return JS_EXCEPTION;
    return JS_NewInt32(context, value + 1);
}
void no_free(JSRuntime*, void*, void*) {}
}
namespace port {
bool probe_quickjs(std::string& error) {
    JSRuntime* runtime = JS_NewRuntime();
    if (!runtime) { error = "QuickJS runtime allocation failed"; return false; }
    JS_SetMemoryLimit(runtime, 8 * 1024 * 1024);
    JSContext* context = JS_NewContext(runtime);
    if (!context) { JS_FreeRuntime(runtime); error = "QuickJS context allocation failed"; return false; }
    std::array<uint8_t, 16> shared{};
    JSValue global = JS_GetGlobalObject(context);
    JS_SetPropertyStr(context, global, "shared", JS_NewArrayBuffer(context, shared.data(), shared.size(), no_free, nullptr, false));
    JS_SetPropertyStr(context, global, "inc", JS_NewCFunction(context, native_increment, "inc", 1));
    constexpr const char* source = "new Uint8Array(shared)[3] = inc(40); 'probe-ok'";
    JSValue result = JS_Eval(context, source, std::strlen(source), "probe.js", JS_EVAL_TYPE_GLOBAL);
    const char* text = JS_IsException(result) ? nullptr : JS_ToCString(context, result);
    bool ok = text && std::strcmp(text, "probe-ok") == 0 && shared[3] == 41;
    if (text) JS_FreeCString(context, text);
    JS_FreeValue(context, result);
    constexpr const char* failing = "throw new Error('expected-probe-error')";
    result = JS_Eval(context, failing, std::strlen(failing), "exception.js", JS_EVAL_TYPE_GLOBAL);
    ok = ok && JS_IsException(result);
    JSValue exception = JS_GetException(context);
    text = JS_ToCString(context, exception);
    ok = ok && text && std::strstr(text, "expected-probe-error");
    if (text) JS_FreeCString(context, text);
    JS_FreeValue(context, exception);
    JS_FreeValue(context, result);
    JS_FreeValue(context, global);
    JS_FreeContext(context);
    JS_FreeRuntime(runtime);
    if (!ok) error = "QuickJS evaluation/native call/shared-buffer/exception probe failed";
    return ok;
}
}
