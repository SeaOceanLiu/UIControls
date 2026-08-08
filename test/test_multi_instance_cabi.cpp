// test_multi_instance_cabi.cpp — 多实例隔离性测试（设计文档 §5.12.2 测试 1-5）
// 纯 DLL 动态加载方式：LoadLibrary("UICornerstone.dll") + GetProcAddress 解析全部
// C ABI 函数，经 CreateInstanceFromPlugin → 核心 DLL 内部 LoadLibrary(UIBackend_xxx.dll)
// + GetUIBackendCallbacks 回调表创建实例。与 test_multi_instance.cpp（静态链接）
// 逻辑一致，验证动态加载插件链路下的隔离性。
// Debug 构建运行（依赖 Debug 辅助 API 与 _DEBUG 校验）。
#include "UICornerstoneAPI.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#ifdef _MSC_VER
#define DISABLE_ASSERT_DIALOG() _set_error_mode(_OUT_TO_STDERR), _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT)
#else
#define DISABLE_ASSERT_DIALOG() ((void)0)
#endif

// ===== C ABI 函数指针（动态加载） =====
typedef UIInstance (*UIPluginCreateInstanceFn)(const char*, const UIInstanceConfig*);
typedef void       (*UIDestroyInstanceFn)(UIInstance);
typedef void       (*UIProcessEventsFn)(UIInstance);
typedef void       (*UIUpdateFn)(UIInstance, double);
typedef void       (*UIRenderFn)(UIInstance);
typedef void       (*UIPushUIEventFn)(UIInstance, const UIEvent*);
typedef int        (*UIIsQuitRequestedFn)(UIInstance);
typedef void*      (*UICreateButtonFn)(UIInstance, const char*, float, float, float, float);
typedef UIInstance (*UICreateViewportFn)(UIInstance, UIRect);
typedef int        (*UISetCallbackFn)(UIInstance, void*, const char*, void(*)(void*, const UIEventData*, void*), void*);
typedef void       (*UIRegisterActionFn)(UIInstance, const char*, void(*)(void*, void*), void*);
typedef int        (*UILoadLayoutFn)(UIInstance, const char*);
typedef void*      (*UIFindControlFn)(UIInstance, const char*);
typedef void       (*UISetRectFn)(UIInstance, void*, float, float, float, float);
typedef int        (*UIDebugGetAliveCountFn)(void);

static UIPluginCreateInstanceFn uiCreateInstanceFromPlugin = nullptr;
static UIDestroyInstanceFn      uiDestroyInstance          = nullptr;
static UIProcessEventsFn        uiProcessEvents            = nullptr;
static UIUpdateFn               uiUpdate                   = nullptr;
static UIRenderFn               uiRender                   = nullptr;
static UIPushUIEventFn          uiPushUIEvent              = nullptr;
static UIIsQuitRequestedFn      uiIsQuitRequested          = nullptr;
static UICreateButtonFn         uiCreateButton             = nullptr;
static UICreateViewportFn       uiCreateViewport           = nullptr;
static UISetCallbackFn          uiSetCallback              = nullptr;
static UIRegisterActionFn       uiRegisterAction           = nullptr;
static UILoadLayoutFn           uiLoadLayout               = nullptr;
static UIFindControlFn          uiFindControl              = nullptr;
static UISetRectFn              uiSetRect                  = nullptr;
static UIDebugGetAliveCountFn   uiDebug_GetAliveCount = nullptr;

static HMODULE g_uiDll = nullptr;

