// test_multiviewport_cabi.cpp — 多视口测试（设计文档 §5.13.7 + K1-K8 键盘跨视口导航）
// 纯 DLL 动态加载方式：LoadLibrary("UICornerstone.dll") + GetProcAddress 解析全部
// C ABI 函数，经 CreateInstanceFromPlugin → 核心 DLL 内部 LoadLibrary(UIBackend_xxx.dll)
// + GetUIBackendCallbacks 回调表创建实例。与 test_multiviewport.cpp（静态链接）
// 逻辑一致（K1-K8），验证动态加载插件链路下的跨视口行为。
// Debug 构建运行（依赖 Debug 辅助 API 与 _DEBUG 校验）。
#include "UICornerstoneAPI.h"
#include "EventTypes.h"
#include <windows.h>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <cstdlib>

#ifdef _MSC_VER
#define DISABLE_ASSERT_DIALOG() _set_error_mode(_OUT_TO_STDERR), _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT)
#else
#define DISABLE_ASSERT_DIALOG() ((void)0)
#endif

// ===== C ABI 函数指针（动态加载） =====
typedef UIInstance (*UIPluginCreateInstanceFn)(const char*, const UIInstanceConfig*);
typedef UIInstance (*UICreateViewportFn)(UIInstance, UIRect);
typedef void*      (*UICreateWinFrameFn)(UIInstance, const char*, float, float, float, float);
typedef void*      (*UICreateEditBoxFn)(UIInstance, float, float, float, float);
typedef int        (*UISetBoolFn)(UIInstance, void*, const char*, int);
typedef void       (*UIDestroyInstanceFn)(UIInstance);
typedef void       (*UIProcessEventsFn)(UIInstance);
typedef void       (*UIUpdateFn)(UIInstance, double);
typedef void       (*UIRenderFn)(UIInstance);
typedef void       (*UIPushUIEventFn)(UIInstance, const UIEvent*);
typedef UIInstance (*UIDebugGetActiveViewportFn)(UIInstance);
typedef int        (*UIDebugIsControlFocusedFn)(UIInstance, void*);
typedef int        (*UIDebugGetAliveCountFn)(void);

static UIPluginCreateInstanceFn  uiCreateInstanceFromPlugin = nullptr;
static UICreateViewportFn        uiCreateViewport          = nullptr;
static UICreateWinFrameFn        uiCreateWinFrame          = nullptr;
static UICreateEditBoxFn         uiCreateEditBox           = nullptr;
static UISetBoolFn               uiSetBool                 = nullptr;
static UIDestroyInstanceFn       uiDestroyInstance         = nullptr;
static UIProcessEventsFn         uiProcessEvents           = nullptr;
static UIUpdateFn                uiUpdate                  = nullptr;
static UIRenderFn                uiRender                  = nullptr;
static UIPushUIEventFn           uiPushUIEvent             = nullptr;
static UIDebugGetActiveViewportFn uiDebug_GetActiveViewport = nullptr;
static UIDebugIsControlFocusedFn uiDebug_IsControlFocused   = nullptr;
static UIDebugGetAliveCountFn    uiDebug_GetAliveCount      = nullptr;

static HMODULE g_uiDll = nullptr;

