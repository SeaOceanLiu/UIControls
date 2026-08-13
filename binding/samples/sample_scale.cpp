// UICornerstone C++ Binding — 缩放功能用例（测试）：创建参数 xScale/yScale
// 对照组：同参数创建 1x 与 2x 两组"内容型按钮"（带文字 Button / 三态图片
// ImageButton / rotateBtn 动画按钮[CreateAnimatedButton：Button 承载内嵌动画]），
// 运行若干帧后自动退出。
// 本用例验证：
//   1) 六按钮创建与运行稳定（不崩溃、句柄有效、IsValid 保持）
//   2) 缩放不改变布局 rect（GetRect = 创建参数，与引擎语义一致）
//   3) 默认参数（不传 xScale/yScale）等价于显式 1.0f
//   4) 图片按钮用字符串属性设置三态图；动画按钮创建后显式 SetBool("playing") 启动
//   5) 所有按钮红色边框对照框体大小 + "click" 回调打印日志（人工点击验证）
// 运行：sample_scale [backend=<后端>] [auto=<秒>]（人工模式关闭窗口退出；
// auto=<秒> 模式渲染满时长后自动退出，与标准测试命令行方案一致）
#include "UICornerstone.h"
#include "Control.h"
#include "Event.h"
#include "auto_args.h"
#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>
#include <algorithm>
#include <string>

// 布局参数（横向三列，两组纵向；2x 内容宽度/高度翻倍，列间距按 2x 内容尺寸
// 预留避免内容覆盖；内容缩放仅作用于内容，布局 rect 与缩放无关）
static const UIRect kBtn = {60, 80, 220, 70};     // 文字按钮（列1）
static const UIRect kImg = {580, 80, 220, 150};   // 三态图片按钮（列2）
static const UIRect kAni = {1100, 80, 220, 200};  // rotateBtn 动画按钮（列3）
static const UIRect kDef = {60, 1040, 220, 70};   // 默认参数组（等价 1.0f，第三行）
static const float kShiftY = 480.0f;

// 三态图片：normal / hover / pressed 三张不同图
static const char* kImgNormal  = "assets/images/down.png";
static const char* kImgHover   = "assets/images/down_hover.png";
static const char* kImgPressed = "assets/images/down_pressed.png";

// rotateBtn 动画（loop=true）
static const char* kAnimJsonc = "assets/animations/rotateBtn/rotateBtn.jsonc";

// 醒目红色边框（便于目测对比框体大小，缩放不改变框体尺寸）
static const UIColor kBorder = {255, 0, 0, 255};

// 点击回调：人工点击按钮时打印日志
static void onClicked(const char* tag, const Event& ev) {
    std::printf(u8"[sample_scale] CLICK %s (event=%s)\n", tag,
                ev.GetNameRaw() ? ev.GetNameRaw() : u8"null");
    std::fflush(stdout);
}

// 为按钮应用红色边框并绑定点击回调
static void styleAndRespond(Control& ctl, const char* tag) {
    ctl.SetColor("border", kBorder);
    ctl.SetBool("border-visible", true);
    ctl.SetCallback("click", [tag](const Event& ev) { onClicked(tag, ev); });
}

// 创建一组三个内容型按钮（文字 / 三态图片[属性设置] / rotateBtn 动画[显式播放]）
static bool createGroup(UICornerstone::UICornerstone& ui, float dy, float xScale, float yScale,
                        const char* tag, Control& btn, Control& img, Control& ani) {
    btn = ui.CreateButton(u8"缩放测试", kBtn.x, kBtn.y + dy, kBtn.w, kBtn.h, xScale, yScale);
    img = ui.CreateButton(u8"", kImg.x, kImg.y + dy, kImg.w, kImg.h, xScale, yScale);
    img.SetString("normal-image", kImgNormal);
    img.SetString("hover-image", kImgHover);
    img.SetString("pressed-image", kImgPressed);
    ani = ui.CreateAnimatedButton(kAnimJsonc, kAni.x, kAni.y + dy, kAni.w, kAni.h, xScale, yScale);
    ani.SetBool("playing", true);

    bool ok = btn.IsValid() && img.IsValid() && ani.IsValid();
    std::printf(u8"[sample_scale] %s (%.1fx%.1f) created: btn=%s imgBtn=%s ani=%s playing=%s\n",
                tag, xScale, yScale, btn.IsValid() ? u8"ok" : u8"nil",
                img.IsValid() ? u8"ok" : u8"nil", ani.IsValid() ? u8"ok" : u8"nil",
                ani.IsValid() ? u8"yes" : u8"FAIL");
    return ok;
}

