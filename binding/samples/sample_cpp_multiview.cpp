// UICornerstone C++ Binding — 示例：多实例（子视口）— 一个窗口内两个 Bench
// 许可证 MIT。编译：链接 UICornerstoneBinding 即可；核心库/后端经 LoadLibrary 动态加载。
//
// 布局：
//   ┌────────────────────┐
//   │ Bench A (视口1)     │  ← 左上 (0,0,400,300)
//   │ Label/EditBox/Button│
//   ├────────────────────┤
//   │                    │  ← 右下 (400,300,400,300)
//   │    Bench B (视口2)  │
//   └────────────────────┘
//
// 每个 Bench 内：Label（caption 标明是哪个 Bench）、EditBox、Button。
// Button 按下 → 读取本 Bench EditBox 的内容 → CreateDialog 弹窗显示。
#include "UICornerstone.h"
#include "Control.h"
#include "Event.h"
#include "PropertyNames.h"

#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <thread>

namespace {

// 单个 Bench 区域：封装一个子视口内的控件组
struct Bench {
    ::UICornerstone::UICornerstone* vp = nullptr;   // 子视口实例（owner 的子视口）
    std::string name;                               // "Bench A" / "Bench B"
    Control edit;

    Bench(::UICornerstone::UICornerstone* viewport, const char* title)
        : vp(viewport), name(title) {}

    void Build(const char* initialText) {
        // Label：caption 标明是哪个 Bench
        auto label = vp->CreateLabel(name, 20.0f, 12, 10, 200, 24);
        label.SetColor(PropertyNames::kText, {74, 144, 217, 255});

        // EditBox：内容源
        edit = vp->CreateEditBox(12, 42, 220, 28);
        edit.SetString(PropertyNames::kTextContent, initialText);

        // Button：读取 EditBox → popup 弹窗显示（内容文本 Label 放在弹窗中央）
        auto btn = vp->CreateButton("Show", 12, 82, 100, 30);
        btn.SetCallback(PropertyNames::kEventClick, [this](const Event&) {
            std::string content = edit.GetString(PropertyNames::kTextContent);
            std::printf("[%s] popup: %s\n", name.c_str(), content.c_str());
            auto dialog = vp->CreateDialog("OK", "", 0, 0, 280, 120);   // 居中弹窗
            // 内容文本：Label 坐标相对 Dialog（弹窗 280x120，居中于内容区）
            auto label = vp->CreateLabel(content, 14.0f, 20, 30, 240, 60);
            dialog.AddChild(label);
        });
    }
};

} // namespace

int main() {
    auto ui = UICornerstone::UICornerstone::Create(
        UICornerstone::UICornerstone::Config{}
            .WithBackend("sdl3")
            .WithWindow("C++ Multi-Viewport Sample", 800, 600));
    if (!ui) {
        std::printf("Create failed\n");
        return 1;
    }

    // 一个窗口内的两个子视口：左上 + 右下（各自独立控件树，共享后端）
    auto vpA = ui->CreateViewport(0, 0, 400, 300);
    auto vpB = ui->CreateViewport(400, 300, 400, 300);
    if (!vpA || !vpB) {
        std::printf("CreateViewport failed\n");
        return 1;
    }

    Bench benchA(vpA.get(), "Bench A");
    Bench benchB(vpB.get(), "Bench B");
    benchA.Build("Hello from Bench A");
    benchB.Build("Hello from Bench B");

    if (std::getenv("UICORN_AUTO")) {
        std::fprintf(stderr, "[A] AUTO mode: inject clicks into both viewports\n");
    }

    // ── 主循环（多视口必须显式驱动：Run() 只驱动 owner，子视口需各自
    //    ProcessEvents/Update/Render）──
    using Clock = std::chrono::steady_clock;
    auto last = Clock::now();
    int frame = 0;
    while (!ui->IsQuitRequested()) {
        ui->ProcessEvents();        // 窗口输入轮询（按坐标路由到子视口）+ owner 注入队列
        vpA->ProcessEvents();       // 子视口注入队列（各自消费）
        vpB->ProcessEvents();

        if (std::getenv("UICORN_AUTO")) {
            // 事件坐标为窗口绝对坐标：vpA 按钮在 (12,82)，vpB 按钮在 (412,382)
            if (frame == 0) { vpA->PushMouseButton(1, 62, 97, true);  std::fprintf(stderr, "[A] Bench A btn down\n"); }
            if (frame == 2) { vpA->PushMouseButton(1, 62, 97, false); }
            if (frame == 6) { vpB->PushMouseButton(1, 462, 397, true); std::fprintf(stderr, "[A] Bench B btn down\n"); }
            if (frame == 8) { vpB->PushMouseButton(1, 462, 397, false); }
            if (frame >= 240) break;
        }

        auto now = Clock::now();
        double dt = std::chrono::duration<double>(now - last).count();
        last = now;
        if (dt > 0.1) dt = 0.1;

        vpA->Update(dt);
        vpB->Update(dt);
        ui->Clear();
        vpA->Render();              // 各自 clip 到自身视口区域
        vpB->Render();
        ui->Present();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        ++frame;
    }
    std::printf("[AUTO] done, %d frames\n", frame);
    ui->Shutdown();
    return 0;
}
