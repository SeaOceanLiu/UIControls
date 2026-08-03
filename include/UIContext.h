#ifndef UICONTEXT_H
#define UICONTEXT_H

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <memory>
#include <vector>
#include <utility>
#include "UICornerstoneAPI.h"
#include "Utility.h"

class Window;
class RenderDevice;
class InputBackend;
class TextRenderer;
class ResourceProvider;
class MainWindow;
class Bench;
class EventQueue;
class DataContext;
class FocusManager;
class Popup;
class BackendManager;
class Control;

// 实例上下文：聚合一个 UI 实例（窗口或视口）的全部状态。
// UIInstance = UIContext*（见 UICornerstoneAPI.h）。
struct UIContext {
    // ── 层级关系 ──
    UIContext* owner = nullptr;        // 拥有后端的父实例，nullptr = 自己是 owner
    bool ownsBackend = true;           // false = 共享 owner 的后端
    std::vector<UIContext*> children;  // 子视口列表（CreateViewport 注册）

    // ── Backend 资源 ──
    // 当 ownsBackend == false 时以下指针从 owner 继承
    const UIBackendCallbacks* callbacks = nullptr;
    BackendManager* backendManager = nullptr;
    Window* window = nullptr;
    RenderDevice* renderDevice = nullptr;
    InputBackend* inputBackend = nullptr;
    TextRenderer* textRenderer = nullptr;
    ResourceProvider* resourceProvider = nullptr;

    // ── 实例配置 ──
    std::string windowTitle;   // 窗口标题，空 → "UICornerstone"
    int windowWidth = 0;       // 0 → 默认 1024
    int windowHeight = 0;      // 0 → 默认 768
    uint32_t windowFlags = 0;  // 跨后端统一窗口标志（UIWindowFlags，见 Window.h）
    std::string resourceRoot;  // 资源根目录，空 → ConstDef::pathPrefix

    // ── 视口状态 ──
    bool initialized = false;
    bool quit = false;
    bool destroying = false;   // DestroyInstance 期间置位，防回调重入
    SRect viewport{0, 0, 1024, 768};
    UIInstance activeViewport = nullptr;  // 仅 owner 使用，当前焦点视口；nullptr = 无子视口或焦点在 owner 树
    FocusManager* focusManager = nullptr;  // 每实例独立焦点管理

    // ── 原单例（由 UIContext 持有） ──
    Bench* bench = nullptr;
    MainWindow* mainWindow = nullptr;
    EventQueue* eventQueue = nullptr;
    DataContext* dataContext = nullptr;

    // ── 动作与控件查找 ──
    std::unordered_map<std::string, std::pair<UIActionCallback, void*>> actions;
    std::unordered_map<std::string, UIControlHandle> controlsById;

    // ── 注入事件队列 ──
    std::queue<UIEvent> queuedEvents;

    // ── 弹窗 / 菜单生命周期池 ──
    std::vector<std::shared_ptr<Popup>> popupPool;
    std::vector<std::shared_ptr<Control>> menuPool;

    // ── 实例内字符串缓冲（GetControlId 输出） ──
    std::string strBuf;

    // ── 调试标识 ──
    uint32_t instanceId = 0;
    std::string debugLabel;

    // ── 生命周期（全有或全无） ──
    bool initialize();
    void destroy();

    // ── 最近实例解析（浮层控件无 parent 归属时的兜底） ──
    static UIContext* getLastInstance() { return s_lastInstance; }
    static void setLastInstance(UIContext* ctx) { s_lastInstance = ctx; }

    // ── 实例活跃注册表（析构守卫） ──
    // 静态/全局残留控件（shared_ptr 逃逸控件树）在进程退出阶段析构时，
    // 其 m_context 可能已随 DestroyInstance 释放（悬垂）。析构路径中的
    // 上下文访问（如 unregisterControl）必须通过 isActive 确认实例仍存活。
    // 实现用堆分配容器（永不析构）：跨编译单元的静态析构顺序无保证，
    // 若容器先于残留控件析构，查询即访问已销毁对象。
    static void registerActive(UIContext* ctx);
    static void unregisterActive(UIContext* ctx);
    static bool isActive(UIContext* ctx);

private:
    static UIContext* s_lastInstance;
    static std::unordered_set<UIContext*>& activeContexts();
};

// 日志辅助：实例标签前缀
#define UI_LOG(instance, fmt, ...) \
    do { \
        if ((instance) && !(instance)->debugLabel.empty()) { \
            printf("[%s] " fmt "\n", (instance)->debugLabel.c_str(), ##__VA_ARGS__); \
        } \
    } while (0)

#endif // UICONTEXT_H
