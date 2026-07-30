#include "UICornerstoneAPI.h"
#include "SColor.h"
#include "CallbackAdapters.h"
#include "BackendPlugin.h"
#include "Bench.h"
#include "Button.h"
#include "Label.h"
#include "CheckBox.h"
#include "EditBox.h"
#include "ProgressBar.h"
#include "Panel.h"
#include "TextArea.h"
#include "Slider.h"
#include "ColorPicker.h"
#include "ComboBox.h"
#include "NumericUpDown.h"
#include "Splitter.h"
#include "Dialog.h"
#include "WinFrame.h"
#include "TreeView.h"
#include "LayoutParser.h"
#include "PlatformUtils.h"
#include "Actor.h"
#include "LuotiAni.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <functional>
#include <queue>
#include <vector>
#include <algorithm>

// ============================================================
// 全局状态
// ============================================================
namespace {

const UIBackendCallbacks* g_callbacks = nullptr;
Window*                   g_window = nullptr;
RenderDevice*             g_renderDevice = nullptr;
InputBackend*             g_inputBackend = nullptr;
TextRenderer*             g_textRenderer = nullptr;
CallbackResourceProvider* g_resourceProvider = nullptr;
bool g_initialized = false;
bool g_quit = false;

SRect g_viewport(0, 0, 1024, 768);

std::unordered_map<std::string, std::pair<UIActionCallback, void*>> g_actions;
std::unordered_map<std::string, UIControlHandle> g_controlsById;
std::queue<UIEvent> g_queuedEvents;

// 保持 Dialog/Popup 生命期：CreateDialog 中创建后立即加入，
// close() 的 onClose 回调中自动清理。
static std::vector<std::shared_ptr<Popup>> g_popupPool;

static void registerControlById(const std::string& id, UIControlHandle ctl) {
    if (!id.empty()) g_controlsById[id] = ctl;
}

} // anonymous namespace

// ============================================================
// UICornerstone_Init / Shutdown
// ============================================================
int UICornerstone_Init(const UIBackendCallbacks* callbacks) {
    if (g_initialized) return 1;
    if (!callbacks || callbacks->version != 1) return 0;

    g_callbacks = callbacks;
    g_quit = false;

    if (!BackendManager::instance()->initialize(callbacks)) {
        printf("UICornerstone: BackendManager::initialize(callbacks) failed\n");
        return 0;
    }

    auto* bm = BackendManager::instance();
    g_window = bm->window();
    g_renderDevice = bm->renderDevice();
    g_textRenderer = bm->textRenderer();
    g_inputBackend = bm->inputBackend();

    if (callbacks->createResourceProvider) {
        std::string rpBasePath = Platform::GetBasePath() + "assets";
        UIResourceProviderHandle rpHandle = callbacks->createResourceProvider(rpBasePath.c_str());
        g_resourceProvider = new CallbackResourceProvider(callbacks, rpHandle);
    }

    BENCH->setRenderDevice(g_renderDevice);
    BENCH->setTextRenderer(g_textRenderer);
    BENCH->setInputBackend(g_inputBackend);
    if (g_resourceProvider) BENCH->setResourceProvider(g_resourceProvider);

    if (g_window) {
        SSize sz = g_window->getSize();
        g_viewport = SRect(0, 0, sz.width, sz.height);
    }
    BENCH->resized(g_viewport);

    // Perform a dummy clear+present to ensure the OpenGL context is active
    // for subsequent texture creation (relevant for OpenGL-based backends).
    // Without this, textures created during init have no valid GPU data.
    if (g_renderDevice) {
        g_renderDevice->setDrawColor(SColor(0, 0, 0, 0));
        g_renderDevice->clear();
        g_renderDevice->present();
    }

    g_initialized = true;
    return 1;
}

void UICornerstone_Shutdown(void) {
    if (!g_initialized) return;

    g_controlsById.clear();
    g_actions.clear();

    // 清理所有 Dialog/Popup，确保在 BackendManager shutdown 前析构
    g_popupPool.clear();

    // 销毁整个控制树，确保所有控件析构时后端仍存活。
    // 避免 FreeLibrary 触发静态析构时 ~Splitter/~Actor 等访问已释放的后端资源。
    BENCH->removeAllControls();

    delete g_resourceProvider; g_resourceProvider = nullptr;

    BackendManager::instance()->shutdown();
    g_window = nullptr;
    g_renderDevice = nullptr;
    g_textRenderer = nullptr;
    g_inputBackend = nullptr;

    g_callbacks = nullptr;
    g_initialized = false;
    printf("UICornerstone shutdown\n");
}