int main(int argc, char* argv[]) {
    const char* backend = "sdl3";
    const char* extra[] = {"backend="};
    uicorn_sample::AutoTimer autoTimer;
    int autoSeconds = autoTimer.parse(argc, argv, extra, 1);
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "backend=", 8) == 0) backend = argv[i] + 8;
    }
    std::printf(u8"[sample_scale] autoSeconds=%d backend=%s\n", autoSeconds, backend);

    auto ui = UICornerstone::UICornerstone::Create(
        UICornerstone::UICornerstone::Config{}
            .WithBackend(backend)
            .WithWindow(u8"Scale Binding Sample: 上方1x vs 下方2x（点击按钮打印 CLICK）", 1600, 1140));
    if (!ui) {
        std::printf(u8"Create failed\n");
        return 1;
    }

    // ── 1x 组 ──
    Control btn1, img1, ani1;
    bool ok1 = createGroup(*ui, 0.0f, 1.0f, 1.0f, u8"[1x]", btn1, img1, ani1);

    // ── 2x 组（布局 rect 与 1x 组一致，仅 y 平移；参数尽可能一致）──
    Control btn2, img2, ani2;
    bool ok2 = createGroup(*ui, kShiftY, 2.0f, 2.0f, u8"[2x]", btn2, img2, ani2);

    // ── 1x 等价组：不传参数（默认 1.0f，向后兼容性验证）──
    auto btn0 = ui->CreateButton(u8"缩放测试", kDef.x, kDef.y, kDef.w, kDef.h);

    if (!btn0.IsValid()) {
        std::printf(u8"[sample_scale] FAIL: 默认参数按钮创建失败（句柄无效）\n");
        return 1;
    }

    // ── 所有按钮：红色边框 + click 回调（人工点击打印日志）──
    styleAndRespond(btn1, u8"[1x]btn");
    styleAndRespond(btn2, u8"[2x]btn");
    styleAndRespond(btn0, u8"[default]btn");
    styleAndRespond(img1, u8"[1x]imgBtn");
    styleAndRespond(img2, u8"[2x]imgBtn");
    styleAndRespond(ani1, u8"[1x]aniBtn");
    styleAndRespond(ani2, u8"[2x]aniBtn");
    std::printf(u8"[sample_scale] 3 变体 x 2 组 + 1 默认参数组 创建成功\n");

    // ── 布局 rect 断言：缩放不得改变布局（引擎语义：内容缩放、rect 不变）──
    const UIRect exp[3][2] = {
        {kBtn, {kBtn.x, kBtn.y + kShiftY, kBtn.w, kBtn.h}},
        {kImg, {kImg.x, kImg.y + kShiftY, kImg.w, kImg.h}},
        {kAni, {kAni.x, kAni.y + kShiftY, kAni.w, kAni.h}},
    };
    const Control* pair[3][2] = {{&btn1, &btn2}, {&img1, &img2}, {&ani1, &ani2}};
    const char* names[3] = {"btn(text)", "imgButton(3-state)", "ani(rotateBtn)"};

    bool rectOk = ok1 && ok2;
    for (int i = 0; i < 3; i++) {
        for (int g = 0; g < 2; g++) {
            UIRect r = pair[i][g]->GetRect();
            bool eq = r.x == exp[i][g].x && r.y == exp[i][g].y &&
                      r.w == exp[i][g].w && r.h == exp[i][g].h;
            std::printf(u8"[sample_scale] %s %dx rect=(%.0f,%.0f,%.0f,%.0f) expect=(%.0f,%.0f,%.0f,%.0f) -> %s\n",
                        names[i], g == 0 ? 1 : 2, r.x, r.y, r.w, r.h,
                        exp[i][g].x, exp[i][g].y, exp[i][g].w, exp[i][g].h, eq ? u8"PASS" : u8"FAIL");
            rectOk &= eq;
        }
    }
    // 默认参数组：不传缩放 == 1.0f，rect 亦等于创建参数
    UIRect r0 = btn0.GetRect();
    bool defOk = r0.x == kDef.x && r0.y == kDef.y && r0.w == kDef.w && r0.h == kDef.h;
    std::printf(u8"[sample_scale] btn(default) rect=(%.0f,%.0f,%.0f,%.0f) expect=(%.0f,%.0f,%.0f,%.0f) -> %s\n",
                r0.x, r0.y, r0.w, r0.h, kDef.x, kDef.y, kDef.w, kDef.h, defOk ? u8"PASS" : u8"FAIL");
    rectOk &= defOk;

    // ── 主循环：渲染 auto=<秒> 后自动退出（人工模式关闭窗口退出）──
    using Clock = std::chrono::steady_clock;
    auto last = Clock::now();
    int frame = 0;
    while (!ui->IsQuitRequested()) {
        ui->ProcessEvents();

        if (autoTimer.expired()) {
            std::printf(u8"[sample_scale] PASS: auto=%d 秒渲染完成后自动退出\n", autoSeconds);
            break;
        }

        ui->Update(0.016);
        ui->Clear();
        ui->Render();
        ui->Present();
        ui->Clear();

        auto now = Clock::now();
        std::this_thread::sleep_for(std::chrono::milliseconds(
            std::max(1, 16 - static_cast<int>(std::chrono::duration<double, std::milli>(now - last).count()))));
        last = now;
    }
    std::printf(u8"[sample_scale] done (frames=%d). 2x 内容应为 1x 的两倍且框体一致；点击任意按钮打印 CLICK 日志\n", frame);
    return rectOk ? 0 : 2;
}