// UICornerstone C++ Binding — 示例：Hosted 模式（UI 循环托管游戏逻辑）
// 许可证 MIT。编译：与 UICornerstoneBinding + UICornerstone(核心) 链接。
#include "UICornerstone.h"
#include "Control.h"
#include "Event.h"
#include "Names.h"

#include <cstdio>

int main() {
    auto ui = UICornerstone::UICornerstone::Create(
        UICornerstone::UICornerstone::Config{}
            .WithBackend("sdl3")
            .WithWindow("C++ Hosted Sample", 800, 600));
    if (!ui) {
        std::printf("Create failed\n");
        return 1;
    }

    // ── 控件 ──
    auto button = ui->CreateButton("Click Me", 20, 20, 140, 36);
    auto slider = ui->CreateSlider(20, 70, 260, 32, 0.f, 100.f, 50.f);
    auto label  = ui->CreateLabel("Slider: 50.0", 20.0f, 120, 110, 220, 40);

    int clickCount = 0;
    button.SetCallback(UICornerstone::Names::kClick, [&](const Event&) {
        ++clickCount;
        label.SetText("Clicked " + std::to_string(clickCount) + " times");
    });

    slider.SetCallback(UICornerstone::Names::kValueChanged, [&](const Event& e) {
        label.SetText("Slider: " + std::to_string((int)e.GetValueChanged()));
    });

    // ── Hosted 模式：UI 主循环 + 每帧逻辑回调 ──
    return ui->Run(
        [](double dt) { (void)dt; },   // 游戏逻辑更新
        []() { });                     // 每帧绘制后回调
}