# 旧 C ABI 清理计划

**状态：属性扩展全部完成，待删除旧函数 + 更新示例。**

| 动作 | 状态 | 新增 CAPI / 属性扩展 | 替代的旧函数 |
|------|------|---------------------|-------------|
| **新增 Ptr 属性类型** | ✅ 已实现 | `SetPtr(ctl, prop, void*)`, `GetPtr(ctl, prop, void**)` | `SetContent`, `SetSplitterLinkedControls`, `TreeViewGetSelectedUserData` |
| **属性扩展** | ✅ 已实现 | `SetString(ctl, "items", json)` | `SetComboItems` |
| | ✅ 已实现 | `SetString(ctl, "animation", jsoncPath)` | `SetButtonAnimation` |
| | ✅ 已实现 | `SetFloat(ctl, "first-min"/"second-min")` | `SetSplitterMinSize` |
| | ✅ 已实现 | `SetEnum(ctl, "centered-mode", ...)` | `SetDialogCentered` |
| | ✅ 已实现 | `SetString("expand"/"collapse")` / `SetBool("expand-all"/"collapse-all")` | `TreeViewExpandNode` / `CollapseNode` / `ExpandAll` / `CollapseAll` |
| **删除旧函数** | 🔲 待做 | — | 见 §7 清单 |
| **示例迁移** | 🔲 待做 | 改用新属性系统 API | `sample_fromsource.c`, `sample_programmatic.c`, `hello_uicornerstone.c` |

---

## 分类标准

- **DELETE** — 有属性系统等价入口，待删除
- **KEEP** — 保留（本质操作 / 工厂 / 基础设施）

---

## 1. 控件通用操作

| 函数 | 分类 | 方案 |
|------|------|------|
| `UICornerstone_SetRect` | KEEP | 便捷函数，保留 |
| `UICornerstone_GetRect` | KEEP | 便捷函数，保留 |
| `UICornerstone_AddChild` | KEEP | 树操作，非属性 |
| `UICornerstone_DestroyControl` | KEEP | 生命周期，非属性 |
| `UICornerstone_GetControlId` | KEEP | 只读查询，非属性 |
| `UICornerstone_WinFrameSetClientText` | DELETE | `SetString(ctl, "caption", text)` |

## 2. Dialog/Popup

| 函数 | 分类 | 替代方案 |
|------|------|----------|
| `UICornerstone_CreateDialog` | KEEP | 工厂函数 |
| `UICornerstone_Show` | DELETE | `SetBool(ctl, "visible", 1)` |
| `UICornerstone_Close` | DELETE | `SetBool(ctl, "visible", 0)` |
| `UICornerstone_SetDialogCentered` | DELETE | `SetEnum(ctl, "centered-mode", "centered")` |
| `UICornerstone_SetDialogPosition` | DELETE | `SetRect(ctl, x, y, w, h)` |
| `UICornerstone_SetContent` | DELETE | `SetPtr(ctl, "content", ctlHandle)` |
| `UICornerstone_SetOnConfirm` | DELETE | `SetCallback(ctl, "confirm", cb, ud)` |
| `UICornerstone_SetOnCancel` | DELETE | `SetCallback(ctl, "cancel", cb, ud)` |
| `UICornerstone_SetOnClose` | DELETE | `SetCallback(ctl, "close", cb, ud)` |
| `UICornerstone_SetConfirmButtonText` | DELETE | `SetString(ctl, "confirm-text", text)` |
| `UICornerstone_SetCancelButtonText` | DELETE | `SetString(ctl, "cancel-text", text)` |

## 3. TreeView

| 函数 | 分类 | 替代方案 |
|------|------|----------|
| `UICornerstone_TreeViewGetSelectedId` | DELETE | `GetString(ctl, "selected-id", buf, len)` |
| `UICornerstone_TreeViewGetSelectedUserData` | DELETE | `GetPtr(ctl, "selected-user-data", &out)` |
| `UICornerstone_TreeViewExpandNode` | DELETE | `SetString(ctl, "expand", nodeId)` |
| `UICornerstone_TreeViewCollapseNode` | DELETE | `SetString(ctl, "collapse", nodeId)` |
| `UICornerstone_TreeViewExpandAll` | DELETE | `SetBool(ctl, "expand-all", 1)` |
| `UICornerstone_TreeViewCollapseAll` | DELETE | `SetBool(ctl, "collapse-all", 1)` |

## 4. 未在 .h 声明（仅 .cpp、通过 GetProcAddress 使用）

| 函数 | 分类 | 替代方案 |
|------|------|----------|
| `UICornerstone_SetButtonAnimation` | DELETE | `SetString(ctl, "animation", jsoncPath)` |
| `UICornerstone_SetComboItems` | DELETE | `SetString(ctl, "items", jsonItems)` |
| `UICornerstone_SetSplitterLinkedControls` | DELETE | `SetPtr(ctl, "first-linked", first) + SetPtr(ctl, "second-linked", second)` |
| `UICornerstone_SetSplitterMinSize` | DELETE | `SetFloat(ctl, "first-min", min1) + SetFloat(ctl, "second-min", min2)` |