#if !UICORNERSTONE_BUILD_SHARED
extern "C" UIBackendCallbacks* GetUIBackendCallbacks(void);
#endif

int UICornerstone_InitFromPlugin(const char* pluginName) {
    if (!pluginName || !pluginName[0]) return 0;

    char dllName[128];
    snprintf(dllName, sizeof(dllName), "UIBackend_%s.dll", pluginName);
    HMODULE dll = LoadLibraryA(dllName);
    if (!dll) {
#if !UICORNERSTONE_BUILD_SHARED
        printf("UICornerstone: InitFromPlugin(%s) — LoadLibrary failed, trying static...\n", pluginName);
        UIBackendCallbacks* callbacks = GetUIBackendCallbacks();
        if (callbacks) {
            printf("UICornerstone: static GetUIBackendCallbacks ready\n");
            return UICornerstone_Init(callbacks);
        }
#endif
        printf("UICornerstone: InitFromPlugin(%s) — LoadLibrary failed\n", pluginName);
        return 0;
    }

    auto getter = (UIBackendCallbacks*(*)())GetProcAddress(dll, "GetUIBackendCallbacks");
    if (!getter) {
        printf("UICornerstone: %s has no GetUIBackendCallbacks\n", dllName);
        FreeLibrary(dll);
        return 0;
    }

    UIBackendCallbacks* callbacks = getter();
    if (!callbacks) {
        printf("UICornerstone: %s GetUIBackendCallbacks returned null\n", dllName);
        FreeLibrary(dll);
        return 0;
    }

    printf("UICornerstone: loaded %s\n", dllName);
    return UICornerstone_Init(callbacks);
}

// ============================================================
// 视口控制
// ============================================================
void UICornerstone_SetViewport(float x, float y, float w, float h) {
    g_viewport = SRect(x, y, w, h);
    BENCH->resized(g_viewport);
}

void UICornerstone_GetViewport(float* x, float* y, float* w, float* h) {
    if (x) *x = g_viewport.left;
    if (y) *y = g_viewport.top;
    if (w) *w = g_viewport.width;
    if (h) *h = g_viewport.height;
}

// ============================================================
// 帧循环
// ============================================================
static bool uiEventToEvent(const UIEvent& ue, Event& event) {
    event = Event();
    event.customInt = 0;
    event.customPtr = nullptr;

    switch (ue.type) {
    case UI_EVENT_MOUSE_MOVE:
        event.m_type = EventType::MouseMove;
        event.mousePos = EventMousePos{UI_EVENT_MOUSE_X(&ue), UI_EVENT_MOUSE_Y(&ue)};
        return true;
    case UI_EVENT_MOUSE_DOWN:
        event.m_type = EventType::MouseDown;
        event.mouseButton = EventMouseButton{UI_EVENT_MOUSE_X(&ue), UI_EVENT_MOUSE_Y(&ue),
            static_cast<MouseButton>(UI_EVENT_BUTTON(&ue))};
        return true;
    case UI_EVENT_MOUSE_UP:
        event.m_type = EventType::MouseUp;
        event.mouseButton = EventMouseButton{UI_EVENT_MOUSE_X(&ue), UI_EVENT_MOUSE_Y(&ue),
            static_cast<MouseButton>(UI_EVENT_BUTTON(&ue))};
        return true;
    case UI_EVENT_MOUSE_WHEEL:
        event.m_type = EventType::MouseWheel;
        event.mouseWheel = EventMouseWheel{
            UI_EVENT_WHEEL_MOUSE_X(&ue),
            UI_EVENT_WHEEL_MOUSE_Y(&ue),
            0,
            UI_EVENT_WHEEL_DELTA(&ue)
        };
        return true;
    case UI_EVENT_KEY_DOWN:
        event.m_type = EventType::KeyDown;
        event.keyEvent = EventKey{static_cast<KeyCode>(UI_EVENT_KEY_CODE(&ue)),
            static_cast<KeyMod>(UI_EVENT_KEY_MOD(&ue)), 0, false};
        return true;
    case UI_EVENT_KEY_UP:
        event.m_type = EventType::KeyUp;
        event.keyEvent = EventKey{static_cast<KeyCode>(UI_EVENT_KEY_CODE(&ue)),
            static_cast<KeyMod>(UI_EVENT_KEY_MOD(&ue)), 0, false};
        return true;
    case UI_EVENT_TEXT_INPUT:
        event.m_type = EventType::TextInput;
        strncpy(event.textInput.text, UI_EVENT_TEXT(&ue), 31);
        event.textInput.text[31] = '\0';
        return true;
    case UI_EVENT_WINDOW_RESIZE: {
        int w = UI_EVENT_RESIZE_W(&ue), h = UI_EVENT_RESIZE_H(&ue);
        event.m_type = EventType::WindowResize;
        event.resizeEvent = EventResize{w, h};
        return true;
    }
    case UI_EVENT_WINDOW_CLOSE:
        event.m_type = EventType::WindowClose;
        return true;
    case UI_EVENT_FOCUS_GAINED:
        event.m_type = EventType::FocusGained;
        event.focusEvent = EventFocus{true};
        return true;
    case UI_EVENT_FOCUS_LOST:
        event.m_type = EventType::FocusLost;
        event.focusEvent = EventFocus{false};
        return true;
    default:
        return false;
    }
}

