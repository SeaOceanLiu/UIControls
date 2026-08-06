// UICornerstone C++ Binding — 事件名 / 属性名常量
// 许可证 MIT。字符串值与核心库事件字典一致（CABI_Property_Design.md §6.9）。
// 不得 include 核心库 GPL 头（PropertyNames.h / EventTypes.h）。
#ifndef UICORNERSTONE_BINDING_NAMES_H
#define UICORNERSTONE_BINDING_NAMES_H

namespace UICornerstone {
namespace Names {

// ── 事件名 ──
inline constexpr const char* kClick            = "click";
inline constexpr const char* kValueChanged     = "value-changed";
inline constexpr const char* kTextChanged      = "text-changed";
inline constexpr const char* kSelectionChanged = "selection-changed";
inline constexpr const char* kCheckChanged     = "check-changed";
inline constexpr const char* kColorChanged     = "color-changed";
inline constexpr const char* kPositionChanged  = "position-changed";
inline constexpr const char* kMoved            = "moved";
inline constexpr const char* kConfirm          = "confirm";
inline constexpr const char* kCancel           = "cancel";
inline constexpr const char* kClose            = "close";
inline constexpr const char* kEnter            = "enter";
inline constexpr const char* kSelect           = "select";
inline constexpr const char* kExpand           = "expand";
inline constexpr const char* kCollapse         = "collapse";

// ── 通用属性名 ──
inline constexpr const char* kBackground = "background";
inline constexpr const char* kBorder     = "border";
inline constexpr const char* kText       = "text";
inline constexpr const char* kCaption    = "caption";
inline constexpr const char* kVisible    = "visible";
inline constexpr const char* kEnabled    = "enabled";
inline constexpr const char* kImage      = "image";
inline constexpr const char* kAnimation  = "animation";
inline constexpr const char* kId         = "id";

} // namespace Names
} // namespace UICornerstone

#endif