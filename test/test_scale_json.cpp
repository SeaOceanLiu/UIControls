// =========================================================================
// test_scale_json.cpp -- JSON 声明式布局缩放功能测试
//
// 对照组：同一 JSON 布局声明 1x 与 2x 两组（仅 "scale" 不同，布局 rect 完全
// 一致仅 y 平移），控件类型覆盖：
//   - "button"        文字按钮（JSON 声明式缩放）
//   - "image-button"  三态图片按钮（JSON "actors"：normal/hover/pressed 三图）
//   - "animation"     rotateBtn 动画按钮（JSON "path" 加载动画描述文件）
// 窗口内上下显示供目测内容 2 倍关系；布局 rect 由 UICornerstone_GetRect
// 自动断言与 JSON 声明一致（缩放只作用于内容，不得改变布局矩形）。
// 所有按钮运行时补注册 "click" 回调（红色边框便于对照框体），人工点击打印日志。
// =========================================================================

#include <cstdio>
#include <cstring>
#include "UICornerstoneAPI.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include "PlatformUtils.h"
#include "MainWindow.h"
#include "Bench.h"
#include "AppCallbacks.h"
#include "TestInstance.h"

using namespace std;

// JSON 布局：1x/2x 两组 × 3 类型（u8R 前缀保证 UTF-8：nlohmann 严格校验 UTF-8，
// MSVC 默认执行字符集为本地代码页，非 u8 前缀会把中文转为 GBK 导致解析失败）
static const char* kLayoutJson = u8R"({
  "controls": [
    {
      "type": "panel",
      "id": "rootPanel",
      "rect": { "x": 0, "y": 0, "w": 1600, "h": 1080 },
      "children": [
        { "type": "button", "id": "btn1x", "rect": { "x": 60, "y": 80, "w": 240, "h": 80 },
          "scale": { "x": 1, "y": 1 }, "caption": "JSON缩放" },
        { "type": "button", "id": "btn2x", "rect": { "x": 60, "y": 560, "w": 240, "h": 80 },
          "scale": { "x": 2, "y": 2 }, "caption": "JSON缩放" },
        { "type": "image-button", "id": "img1x", "rect": { "x": 580, "y": 80, "w": 240, "h": 176 },
          "scale": { "x": 1, "y": 1 },
          "actors": { "normal": "assets/images/down.png",
                      "hover": "assets/images/down_hover.png",
                      "pressed": "assets/images/down_pressed.png" } },
        { "type": "image-button", "id": "img2x", "rect": { "x": 580, "y": 560, "w": 240, "h": 176 },
          "scale": { "x": 2, "y": 2 },
          "actors": { "normal": "assets/images/down.png",
                      "hover": "assets/images/down_hover.png",
                      "pressed": "assets/images/down_pressed.png" } },
        { "type": "button", "id": "ani1x", "rect": { "x": 1100, "y": 80, "w": 220, "h": 220 },
          "scale": { "x": 1, "y": 1 }, "luotiAni": "assets/animations/rotateBtn/rotateBtn.jsonc" },
        { "type": "button", "id": "ani2x", "rect": { "x": 1100, "y": 560, "w": 220, "h": 220 },
          "scale": { "x": 2, "y": 2 }, "luotiAni": "assets/animations/rotateBtn/rotateBtn.jsonc" }
      ]
    }
  ]
})";

static UIInstance s_inst = nullptr;

// 点击回调：人工点击按钮时打印日志
static void onControlClicked(UIControlHandle ctl, const UIEventData* event, void* userData) {
    const char* tag = static_cast<const char*>(userData);
    printf(u8"[scale-json] CLICK %s (event=%s)\n", tag,
           event ? event->eventName : u8"null");
    fflush(stdout);
}

static bool styleAndRespond(UIControlHandle ctl, const char* tag) {
    bool border = UICornerstone_SetColor(s_inst, ctl, "border",
                                         UIColor{255, 0, 0, 255}) == 1;
    bool visible = UICornerstone_SetBool(s_inst, ctl, "border-visible", 1) == 1;
    bool click = UICornerstone_SetCallback(s_inst, ctl, "click", &onControlClicked,
                                           const_cast<char*>(tag)) == 1;
    printf(u8"[scale-json] %s styled: border=%s visible=%s click-reg=%s\n",
           tag, border ? u8"#FF0000" : u8"FAIL", visible ? u8"yes" : u8"FAIL",
           click ? u8"ok" : u8"FAIL");
    return border && visible && click;
}

