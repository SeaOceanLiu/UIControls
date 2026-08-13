// test_scale_cabi — C ABI 缩放功能可视化验证
//
// 目的：同一布局参数分别以 1x 与 2x 创建三组"内容型按钮"（带文字 Button、
// 三态图片 Button、rotateBtn 动画 Button），两组同时渲染在窗口中供目测：
// 2x 组的内容（文字/贴图/动画画布）应为 1x 组的两倍大小，而控件布局矩形
// （UICornerstone_GetRect）应与缩放前完全一致（缩放只作用于内容）。
//
// 用法要点（C ABI 设计验证）：
//   - 图片按钮 = CreateButton + 三态字符串属性（normal-image/hover-image/
//     pressed-image）设置，无专用工厂（Actor 经 UICornerstone_CreateActor 直接创建）；
//   - 动画按钮 = CreateAnimatedButton（Button 承载内嵌 LuotiAni 动画资源）
//     + SetBool "playing" 显式启动播放；点击交互由按钮自身响应；
//   - 底部两个独立 Actor 演示 UICornerstone_CreateActor。
//
// 交互验证：所有按钮注册 "click" 回调，点击时打印日志。
//
// 运行：test_scale_cabi.exe [auto=<秒>]  —— 窗口保持显示，目测后自动退出。
#include "UICornerstoneAPI.h"
#include "PlatformUtils.h"
#include "AppCallbacks.h"
#include "MainWindow.h"
#include "Bench.h"
#include "RenderDevice.h"
#include "TestInstance.h"

#include <cstdio>
#include <string>
#include <vector>

using std::string;

// 布局参数（横向三列，两组纵向；内容 2x 时宽度/高度翻倍，列间距与行间距按
// 2x 内容尺寸预留，避免内容互相覆盖；布局 rect 与缩放无关）
static const SRect kBtn = {60, 80, 240, 80};      // 文字按钮（列1）
static const SRect kImg = {580, 80, 240, 176};    // 三态图片按钮（列2）
static const SRect kAni = {1100, 80, 220, 220};   // rotateBtn 动画按钮（列3）
static const SRect kAct = {340, 1040, 160, 100};  // 独立 Actor 演示（CreateActor，第三行）
static const float kShiftY = 480.0f;              // 2x 行偏移（1x y=80；2x y=560）
static const float kShiftYAni = 480.0f;           // ani 组偏移与其余行一致

// 三态图片：normal / hover / pressed 三张不同图
static const char* kImgNormal  = "assets/images/down.png";
static const char* kImgHover   = "assets/images/down_hover.png";
static const char* kImgPressed = "assets/images/down_pressed.png";

// rotateBtn 动画（loop=true）
static const char* kAnimJsonc = "assets/animations/rotateBtn/rotateBtn.jsonc";

// 边框：醒目红色（人工目测对比框体大小；缩放只作用于内容，框体尺寸不变）
static const UIColor kBorderColor = {255, 0, 0, 255};

static UIInstance s_inst = nullptr;
static UIControlHandle s_btn1 = nullptr, s_btn2 = nullptr;
static UIControlHandle s_img1 = nullptr, s_img2 = nullptr;
static UIControlHandle s_ani1 = nullptr, s_ani2 = nullptr;
static UIControlHandle s_act1 = nullptr, s_act2 = nullptr;

static int s_clickCount = 0;   // 点击回调触发计数

// 点击回调：人工点击按钮时打印日志
static void onControlClicked(UIControlHandle ctl, const UIEventData* event, void* userData) {
    const char* tag = static_cast<const char*>(userData);
    s_clickCount++;
    printf(u8"[scale-cabi] CLICK %s (count=%d, event=%s)\n",
           tag, s_clickCount, event ? event->eventName : u8"null");
    fflush(stdout);
}

// 为控件应用醒目红色边框并注册点击回调（返回是否全部成功）
static bool styleAndRespond(UIControlHandle ctl, const char* tag) {
    bool border = UICornerstone_SetColor(s_inst, ctl, "border", kBorderColor) == 1;
    bool visible = UICornerstone_SetBool(s_inst, ctl, "border-visible", 1) == 1;
    bool click = UICornerstone_SetCallback(s_inst, ctl, "click", &onControlClicked,
                                           const_cast<char*>(tag)) == 1;
    printf(u8"[scale-cabi] %s styled: border=%s visible=%s click-reg=%s\n",
           tag, border ? u8"#FF0000" : u8"FAIL", visible ? u8"yes" : u8"FAIL",
           click ? u8"ok" : u8"FAIL");
    return border && visible && click;
}