void UICornerstone_PushUIEvent(const UIEvent* ue) {
    if (ue) g_queuedEvents.push(*ue);
}

void UICornerstone_ProcessEvents(void) {
    // 处理外部注入的队列事件
    while (!g_queuedEvents.empty()) {
        UIEvent ue = g_queuedEvents.front();
        g_queuedEvents.pop();

        Event event;
        if (!uiEventToEvent(ue, event)) continue;

        if (event.m_type == EventType::WindowClose) {
            g_quit = true;
        } else if (event.m_type == EventType::WindowResize) {
            BENCH->resized(SRect(0, 0, (float)event.resizeEvent.width, (float)event.resizeEvent.height));
        } else {
            auto sharedEvent = std::make_shared<Event>(event);
            BENCH->inputControl(sharedEvent);
        }
    }

    // 后备：从 InputBackend 轮询事件（非回调模式使用）
    if (!g_inputBackend) return;
    g_inputBackend->newFrame();

    Event event;
    while (g_inputBackend->pollEvent(event)) {
        switch (event.m_type) {
        case EventType::WindowClose:
            g_quit = true;
            break;
        case EventType::WindowResize:
            BENCH->resized(SRect(0, 0, (float)event.resizeEvent.width, (float)event.resizeEvent.height));
            break;
        default:
            {
                auto sharedEvent = std::make_shared<Event>(event);
                BENCH->inputControl(sharedEvent);
            }
            break;
        }
    }
}

void UICornerstone_Update(double deltaTime) {
    (void)deltaTime;
    BENCH->eventLoopEntry();
    BENCH->update();
}

void UICornerstone_Render(void) {
    if (!g_renderDevice) return;
    g_renderDevice->pushClipRect(g_viewport);
    BENCH->draw();
    g_renderDevice->popClipRect();
}

void UICornerstone_Clear(void) {
    if (!g_renderDevice) return;
    g_renderDevice->setDrawColor(SColor(0.2f, 0.2f, 0.22f, 1.0f));
    g_renderDevice->clear();
}

void UICornerstone_Present(void) {
    if (!g_renderDevice) return;
    g_renderDevice->present();
}

int UICornerstone_IsQuitRequested(void) {
    return g_quit ? 1 : 0;
}

