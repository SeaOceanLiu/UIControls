// test_window.cpp - 运行期窗口 API 三后端验证
// GetWindowSize / SetWindowSize / GetNativeWindowHandle / SetWindowResizeCallback
// 逻辑测试：注入 WindowResize 事件验证回调（避免真实 OS resize 时序依赖）；
// SetWindowSize 采用真实窗口 resize 后 GetWindowSize 读回断言（±1 容差）。
#include "UICornerstoneAPI.h"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <cmath>

#ifdef _MSC_VER
#define DISABLE_ASSERT_DIALOG() _set_error_mode(_OUT_TO_STDERR), _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT)
#else
#define DISABLE_ASSERT_DIALOG() ((void)0)
#endif

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) { ++g_pass; } else { ++g_fail; printf("FAIL: %s\n", msg); } } while (0)

// ===== C ABI 函数指针（动态加载，CABI 路径验证） =====
typedef UIInstance (*UIPluginCreateInstanceFn)(const char*, const UIInstanceConfig*);
typedef void       (*UIDestroyInstanceFn)(UIInstance);
typedef UIInstance (*UICreateViewportFn)(UIInstance, UIRect);
typedef int        (*UIGetWindowSizeFn)(UIInstance, float*, float*);
typedef int        (*UISetWindowSizeFn)(UIInstance, float, float);
typedef void*      (*UIGetNativeWindowHandleFn)(UIInstance);
typedef void       (*UISetWindowResizeCallbackFn)(UIInstance, UIWindowResizeCallback, void*);
typedef void       (*UIPushUIEventFn)(UIInstance, const UIEvent*);
typedef int        (*UIProcessEventsFn)(UIInstance);
typedef int        (*UIGetBackendCapabilitiesFn)(UIInstance);
typedef int        (*UIIsQuitRequestedFn)(UIInstance);

static UIPluginCreateInstanceFn           g_CreateInstanceFromPlugin;
static UIDestroyInstanceFn                g_DestroyInstance;
static UICreateViewportFn                 g_CreateViewport;
static UIGetWindowSizeFn                  g_GetWindowSize;
static UISetWindowSizeFn                  g_SetWindowSize;
static UIGetNativeWindowHandleFn          g_GetNativeWindowHandle;
static UISetWindowResizeCallbackFn        g_SetWindowResizeCallback;
static UIPushUIEventFn                    g_PushUIEvent;
static UIProcessEventsFn                  g_ProcessEvents;
static UIGetBackendCapabilitiesFn         g_GetBackendCapabilities;
static UIIsQuitRequestedFn                g_IsQuitRequested;