// 创建一组三个内容型按钮（文字按钮 / 三态图片按钮 / rotateBtn 动画按钮）
static bool createGroup(float dy, float xScale, float yScale, const char* tag, bool group2) {
    // 1) 带文字（label）的普通按钮
    UIControlHandle btn = UICornerstone_CreateButton(s_inst, u8"缩放测试",
                                                     kBtn.left, kBtn.top + dy, kBtn.width, kBtn.height, xScale, yScale);
    // 2) 三态图片按钮：CreateButton + 字符串属性设置三态图（无专用工厂）
    UIControlHandle img = UICornerstone_CreateButton(s_inst, "",
                                                     kImg.left, kImg.top + dy, kImg.width, kImg.height, xScale, yScale);
    bool imgSet = UICornerstone_SetString(s_inst, img, "normal-image", kImgNormal) == 1
                  && UICornerstone_SetString(s_inst, img, "hover-image", kImgHover) == 1
                  && UICornerstone_SetString(s_inst, img, "pressed-image", kImgPressed) == 1;
    // 3) rotateBtn 动画按钮（CreateAnimatedButton：Button 承载内嵌 LuotiAni，
    //    点击响应来自按钮本身；SetBool "playing" 显式启动播放）
    float aniDy = group2 ? kShiftYAni : 0.0f;
    UIControlHandle ani = UICornerstone_CreateAnimatedButton(s_inst, kAnimJsonc,
                                                             kAni.left, kAni.top + aniDy, kAni.width, kAni.height, xScale, yScale);
    bool playing = ani && UICornerstone_SetBool(s_inst, ani, "playing", 1) == 1;

    bool ok = btn && img && imgSet && ani && playing;
    if (ok) {
        if (!group2) { s_btn1 = btn; s_img1 = img; s_ani1 = ani; }
        else         { s_btn2 = btn; s_img2 = img; s_ani2 = ani; }

        char tagBtn[32], tagImg[32], tagAni[32];
        std::snprintf(tagBtn, sizeof(tagBtn), "%s-btn", tag);
        std::snprintf(tagImg, sizeof(tagImg), "%s-img", tag);
        std::snprintf(tagAni, sizeof(tagAni), "%s-ani", tag);
        ok = styleAndRespond(btn, tagBtn) && styleAndRespond(img, tagImg)
             && styleAndRespond(ani, tagAni);
        printf(u8"[scale-cabi] %s (%.1fx%.1f) img-set=%s anim-playing=%s\n",
               tag, xScale, yScale, imgSet ? u8"ok" : u8"FAIL", playing ? u8"yes" : u8"FAIL");
    }
    printf(u8"[scale-cabi] %s (%.1fx%.1f) created: btn=%s imgBtn=%s ani=%s\n",
           tag, xScale, yScale, btn ? u8"ok" : u8"nil", img ? u8"ok" : u8"nil", ani ? u8"ok" : u8"nil");
    return ok;
}

