// UICornerstone C++ Binding — 主类实现
// 许可证 MIT。仅调用 C ABI，不引用核心库内部头。
#include "UICornerstone.h"
#include "Control.h"
#include "Event.h"
#include "Impl.h"
#include "UIEventFactory.h"   // UICornerstone::Input::* 内联定义

#include <chrono>
#include <algorithm>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#undef NOMINMAX
// WinUser.h 以宏形式导出 CreateDialog*A/W 等，会污染名含 CreateDialog 的符号。
#undef CreateDialog
#undef CreateDialogA
#undef CreateDialogW
#undef CreateDialogParamA
#undef CreateDialogParamW
#undef DialogBoxParamA
#undef DialogBoxParamW
#endif

namespace UICornerstone {

// ============================================================
// BackendResolver（内部）：自定义搜索路径 DLL 加载
// ============================================================
namespace {

struct BackendResolver {
    // 返回回调表 + 持有的 DLL 句柄（失败返回 {nullptr, nullptr}）
    static std::pair<UIBackendCallbacks*, void*> LoadFromPath(
        const std::string& searchPath, const std::string& backend)
    {
#ifdef _WIN32
        std::string dllPath = searchPath + "/UIBackend_" + backend + ".dll";
        HMODULE h = LoadLibraryA(dllPath.c_str());
        if (!h) return {nullptr, nullptr};
        auto fn = reinterpret_cast<UIBackendCallbacks* (*)(void)>(GetProcAddress(h, "GetUIBackendCallbacks"));
        if (!fn) { FreeLibrary(h); return {nullptr, nullptr}; }
        return {fn(), (void*)h};
#else
        (void)searchPath; (void)backend;
        return {nullptr, nullptr};
#endif
    }
};

// 帧时钟（跨平台）
uint64_t nowMillis() {
    auto t = std::chrono::steady_clock::now().time_since_epoch();
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(t).count();
}

} // namespace

// ============================================================
// 生命周期
// ============================================================
UICornerstone::UICornerstone(UIInstance instance, bool ownsInstance)
    : m_impl(new Impl())
{
    m_impl->instance = instance;
    m_impl->ownsInstance = ownsInstance;
    m_impl->initialized = (instance != nullptr);
    m_impl->resourceRoot = m_impl->config.resourceRoot;
}

UICornerstone::~UICornerstone() {
    if (m_impl && m_impl->ownsInstance && m_impl->instance)
        UICornerstone_DestroyInstance(m_impl->instance);
    if (m_impl && m_impl->dllHandle) {
#ifdef _WIN32
        FreeLibrary((HMODULE)m_impl->dllHandle);
#endif
        m_impl->dllHandle = nullptr;
    }
}

UIInstance UICornerstone::Handle() const { return m_impl ? m_impl->instance : nullptr; }

std::unique_ptr<UICornerstone> UICornerstone::Create(const Config& config) {
    if (config.backend.empty()) return nullptr;

    UIInstanceConfig cfg = UI_INSTANCE_CONFIG_DEFAULT;
    cfg.resourceRoot  = config.resourceRoot.c_str();
    cfg.windowTitle   = config.windowTitle.c_str();
    cfg.windowWidth   = config.windowWidth;
    cfg.windowHeight  = config.windowHeight;
    cfg.windowFlags   = config.windowFlags;

    UIInstance instance = nullptr;
    void* dllHandle = nullptr;

    if (config.backendSearchPath.empty()) {
        // 默认：核心库插件加载（静态符号回退）
        instance = UICornerstone_CreateInstanceFromPlugin(config.backend.c_str(), &cfg);
        if (!instance) return nullptr;
    } else {
        // 自定义搜索路径：自加载 DLL → 回调查表模式
        auto [callbacks, h] = BackendResolver::LoadFromPath(config.backendSearchPath, config.backend);
        if (!callbacks) return nullptr;
        instance = UICornerstone_CreateInstance(callbacks, &cfg);
        if (!instance) { FreeLibrary((HMODULE)h); return nullptr; }
        dllHandle = h;
    }

    auto ui = std::unique_ptr<UICornerstone>(new UICornerstone(instance, true));
    ui->m_impl->config = config;
    ui->m_impl->resourceRoot = config.resourceRoot;
    ui->m_impl->dllHandle = dllHandle;
    return ui;
}

std::unique_ptr<UICornerstone> UICornerstone::Create(const UIBackendCallbacks* callbacks,
                                                     const Config& config) {
    if (!callbacks) return nullptr;
    UIInstanceConfig cfg = UI_INSTANCE_CONFIG_DEFAULT;
    cfg.resourceRoot  = config.resourceRoot.c_str();
    cfg.windowTitle   = config.windowTitle.c_str();
    cfg.windowWidth   = config.windowWidth;
    cfg.windowHeight  = config.windowHeight;
    cfg.windowFlags   = config.windowFlags;

    UIInstance instance = UICornerstone_CreateInstance(callbacks, &cfg);
    if (!instance) return nullptr;

    auto ui = std::unique_ptr<UICornerstone>(new UICornerstone(instance, true));
    ui->m_impl->config = config;
    ui->m_impl->resourceRoot = config.resourceRoot;
    return ui;
}

std::unique_ptr<UICornerstone> UICornerstone::CreateViewport(float x, float y, float w, float h) {
    if (!m_impl->instance) return nullptr;
    UIRect rect{x, y, w, h};
    UIInstance vp = UICornerstone_CreateViewport(m_impl->instance, rect);
    if (!vp) return nullptr;
    // 视口共享 owner 后端：析构时不 DestroyInstance（owner 级联销毁）
    auto ui = std::unique_ptr<UICornerstone>(new UICornerstone(vp, false));
    ui->m_impl->config = m_impl->config;
    ui->m_impl->resourceRoot = m_impl->resourceRoot;
    return ui;
}

// ============================================================
// 双模式循环
// ============================================================
int UICornerstone::Run(FrameCallback update, RenderCallback onRender) {
    if (!m_impl->initialized) return 1;

    uint64_t lastTicks = nowMillis();
    while (!IsQuitRequested()) {
        ProcessEvents();

        uint64_t now = nowMillis();
        double dt = (now - lastTicks) / 1000.0;
        lastTicks = now;
        dt = std::min(dt, 0.1);   // 防止长时间挂起后的 dt 暴增

        Update(dt);
        if (update) update(dt);

        Clear();
        Render();
        if (onRender) onRender();
        Present();
    }
    Shutdown();
    return 0;
}

void UICornerstone::ProcessEvents() { if (m_impl->instance) UICornerstone_ProcessEvents(m_impl->instance); }
void UICornerstone::Update(double deltaTime) { if (m_impl->instance) UICornerstone_Update(m_impl->instance, deltaTime); }
void UICornerstone::Render() { if (m_impl->instance) UICornerstone_Render(m_impl->instance); }
void UICornerstone::Clear() { if (m_impl->instance) UICornerstone_Clear(m_impl->instance); }
void UICornerstone::Present() { if (m_impl->instance) UICornerstone_Present(m_impl->instance); }
bool UICornerstone::IsQuitRequested() const { return m_impl->instance && UICornerstone_IsQuitRequested(m_impl->instance); }

void UICornerstone::Shutdown() {
    if (m_impl->instance) {
        if (m_impl->ownsInstance) UICornerstone_DestroyInstance(m_impl->instance);
        m_impl->instance = nullptr;
        m_impl->initialized = false;
    }
}

// ============================================================
// 资源路径
// ============================================================
void UICornerstone::SetResourceRoot(const std::string& path) { m_impl->resourceRoot = path; }
std::string UICornerstone::GetResourceRoot() const { return m_impl->resourceRoot; }
std::string UICornerstone::ResolveResource(const std::string& relativePath) const {
    return m_impl->resourceRoot + "/" + relativePath;
}

// ============================================================
// 布局
// ============================================================
bool UICornerstone::LoadLayout(const std::string& jsonContent) {
    return m_impl->instance && UICornerstone_LoadLayout(m_impl->instance, jsonContent.c_str()) != 0;
}
bool UICornerstone::LoadLayoutFromFile(const std::string& filePath) {
    return m_impl->instance && UICornerstone_LoadLayoutFromFile(m_impl->instance, filePath.c_str()) != 0;
}
Control UICornerstone::FindControl(const std::string& id) {
    if (!m_impl->instance) return Control();
    return MakeControl(UICornerstone_FindControl(m_impl->instance, id.c_str()));
}

// ============================================================
// Control 生命周期注册
// ============================================================
Control UICornerstone::MakeControl(UIControlHandle h) {
    if (!h) return Control();
    auto it = m_impl->liveControls.find(h);
    if (it != m_impl->liveControls.end()) {
        if (auto state = it->second.lock()) return Control(std::move(state));
        m_impl->liveControls.erase(it);   // 弱引用已死，重注册
    }
    auto state = std::make_shared<ControlState>();
    state->instance = m_impl->instance;
    state->handle = h;
    state->ownerImpl = m_impl.get();
    m_impl->liveControls[h] = state;
    return Control(std::move(state));
}

// ============================================================
// 控件工厂
// ============================================================
#define UI_FACTORY(name, argdecl, ...) \
    Control UICornerstone::name argdecl { \
        if (!m_impl->instance) return Control(); \
        return MakeControl(UICornerstone_##name(m_impl->instance, __VA_ARGS__)); \
    }

UI_FACTORY(CreateButton,
    (const std::string& text, float x, float y, float w, float h),
    text.c_str(), x, y, w, h)
UI_FACTORY(CreateLabel,
    (const std::string& text, float fontSize, float x, float y, float w, float h),
    text.c_str(), fontSize, x, y, w, h)
UI_FACTORY(CreateCheckBox,
    (const std::string& text, float x, float y, float w, float h),
    text.c_str(), x, y, w, h)
UI_FACTORY(CreateEditBox,
    (float x, float y, float w, float h),
    x, y, w, h)
UI_FACTORY(CreateProgressBar,
    (float x, float y, float w, float h),
    x, y, w, h)
UI_FACTORY(CreateSlider,
    (float x, float y, float w, float h, float min, float max, float value),
    x, y, w, h, min, max, value)
UI_FACTORY(CreatePanel,
    (float x, float y, float w, float h),
    x, y, w, h)
UI_FACTORY(CreateTextArea,
    (float x, float y, float w, float h),
    x, y, w, h)
UI_FACTORY(CreateWinFrame,
    (const std::string& title, float x, float y, float w, float h),
    title.c_str(), x, y, w, h)
UI_FACTORY(CreateComboBox,
    (float x, float y, float w, float h),
    x, y, w, h)
UI_FACTORY(CreateColorPicker,
    (float x, float y, float w, float h, const std::string& color),
    x, y, w, h, color.c_str())
UI_FACTORY(CreateNumericUpDown,
    (float x, float y, float w, float h),
    x, y, w, h)
UI_FACTORY(CreateSplitter,
    (float x, float y, float w, float h, int orientation),
    x, y, w, h, orientation)
UI_FACTORY(CreateImageButton,
    (const std::string& normal, const std::string& hover, const std::string& pressed,
     float x, float y, float w, float h),
    normal.c_str(), hover.c_str(), pressed.c_str(), x, y, w, h)
UI_FACTORY(CreateImage,
    (const std::string& image, float x, float y, float w, float h),
    image.c_str(), x, y, w, h)
UI_FACTORY(CreateAnimation,
    (const std::string& jsoncPath, float x, float y, float w, float h),
    jsoncPath.c_str(), x, y, w, h)
UI_FACTORY(CreateDialog,
    (const std::string& confirmText, const std::string& cancelText, float x, float y, float w, float h),
    confirmText.c_str(), cancelText.c_str(), x, y, w, h)
UI_FACTORY(CreateMenuBar,
    (float x, float y, float w, float h),
    x, y, w, h)
UI_FACTORY(CreateScrollBar,
    (float x, float y, float w, float h, int orientation),
    x, y, w, h, orientation)
UI_FACTORY(CreateTreeView,
    (float x, float y, float w, float h),
    x, y, w, h)

Control UICornerstone::CreateMenuPanel() {
    if (!m_impl->instance) return Control();
    return MakeControl(UICornerstone_CreateMenuPanel(m_impl->instance));
}
Control UICornerstone::CreateMenuItem(const std::string& caption, int type) {
    if (!m_impl->instance) return Control();
    return MakeControl(UICornerstone_CreateMenuItem(m_impl->instance, caption.c_str(), type));
}
Control UICornerstone::CreateHandleControl(Control target, float x, float y, float w, float h) {
    if (!m_impl->instance) return Control();
    return MakeControl(UICornerstone_CreateHandleControl(m_impl->instance,
        target.Handle(), x, y, w, h));
}

void UICornerstone::MenuBarAddMenu(Control& bar, const std::string& caption, Control& panel) {
    if (m_impl->instance && bar.Handle() && panel.Handle())
        UICornerstone_MenuBarAddMenu(m_impl->instance, bar.Handle(), caption.c_str(), panel.Handle());
}
void UICornerstone::MenuPanelAddItem(Control& panel, Control& item) {
    if (m_impl->instance && panel.Handle() && item.Handle())
        UICornerstone_MenuPanelAddItem(m_impl->instance, panel.Handle(), item.Handle());
}
void UICornerstone::MenuPanelAddSeparator(Control& panel) {
    if (m_impl->instance && panel.Handle())
        UICornerstone_MenuPanelAddSeparator(m_impl->instance, panel.Handle());
}
void UICornerstone::MenuItemSetSubMenu(Control& item, Control& panel) {
    if (m_impl->instance && item.Handle() && panel.Handle())
        UICornerstone_MenuItemSetSubMenu(m_impl->instance, item.Handle(), panel.Handle());
}

// ============================================================
// 视口
// ============================================================
void UICornerstone::SetViewport(float x, float y, float w, float h) {
    if (m_impl->instance) UICornerstone_SetViewport(m_impl->instance, x, y, w, h);
}
UIRect UICornerstone::GetViewport() const {
    UIRect r{0, 0, 0, 0};
    if (m_impl->instance) UICornerstone_GetViewport(m_impl->instance, &r.x, &r.y, &r.w, &r.h);
    return r;
}

// ============================================================
// 事件注入
// ============================================================
void UICornerstone::PushEvent(const UIEvent& event) {
    if (m_impl->instance) UICornerstone_PushUIEvent(m_impl->instance, &event);
}
void UICornerstone::PushMouseButton(int button, float x, float y, bool down) {
    PushEvent(Input::MouseButton(button, x, y, down));
}
void UICornerstone::PushMouseMove(float x, float y) {
    PushEvent(Input::MouseMove(x, y));
}
void UICornerstone::PushMouseWheel(float dx, float dy, float x, float y) {
    PushEvent(Input::MouseWheel(dx, dy, x, y));
}
void UICornerstone::PushKey(int keyCode, uint16_t mod, bool down) {
    PushEvent(Input::Key(keyCode, mod, down));
}
void UICornerstone::PushTextInput(const std::string& text) {
    PushEvent(Input::TextInput(text));
}

// ============================================================
// Action 注册
// ============================================================
void UICornerstone::RegisterAction(const std::string& name, ActionCallback callback) {
    if (!m_impl->instance) return;
    auto cb = std::make_shared<ActionCallback>(std::move(callback));
    m_impl->actions[name] = cb;
    UICornerstone_RegisterAction(m_impl->instance, name.c_str(),
        [](UIControlHandle ctl, void* userData) {
            auto& fn = *static_cast<ActionCallback*>(userData);
            if (!ctl) return;
            auto state = std::make_shared<ControlState>();
            state->instance = nullptr;  // 动作回调不绑定实例上下文（ctl 裸句柄）
            state->handle = ctl;
            fn(Control(std::move(state)));
        },
        cb.get());
}

// ============================================================
// 后端配置
// ============================================================
bool UICornerstone::SetBackendConfig(const char* key, const char* value) {
    return m_impl->instance && UICornerstone_SetBackendConfig(m_impl->instance, key, value) != 0;
}
bool UICornerstone::SetBackendConfigBool(const char* key, bool value) {
    return m_impl->instance && UICornerstone_SetBackendConfigBool(m_impl->instance, key, value ? 1 : 0) != 0;
}
bool UICornerstone::GetBackendConfigBool(const char* key, bool& out) const {
    if (!m_impl->instance) return false;
    int v = 0;
    if (!UICornerstone_GetBackendConfigBool(m_impl->instance, key, &v)) return false;
    out = (v != 0);
    return true;
}

// ============================================================
// 错误查询 / Debug
// ============================================================
const std::string& UICornerstone::GetLastError() const { return m_impl->lastError; }

int UICornerstone::DebugGetAliveCount() { return UICornerstone_Debug_GetAliveCount(); }
UIInstance UICornerstone::DebugGetAliveInstance(int index) { return UICornerstone_Debug_GetAliveInstance(index); }
UIInstance UICornerstone::DebugGetActiveViewport(UIInstance instance) { return UICornerstone_Debug_GetActiveViewport(instance); }
bool UICornerstone::DebugIsControlFocused(UIInstance instance, UIControlHandle control) {
    return UICornerstone_Debug_IsControlFocused(instance, control) != 0;
}

} // namespace UICornerstone