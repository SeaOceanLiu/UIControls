// UICornerstone C++ Binding — 示例：Embedded 模式（用户循环嵌入 UI）
// 许可证 MIT。编译：链接 UICornerstoneBinding 即可；核心库/后端经 LoadLibrary 动态加载。
// 命令行参数：backend=<后端名> 指定加载哪个后端（sdl3/sfml/raylib，任意顺序，缺省 sdl3）
#include "UICornerstone.h"
#include "Control.h"
#include "Event.h"
#include "PropertyNames.h"
#include "UIEventFactory.h"

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
            .WithWindow("C++ Embed Sample", 800, 600));
    if (!ui) {
        std::printf("Create failed\n");
        return 1;
    }

    auto label  = ui->CreateLabel("Embedded loop", 24.0f, 20, 20, 240, 40);
    auto button = ui->CreateButton("Toggle", 20, 80, 120, 36);
    int count = 0;
    button.SetCallback(PropertyNames::kEventClick, [&](const Event&) {
        bool vis = label.GetBool(PropertyNames::kVisible);
        label.SetBool(PropertyNames::kVisible, !vis);
        label.SetString(PropertyNames::kCaption, "Clicked " + std::to_string(++count));
    });

    bool autoMode = std::getenv("UICORN_AUTO") != nullptr;

    // ── 用户主循环（游戏/应用逻辑在前，UI 帧在其中）──
    using Clock = std::chrono::steady_clock;
    auto last = Clock::now();
    int frame = 0;

    while (!ui->IsQuitRequested()) {
        ui->ProcessEvents();

        if (autoMode) {
            int f = frame;
            if (f == 0)  ui->PushMouseButton(1, 80, 98, true);
            if (f == 2)  ui->PushMouseButton(1, 80, 98, false);
            if (f >= 240) {
                std::printf("[AUTO] done, %d frames\n", frame);
                break;
            }
        }

        auto now = Clock::now();
        double dt = std::chrono::duration<double>(now - last).count();
        last = now;
        if (dt > 0.1) dt = 0.1;

        // ... 用户自己的逻辑更新 ...
        ui->Update(dt);

        // ── 渲染帧 ──
        ui->Clear();
        ui->Render();
        ui->Present();

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        ++frame;
    }

    ui->Shutdown();
    std::printf("Embed sample exited cleanly\n");
    return 0;
}