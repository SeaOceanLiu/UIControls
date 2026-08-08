// UICornerstone C++ Binding — 主类入口
// 许可证 MIT。仅依赖 include/UICornerstoneAPI.h 的 C ABI，不引用核心库内部头。
#ifndef UICORNERSTONE_BINDING_UICORNERSTONE_H
#define UICORNERSTONE_BINDING_UICORNERSTONE_H

#include <string>
#include <memory>
#include <functional>
#include "UICornerstoneAPI.h"

class Control;
class Event;

namespace UICornerstone {

// 内部实现（pimpl）。定义见 src/Impl.h。
struct Impl;

class UICornerstone {
public:
    struct Config {
        std::string backend = "sdl3";             // "sdl3" | "sfml" | "raylib"
        std::string backendSearchPath;            // 后端 DLL 搜索目录（空=exe 同目录/系统搜索）
        std::string coreLibraryDir;               // 核心 DLL 所在目录（空=exe 同目录/系统搜索）
        std::string resourceRoot;                 // 资源根路径（空 → 核心默认 exe 目录/assets，任何 cwd 均可运行）
        std::string windowTitle    = "UICornerstone";
        int windowWidth  = 1024;
        int windowHeight = 768;
        uint32_t windowFlags = 0;                 // 对齐 UIWindowFlags

        Config& WithBackend(const std::string& name)          { backend = name; return *this; }
        Config& WithBackendSearchPath(const std::string& p)   { backendSearchPath = p; return *this; }
        Config& WithCoreLibraryDir(const std::string& p)      { coreLibraryDir = p; return *this; }
        Config& WithResourceRoot(const std::string& r)        { resourceRoot = r; return *this; }
        Config& WithWindow(const std::string& t, int w, int h)
            { windowTitle = t; windowWidth = w; windowHeight = h; return *this; }
        Config& WithWindowFlags(uint32_t f)                   { windowFlags = f; return *this; }
    };

    // 通过 Config 创建（核心/后端 DLL 均经 LoadLibrary 纯动态加载，见 §5.15）
    static std::unique_ptr<UICornerstone> Create(const Config& config);
    // 通过回调查表创建（不管理后端生命周期）
    static std::unique_ptr<UICornerstone> Create(const UIBackendCallbacks* callbacks,
                                                 const Config& config = Config{});

    ~UICornerstone();
    UICornerstone(const UICornerstone&) = delete;
    UICornerstone& operator=(const UICornerstone&) = delete;

    // ── 实例句柄 ──
    UIInstance Handle() const;

    // ── Hosted 模式 ──
    using FrameCallback  = std::function<void(double deltaTime)>;
    using RenderCallback = std::function<void()>;
    int Run(FrameCallback update, RenderCallback onRender = nullptr);

    // ── Embedded 模式 ──
    bool ProcessEvents();   // 返回是否处理了至少一个事件（多实例主循环调度用）
    void Update(double deltaTime);
    void Render();
    void Clear();
    void Present();
    bool IsQuitRequested() const;
    // 后端能力位（UICORN_BACKEND_CAP_*，按位与）：调用方据此决定行为——
    // 例如多实例双窗口渲染仅在 MULTI_WINDOW 能力下执行（单窗口架构后端
    // 如 raylib 非首个实例为 headless，渲染会串扰到主实例窗口）。
    uint32_t GetBackendCapabilities() const;
    void Shutdown();

    // ── 子视口 ──
    std::unique_ptr<UICornerstone> CreateViewport(float x, float y, float w, float h);

    // ── 资源路径（仅影响 Binding 侧路径解析；核心库 config 已固化）──
    void SetResourceRoot(const std::string& path);
    std::string GetResourceRoot() const;
    std::string ResolveResource(const std::string& relativePath) const;

    // ── 布局 ──
    bool LoadLayout(const std::string& jsonContent);
    bool LoadLayoutFromFile(const std::string& filePath);
    Control FindControl(const std::string& id);

    // ── 控件工厂 ──
    Control CreateButton(const std::string& text, float x, float y, float w, float h);
    Control CreateLabel(const std::string& text, float fontSize, float x, float y, float w, float h);
    Control CreateCheckBox(const std::string& text, float x, float y, float w, float h);
    Control CreateEditBox(float x, float y, float w, float h);
    Control CreateProgressBar(float x, float y, float w, float h);
    Control CreateSlider(float x, float y, float w, float h, float min, float max, float value);
    Control CreatePanel(float x, float y, float w, float h);
    Control CreateTextArea(float x, float y, float w, float h);
    Control CreateWinFrame(const std::string& title, float x, float y, float w, float h);
    Control CreateComboBox(float x, float y, float w, float h);
    Control CreateColorPicker(float x, float y, float w, float h, const std::string& color);
    Control CreateNumericUpDown(float x, float y, float w, float h);
    Control CreateSplitter(float x, float y, float w, float h, int orientation);
    Control CreateImageButton(const std::string& normal, const std::string& hover,
                              const std::string& pressed, float x, float y, float w, float h);
    Control CreateImage(const std::string& image, float x, float y, float w, float h);
    Control CreateAnimation(const std::string& jsoncPath, float x, float y, float w, float h);
    Control CreateDialog(const std::string& confirmText, const std::string& cancelText,
                         float x, float y, float w, float h);

    // ── 菜单族 / 滚动条 / 树 / 句柄 ──
    Control CreateMenuBar(float x, float y, float w, float h);
    Control CreateMenuPanel();
    Control CreateMenuItem(const std::string& caption, int type);
    void MenuBarAddMenu(Control& bar, const std::string& caption, Control& panel);
    void MenuPanelAddItem(Control& panel, Control& item);
    void MenuPanelAddSeparator(Control& panel);
    void MenuItemSetSubMenu(Control& item, Control& panel);
    Control CreateScrollBar(float x, float y, float w, float h, int orientation);
    Control CreateTreeView(float x, float y, float w, float h);
    Control CreateHandleControl(Control target, float x, float y, float w, float h);

    // ── 视口 ──
    void SetViewport(float x, float y, float w, float h);
    UIRect GetViewport() const;

    // ── 事件注入 ──
    void PushEvent(const UIEvent& event);
    void PushMouseButton(int button, float x, float y, bool down);
    void PushMouseMove(float x, float y);
    void PushMouseWheel(float dx, float dy, float x, float y);
    void PushKey(int keyCode, uint16_t mod, bool down);
    void PushTextInput(const std::string& text);

    // ── JSON 布局动作注册 ──
    using ActionCallback = std::function<void(Control)>;
    void RegisterAction(const std::string& name, ActionCallback callback);

    // ── 后端配置 ──
    bool SetBackendConfig(const char* key, const char* value);
    bool SetBackendConfigBool(const char* key, bool value);
    bool GetBackendConfigBool(const char* key, bool& out) const;

    // ── 错误查询 ──
    const std::string& GetLastError() const;

    // ── Debug 辅助 ──
    static int  DebugGetAliveCount();
    static UIInstance DebugGetAliveInstance(int index);
    static UIInstance DebugGetActiveViewport(UIInstance instance);
    static bool DebugIsControlFocused(UIInstance instance, UIControlHandle control);

private:
    explicit UICornerstone(UIInstance instance, bool ownsInstance);
    Control MakeControl(UIControlHandle h);
    friend class ::Control;

    std::unique_ptr<Impl> m_impl;
};

} // namespace UICornerstone

#endif