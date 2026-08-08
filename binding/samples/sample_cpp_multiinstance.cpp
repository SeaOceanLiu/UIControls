// UICornerstone C++ Binding — 示例：多实例（多窗口）— 两个独立窗口各自一套控件树
// 许可证 MIT。编译：链接 UICornerstoneBinding 即可；核心库/后端经 LoadLibrary 动态加载。
// 命令行参数：backend=<后端名> 指定加载哪个后端（sdl3/sfml/raylib，任意顺序，缺省 sdl3）
//
// 布局：
//   Window A (640×480)          Window B (640×480)
//   ┌──────────────────┐        ┌──────────────────┐
//   │ Label "Window A"  │        │ Label "Window B"  │
//   │ EditBox           │        │ Label 消息显示区   │
//   │ Button "Send to B"│        │                  │
//   └──────────────────┘        └──────────────────┘
//
// 每个实例是独立窗口 + 独立控件树 + 独立事件循环，互不干扰；
// 跨实例通信通过普通 C++ 回调（控件句柄属于各自实例）。
// 本样例演示：Window A 按钮点击 → 读取 A 的 EditBox → 更新 B 的 Label。
#include "UICornerstone.h"
#include "Control.h"
#include "Event.h"
#include "PropertyNames.h"

#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <thread>

int main(int argc, char* argv[]) {
    // 命令行参数任意顺序：backend=<后端名> 指定加载哪个后端（缺省 sdl3）
    const char* backend = "sdl3";
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "backend=", 8) == 0) backend = argv[i] + 8;
        else std::printf("WARN: 忽略无法识别的参数: %s\n", argv[i]);
    }
    // ── 创建两个独立窗口实例 ──
    auto uiA = UICornerstone::UICornerstone::Create(
        UICornerstone::UICornerstone::Config{}
            .WithBackend(backend)
            .WithWindow("C++ Multi-Instance Sample A", 640, 480));
    auto uiB = UICornerstone::UICornerstone::Create(
        UICornerstone::UICornerstone::Config{}
            .WithBackend(backend)
            .WithWindow("C++ Multi-Instance Sample B", 640, 480));
    if (!uiA || !uiB) {
        std::printf("Create failed\n");
        return 1;
    }

    // ── Window A 控件：标题 / 输入 / 发送按钮 / 接收区 ──
    auto labelA = uiA->CreateLabel("Window A (independent instance)", 18.0f, 20, 12, 420, 30);
    labelA.SetColor(PropertyNames::kText, {74, 144, 217, 255});
    auto editA = uiA->CreateEditBox(20, 60, 380, 32);
    editA.SetString(PropertyNames::kTextContent, "hello from A");
    auto btnA = uiA->CreateButton("Send to B", 20, 110, 140, 36);
    auto msgA = uiA->CreateLabel("Message: (none)", 16.0f, 20, 160, 420, 60);
    auto statusA = uiA->CreateLabel("Status: idle", 14.0f, 20, 230, 380, 28);

    // ── Window B 控件：标题 / 输入 / 发送按钮 / 接收区 ──
    auto labelB = uiB->CreateLabel("Window B (independent instance)", 18.0f, 20, 12, 420, 30);
    labelB.SetColor(PropertyNames::kText, {74, 144, 217, 255});
    auto editB = uiB->CreateEditBox(20, 60, 380, 32);
    editB.SetString(PropertyNames::kTextContent, "hello from B");
    auto btnB = uiB->CreateButton("Send to A", 20, 110, 140, 36);
    auto msgB = uiB->CreateLabel("Message: (none)", 16.0f, 20, 160, 420, 60);
    auto statusB = uiB->CreateLabel("Status: idle", 14.0f, 20, 230, 380, 28);

    // ── 跨实例通信（双向）：A 按钮 → B 标签；B 按钮 → A 标签 ──
    btnA.SetCallback(PropertyNames::kEventClick, [&](const Event&) {
        std::string text = editA.GetString(PropertyNames::kTextContent);
        msgB.SetString(PropertyNames::kCaption, "Message: " + text);
        statusA.SetString(PropertyNames::kCaption, "Status: sent to B");
        std::printf("[A] sent to B: %s\n", text.c_str());
    });
    btnB.SetCallback(PropertyNames::kEventClick, [&](const Event&) {
        std::string text = editB.GetString(PropertyNames::kTextContent);
        msgA.SetString(PropertyNames::kCaption, "Message: " + text);
        statusB.SetString(PropertyNames::kCaption, "Status: sent to A");
        std::printf("[B] sent to A: %s\n", text.c_str());
    });

    // ── Embedded 模式主循环：两个实例都必须显式驱动 ──
    using Clock = std::chrono::steady_clock;
    auto last = Clock::now();
    int frame = 0;
    bool autoMode = std::getenv("UICORN_AUTO") != nullptr;
    while (!uiA->IsQuitRequested() && !uiB->IsQuitRequested()) {
        // 全局事件泵：依次驱动每个实例，直到队列中没有任何事件可处理
        // （每个实例的 ProcessEvents 只消费属于自己窗口的事件，其余事件
        //  留在队列由对应实例处理；返回 true 表示处理了至少一个事件）
        int processedCount = 1;
        while (processedCount > 0) {
            processedCount = 0;
            int pa = uiA->ProcessEvents() ? 1 : 0;
            int pb = uiB->ProcessEvents() ? 1 : 0;
            processedCount = pa + pb;
        }

        if (autoMode) {
            // AUTO 模式：A 按钮 (20,110,140,36)→(90,128)；B 按钮 (20,110,140,36)→(90,128)
            if (frame == 0) { uiA->PushMouseButton(1, 90, 128, true); std::fprintf(stderr, "[A] btn down\n"); }
            if (frame == 2) { uiA->PushMouseButton(1, 90, 128, false); std::fprintf(stderr, "[A] btn up\n"); }
            if (frame == 4) {
                std::string got = msgB.GetString(PropertyNames::kCaption);
                std::fprintf(stderr, "[A] msgB = \"%s\"\n", got.c_str());
            }
            if (frame == 8) { uiB->PushMouseButton(1, 90, 128, true); std::fprintf(stderr, "[B] btn down\n"); }
            if (frame == 10) { uiB->PushMouseButton(1, 90, 128, false); std::fprintf(stderr, "[B] btn up\n"); }
            if (frame == 12) {
                std::string got = msgA.GetString(PropertyNames::kCaption);
                std::fprintf(stderr, "[B] msgA = \"%s\"\n", got.c_str());
            }
            if (frame >= 240) break;
        }

        auto now = Clock::now();
        double dt = std::chrono::duration<double>(now - last).count();
        last = now;
        if (dt > 0.1) dt = 0.1;

        uiA->Update(dt);
        uiB->Update(dt);
        uiA->Clear();
        uiA->Render();
        uiA->Present();
        // 单窗口架构后端（raylib）的非首个实例为 headless：跳过其渲染/
        // 交换（否则内容串扰到主实例窗口），多窗口后端按能力位正常渲染
        if (uiA->GetBackendCapabilities() & UICORN_BACKEND_CAP_MULTI_WINDOW) {
            uiB->Clear();
            uiB->Render();
            uiB->Present();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
        ++frame;
    }
    std::printf("[DONE] %d frames (auto=%d)\n", frame, (int)autoMode);

    uiA->Shutdown();
    uiB->Shutdown();
    return 0;
}