## 5. 示例需要更新

这些函数已在 .h/.cpp 中删除，但示例 `.c` 仍调用：

| 函数 | 替代方案 |
|------|----------|
| `UICornerstone_SetBGColor(r,g,b,a)` | `SetColor(ctl, "background", (UIColor){r,g,b,a})` |
| `UICornerstone_SetText(ctl, text)` | `SetString(ctl, "text"/"caption", text)` |
| `UICornerstone_SetOnClick(cb, ud)` | `SetCallback(ctl, "click", cb, ud)` |

**受影响文件：**
- `samples/sample_programmatic/sample_programmatic.c`
- `samples/sample_fromsource/sample_fromsource.c`
- `samples/hello_uicornerstone/hello_uicornerstone.c`

---

## 7. 完整全库 C ABI 逐函数审计

共 **76 个导出函数**（含已实现的属性系统扩展）。分类：

| 标签 | 含义 |
|------|------|
| **INFRA** | 基础设施 — 保留 |
| **FACTORY** | 控件工厂 — 保留 |
| **NEW-PROP** | 新属性系统 — 保留 |
| **CONVENIENCE** | 便捷函数 — 保留 |
| **CONTROL** | 控件特有操作 — 保留 |
| **DELETE** | 有等价属性系统入口，可删 |

### 7.1–7.5 基础设施 / 工厂（INFRA + FACTORY，共 32 个，不变，略）

保留函数（32 个）：Init/Shutdown/InitFromPlugin(3), Set/GetViewport(2), ProcessEvents/Update/PushUIEvent(3), Render/Clear/Present/IsQuitRequested(4), LoadLayout/LoadLayoutFromFile/FindControl/RegisterAction(4), 16 个工厂。

### 7.6 控件通用操作

| # | 函数 | .h 行 | .cpp 行 | 分类 | 方案 |
|---|------|-------|---------|------|------|
| 33 | `UICornerstone_SetRect` | 228 | 557 | CONVENIENCE | 保留 |
| 34 | `UICornerstone_GetRect` | 229 | 561 | CONVENIENCE | 保留 |
| 35 | `UICornerstone_AddChild` | 230 | 570 | CONTROL | 保留 |
| 36 | `UICornerstone_DestroyControl` | 231 | 594 | CONTROL | 保留 |
| 37 | `UICornerstone_GetControlId` | 232 | 580 | CONTROL | 保留 |
| 38 | `UICornerstone_WinFrameSetClientText` | 233 | 609 | **DELETE** | `SetString(ctl, "caption", text)` |

### 7.7 Dialog / Popup

| # | 函数 | .h 行 | .cpp 行 | 分类 | 方案 |
|---|------|-------|---------|------|------|
| 39 | `UICornerstone_Show` | 239 | 701 | **DELETE** | `SetBool(ctl, "visible", 1)` |
| 40 | `UICornerstone_Close` | 240 | 711 | **DELETE** | `SetBool(ctl, "visible", 0)` |
| 41 | `UICornerstone_SetDialogCentered` | 241 | 724 | **DELETE** | `SetEnum(ctl, "centered-mode", "centered")` |
| 42 | `UICornerstone_SetDialogPosition` | 242 | 734 | **DELETE** | `SetRect(ctl, x, y, w, h)` |
| 43 | `UICornerstone_SetContent` | 243 | 740 | **DELETE** | `SetPtr(ctl, "content", ctlHandle)` |
| 44 | `UICornerstone_SetOnConfirm` | 244 | 758 | **DELETE** | `SetCallback(ctl, "confirm", cb, ud)` |
| 45 | `UICornerstone_SetOnCancel` | 245 | 775 | **DELETE** | `SetCallback(ctl, "cancel", cb, ud)` |
| 46 | `UICornerstone_SetOnClose` | 246 | 785 | **DELETE** | `SetCallback(ctl, "close", cb, ud)` |
| 47 | `UICornerstone_SetConfirmButtonText` | 247 | 799 | **DELETE** | `SetString(ctl, "confirm-text", text)` |
| 48 | `UICornerstone_SetCancelButtonText` | 248 | 807 | **DELETE** | `SetString(ctl, "cancel-text", text)` |

### 7.8 新属性系统（15 个，保留，不变）