// ============================================================
// 布局系统
// ============================================================
int UICornerstone_LoadLayout(const char* jsonContent) {
    if (!jsonContent) return 0;

    LayoutParser parser;

    // 注册所有 g_actions 到 LayoutParser
    for (auto& [name, pair] : g_actions) {
        UIActionCallback cb = pair.first;
        void* userData = pair.second;
        parser.registerHandler(name, [cb, userData](shared_ptr<Control> ctl) {
            cb(reinterpret_cast<UIControlHandle>(ctl.get()), userData);
        });
    }

    auto root = parser.parseLayout(std::string(jsonContent));
    if (!root) return 0;

    BENCH->addControl(root);

    for (auto& mb : parser.getMenuBars()) {
        BENCH->addControl(mb);
    }

    // 将 JSON 定义的 Dialog 加入 g_popupPool 保持生命期
    for (auto& pop : parser.getDialogs()) {
        g_popupPool.push_back(pop);
    }

    for (auto& id : parser.getAllControlIds()) {
        auto ctl = parser.findControlById(id);
        if (ctl) g_controlsById[id] = reinterpret_cast<UIControlHandle>(ctl.get());
    }

    printf("UICornerstone: LoadLayout OK (%zu control ids, %zu menu bars, %zu dialogs)\n",
           parser.getAllControlIds().size(), parser.getMenuBars().size(),
           parser.getDialogs().size());
    return 1;
}

int UICornerstone_LoadLayoutFromFile(const char* filePath) {
    if (!filePath) return 0;
    if (!g_resourceProvider) {
        printf("UICornerstone: LoadLayoutFromFile requires ResourceProvider\n");
        return 0;
    }
    auto data = g_resourceProvider->readFile(filePath);
    if (!data || data->empty()) return 0;
    data->push_back('\0');
    return UICornerstone_LoadLayout(data->data());
}

UIControlHandle UICornerstone_FindControl(const char* id) {
    if (!id) return nullptr;
    auto it = g_controlsById.find(id);
    return (it != g_controlsById.end()) ? it->second : nullptr;
}

void UICornerstone_RegisterAction(const char* name, UIActionCallback cb, void* userData) {
    if (name) g_actions[name] = {cb, userData};
}

// ============================================================
// 控件工厂
// ============================================================
UIControlHandle UICornerstone_CreateButton(const char* text,
    float x, float y, float w, float h)
{
    auto ctl = std::make_shared<Button>(BENCH, SRect(x, y, w, h));
    if (text) ctl->setCaption(text);
    BENCH->addControl(ctl);
    ctl->create();
    ctl->setVisible(true);
    return reinterpret_cast<UIControlHandle>(static_cast<Control*>(ctl.get()));
}

UIControlHandle UICornerstone_CreateLabel(const char* text, float fontSize,
    float x, float y, float w, float h)
{
    auto ctl = std::make_shared<Label>(BENCH, SRect(x, y, w, h));
    if (text) ctl->setCaption(text);
    ctl->setFont(FontName::HarmonyOS_Sans_SC_Regular);
    if (fontSize > 0) ctl->setFontSize((int)fontSize);
    BENCH->addControl(ctl);
    ctl->create();
    ctl->setVisible(true);
    return reinterpret_cast<UIControlHandle>(static_cast<Control*>(ctl.get()));
}

UIControlHandle UICornerstone_CreateCheckBox(const char* text,
    float x, float y, float w, float h)
{
    auto ctl = std::make_shared<CheckBox>(BENCH, SRect(x, y, w, h));
    ctl->createCaption();
    if (text) ctl->getCaption()->setCaption(text);
    BENCH->addControl(ctl);
    ctl->create();
    ctl->setVisible(true);
    return reinterpret_cast<UIControlHandle>(static_cast<Control*>(ctl.get()));
}

UIControlHandle UICornerstone_CreateEditBox(
    float x, float y, float w, float h)
{
    auto ctl = std::make_shared<EditBox>(BENCH, SRect(x, y, w, h));
    BENCH->addControl(ctl);
    ctl->create();
    ctl->setVisible(true);
    return reinterpret_cast<UIControlHandle>(static_cast<Control*>(ctl.get()));
}

UIControlHandle UICornerstone_CreateProgressBar(
    float x, float y, float w, float h)
{
    auto ctl = std::make_shared<ProgressBar>(BENCH, SRect(x, y, w, h));
    BENCH->addControl(ctl);
    ctl->create();
    ctl->setVisible(true);
    return reinterpret_cast<UIControlHandle>(static_cast<Control*>(ctl.get()));
}

UIControlHandle UICornerstone_CreateSlider(
    float x, float y, float w, float h, float min, float max, float value)
{
    auto ctl = std::make_shared<Slider>(BENCH, SRect(x, y, w, h));
    ctl->setRange(min, max);
    ctl->setValue(value);
    BENCH->addControl(ctl);
    ctl->create();
    ctl->setVisible(true);
    return reinterpret_cast<UIControlHandle>(static_cast<Control*>(ctl.get()));
}