static bool loadAllProcs() {
#define RESOLVE(name) \
    *(void**)&ui##name = GetProcAddress(g_uiDll, "UICornerstone_" #name); \
    if (!ui##name) { printf("FAIL: GetProcAddress(UICornerstone_" #name ")\n"); return false; }
    RESOLVE(CreateInstanceFromPlugin)
    RESOLVE(CreateViewport)
    RESOLVE(CreateWinFrame)
    RESOLVE(CreateEditBox)
    RESOLVE(SetBool)
    RESOLVE(DestroyInstance)
    RESOLVE(ProcessEvents)
    RESOLVE(Update)
    RESOLVE(Render)
    RESOLVE(PushUIEvent)
    RESOLVE(Debug_GetActiveViewport)
    RESOLVE(Debug_IsControlFocused)
    RESOLVE(Debug_GetAliveCount)
#undef RESOLVE
    return true;
}

// ── 事件注入辅助 ──
static void injectKey(UIInstance inst, KeyCode code, KeyMod mod, bool down) {
    UIEvent ev; memset(&ev, 0, sizeof(ev));
    ev.type = down ? UI_EVENT_KEY_DOWN : UI_EVENT_KEY_UP;
    UI_EVENT_KEY_CODE(&ev) = (int)code;
    UI_EVENT_KEY_MOD(&ev) = (uint16_t)mod;
    uiPushUIEvent(inst, &ev);
    uiProcessEvents(inst);
    uiUpdate(inst, 0.016);   // 事件入队后经 eventLoopEntry 分发
}

static void frame(UIInstance win, UIInstance vp1, UIInstance vp2) {
    uiProcessEvents(win);
    if (vp1) { uiUpdate(vp1, 0.016); uiRender(vp1); }
    if (vp2) { uiUpdate(vp2, 0.016); uiRender(vp2); }
}

static UIInstance newWindow() {
    return uiCreateInstanceFromPlugin(UICORNERSTONE_BACKEND_NAME, NULL);
}

// ── 每用例独立窗口，避免状态纠缠 ──
static void testK1() {
    // K1：单视口（无子视口）+ 2 WinFrame，Ctrl+Tab 行为不变
    UIInstance win = newWindow();
    assert(win);
    UIControlHandle wfA = uiCreateWinFrame(win, "WinA", 10, 10, 300, 200);
    UIControlHandle wfB = uiCreateWinFrame(win, "WinB", 10, 240, 300, 200);
    assert(wfA && wfB);
    frame(win, NULL, NULL);

    // children.size()==0 → tryViewportScopeSwitch 短路，Ctrl+Tab 原样进视口内 FocusManager
    injectKey(win, KeyCode::Tab, KeyMod::LCtrl, true);
    assert(uiDebug_GetActiveViewport(win) == NULL);
    injectKey(win, KeyCode::Tab, (KeyMod)(KeyMod::LCtrl | KeyMod::LShift), true);
    assert(uiDebug_GetActiveViewport(win) == NULL);

    uiDestroyInstance(win);
    printf("PASS: K1 single viewport Ctrl+Tab\n");
}

static void testK2() {
    // K2：双视口各 1 WinFrame；首次 Ctrl+Tab 视口内优先，隐藏后跨视口
    UIInstance win = newWindow();
    UIInstance vp1 = uiCreateViewport(win, UIRect{0, 0, 640, 480});
    UIInstance vp2 = uiCreateViewport(win, UIRect{640, 0, 640, 480});
    assert(vp1 && vp2 && vp1 != vp2);
    assert(uiDebug_GetActiveViewport(win) == vp1);  // 首子视口自动 active

    UIControlHandle editA = uiCreateEditBox(vp1, 10, 10, 200, 30);
    UIControlHandle wfA = uiCreateWinFrame(vp1, "WinA", 10, 60, 300, 200);
    UIControlHandle editB1 = uiCreateEditBox(vp2, 10, 10, 200, 30);
    assert(editA && wfA && editB1);
    frame(win, vp1, vp2);

    // 首次 Ctrl+Tab：vp1 有 1 个可见 boundary → 视口内优先，不跨视口
    injectKey(win, KeyCode::Tab, KeyMod::LCtrl, true);
    assert(uiDebug_GetActiveViewport(win) == vp1);

    // 隐藏 vp1 的 WinFrame → vp1 可见 boundary == 0 → 跨视口切 vp2，
    // focusFirstInScope(vp2.bench) 聚焦第一个可聚焦控件 EditBox_B1
    assert(uiSetBool(vp1, wfA, "visible", 0) == 1);
    injectKey(win, KeyCode::Tab, KeyMod::LCtrl, true);
    assert(uiDebug_GetActiveViewport(win) == vp2);
    assert(uiDebug_IsControlFocused(win, editB1) == 1);
    assert(uiDebug_IsControlFocused(win, editA) == 0);  // vp1 旧焦点被清

    uiDestroyInstance(vp2);
    uiDestroyInstance(vp1);
    uiDestroyInstance(win);
    printf("PASS: K2 viewport-priority then cross-viewport\n");
}

static void testK3K4K5() {
    // K3：vp1 内 2 WinFrame，Ctrl+Tab 在视口内切换；K4：全部隐藏后跨视口；K5：反向
    UIInstance win = newWindow();
    UIInstance vp1 = uiCreateViewport(win, UIRect{0, 0, 640, 480});
    UIInstance vp2 = uiCreateViewport(win, UIRect{640, 0, 640, 480});
    assert(vp1 && vp2);

    UIControlHandle editA = uiCreateEditBox(vp1, 10, 10, 200, 30);
    UIControlHandle wfA1 = uiCreateWinFrame(vp1, "WinA1", 10, 60, 300, 180);
    UIControlHandle wfA2 = uiCreateWinFrame(vp1, "WinA2", 10, 260, 300, 180);
    UIControlHandle editB1 = uiCreateEditBox(vp2, 10, 10, 200, 30);
    assert(editA && wfA1 && wfA2 && editB1);
    frame(win, vp1, vp2);

    // K3：vp1 有 2 个可见 boundary → 视口内优先
    injectKey(win, KeyCode::Tab, KeyMod::LCtrl, true);
    assert(uiDebug_GetActiveViewport(win) == vp1);

    // K4：vp1 的 2 个 WinFrame 全部隐藏 → 跨视口跳 vp2
    assert(uiSetBool(vp1, wfA1, "visible", 0) == 1);
    assert(uiSetBool(vp1, wfA2, "visible", 0) == 1);
    injectKey(win, KeyCode::Tab, KeyMod::LCtrl, true);
    assert(uiDebug_GetActiveViewport(win) == vp2);
    assert(uiDebug_IsControlFocused(win, editB1) == 1);

    // K5：Ctrl+Shift+Tab 反向：vp2 → vp1
    injectKey(win, KeyCode::Tab, (KeyMod)(KeyMod::LCtrl | KeyMod::LShift), true);
    assert(uiDebug_GetActiveViewport(win) == vp1);
    assert(uiDebug_IsControlFocused(win, editA) == 1);  // focusFirstInScope(vp1)
    assert(uiDebug_IsControlFocused(win, editB1) == 0);

    uiDestroyInstance(vp2);
    uiDestroyInstance(vp1);
    uiDestroyInstance(win);
    printf("PASS: K3/K4/K5 in-viewport switch, cross on hidden, reverse\n");
}

static void testK6() {
    // K6：Tab 只在当前 activeViewport 内循环，不进入 vp1（注入目标须为 vp2）
    UIInstance win = newWindow();
    UIInstance vp1 = uiCreateViewport(win, UIRect{0, 0, 640, 480});
    UIInstance vp2 = uiCreateViewport(win, UIRect{640, 0, 640, 480});
    assert(vp1 && vp2);

    UIControlHandle editA = uiCreateEditBox(vp1, 10, 10, 200, 30);
    UIControlHandle editB1 = uiCreateEditBox(vp2, 10, 10, 200, 30);
    UIControlHandle editB2 = uiCreateEditBox(vp2, 10, 50, 200, 30);
    assert(editA && editB1 && editB2);
    frame(win, vp1, vp2);

    // 无焦点 → Tab 聚焦 vp2 第一个可聚焦控件 EditBox_B1
    injectKey(vp2, KeyCode::Tab, KeyMod::None, true);
    assert(uiDebug_IsControlFocused(win, editB1) == 1);
    assert(uiDebug_IsControlFocused(win, editA) == 0);   // vp1 不受影响

    // 再 Tab → B1 → B2，仍在 vp2 内
    injectKey(vp2, KeyCode::Tab, KeyMod::None, true);
    assert(uiDebug_IsControlFocused(win, editB2) == 1);
    assert(uiDebug_IsControlFocused(win, editB1) == 0);
    assert(uiDebug_IsControlFocused(win, editA) == 0);

    uiDestroyInstance(vp2);
    uiDestroyInstance(vp1);
    uiDestroyInstance(win);
    printf("PASS: K6 Tab stays inside active viewport\n");
}

static void testK7() {
    // K7：焦点回跳——切回 vp1 时 focusFirstInScope 聚焦第一个可聚焦控件，不记忆原焦点
    UIInstance win = newWindow();
    UIInstance vp1 = uiCreateViewport(win, UIRect{0, 0, 640, 480});
    UIInstance vp2 = uiCreateViewport(win, UIRect{640, 0, 640, 480});
    assert(vp1 && vp2);

    UIControlHandle editA1 = uiCreateEditBox(vp1, 10, 10, 200, 30);
    UIControlHandle editA2 = uiCreateEditBox(vp1, 10, 50, 200, 30);
    UIControlHandle editB1 = uiCreateEditBox(vp2, 10, 10, 200, 30);
    assert(editA1 && editA2 && editB1);
    frame(win, vp1, vp2);

    // 原焦点在 vp1 的 EditBox_A2
    injectKey(vp1, KeyCode::Tab, KeyMod::None, true);   // → A1
    injectKey(vp1, KeyCode::Tab, KeyMod::None, true);   // → A2
    assert(uiDebug_IsControlFocused(win, editA2) == 1);

    // 跨视口切到 vp2（vp1 无 boundary）→ B1 聚焦，A2 失焦
    injectKey(win, KeyCode::Tab, KeyMod::LCtrl, true);
    assert(uiDebug_GetActiveViewport(win) == vp2);
    assert(uiDebug_IsControlFocused(win, editB1) == 1);
    assert(uiDebug_IsControlFocused(win, editA2) == 0);

    // 切回 vp1 → focusFirstInScope 聚焦 A1（不是记忆的 A2）
    injectKey(win, KeyCode::Tab, KeyMod::LCtrl, true);
    assert(uiDebug_GetActiveViewport(win) == vp1);
    assert(uiDebug_IsControlFocused(win, editA1) == 1);
    assert(uiDebug_IsControlFocused(win, editA2) == 0);

    uiDestroyInstance(vp2);
    uiDestroyInstance(vp1);
    uiDestroyInstance(win);
    printf("PASS: K7 focus returns to first control in scope\n");
}

static void testK8() {
    // K8：activeViewport 为 null 时（直接销毁活动视口后）Ctrl+Tab 从 children.front() 切入。
    // 注意：children.size()<=1 时 tryViewportScopeSwitch 短路，故用 3 个视口，
    // 销毁活动视口 vp1 后仍剩 2 个子视口，覆盖 cur==nullptr 分支
    UIInstance win = newWindow();
    UIInstance vp1 = uiCreateViewport(win, UIRect{0, 0, 320, 480});
    UIInstance vp2 = uiCreateViewport(win, UIRect{320, 0, 320, 480});
    UIInstance vp3 = uiCreateViewport(win, UIRect{640, 0, 320, 480});
    assert(vp1 && vp2 && vp3);

    UIControlHandle editA = uiCreateEditBox(vp1, 10, 10, 200, 30);
    UIControlHandle editB1 = uiCreateEditBox(vp2, 10, 10, 200, 30);
    assert(editA && editB1);
    frame(win, vp1, vp2);

    // 直接销毁活动视口 vp1 → owner 将 activeViewport 置空（防悬垂）
    uiDestroyInstance(vp1);
    assert(uiDebug_GetActiveViewport(win) == NULL);

    // Ctrl+Tab：cur==nullptr 分支 → nextViewport 从 children.front()（vp2）切入，不崩溃
    injectKey(win, KeyCode::Tab, KeyMod::LCtrl, true);
    assert(uiDebug_GetActiveViewport(win) == vp2);
    assert(uiDebug_IsControlFocused(win, editB1) == 1);

    uiDestroyInstance(vp3);
    uiDestroyInstance(vp2);
    uiDestroyInstance(win);
    printf("PASS: K8 activeViewport==null after destroy, Ctrl+Tab re-enters\n");
}

int main(int argc, char* argv[]) {
    DISABLE_ASSERT_DIALOG();
    // 命令行参数任意顺序：识别 auto=<秒>（本测试为有限步骤逻辑测试，
    // 无窗口循环，接受参数但不改变行为）；无法识别参数 WARN 后忽略
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "auto=", 5) == 0) {
            printf("auto= 已接受（本测试无窗口循环，直接执行）\n");
        } else {
            printf("WARN: 忽略无法识别的参数: %s\n", argv[i]);
        }
    }

    g_uiDll = LoadLibraryA("UICornerstone.dll");
    if (!g_uiDll) { printf("FAIL: LoadLibrary(UICornerstone.dll)\n"); return 1; }
    if (!loadAllProcs()) { FreeLibrary(g_uiDll); return 1; }
    printf("OK: dynamically loaded UICornerstone.dll\n");

    testK1();
    testK2();
    testK3K4K5();
    testK6();
    testK7();
    testK8();

    assert(uiDebug_GetAliveCount() == 0);
    FreeLibrary(g_uiDll);
    printf("ALL PASS: multiviewport + keyboard navigation (CABI dynamic DLL)\n");
    return 0;
}
