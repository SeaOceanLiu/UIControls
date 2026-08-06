// UICornerstone C++ Binding — 控件代理实现
// 许可证 MIT。仅调用 C ABI。
#include "Control.h"
#include "Event.h"
#include "Impl.h"

#include <cstring>

namespace {
// 访问 Impl 中的回调 userData 注册表（ControlState::ownerImpl 指向 UICornerstone::Impl*）
UICornerstone::Impl* ownerOf(const ControlState& s) {
    return static_cast<UICornerstone::Impl*>(s.ownerImpl);
}
} // namespace

// ============================================================
// 构造 / 有效性
// ============================================================
Control::Control(std::shared_ptr<ControlState> state) : m_state(std::move(state)) {}

bool Control::IsValid() const {
    return m_state && m_state->alive && m_state->instance && m_state->handle;
}

// ============================================================
// 属性设置（全部经 C ABI 转发；悬挂/无效时静默忽略）
// ============================================================
void Control::SetColor(const char* prop, UIColor value) {
    if (IsValid()) UICornerstone_SetColor(m_state->instance, m_state->handle, prop, value);
}
void Control::SetStateColor(const char* prop, UIStateColor value) {
    if (IsValid()) UICornerstone_SetStateColor(m_state->instance, m_state->handle, prop, value);
}
void Control::SetBool(const char* prop, bool value) {
    if (IsValid()) UICornerstone_SetBool(m_state->instance, m_state->handle, prop, value ? 1 : 0);
}
void Control::SetInt(const char* prop, int value) {
    if (IsValid()) UICornerstone_SetInt(m_state->instance, m_state->handle, prop, value);
}
void Control::SetFloat(const char* prop, float value) {
    if (IsValid()) UICornerstone_SetFloat(m_state->instance, m_state->handle, prop, value);
}
void Control::SetString(const char* prop, const std::string& value) {
    if (IsValid()) UICornerstone_SetString(m_state->instance, m_state->handle, prop, value.c_str());
}
void Control::SetEnum(const char* prop, const std::string& value) {
    if (IsValid()) UICornerstone_SetEnum(m_state->instance, m_state->handle, prop, value.c_str());
}
void Control::SetPtr(const char* prop, void* value) {
    if (IsValid()) UICornerstone_SetPtr(m_state->instance, m_state->handle, prop, value);
}

// ============================================================
// 属性读取
// ============================================================
UIColor Control::GetColor(const char* prop) const {
    UIColor v{0, 0, 0, 255};
    if (IsValid()) UICornerstone_GetColor(m_state->instance, m_state->handle, prop, &v);
    return v;
}
UIStateColor Control::GetStateColor(const char* prop) const {
    UIStateColor v{};
    if (IsValid()) UICornerstone_GetStateColor(m_state->instance, m_state->handle, prop, &v);
    return v;
}
bool Control::GetBool(const char* prop) const {
    int v = 0;
    if (IsValid()) UICornerstone_GetBool(m_state->instance, m_state->handle, prop, &v);
    return v != 0;
}
int Control::GetInt(const char* prop) const {
    int v = 0;
    if (IsValid()) UICornerstone_GetInt(m_state->instance, m_state->handle, prop, &v);
    return v;
}
float Control::GetFloat(const char* prop) const {
    float v = 0.f;
    if (IsValid()) UICornerstone_GetFloat(m_state->instance, m_state->handle, prop, &v);
    return v;
}
std::string Control::GetString(const char* prop) const {
    char buf[256];
    buf[0] = '\0';
    if (IsValid()) UICornerstone_GetString(m_state->instance, m_state->handle, prop, buf, sizeof(buf));
    return std::string(buf);
}
std::string Control::GetEnum(const char* prop) const {
    char buf[64];
    buf[0] = '\0';
    if (IsValid()) UICornerstone_GetEnum(m_state->instance, m_state->handle, prop, buf, sizeof(buf));
    return std::string(buf);
}
void* Control::GetPtr(const char* prop) const {
    void* v = nullptr;
    if (IsValid()) UICornerstone_GetPtr(m_state->instance, m_state->handle, prop, &v);
    return v;
}

// ============================================================
// 回调桥接：C thunk → std::function
// ============================================================
void Control::CallbackThunk(UIControlHandle ctl, const UIEventData* event, void* userData) {
    auto* cb = static_cast<std::shared_ptr<EventCallback>*>(userData);
    if (!cb || !*cb) return;
    Event ev(event);
    (**cb)(ev);
    (void)ctl;
}

void Control::SetCallback(const char* event, EventCallback callback) {
    if (!IsValid()) return;

    auto slot = std::make_shared<EventCallback>(std::move(callback));

    // 注册到 Impl 级注册表（跨 Control 拷贝共享，与 C 端 userData 生命周期一致）
    auto* impl = ownerOf(*m_state);
    if (!impl) return;
    impl->callbackUserData[m_state->handle].push_back(slot);

    UICornerstone_SetCallback(m_state->instance, m_state->handle, event,
                              &Control::CallbackThunk, slot.get());
}

// ============================================================
// 便捷方法
// ============================================================
void Control::SetText(const std::string& text) { SetString("text", text); }
std::string Control::GetText() const { return GetString("text"); }
void Control::SetVisible(bool visible) { SetBool("visible", visible); }
bool Control::IsVisible() const { return GetBool("visible"); }
void Control::SetEnabled(bool enabled) { SetBool("enabled", enabled); }
bool Control::IsEnabled() const { return GetBool("enabled"); }

void Control::SetRect(float x, float y, float w, float h) {
    if (IsValid()) UICornerstone_SetRect(m_state->instance, m_state->handle, x, y, w, h);
}
UIRect Control::GetRect() const {
    UIRect r{0, 0, 0, 0};
    if (IsValid()) UICornerstone_GetRect(m_state->instance, m_state->handle, &r.x, &r.y, &r.w, &r.h);
    return r;
}
void Control::AddChild(Control child) {
    if (IsValid() && child.IsValid())
        UICornerstone_AddChildControl(m_state->instance, m_state->handle, child.Handle());
}
void Control::Destroy() {
    if (m_state && m_state->alive && m_state->instance && m_state->handle) {
        UICornerstone_DestroyControl(m_state->instance, m_state->handle);
        m_state->alive = false;   // 悬挂检测：后续调用静默忽略
    }
}
std::string Control::GetId() const {
    if (!IsValid()) return std::string();
    const char* id = UICornerstone_GetControlId(m_state->instance, m_state->handle);
    return id ? std::string(id) : std::string();
}