UIControlHandle UICornerstone_CreatePanel(
    float x, float y, float w, float h)
{
    auto ctl = std::make_shared<Panel>(BENCH, SRect(x, y, w, h));
    BENCH->addControl(ctl);
    ctl->create();
    ctl->setVisible(true);
    return reinterpret_cast<UIControlHandle>(static_cast<Control*>(ctl.get()));
}

UIControlHandle UICornerstone_CreateTextArea(
    float x, float y, float w, float h)
{
    auto ctl = std::make_shared<TextArea>(BENCH, SRect(x, y, w, h));
    BENCH->addControl(ctl);
    ctl->create();
    ctl->setVisible(true);
    return reinterpret_cast<UIControlHandle>(static_cast<Control*>(ctl.get()));
}

UIControlHandle UICornerstone_CreateWinFrame(
    const char* title, float x, float y, float w, float h)
{
    auto ctl = std::make_shared<WinFrame>(BENCH, SRect(x, y, w, h));
    if (title) ctl->setTitle(title);
    ctl->setTitleTextColor(SColor(0, 0, 0, 255));
    BENCH->addControl(ctl);
    ctl->create();
    ctl->show();
    return reinterpret_cast<UIControlHandle>(static_cast<Control*>(ctl.get()));
}

UIControlHandle UICornerstone_CreateMenu(void) {
    printf("UICornerstone: CreateMenu not implemented yet\n");
    return nullptr;
}

UIControlHandle UICornerstone_CreateImageButton(
    const char* normalImage, const char* hoverImage, const char* pressedImage,
    float x, float y, float w, float h)
{
    auto ctl = std::make_shared<Button>(BENCH, SRect(x, y, w, h));
    if (normalImage) {
        auto actor = std::make_shared<Actor>(ctl.get(), fs::path(normalImage), true);
        ctl->setNormalStateActor(actor);
    }
    if (hoverImage) {
        auto actor = std::make_shared<Actor>(ctl.get(), fs::path(hoverImage), true);
        ctl->setHoverStateActor(actor);
    }
    if (pressedImage) {
        auto actor = std::make_shared<Actor>(ctl.get(), fs::path(pressedImage), true);
        ctl->setPressedStateActor(actor);
    }
    BENCH->addControl(ctl);
    ctl->create();
    ctl->setVisible(true);
    return reinterpret_cast<UIControlHandle>(static_cast<Control*>(ctl.get()));
}

// ============================================================
// 控件通用操作
// ============================================================
void UICornerstone_SetRect(UIControlHandle ctl, float x, float y, float w, float h) {
    if (ctl) static_cast<Control*>(ctl)->setRect(SRect(x, y, w, h));
}

void UICornerstone_GetRect(UIControlHandle ctl, float* x, float* y, float* w, float* h) {
    if (!ctl) return;
    SRect r = static_cast<Control*>(ctl)->getRect();
    if (x) *x = r.left;
    if (y) *y = r.top;
    if (w) *w = r.width;
    if (h) *h = r.height;
}

void UICornerstone_AddChildControl(UIControlHandle parent, UIControlHandle child) {
    if (!parent || !child) return;
    auto* ctlImpl = dynamic_cast<ControlImpl*>(static_cast<Control*>(child));
    auto* panel = dynamic_cast<Panel*>(static_cast<Control*>(parent));
    if (!ctlImpl || !panel) return;
    auto sp = ctlImpl->shared_from_this();
    BENCH->removeControl(sp);
    panel->addControl(sp);
}