static bool loadAllProcs() {
#define RESOLVE(name) \
    *(void**)&ui##name = GetProcAddress(g_uiDll, "UICornerstone_" #name); \
    if (!ui##name) { printf("FAIL: GetProcAddress(UICornerstone_" #name ")\n"); return false; }
    RESOLVE(CreateInstanceFromPlugin)
    RESOLVE(DestroyInstance)
    RESOLVE(ProcessEvents)
    RESOLVE(Update)
    RESOLVE(Render)
    RESOLVE(PushUIEvent)
    RESOLVE(IsQuitRequested)
    RESOLVE(CreateButton)
    RESOLVE(CreateViewport)
    RESOLVE(SetCallback)
    RESOLVE(RegisterAction)
    RESOLVE(LoadLayout)
    RESOLVE(FindControl)
    RESOLVE(SetRect)
    RESOLVE(Debug_GetAliveCount)
#undef RESOLVE
    return true;
}

static void actionCb1(UIControlHandle ctl, void* userData) { (void)ctl; (*(int*)userData)++; }
static void actionCb2(UIControlHandle ctl, void* userData) { (void)ctl; (*(int*)userData)++; }
static void clickCb(UIControlHandle ctl, const UIEventData* event, void* userData) {
    (void)ctl; (void)event; (*(int*)userData)++;
}

static void injectMouseClick(UIInstance inst, float x, float y) {
    UIEvent down; memset(&down, 0, sizeof(down));
    down.type = UI_EVENT_MOUSE_DOWN;
    UI_EVENT_MOUSE_X(&down) = x;
    UI_EVENT_MOUSE_Y(&down) = y;
    UI_EVENT_BUTTON(&down) = 1;   // MouseButton::Left
    uiPushUIEvent(inst, &down);
    UIEvent up; memset(&up, 0, sizeof(up));
    up.type = UI_EVENT_MOUSE_UP;
    UI_EVENT_MOUSE_X(&up) = x;
    UI_EVENT_MOUSE_Y(&up) = y;
    UI_EVENT_BUTTON(&up) = 1;     // MouseButton::Left
    uiPushUIEvent(inst, &up);
    uiProcessEvents(inst);
    uiUpdate(inst, 0.016);   // 事件入队后经 eventLoopEntry 分发
}

static void runFrame(UIInstance inst) {
    uiProcessEvents(inst);
    uiUpdate(inst, 0.016);
    uiRender(inst);
}

int main() {
    DISABLE_ASSERT_DIALOG();

    g_uiDll = LoadLibraryA("UICornerstone.dll");
    if (!g_uiDll) { printf("FAIL: LoadLibrary(UICornerstone.dll)\n"); return 1; }
    if (!loadAllProcs()) { FreeLibrary(g_uiDll); return 1; }
    printf("OK: dynamically loaded UICornerstone.dll\n");

    // ── 测试 5：空值容错（不依赖实例，先测） ──
    uiProcessEvents(NULL);
    uiUpdate(NULL, 0.016);
    uiRender(NULL);
    uiDestroyInstance(NULL);
    assert(uiCreateButton(NULL, "OK", 0, 0, 100, 30) == NULL);
    assert(uiCreateViewport(NULL, UIRect{0, 0, 100, 100}) == NULL);
    uiSetRect(NULL, NULL, 0, 0, 10, 10); // void，仅验证不崩溃
    UIEvent evt0 = {};
    evt0.type = UI_EVENT_MOUSE_DOWN;
    uiPushUIEvent(NULL, &evt0);
    printf("PASS: null tolerance\n");

    // ── 通路验证：插件 DLL 加载 + WindowClose 注入 → quit 标志 ──
    {
        UIInstance inst0 = uiCreateInstanceFromPlugin(UICORNERSTONE_BACKEND_NAME, NULL);
        assert(inst0);
        UIEvent ce; memset(&ce, 0, sizeof(ce));
        ce.type = UI_EVENT_WINDOW_CLOSE;
        uiPushUIEvent(inst0, &ce);
        uiProcessEvents(inst0);
        assert(uiIsQuitRequested(inst0) == 1);
        uiDestroyInstance(inst0);
    }
    printf("PASS: plugin DLL load + WindowClose\n");

    // ── 测试 1：双实例生命周期 ──
    UIInstance inst1 = uiCreateInstanceFromPlugin(UICORNERSTONE_BACKEND_NAME, NULL);
    UIInstance inst2 = uiCreateInstanceFromPlugin(UICORNERSTONE_BACKEND_NAME, NULL);
    assert(inst1 && inst2);
    assert(inst1 != inst2);

    UIControlHandle btn1 = uiCreateButton(inst1, "OK", 0, 0, 100, 30);
    UIControlHandle btn2 = uiCreateButton(inst2, "OK", 0, 0, 100, 30);
    assert(btn1 && btn2);
    assert(btn1 != btn2);

    for (int i = 0; i < 2; i++) {
        runFrame(inst1);
        runFrame(inst2);
    }
    printf("PASS: dual instance lifecycle\n");

    // ── 测试 2：实例间事件隔离 ──
    int fired1 = 0, fired2 = 0;
    assert(uiSetCallback(inst1, btn1, "click", clickCb, &fired1) == 1);
    assert(uiSetCallback(inst2, btn2, "click", clickCb, &fired2) == 1);

    injectMouseClick(inst1, 50.0f, 15.0f);   // 命中 btn1
    injectMouseClick(inst2, 50.0f, 15.0f);   // 命中 btn2（证明 inst2 通路正常）
    assert(fired1 == 1);
    assert(fired2 == 1);

    injectMouseClick(inst1, 50.0f, 15.0f);   // 再点 inst1
    assert(fired1 == 2);
    assert(fired2 == 1);                     // inst2 未收到 inst1 的事件
    printf("PASS: event isolation (fired1=%d fired2=%d)\n", fired1, fired2);

    // ── 测试 3：实例间 Action 隔离（同名 action 互不覆盖） ──
    int act1 = 0, act2 = 0;
    uiRegisterAction(inst1, "act", actionCb1, &act1);
    uiRegisterAction(inst2, "act", actionCb2, &act2);

    const char* layoutJson =
        "{\"controls\":[{\"type\":\"button\",\"id\":\"ab\",\"text\":\"A\","
        "\"rect\":{\"x\":200,\"y\":200,\"w\":100,\"h\":30},"
        "\"events\":{\"onClick\":\"act\"}}]}";
    assert(uiLoadLayout(inst1, layoutJson) == 1);
    assert(uiLoadLayout(inst2, layoutJson) == 1);
    UIControlHandle ab1 = uiFindControl(inst1, "ab");
    UIControlHandle ab2 = uiFindControl(inst2, "ab");
    assert(ab1 && ab2 && ab1 != ab2);

    injectMouseClick(inst1, 250.0f, 215.0f); // 命中 inst1 的 JSON 按钮（在 btn1 区域外）
    assert(act1 == 1);
    assert(act2 == 0);
    injectMouseClick(inst2, 250.0f, 215.0f);
    assert(act2 == 1);
    printf("PASS: action isolation (act1=%d act2=%d)\n", act1, act2);

    // ── 测试 4：销毁再创建（泄漏检查） ──
    for (int i = 0; i < 100; i++) {
        UIInstance inst = uiCreateInstanceFromPlugin(UICORNERSTONE_BACKEND_NAME, NULL);
        assert(inst);
        uiDestroyInstance(inst);
    }
    assert(uiDebug_GetAliveCount() == 2);  // 仅剩 inst1 + inst2
    printf("PASS: create/destroy x100 (alive=%d)\n", uiDebug_GetAliveCount());

    // 逆序销毁：先 inst2 再 inst1，验证互不影响
    uiDestroyInstance(inst2);
    uiDestroyInstance(inst1);
    assert(uiDebug_GetAliveCount() == 0);
    printf("PASS: reverse-order destroy, alive=%d\n", uiDebug_GetAliveCount());

    FreeLibrary(g_uiDll);
    printf("ALL PASS: multi-instance isolation (CABI dynamic DLL)\n");
    return 0;
}
