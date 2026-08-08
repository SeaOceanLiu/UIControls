// UICornerstone C++ Binding — 示例：Hosted 模式（UI 循环托管游戏逻辑）
// 许可证 MIT。编译：链接 UICornerstoneBinding 即可；核心库/后端经 LoadLibrary 动态加载。
// 命令行参数：backend=<后端名> 指定加载哪个后端（sdl3/sfml/raylib，任意顺序，缺省 sdl3）
#include "UICornerstone.h"
#include "Control.h"
#include "Event.h"
#include "PropertyNames.h"

#include <cstdio>
#include <chrono>
#include <thread>
#include <cstdlib>

int main(int argc, char* argv[]) {
    // 命令行参数任意顺序：backend=<后端名> 指定加载哪个后端（缺省 sdl3）
    const char* backend = "sdl3";
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "backend=", 8) == 0) backend = argv[i] + 8;
        else std::printf("WARN: 忽略无法识别的参数: %s\n", argv[i]);
    }
    auto ui = UICornerstone::UICornerstone::Create(
        UICornerstone::UICornerstone::Config{}
            .WithBackend(backend)
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
    button.SetCallback(PropertyNames::kEventClick, [&](const Event&) {
        ++clickCount;
        label.SetString(PropertyNames::kCaption, "Clicked " + std::to_string(clickCount) + " times");
    });

    slider.SetCallback(PropertyNames::kEventValueChanged, [&](const Event& e) {
        label.SetString(PropertyNames::kCaption, "Slider: " + std::to_string((int)e.GetValueChanged()));
    });

    // ── Hosted 模式：UI 主循环 + 每帧逻辑回调 ──
    if (std::getenv("UICORN_AUTO")) {
        // AUTO 模式：手动循环 + 注入点击/拖动，用于回归测试
        using Clock = std::chrono::steady_clock;
        auto last = Clock::now();
        int frame = 0;
        while (!ui->IsQuitRequested()) {
            ui->ProcessEvents();
            if (frame == 0) { ui->PushMouseButton(1, 90, 38, true); std::fprintf(stderr, "[A] btn down\n"); }
            if (frame == 2) { ui->PushMouseButton(1, 90, 38, false); std::fprintf(stderr, "[A] btn up\n"); }
            if (frame == 4) { ui->PushMouseButton(1, 150, 86, true); std::fprintf(stderr, "[A] slider down\n"); }
            if (frame == 6) { ui->PushMouseMove(200, 86); std::fprintf(stderr, "[A] slider move\n"); }
            if (frame == 8) { ui->PushMouseButton(1, 200, 86, false); std::fprintf(stderr, "[A] slider up\n"); }
            if (frame >= 240) break;
            auto now = Clock::now();
            double dt = std::chrono::duration<double>(now - last).count();
            last = now;
            if (dt > 0.1) dt = 0.1;
            ui->Update(dt);
            ui->Clear();
            ui->Render();
            ui->Present();
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            ++frame;
        }
        std::printf("[AUTO] done, %d frames\n", frame);
        ui->Shutdown();
        return 0;
    }

    return ui->Run(
        [](double dt) { (void)dt; },   // 游戏逻辑更新
        []() { });                     // 每帧绘制后回调
}