| # | 函数 | 分类 |
|---|------|------|
| 49 | `UICornerstone_SetColor` | NEW-PROP |
| 50 | `UICornerstone_SetStateColor` | NEW-PROP |
| 51 | `UICornerstone_SetBool` | NEW-PROP |
| 52 | `UICornerstone_SetInt` | NEW-PROP |
| 53 | `UICornerstone_SetFloat` | NEW-PROP |
| 54 | `UICornerstone_SetString` | NEW-PROP |
| 55 | `UICornerstone_SetEnum` | NEW-PROP |
| 56 | `UICornerstone_SetPtr` | NEW-PROP |
| 57 | `UICornerstone_SetCallback` | NEW-PROP |
| 58 | `UICornerstone_GetColor` | NEW-PROP |
| 59 | `UICornerstone_GetStateColor` | NEW-PROP |
| 60 | `UICornerstone_GetBool` | NEW-PROP |
| 61 | `UICornerstone_GetInt` | NEW-PROP |
| 62 | `UICornerstone_GetFloat` | NEW-PROP |
| 63 | `UICornerstone_GetString` | NEW-PROP |
| 64 | `UICornerstone_GetEnum` | NEW-PROP |
| 65 | `UICornerstone_GetPtr` | NEW-PROP |

### 7.9 TreeView

| # | 函数 | .h 行 | .cpp 行 | 分类 | 方案 |
|---|------|-------|---------|------|------|
| 66 | `UICornerstone_TreeViewGetSelectedId` | 306 | 981 | **DELETE** | `GetString(ctl, "selected-id", buf, len)` |
| 67 | `UICornerstone_TreeViewGetSelectedUserData` | 307 | 989 | **DELETE** | `GetPtr(ctl, "selected-user-data", &out)` |
| 68 | `UICornerstone_TreeViewExpandNode` | 308 | 1000 | **DELETE** | `SetString(ctl, "expand", nodeId)` |
| 69 | `UICornerstone_TreeViewCollapseNode` | 309 | 1005 | **DELETE** | `SetString(ctl, "collapse", nodeId)` |
| 70 | `UICornerstone_TreeViewExpandAll` | 310 | 1010 | **DELETE** | `SetBool(ctl, "expand-all", 1)` |
| 71 | `UICornerstone_TreeViewCollapseAll` | 311 | 1015 | **DELETE** | `SetBool(ctl, "collapse-all", 1)` |

### 7.10 未在 .h 声明（仅 .cpp 实现）

| # | 函数 | .cpp 行 | 分类 | 方案 |
|---|------|---------|------|------|
| 72 | `UICornerstone_SetButtonAnimation` | 535 | **DELETE** | `SetString(ctl, "animation", jsoncPath)` |
| 73 | `UICornerstone_SetComboItems` | 664 | **DELETE** | `SetString(ctl, "items", jsonItems)` |
| 74 | `UICornerstone_SetSplitterLinkedControls` | 834 | **DELETE** | `SetPtr(ctl, "first-linked", f) + SetPtr(ctl, "second-linked", s)` |
| 75 | `UICornerstone_SetSplitterMinSize` | 851 | **DELETE** | `SetFloat(ctl, "first-min", f) + SetFloat(ctl, "second-min", s)` |

### 7.11 示例中已删除 / 待迁移

| # | 函数 | 被调用于 | 替代方案 |
|---|------|---------|----------|
| 76 | `UICornerstone_SetBGColor` | sample_fromsource.c, sample_programmatic.c | `SetColor(ctl, "background", ...)` |
| 77 | `UICornerstone_SetText` | sample_fromsource.c, sample_programmatic.c, hello_uicornerstone.c | `SetString(ctl, "text"/"caption", ...)` |
| 78 | `UICornerstone_SetOnClick` | sample_fromsource.c, sample_programmatic.c | `SetCallback(ctl, "click", ...)` |

---

## 8. 汇总

| 分类 | 数量 | 说明 |
|------|------|------|
| **INFRA** (保留) | 16 | Init/Shutdown/.../FindControl/RegisterAction |
| **FACTORY** (保留) | 16 | CreateXxx 共 16 个 |
| **NEW-PROP** (保留) | 17 | Set/Get Color/StateColor/Bool/Int/Float/String/Enum/Ptr + SetCallback |
| **CONVENIENCE** (保留) | 2 | SetRect, GetRect |
| **CONTROL** (保留) | 3 | AddChild, DestroyControl, GetControlId |
| **DELETE** (待删除) | 19 | WinFrameSetClientText, Show, Close, SetDialogCentered, SetDialogPosition, SetContent, SetOnConfirm/Cancel/Close, SetConfirmButtonText/CancelButtonText, TreeViewGetSelectedId/UserData, TreeViewExpandNode/CollapseNode, TreeViewExpandAll/CollapseAll, SetButtonAnimation, SetComboItems, SetSplitterLinkedControls, SetSplitterMinSize |
| **示例需迁移** | 3 文件 | sample_fromsource.c, sample_programmatic.c, hello_uicornerstone.c |

**剩余工作：**
1. 从 `UICornerstoneAPI.h` 和 `UICornerstoneAPI.cpp` 中 DELETE 标记的 19 个旧函数
2. 更新 3 个示例文件
