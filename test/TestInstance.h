// 测试适配头（多实例阶段）：为旧式测试提供单实例全局访问。
// 用法：在测试文件的 include 区块【最后】include 本头（须在控件头之后，
// 以覆盖 ControlBase.h 中引用 m_context 的 BENCH/MAINWIN/GET_* 宏），
// 然后 main() 改为：
//     int main(int argc, char* argv[]) {
//         return TestRunMain<MyApp>(argc, argv);
//     }
#ifndef TestInstanceH
#define TestInstanceH

#include "UIContext.h"
#include "MainWindow.h"
#include "Bench.h"
#include "RenderDevice.h"
#include <cstdlib>
#include <cstring>
#include <thread>
#include <chrono>

extern "C" UIBackendCallbacks* GetUIBackendCallbacks(void);

// 当前测试实例（每个测试可执行文件只有一个 TU，inline 保证单份）
inline UIInstance g_uiInstance = nullptr;

// ── 覆盖控件内上下文宏 ──
// 测试的全局函数 / AppCallbacks 回调不属于任何控件，m_context 不可用，
// 统一解析到当前测试实例。
#undef BENCH
#undef MAINWIN
#undef GET_RENDERDEVICE
#undef GET_TEXTRENDERER
#undef GET_INPUTBACKEND
#undef GET_RESOURCEPROVIDER
#undef GET_FOCUSMANAGER

#define BENCH (g_uiInstance ? g_uiInstance->bench : nullptr)
#define MAINWIN (g_uiInstance ? g_uiInstance->mainWindow : nullptr)
#define GET_RENDERDEVICE (g_uiInstance ? g_uiInstance->renderDevice : nullptr)
#define GET_TEXTRENDERER (g_uiInstance ? g_uiInstance->textRenderer : nullptr)
#define GET_INPUTBACKEND (g_uiInstance ? g_uiInstance->inputBackend : nullptr)
#define GET_RESOURCEPROVIDER (g_uiInstance ? g_uiInstance->resourceProvider : nullptr)
#define GET_FOCUSMANAGER (g_uiInstance ? g_uiInstance->focusManager : nullptr)

// 无人值守自动退出：命令行参数任意顺序，识别 "auto=<秒>" → 定时线程经
// UICornerstone_PushUIEvent 投递 WINDOW_CLOSE（互斥事件队列，实例销毁后 Push
// 走 null 安全路径，无悬垂访问）。用于自动化回归/CI：窗口无需人工关闭，
// 达到时长后测试自行退出；无法识别的参数 WARN 提示后忽略。
inline void scheduleAutoQuit(int argc, char* argv[]) {
    int sec = 0;
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "auto=", 5) == 0) {
            sec = atoi(argv[i] + 5);
        } else {
            printf("WARN: 忽略无法识别的参数: %s\n", argv[i]);
        }
    }
    if (sec <= 0) return;
    printf("[TestInstance] auto-quit scheduled in %ds (无人值守)\n", sec); fflush(stdout);
    std::thread([sec]() {
        std::this_thread::sleep_for(std::chrono::seconds(sec));
        UIEvent ue;
        std::memset(&ue, 0, sizeof(ue));
        ue.type = UI_EVENT_WINDOW_CLOSE;
        UICornerstone_PushUIEvent(g_uiInstance, &ue);
    }).detach();
}

// main() 便捷封装：CreateInstance → run(&app) → DestroyInstance
// 默认窗口 1400×900（部分测试布局延伸至 x=1250，超过 1024 默认宽），
// 单测可用 TestRunMain<AppT, 宽, 高> 定制；Flags 为跨后端统一窗口标志（UIWindowFlags）。
template <class AppT, int DefW = 1400, int DefH = 900, uint32_t Flags = 0>
int TestRunMain(int argc, char* argv[]) {
    (void)argc; (void)argv;
    UIInstanceConfig cfg = UI_INSTANCE_CONFIG_DEFAULT;
    cfg.windowWidth = DefW;
    cfg.windowHeight = DefH;
    cfg.windowFlags = Flags;
    g_uiInstance = UICornerstone_CreateInstance(GetUIBackendCallbacks(), &cfg);
    if (!g_uiInstance) {
        printf("[TestInstance] CreateInstance failed\n");
        return 1;
    }
    scheduleAutoQuit(argc, argv);
    AppT app;
    int rc = g_uiInstance->mainWindow->run(&app);
    UICornerstone_DestroyInstance(g_uiInstance);
    g_uiInstance = nullptr;
    printf("[TestInstance] run returned %d, exiting\n", rc); fflush(stdout);
    return rc;
}

#endif // TestInstanceH