static bool loadAllProcs(HMODULE dll) {
#define RESOLVE(name) \
    *(void**)&g_##name = GetProcAddress(dll, "UICornerstone_" #name); \
    if (!g_##name) { printf("FAIL: GetProcAddress(UICornerstone_" #name ")\n"); return false; }
    RESOLVE(CreateInstanceFromPlugin)
    RESOLVE(DestroyInstance)
    RESOLVE(CreateViewport)
    RESOLVE(GetWindowSize)
    RESOLVE(SetWindowSize)
    RESOLVE(GetNativeWindowHandle)
    RESOLVE(SetWindowResizeCallback)
    RESOLVE(PushUIEvent)
    RESOLVE(ProcessEvents)
    RESOLVE(GetBackendCapabilities)
    RESOLVE(IsQuitRequested)
#undef RESOLVE
    return true;
}

// ===== 回调（C 函数指针路径） =====
static int g_resizeCount = 0;
static int g_lastW = 0, g_lastH = 0;
static void onResize(int width, int height, void* userData) {
    ++g_resizeCount;
    g_lastW = width;
    g_lastH = height;
    (void)userData;
}

int main(int argc, char* argv[]) {
    DISABLE_ASSERT_DIALOG();
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "auto=", 5) == 0) {
            printf("auto= 已接受（本测试为逻辑测试，直接执行）\n");
        } else {
            printf("WARN: 忽略无法识别的参数: %s\n", argv[i]);
        }
    }

    HMODULE dll = LoadLibraryA("UICornerstone.dll");
    if (!dll) { printf("FAIL: LoadLibrary(UICornerstone.dll)\n"); return 1; }
    if (!loadAllProcs(dll)) { FreeLibrary(dll); return 1; }

    UIInstanceConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.structSize = sizeof(cfg);
    cfg.windowTitle = "test_window";
    cfg.windowWidth = 1200;
    cfg.windowHeight = 800;

    UIInstance inst = g_CreateInstanceFromPlugin(UICORNERSTONE_BACKEND_NAME, &cfg);
    if (!inst) { printf("FAIL: CreateInstanceFromPlugin\n"); FreeLibrary(dll); return 1; }

    // 1. 初始窗口尺寸
    float w = 0, h = 0;
    CHECK(g_GetWindowSize(inst, &w, &h) == 1, "GetWindowSize 返回 1");
    CHECK(w == 1200.0f && h == 800.0f, "初始尺寸 = 创建时 1200x800");

    // 2. 能力位（运行期窗口 API 三后端均声明）
    CHECK((g_GetBackendCapabilities(inst) & UICORN_BACKEND_CAP_WINDOW_SET_SIZE) != 0,
        "capabilities 含 WINDOW_SET_SIZE");
    // 多窗口能力按后端差异：sdl3/sfml 声明、raylib 单窗口架构不声明（仅供记录，不做硬断言）
    if ((g_GetBackendCapabilities(inst) & UICORN_BACKEND_CAP_MULTI_WINDOW) != 0) {
        printf("capabilities: MULTI_WINDOW 声明\n");
    } else {
        printf("capabilities: 无 MULTI_WINDOW（单窗口架构）\n");
    }

    // 3. 原生窗口句柄
    void* hwnd = g_GetNativeWindowHandle(inst);
    CHECK(hwnd != nullptr, "nativeHandle 非空（sdl3 有窗口）");

    // 4. SetWindowSize → 读回（真实 resize 即时生效；±1 容差）
    int rc = g_SetWindowSize(inst, 900.0f, 700.0f);
    printf("SetWindowSize rc=%d\n", rc);
    CHECK(rc == 1, "SetWindowSize 返回 1");
    g_GetWindowSize(inst, &w, &h);
    printf("actual after set: %.1f x %.1f\n", w, h);
    CHECK(fabsf(w - 900.0f) <= 1.0f && fabsf(h - 700.0f) <= 1.0f,
        "SetWindowSize 后 GetWindowSize ≈ 900x700");

    // 5. 回调：注入 WindowResize 事件 → ProcessEvents 分发
    g_SetWindowResizeCallback(inst, &onResize, nullptr);
    UIEvent ue;
    memset(&ue, 0, sizeof(ue));
    ue.type = UI_EVENT_WINDOW_RESIZE;
    UI_EVENT_RESIZE_W(&ue) = 1000;
    UI_EVENT_RESIZE_H(&ue) = 750;
    g_PushUIEvent(inst, &ue);
    g_ProcessEvents(inst);
    printf("resizeCount=%d last=%dx%d\n", g_resizeCount, g_lastW, g_lastH);
    // 真实 resize（后端 SizeChanged 事件）与注入事件都可能触发回调 → count>=1
    CHECK(g_resizeCount >= 1, "回调触发（真实 resize 和/或注入 WindowResize）");
    CHECK(g_lastW == 1000 && g_lastH == 750, "回调收到 1000x750");

    // 6. 取消回调后不再触发
    int countAfter = g_resizeCount;
    g_SetWindowResizeCallback(inst, nullptr, nullptr);
    memset(&ue, 0, sizeof(ue));
    ue.type = UI_EVENT_WINDOW_RESIZE;
    UI_EVENT_RESIZE_W(&ue) = 800;
    UI_EVENT_RESIZE_H(&ue) = 640;
    g_PushUIEvent(inst, &ue);
    g_ProcessEvents(inst);
    CHECK(g_resizeCount == countAfter, "取消回调后不再触发");

    printf("test_window: PASS=%d FAIL=%d\n", g_pass, g_fail);

    g_DestroyInstance(inst);
    FreeLibrary(dll);
    return g_fail == 0 ? 0 : 1;
}
