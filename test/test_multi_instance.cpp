// test_multi_instance.cpp — 多实例隔离性测试（设计文档 §5.12.2 测试 1-5）
// 静态链接后端（GetUIBackendCallbacks），Debug 构建运行（依赖 Debug 辅助 API）。
// 验证：双实例生命周期 / 事件隔离 / Action 隔离 / 销毁再创建 / 空值容错。
#include "UICornerstoneAPI.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

extern "C" UIBackendCallbacks* GetUIBackendCallbacks(void);

#ifdef _MSC_VER
#define DISABLE_ASSERT_DIALOG() _set_error_mode(_OUT_TO_STDERR), _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT)
#else
#define DISABLE_ASSERT_DIALOG() ((void)0)
#endif

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
    UICornerstone_PushUIEvent(inst, &down);
    UIEvent up; memset(&up, 0, sizeof(up));
    up.type = UI_EVENT_MOUSE_UP;
    UI_EVENT_MOUSE_X(&up) = x;
    UI_EVENT_MOUSE_Y(&up) = y;
    UI_EVENT_BUTTON(&up) = 1;     // MouseButton::Left
    UICornerstone_PushUIEvent(inst, &up);
    UICornerstone_ProcessEvents(inst);
    UICornerstone_Update(inst, 0.016);   // 事件入队后经 eventLoopEntry 分发
}

static void runFrame(UIInstance inst) {
    UICornerstone_ProcessEvents(inst);
    UICornerstone_Update(inst, 0.016);
    UICornerstone_Render(inst);
}

int main() {
    DISABLE_ASSERT_DIALOG();
    UIBackendCallbacks* cb = GetUIBackendCallbacks();
    assert(cb);

    // ── 测试 5：空值容错（不依赖实例，先测） ──
    UICornerstone_ProcessEvents(NULL);
    UICornerstone_Update(NULL, 0.016);
    UICornerstone_Render(NULL);
    UICornerstone_DestroyInstance(NULL);
    assert(UICornerstone_CreateButton(NULL, "OK", 0, 0, 100, 30) == NULL);
    assert(UICornerstone_CreateViewport(NULL, UIRect{0, 0, 100, 100}) == NULL);
    UICornerstone_SetRect(NULL, NULL, 0, 0, 10, 10); // void，仅验证不崩溃
    UIEvent evt0 = {};
    evt0.type = UI_EVENT_MOUSE_DOWN;
    UICornerstone_PushUIEvent(NULL, &evt0);
    printf("PASS: null tolerance\n");

    // ── 通路验证：WindowClose 注入 → quit 标志（不经控件层） ──
    {
        UIInstance inst0 = UICornerstone_CreateInstance(cb, NULL);
        assert(inst0);
        UIEvent ce; memset(&ce, 0, sizeof(ce));
        ce.type = UI_EVENT_WINDOW_CLOSE;
        UICornerstone_PushUIEvent(inst0, &ce);
        UICornerstone_ProcessEvents(inst0);
        assert(UICornerstone_IsQuitRequested(inst0) == 1);
        UICornerstone_DestroyInstance(inst0);
    }

    // ── 测试 1：双实例生命周期 ──
    UIInstance inst1 = UICornerstone_CreateInstance(cb, NULL);
    UIInstance inst2 = UICornerstone_CreateInstance(cb, NULL);
    assert(inst1 && inst2);
    assert(inst1 != inst2);

    UIControlHandle btn1 = UICornerstone_CreateButton(inst1, "OK", 0, 0, 100, 30);
    UIControlHandle btn2 = UICornerstone_CreateButton(inst2, "OK", 0, 0, 100, 30);
    assert(btn1 && btn2);
    assert(btn1 != btn2);

    for (int i = 0; i < 2; i++) {
        runFrame(inst1);
        runFrame(inst2);
    }
    printf("PASS: dual instance lifecycle\n");

    // ── 测试 2：实例间事件隔离 ──
    int fired1 = 0, fired2 = 0;
    assert(UICornerstone_SetCallback(inst1, btn1, "click", clickCb, &fired1) == 1);
    assert(UICornerstone_SetCallback(inst2, btn2, "click", clickCb, &fired2) == 1);

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
    UICornerstone_RegisterAction(inst1, "act", actionCb1, &act1);
    UICornerstone_RegisterAction(inst2, "act", actionCb2, &act2);

    const char* layoutJson =
        "{\"controls\":[{\"type\":\"button\",\"id\":\"ab\",\"text\":\"A\","
        "\"rect\":{\"x\":200,\"y\":200,\"w\":100,\"h\":30},"
        "\"events\":{\"onClick\":\"act\"}}]}";
    assert(UICornerstone_LoadLayout(inst1, layoutJson) == 1);
    assert(UICornerstone_LoadLayout(inst2, layoutJson) == 1);
    UIControlHandle ab1 = UICornerstone_FindControl(inst1, "ab");
    UIControlHandle ab2 = UICornerstone_FindControl(inst2, "ab");
    assert(ab1 && ab2 && ab1 != ab2);

    injectMouseClick(inst1, 250.0f, 215.0f); // 命中 inst1 的 JSON 按钮（在 btn1 区域外）
    assert(act1 == 1);
    assert(act2 == 0);
    injectMouseClick(inst2, 250.0f, 215.0f);
    assert(act2 == 1);
    printf("PASS: action isolation (act1=%d act2=%d)\n", act1, act2);

    // ── 测试 4：销毁再创建（泄漏检查） ──
    for (int i = 0; i < 100; i++) {
        UIInstance inst = UICornerstone_CreateInstance(cb, NULL);
        assert(inst);
        UICornerstone_DestroyInstance(inst);
    }
    assert(UICornerstone_Debug_GetAliveCount() == 2);  // 仅剩 inst1 + inst2
    printf("PASS: create/destroy x100 (alive=%d)\n", UICornerstone_Debug_GetAliveCount());

    // 逆序销毁：先 inst2 再 inst1，验证互不影响
    UICornerstone_DestroyInstance(inst2);
    UICornerstone_DestroyInstance(inst1);
    assert(UICornerstone_Debug_GetAliveCount() == 0);
    printf("PASS: reverse-order destroy, alive=%d\n", UICornerstone_Debug_GetAliveCount());

    printf("ALL PASS: multi-instance isolation\n");
    return 0;
}