// 结构断言：布局 rect 与 JSON 声明一致（scale 不得改变布局矩形）
static void runChecks() {
    static bool s_checked = false;
    if (s_checked) return;
    s_checked = true;

    // JSON 声明的 rect（1x 组 = 声明值；2x 组 = 声明值）
    const struct { const char* id; float x, y, w, h; } exp[6] = {
        {"btn1x",   60,  80, 240, 80},
        {"btn2x",   60, 560, 240, 80},
        {"img1x",  580,  80, 240, 176},
        {"img2x",  580, 560, 240, 176},
        {"ani1x", 1100,  80, 220, 220},
        {"ani2x", 1100, 560, 220, 220},
    };

    bool ok = true;
    for (int i = 0; i < 6; i++) {
        UIControlHandle ctl = UICornerstone_FindControl(s_inst, exp[i].id);
        float x = -1, y = -1, w = -1, h = -1;
        if (ctl) UICornerstone_GetRect(s_inst, ctl, &x, &y, &w, &h);
        bool eq = ctl && (x == exp[i].x) && (y == exp[i].y) && (w == exp[i].w) && (h == exp[i].h);
        printf(u8"[scale-json] %s rect(%.0f,%.0f,%.0f,%.0f) expect(%.0f,%.0f,%.0f,%.0f) %s\n",
               exp[i].id, x, y, w, h, exp[i].x, exp[i].y, exp[i].w, exp[i].h,
               eq ? u8"ok" : u8"MISMATCH");
        ok &= eq;
    }

    printf(ok ? u8"[scale-json] PASS: JSON scale/类型 rect 全部与声明一致\n"
              : u8"[scale-json] FAIL: JSON scale/类型 改变了布局 rect\n");
}

class ScaleJsonApp : public AppCallbacks {
public:
    bool onInit() override {
        MAINWIN->setTitle(u8"test_scale_json: 上方1x vs 下方2x（内容应为2倍，红色框体一致；点击按钮打印 CLICK）");
        s_inst = g_uiInstance;
        int rc = UICornerstone_LoadLayout(s_inst, kLayoutJson);
        printf(u8"[scale-json] LoadLayout rc=%d (rc 非 0 = 成功)\n", rc);
        fflush(stdout);

        bool ok = (rc != 0);
        if (ok) {
            const struct { const char* id; const char* tag; int playing; } items[6] = {
                {"btn1x", u8"[1x]btn", 0},
                {"btn2x", u8"[2x]btn", 0},
                {"img1x", u8"[1x]img", 0},
                {"img2x", u8"[2x]img", 0},
                {"ani1x", u8"[1x]ani", 1},
                {"ani2x", u8"[2x]ani", 1},
            };
            for (int i = 0; i < 6; i++) {
                UIControlHandle ctl = UICornerstone_FindControl(s_inst, items[i].id);
                if (items[i].playing) {
                    bool p = UICornerstone_SetBool(s_inst, ctl, "playing", 1) == 1;
                    printf(u8"[scale-json] %s playing=%s\n", items[i].tag,
                           p ? u8"yes" : u8"FAIL");
                    ok &= p;
                }
                ok &= styleAndRespond(ctl, items[i].tag);
            }
        }
        printf(u8"[scale-json] onInit returning %s\n", ok ? u8"true" : u8"false");
        fflush(stdout);
        return ok;
    }

    void onUpdate() override {
        BENCH->eventLoopEntry();
        BENCH->update();
    }

    void onRender() override {
        static int frame = 0;
        frame++;
        GET_RENDERDEVICE->setDrawColor(SColor(40.0f / 255.0f, 40.0f / 255.0f, 40.0f / 255.0f, 1.0f));
        GET_RENDERDEVICE->clear();
        BENCH->draw();
        if (frame == 1) { printf(u8"[scale-json] render loop active\n"); fflush(stdout); }
        if (frame == 10) {
            runChecks();
            fflush(stdout);
        }
        // 自动点击验证：向 ani1x（(1100,80,220,220) 中心 (1210,190)）注入
        // MouseDown+MouseUp，期望打印 "CLICK [1x]ani"（事件由 onUpdate 中
        // BENCH->eventLoopEntry() 消费）
        if (frame == 12) {
            auto down = std::make_shared<Event>(EventType::MouseDown);
            down->mouseButton = {1210.0f, 190.0f, MouseButton::Left};
            BENCH->inputControl(down);
            auto up = std::make_shared<Event>(EventType::MouseUp);
            up->mouseButton = {1210.0f, 190.0f, MouseButton::Left};
            BENCH->inputControl(up);
            printf(u8"[scale-json] injected click at (1210,190)\n");
            fflush(stdout);
        }
    }

    void onQuit() override {}
};

int main(int argc, char* argv[]) {
    return TestRunMain<ScaleJsonApp, 1600, 1080>(argc, argv);
}