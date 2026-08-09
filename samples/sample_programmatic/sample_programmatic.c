// =========================================================================
// sample_programmatic.c — UICornerstone 编程式控件创建（命令式 UI）
//
// 【集成模式】完全静态链接
//   与 hello_uicornerstone 相同，框架编译进 exe，零 DLL 依赖。
//
// 【与 JSON 示例的区别】
//   hello_uicornerstone 用 JSON 字符串声明 UI，
//   本示例用 C 代码直接调用工厂函数创建控件。
//   两者可混合使用（部分控件用代码创建，部分用 JSON）。
//
// 【学习要点】
//   ① CreatePanel / CreateButton / CreateLabel — 工厂函数，返回 UIControlHandle
//   ② AddChild — 建立父子关系（Panel 可容纳任意子控件）
//   ③ SetColor — 通过属性系统设置背景色（自动生成 hover/pressed 变体）
//   ④ SetCallback — 通过属性系统绑定点击回调（比 JSON events 更灵活）
//   ⑤ 所有 C ABI 函数见 include/UICornerstoneAPI.h
//
// 【如何开发自己的应用】
//   1. 先创建根 Panel（覆盖窗口），再将其他控件 AddChild 进去
//   2. 工厂函数参数：text/name, x, y, w, h（均为 float 像素值）
//   3. 回调中保存句柄到全局变量，用于运行时更新控件
//   4. 参考 include/UICornerstoneAPI.h 查看更多 API
// =========================================================================

#include "UICornerstoneAPI.h"
#include "PropertyNames.h"
#include <stdio.h>

// ======== 回调函数 ========
//
// 注意：和 JSON 示例不同，这里不需要 RegisterAction。
// SetCallback 直接将函数指针绑定到控件，不经过字符串映射。

static int g_clickCount = 0;
static UIControlHandle g_statusLabel = NULL;   // 保存句柄，供回调中更新
static UIInstance g_inst;

static void onBtnClick(UIControlHandle ctl, const UIEventData* evt, void* user) {
    (void)ctl; (void)evt; (void)user;

    g_clickCount++;
    char buf[64];
    snprintf(buf, sizeof(buf), "Clicked: %d", g_clickCount);

    // g_statusLabel 在 main 中赋值后保持不变。
    // 如果控件可能在运行时被销毁，需先检查句柄有效性。
    if (g_statusLabel)
        UICornerstone_SetString(g_inst, g_statusLabel, "caption", buf);
}

// ======== main ========

int main(void) {
    // ── 初始化（与 JSON 示例完全相同） ──────────────────────────
    g_inst = UICornerstone_CreateInstanceFromPlugin(UICORNERSTONE_BACKEND_NAME, NULL);
    if (!g_inst) return 1;
    UICornerstone_SetViewport(g_inst, 0, 0, 800, 480);

    // ── 编程式创建控件树 ────────────────────────────────────────
    //
    // 控件创建与加载 JSON 的二选一路径：
    //   方式 A: UICornerstone_LoadLayout(JSON)  ← hello_uicornerstone
    //   方式 B: 工厂函数 + AddChild              ← 本示例
    //
    // 工厂函数签名（所有控件通用）：
    //   UICornerstone_CreateButton(caption, x, y, w, h) → UIControlHandle
    //   UICornerstone_CreateLabel(text, fontSize, x, y, w, h) → UIControlHandle
    //   返回值：成功→非空句柄，失败→NULL
    //
    // 父子关系规则：
    //   子控件的位置相对于父控件的左上角（绝对坐标）。
    //   父控件移动时，子控件跟随移动。
    //   父控件裁剪子控件的绘制区域。

    // 1) 创建根 Panel（覆盖整个视口）
    UIControlHandle root = UICornerstone_CreatePanel(g_inst, 0, 0, 800, 480, 1.0f, 1.0f);

    // 2) 创建标题标签并挂到根
    UIControlHandle title = UICornerstone_CreateLabel(
        g_inst, "UICornerstone Sample (Programmatic)", 18,
        20, 10, 760, 30, 1.0f, 1.0f);
    UICornerstone_AddChildControl(g_inst, root, title);

    // 3) 创建按钮
    UIControlHandle btn = UICornerstone_CreateButton(g_inst, "Click Me",
        20, 60, 200, 80, 1.0f, 1.0f);

    // SetColor: 通过属性系统设置 background 色，自动生成 hover（变亮 ~30%）和
    // pressed（变暗 ~30%）。参数为 UIColor 结构体（RGBA 分量 0-255）。
    UICornerstone_SetColor(g_inst, btn, kBackground, (UIColor){74, 144, 217, 255});

    // SetCallback: 通过属性系统绑定点击回调。
    // 第三个参数 userData 会透传给回调（本例传 NULL）。
    UICornerstone_SetCallback(g_inst, btn, kEventClick, onBtnClick, NULL);
    UICornerstone_AddChildControl(g_inst, root, btn);

    // 4) 创建状态标签（用于显示点击次数）
    g_statusLabel = UICornerstone_CreateLabel(
        g_inst, "Click the button above", 14,
        20, 160, 400, 24, 1.0f, 1.0f);
    UICornerstone_AddChildControl(g_inst, root, g_statusLabel);

    // ── 帧循环（与 JSON 示例完全相同） ──────────────────────────
    while (!UICornerstone_IsQuitRequested(g_inst)) {
        UICornerstone_ProcessEvents(g_inst);
        UICornerstone_Update(g_inst, 1.0f / 60.0f);
        UICornerstone_Clear(g_inst);
        UICornerstone_Render(g_inst);
        UICornerstone_Present(g_inst);
    }

    UICornerstone_DestroyInstance(g_inst);
    return 0;
}