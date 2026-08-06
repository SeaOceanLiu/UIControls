// UICornerstone C++ Binding — 示例：Embedded 模式（用户循环嵌入 UI）
// 许可证 MIT。编译：与 UICornerstoneBinding + UICornerstone(核心) 链接。
#include "UICornerstone.h"
#include "Control.h"
#include "Event.h"
#include "Names.h"

#include <cstdio>
#include <chrono>
#include <thread>

int main() {
    auto ui = UICornerstone::UICornerstone::Create(
        UICornerstone::UICornerstone::Config{}
            .WithBackend("sdl3")
            .WithWindow("C++ Embed Sample", 800, 600));
    if (!ui) {
        std::printf("Create failed\n");
        return 1;
    }

    auto label  = ui->CreateLabel("Embedded loop", 24.0f, 20, 20, 240, 40);
    auto button = ui->CreateButton("Toggle", 20, 80, 120, 36);
    button.SetCallback(UICornerstone::Names::kClick, [&](const Event&) {
        label.SetVisible(!label.IsVisible());
    });

    // ── 用户主循环（游戏/应用逻辑在前，UI 帧在其中）──
    using Clock = std::chrono::steady_clock;
    auto last = Clock::now();

    while (!ui->IsQuitRequested()) {
        ui->ProcessEvents();

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
    }

    ui->Shutdown();
    std::printf("Embed sample exited cleanly\n");
    return 0;
}