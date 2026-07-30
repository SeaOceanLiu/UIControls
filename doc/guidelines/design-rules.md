# 设计规则

## 1. 所有位置数据存储规则

- 所有的屏幕位置数据都存储为未缩放的值
- 生成字体时，使用缩放后的字体大小（即和字体大小相关的数据都是缩放后的）
- 绘制时才对缩放进行处理，而字体因为是已缩放的，所以可以直接绘制

## 2. 避免使用魔鬼数字（Magic Number）

- 所有有业务含义的常量（尺寸、颜色、时间间隔、阈值等）必须定义为具名常量，不得在代码中直接使用字面量。
- 仅在 `ConstDef.h/.cpp` 中定义全局共享常量（如 `EDITBOX_DEFAULT_HEIGHT`、`NUMERICUPDOWN_BUTTON_WIDTH`）。
- 控件内部的局域默认值在类声明中定义为 `static constexpr` 成员，或通过构造函数参数注入。
- 例外：0、1、-1、空字符串、true/false 等无歧义的基础值不受此限制。

## 3. Unicode 字符串编码

- 如果字符串中包含中文等非 ASCII 字符，必须使用 `u8` 前缀（如 `u8"中文"`），确保在多字节/Unicode 编译环境下编码一致。
- **禁止**对中文字符做 `\uxxxx` 转义（如 `"\u4E2D\u6587"`），应直接书写原文。转义后的字符串不可读，且不影响编译结果。例外：非 BMP 字符（如 emoji）可用 surrogate pair 转义。

## 4. 裁剪矩形（Clip Rect）规则

- 全局裁剪（如视口）使用 `RenderDevice::pushClipRect()` / `popClipRect()` 成对管理，确保子绘制完成后自动恢复。
- **禁止**控件内部直接调用 `clearClipRect()`（即 `SDL_SetRenderClipRect(nullptr)`）来解除自家裁剪：这会全局关闭裁剪，破坏父层视口限制。
- 控件需要局部裁剪时，使用 `pushClipRect(局部矩形)` → 绘制内容 → `popClipRect()`。
- **不推荐**每个控件在 `draw()` 开头对自己的 `drawRect` 做无条件 `pushClipRect`。原因：
  - 下拉类控件（ComboBox 下拉列表、Menu 菜单等）需要超出父控件边界绘制，自裁剪会破坏此行为。
  - 频繁 clip rect 切换（~50 控件 × 60fps = 6000 次/秒）虽性能可接受，但可能导致 GPU 批次刷新，降低绘制合并效率。
- 仅当控件明确需要裁剪自身内容（如 EditBox 文本区、TextArea 滚动区、TreeView 行区域、ComboBox 下拉项文字）时才使用 push/popClipRect。