// 结构断言：布局 rect 与缩放系数无关 —— 每个控件的 GetRect 必须等于创建参数
static void runChecks() {
    static bool checked = false;
    if (checked) return;
    checked = true;

    bool ok = true;

    const SRect exp[3][2] = {
        {kBtn, {kBtn.left, kBtn.top + kShiftY, kBtn.width, kBtn.height}},
        {kImg, {kImg.left, kImg.top + kShiftY, kImg.width, kImg.height}},
        {kAni, {kAni.left, kAni.top + kShiftYAni, kAni.width, kAni.height}},
    };
    const UIControlHandle pair[3][2] = {
        {s_btn1, s_btn2}, {s_img1, s_img2}, {s_ani1, s_ani2},
    };
    const char* names[3] = {"btn(text)", "imgButton(3-state)", "ani(rotateBtn)"};

    for (int i = 0; i < 3; i++) {
        bool same = true;
        for (int g = 0; g < 2; g++) {
            float x = -1, y = -1, w = -1, h = -1;
            if (pair[i][g]) UICornerstone_GetRect(s_inst, pair[i][g], &x, &y, &w, &h);
            bool eq = (x == exp[i][g].left) && (y == exp[i][g].top) && (w == exp[i][g].width) && (h == exp[i][g].height);
            printf(u8"[scale-cabi] %s %dx rect(%.0f,%.0f,%.0f,%.0f) expect(%.0f,%.0f,%.0f,%.0f) %s\n",
                   names[i], g == 0 ? 1 : 2, x, y, w, h,
                   exp[i][g].left, exp[i][g].top, exp[i][g].width, exp[i][g].height,
                   eq ? u8"ok" : u8"MISMATCH");
            same &= eq;
        }
        ok &= same;
    }

    // 独立 Actor 演示（CreateActor）：rect 断言
    const SRect aexp[2] = {kAct, {kAct.left, kAct.top + kShiftY, kAct.width, kAct.height}};
    const UIControlHandle apair[2] = {s_act1, s_act2};
    for (int g = 0; g < 2; g++) {
        float x = -1, y = -1, w = -1, h = -1;
        if (apair[g]) UICornerstone_GetRect(s_inst, apair[g], &x, &y, &w, &h);
        bool eq = (x == aexp[g].left) && (y == aexp[g].top) && (w == aexp[g].width) && (h == aexp[g].height);
        printf(u8"[scale-cabi] CreateActor %dx rect(%.0f,%.0f,%.0f,%.0f) expect(%.0f,%.0f,%.0f,%.0f) %s\n",
               g + 1, x, y, w, h, aexp[g].left, aexp[g].top, aexp[g].width, aexp[g].height,
               eq ? u8"ok" : u8"MISMATCH");
        ok &= eq;
    }

    printf(u8"[scale-cabi] %s\n", ok ? u8"PASS: 布局 rect 与缩放系数无关（= 创建参数）"
                                     : u8"FAIL: 布局 rect 被缩放改变");
}

// ── App ──
class ScaleCabiApp : public AppCallbacks {
public:
    bool onInit() override {
        MAINWIN->setTitle(u8"test_scale_cabi: 上方1x vs 下方2x（内容应为2倍，红色框体一致；点击按钮打印 CLICK）");
        s_inst = g_uiInstance;

        bool ok1 = createGroup(0.0f, 1.0f, 1.0f, u8"[1x]", false);
        bool ok2 = createGroup(kShiftY, 2.0f, 2.0f, u8"[2x]", true);

        // 独立 Actor 演示（UICornerstone_CreateActor：直接创建图片控件）
        s_act1 = UICornerstone_CreateActor(s_inst, "assets/images/icon.png",
                                           kAct.left, kAct.top, kAct.width, kAct.height, 1.0f, 1.0f);
        s_act2 = UICornerstone_CreateActor(s_inst, "assets/images/icon.png",
                                           kAct.left, kAct.top + kShiftY, kAct.width, kAct.height, 2.0f, 2.0f);
        bool ok3 = styleAndRespond(s_act1, u8"[1x]actor") && styleAndRespond(s_act2, u8"[2x]actor");

        printf(u8"[scale-cabi] create groups: 1x=%s 2x=%s actors=%s  (请目测：2x 内容应为 1x 的两倍，红色框体一致；点击任意按钮应打印 CLICK 日志)\n",
               ok1 ? u8"ok" : u8"FAIL", ok2 ? u8"ok" : u8"FAIL", ok3 ? u8"ok" : u8"FAIL");
        fflush(stdout);
        return ok1 && ok2 && ok3;
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
        if (frame == 10) {
            runChecks();
            fflush(stdout);
        }
        // 自动点击验证：向 ani1x（kAni=(1100,80,220,220) 中心 (1210,190)）注入
        // MouseDown+MouseUp，期望打印 "CLICK [1x]ani"（事件由 onUpdate 中
        // BENCH->eventLoopEntry() 消费）
        if (frame == 12) {
            auto down = std::make_shared<Event>(EventType::MouseDown);
            down->mouseButton = {1210.0f, 190.0f, MouseButton::Left};
            BENCH->inputControl(down);
            auto up = std::make_shared<Event>(EventType::MouseUp);
            up->mouseButton = {1210.0f, 190.0f, MouseButton::Left};
            BENCH->inputControl(up);
            printf(u8"[scale-cabi] injected click at (1210,190)\n");
            fflush(stdout);
        }
    }

    void onQuit() override {}
};

int main(int argc, char* argv[]) {
    return TestRunMain<ScaleCabiApp, 1600, 1260>(argc, argv);
}