const char* UICornerstone_GetControlId(UIControlHandle ctl) {
    if (!ctl) return "";
    static char buf[256];
    for (const auto& pair : g_controlsById) {
        if (pair.second == ctl) {
            strncpy(buf, pair.first.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            return buf;
        }
    }
    buf[0] = '\0';
    return buf;
}

void UICornerstone_DestroyControl(UIControlHandle ctl) {
    if (!ctl) return;
    auto* ctrl = dynamic_cast<ControlImpl*>(static_cast<Control*>(ctl));
    if (!ctrl) return;
    try {
        auto sp = ctrl->shared_from_this();
        Control* parent = ctrl->getParent();
        if (parent && parent != BENCH) {
            parent->removeControl(sp);
        } else {
            BENCH->removeControl(sp);
        }
    } catch (...) {}
}

// ============================================================
// ColorPicker
// ============================================================
UIControlHandle UICornerstone_CreateColorPicker(
    float x, float y, float w, float h, const char* color)
{
    auto ctl = std::make_shared<ColorPicker>(BENCH, SRect(x, y, w, h));
    if (color) ctl->setColor(color);
    BENCH->addControl(ctl);
    ctl->create();
    ctl->setVisible(true);
    return reinterpret_cast<UIControlHandle>(static_cast<Control*>(ctl.get()));
}

// ============================================================
// ComboBox
// ============================================================
UIControlHandle UICornerstone_CreateComboBox(
    float x, float y, float w, float h)
{
    auto ctl = std::make_shared<ComboBox>(BENCH, SRect(x, y, w, h));
    BENCH->addControl(ctl);
    ctl->create();
    ctl->setVisible(true);
    return reinterpret_cast<UIControlHandle>(static_cast<Control*>(ctl.get()));
}

// ============================================================
// Dialog / Popup
// ============================================================
UIControlHandle UICornerstone_CreateDialog(
    const char* confirmText, const char* cancelText,
    float x, float y, float w, float h)
{
    auto ctl = std::make_shared<Dialog>(BENCH, SRect(x, y, w, h));
    if (confirmText) ctl->setConfirmButtonText(confirmText);
    if (cancelText) ctl->setCancelButtonText(cancelText);
    ctl->setCentered();
    ctl->create();

    // 保持 Dialog 生命期：加入 g_popupPool，close() 时自动清理
    g_popupPool.push_back(ctl);

    return reinterpret_cast<UIControlHandle>(static_cast<Control*>(ctl.get()));
}

// ── NumericUpDown C ABI ──

UIControlHandle UICornerstone_CreateNumericUpDown(float x, float y, float w, float h) {
    auto nud = make_shared<NumericUpDown>(BENCH, SRect(x, y, w, h));
    BENCH->addControl(nud);
    nud->create();
    nud->setVisible(true);
    return reinterpret_cast<UIControlHandle>(static_cast<Control*>(nud.get()));
}

// ── Splitter C ABI ──

UIControlHandle UICornerstone_CreateSplitter(float x, float y, float w, float h, int orientation) {
    auto sp = make_shared<Splitter>(BENCH, SRect(x, y, w, h));
    sp->setOrientation(orientation != 0);
    BENCH->addControl(sp);
    sp->create();
    sp->setVisible(true);
    return reinterpret_cast<UIControlHandle>(static_cast<Control*>(sp.get()));
}

// ============================================================
// Property system (string-based, multi-type)
// ============================================================

int UICornerstone_SetColor(UIControlHandle ctl, const char* prop, UIColor value) {
    auto* c = static_cast<Control*>(ctl);
    if (!c || !prop) return 0;
    return c->setColorProperty(prop, SColor(value.r, value.g, value.b, value.a));
}

int UICornerstone_SetStateColor(UIControlHandle ctl, const char* prop, UIStateColor value) {
    auto* c = static_cast<Control*>(ctl);
    if (!c || !prop) return 0;
    return c->setStateColorProperty(prop,
        StateColor(
            SColor(value.normal.r, value.normal.g, value.normal.b, value.normal.a),
            SColor(value.hover.r, value.hover.g, value.hover.b, value.hover.a),
            SColor(value.pressed.r, value.pressed.g, value.pressed.b, value.pressed.a),
            SColor(value.disabled.r, value.disabled.g, value.disabled.b, value.disabled.a)
        ));
}

int UICornerstone_SetInt(UIControlHandle ctl, const char* prop, int value) {
    auto* c = static_cast<Control*>(ctl);
    if (!c || !prop) return 0;
    return c->setIntProperty(prop, value);
}

int UICornerstone_SetFloat(UIControlHandle ctl, const char* prop, float value) {
    auto* c = static_cast<Control*>(ctl);
    if (!c || !prop) return 0;
    return c->setFloatProperty(prop, value);
}

int UICornerstone_SetString(UIControlHandle ctl, const char* prop, const char* value) {
    auto* c = static_cast<Control*>(ctl);
    if (!c || !prop) return 0;
    return c->setStringProperty(prop, value);
}

int UICornerstone_SetBool(UIControlHandle ctl, const char* prop, int value) {
    auto* c = static_cast<Control*>(ctl);
    if (!c || !prop) return 0;
    return c->setBoolProperty(prop, value);
}

int UICornerstone_SetEnum(UIControlHandle ctl, const char* prop, const char* value) {
    auto* c = static_cast<Control*>(ctl);
    if (!c || !prop || !value) return 0;
    return c->setEnumProperty(prop, value);
}

int UICornerstone_SetPtr(UIControlHandle ctl, const char* prop, void* value) {
    auto* c = static_cast<Control*>(ctl);
    if (!c || !prop) return 0;
    return c->setPtrProperty(prop, value);
}

int UICornerstone_GetPtr(UIControlHandle ctl, const char* prop, void** out) {
    auto* c = static_cast<Control*>(ctl);
    if (!c || !prop || !out) return 0;
    return c->getPtrProperty(prop, *out);
}

int UICornerstone_SetCallback(UIControlHandle ctl, const char* event, UIEventCallback cb, void* userData) {
    auto* c = static_cast<Control*>(ctl);
    if (!c || !event) return 0;
    return c->setCallbackProperty(event, reinterpret_cast<void(*)(void*, const void*, void*)>(cb), userData);
}

int UICornerstone_GetColor(UIControlHandle ctl, const char* prop, UIColor* out) {
    auto* c = static_cast<Control*>(ctl);
    if (!c || !prop || !out) return 0;
    SColor s;
    if (!c->getColorProperty(prop, s)) return 0;
    out->r = s.redByte(); out->g = s.greenByte();
    out->b = s.blueByte(); out->a = s.alphaByte();
    return 1;
}

int UICornerstone_GetStateColor(UIControlHandle ctl, const char* prop, UIStateColor* out) {
    auto* c = static_cast<Control*>(ctl);
    if (!c || !prop || !out) return 0;
    SColor def = SColor();
    StateColor sc{def, def, def, def};
    if (!c->getStateColorProperty(prop, sc)) return 0;
    out->normal   = { sc.getNormal().redByte(),   sc.getNormal().greenByte(),   sc.getNormal().blueByte(),   sc.getNormal().alphaByte() };
    out->hover    = { sc.getHover().redByte(),    sc.getHover().greenByte(),    sc.getHover().blueByte(),    sc.getHover().alphaByte() };
    out->pressed  = { sc.getPressed().redByte(),  sc.getPressed().greenByte(),  sc.getPressed().blueByte(),  sc.getPressed().alphaByte() };
    out->disabled = { sc.getDisabled().redByte(), sc.getDisabled().greenByte(), sc.getDisabled().blueByte(), sc.getDisabled().alphaByte() };
    return 1;
}

int UICornerstone_GetBool(UIControlHandle ctl, const char* prop, int* out) {
    auto* c = static_cast<Control*>(ctl);
    if (!c || !prop || !out) return 0;
    return c->getBoolProperty(prop, *out);
}

int UICornerstone_GetInt(UIControlHandle ctl, const char* prop, int* out) {
    auto* c = static_cast<Control*>(ctl);
    if (!c || !prop || !out) return 0;
    return c->getIntProperty(prop, *out);
}

int UICornerstone_GetFloat(UIControlHandle ctl, const char* prop, float* out) {
    auto* c = static_cast<Control*>(ctl);
    if (!c || !prop || !out) return 0;
    return c->getFloatProperty(prop, *out);
}

int UICornerstone_GetString(UIControlHandle ctl, const char* prop, char* out, int maxLen) {
    auto* c = static_cast<Control*>(ctl);
    if (!c || !prop || !out || maxLen <= 0) return 0;
    const char* s = nullptr;
    if (!c->getStringProperty(prop, s) || !s) return 0;
    strncpy_s(out, maxLen, s, _TRUNCATE);
    return 1;
}

int UICornerstone_GetEnum(UIControlHandle ctl, const char* prop, char* out, int maxLen) {
    auto* c = static_cast<Control*>(ctl);
    if (!c || !prop || !out || maxLen <= 0) return 0;
    const char* s = nullptr;
    if (!c->getEnumProperty(prop, s) || !s) return 0;
    strncpy_s(out, maxLen, s, _TRUNCATE);
    return 1;
}



