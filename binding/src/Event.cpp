// UICornerstone C++ Binding — 事件包装实现
// 许可证 MIT。仅读取 C ABI 的 UIEventData（事件字典见 Names.h）。
#include "Event.h"
#include "Names.h"

#include <cstring>

std::string Event::GetName() const {
    return m_raw && m_raw->eventName ? m_raw->eventName : "";
}

// ── 事件名鉴别（字符串比较；值与核心库事件字典一致）──
bool Event::IsClick()          const { return GetName() == UICornerstone::Names::kClick; }
bool Event::IsValueChanged()   const { return GetName() == UICornerstone::Names::kValueChanged; }
float Event::GetValueChanged() const { return m_raw->data.floatVal; }
double Event::GetValueChangedDouble() const { return m_raw->data.doubleVal; }

bool Event::IsTextChanged()    const { return GetName() == UICornerstone::Names::kTextChanged; }
std::string Event::GetTextChanged() const { return m_raw->data.strVal ? m_raw->data.strVal : ""; }

bool Event::IsSelectionChanged() const { return GetName() == UICornerstone::Names::kSelectionChanged; }
int Event::GetSelectedIndex() const { return m_raw->data.selection.idx; }
std::string Event::GetSelectedValue() const { return m_raw->data.selection.val ? m_raw->data.selection.val : ""; }

bool Event::IsCheckChanged() const { return GetName() == UICornerstone::Names::kCheckChanged; }
int Event::GetCheckState() const { return m_raw->data.intVal; }

// ColorPicker 用轮询，绑定不做颜色变更事件推送
bool Event::IsColorChanged() const { return false; }
UIColor Event::GetChangedColor() const {
    UIColor c{m_raw->data.color.r, m_raw->data.color.g, m_raw->data.color.b, m_raw->data.color.a};
    return c;
}

bool Event::IsPositionChanged() const { return GetName() == UICornerstone::Names::kPositionChanged; }
float Event::GetPositionChanged() const { return m_raw->data.floatVal; }

bool Event::IsMoved() const { return GetName() == UICornerstone::Names::kMoved; }
float Event::GetMovedPosition() const { return m_raw->data.floatVal; }

bool Event::IsConfirm() const { return GetName() == UICornerstone::Names::kConfirm; }
bool Event::IsCancel()  const { return GetName() == UICornerstone::Names::kCancel; }

bool Event::IsClose() const { return GetName() == UICornerstone::Names::kClose; }
int Event::GetCloseResult() const { return m_raw->data.intVal; }

bool Event::IsEnter() const { return GetName() == UICornerstone::Names::kEnter; }
bool Event::IsSelect()   const { return GetName() == UICornerstone::Names::kSelect; }
bool Event::IsExpand()   const { return GetName() == UICornerstone::Names::kExpand; }
bool Event::IsCollapse() const { return GetName() == UICornerstone::Names::kCollapse; }
std::string Event::GetNodeId() const { return m_raw->data.treeNode.id ? m_raw->data.treeNode.id : ""; }
void* Event::GetNodeUserData() const { return m_raw->data.treeNode.userData; }

// ── 通用原始访问 ──
int Event::GetIntVal() const { return m_raw->data.intVal; }
float Event::GetFloatVal() const { return m_raw->data.floatVal; }
double Event::GetDoubleVal() const { return m_raw->data.doubleVal; }
const char* Event::GetStrVal() const { return m_raw->data.strVal; }
void* Event::GetPtrVal() const { return m_raw->data.ptrVal; }