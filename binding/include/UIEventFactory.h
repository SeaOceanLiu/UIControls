// UICornerstone C++ Binding — UIEvent 输入事件构造辅助（类型安全）
// 许可证 MIT。内部按 UICornerstoneAPI.h 的 UI_EVENT_* 宏字节布局填充 data[128]。
#ifndef UICORNERSTONE_BINDING_UIEVENTFACTORY_H
#define UICORNERSTONE_BINDING_UIEVENTFACTORY_H

#include <string>
#include <cstring>
#include "UICornerstoneAPI.h"

namespace UICornerstone {
namespace Input {

// 鼠标：x,y 前 8 字节，button 在 data+8
inline UIEvent MouseButton(int button, float x, float y, bool down) {
    UIEvent ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.type = down ? UI_EVENT_MOUSE_DOWN : UI_EVENT_MOUSE_UP;
    UI_EVENT_MOUSE_X(&ev) = x;
    UI_EVENT_MOUSE_Y(&ev) = y;
    UI_EVENT_BUTTON(&ev) = button;
    return ev;
}

inline UIEvent MouseMove(float x, float y) {
    UIEvent ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.type = UI_EVENT_MOUSE_MOVE;
    UI_EVENT_MOUSE_X(&ev) = x;
    UI_EVENT_MOUSE_Y(&ev) = y;
    return ev;
}

// 滚轮：MOUSE_WHEEL（delta, x, y）
inline UIEvent MouseWheel(float dx, float dy, float x, float y) {
    UIEvent ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.type = UI_EVENT_MOUSE_WHEEL;
    UI_EVENT_WHEEL_DELTA(&ev) = dx;
    UI_EVENT_WHEEL_MOUSE_X(&ev) = x;
    UI_EVENT_WHEEL_MOUSE_Y(&ev) = y;
    (void)dy;
    return ev;
}

// 键盘：KEY_DOWN/UP（keyCode 前 4 字节，mod 在 data+4）
inline UIEvent Key(int keyCode, uint16_t mod, bool down) {
    UIEvent ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.type = down ? UI_EVENT_KEY_DOWN : UI_EVENT_KEY_UP;
    UI_EVENT_KEY_CODE(&ev) = keyCode;
    UI_EVENT_KEY_MOD(&ev) = mod;
    return ev;
}

// 文本：data 即 char 缓冲 ≤ UI_TEXT_MAX
inline UIEvent TextInput(const std::string& text) {
    UIEvent ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.type = UI_EVENT_TEXT_INPUT;
    std::strncpy((char*)ev.data, text.c_str(), UI_TEXT_MAX - 1);
    return ev;
}

inline UIEvent WindowResize(int w, int h) {
    UIEvent ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.type = UI_EVENT_WINDOW_RESIZE;
    UI_EVENT_RESIZE_W(&ev) = w;
    UI_EVENT_RESIZE_H(&ev) = h;
    return ev;
}

inline UIEvent WindowClose() {
    UIEvent ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.type = UI_EVENT_WINDOW_CLOSE;
    return ev;
}

} // namespace Input
} // namespace UICornerstone

#endif