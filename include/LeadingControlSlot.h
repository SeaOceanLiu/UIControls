// LeadingControlSlot.h — 行内前置控件（leadingControl）的统一支持组件。
//
// 供需要行内前置控件的控件直接持有复用：Menu / TreeView / 后续 ListView、StatusBar、
// TabControl 等。组件承载前置控件行内呈现所需的全部职责：
//   1. 控件持有与替换（setControl/getControl，含挂载/卸载 attachTo/detachFrom）；
//   2. 槽位几何（自然宽高比、文字高度自适应尺寸、行内定位）；
//   3. 对齐（复用 Label 9 宫格语义：top-left / mid-left / bottom-left ...）；
//   4. 间隙（gap：槽位与文字之间的空隙，textStartX 给出文字起点）；
//   5. 事件命中（containsPoint，绘制坐标）。
//
// 语义约定（与 TreeView/Menu 现有行为一致）：
//   - 槽位高 = 文字高度（无字体时回退 fallbackSize），宽 = 高 × 宽高比（1:1 退化正方形）；
//   - layout() 输出「逻辑局部坐标」，宿主可直接 setRect；
//   - 垂直对齐取 AlignmentMode 垂直分量：Top=0、Mid=0.5、Bottom=1（水平分量由宿主决定）。
#pragma once
#include "Utility.h"
#include "Label.h"

class Control;
class Event;

class LeadingControlSlot {
public:
    // ── 配置：setter / getter ──
    void setControl(const std::shared_ptr<Control>& ctl) { m_control = ctl; }
    std::shared_ptr<Control> getControl() const { return m_control; }
    bool hasControl() const { return m_control != nullptr; }

    void setGap(float px) { m_gap = px; }
    float getGap() const { return m_gap; }

    void setAlignmentMode(AlignmentMode mode) { m_align = mode; }
    AlignmentMode getAlignmentMode() const { return m_align; }

    void setFallbackSize(float px) { m_fallback = px; }
    float getFallbackSize() const { return m_fallback; }

    // ── 几何 ──
    // 自然宽高比：纹理自然尺寸优先 → 控件当前 rect 次之 → 退化 1:1
    static float naturalRatio(Control* ctl);

    // 逻辑槽位高：文字高度优先，无字体时回退 fallbackSize
    float getSlotHeight(float fontH) const { return (fontH > 0) ? fontH : m_fallback; }

    // 逻辑槽位宽 = 槽位高 × 宽高比
    float getSlotWidth(float fontH) const;

    // 对齐垂直分量：Top 系列=0，Mid 系列=0.5，Bottom 系列=1（水平分量忽略）
    float verticalFactor() const;
    // 静态版本：宿主以裸 AlignmentMode 计算（TreeView 节点字段等非组件场景）
    static float verticalFactor(AlignmentMode align);

    // 行内定位：输入像素、输出逻辑局部坐标（宿主可直接 setRect）：
    //   rowTopPx / rowHPx  — 行顶与行高（像素）
    //   slotStartXPx       — 槽位水平起点（像素，如文本起点 / icon 区中心）
    //   crLeftPx / crTopPx — 裁剪区左缘/上缘（像素）
    //   scaleX / scaleY    — 宿主缩放（像素 → 逻辑）
    //   fontH              — 文字高度（缩放后像素，0 = 无字体）
    SRect layout(float rowTopPx, float rowHPx, float slotStartXPx,
                 float crLeftPx, float crTopPx, float scaleX, float scaleY,
                 float fontH) const;

    // 文字起点（像素）：槽位右缘 + gap × scaleX
    float textStartX(float slotStartXPx, float scaleX, float fontH) const;

    // ── 挂载管理（宿主树） ──
    void attachTo(Control* host);      // 未挂载则 addControl 到宿主
    void detachFrom(Control* host);    // 已挂载则 removeControl 摘除
    bool isAttached() const { return m_attached; }

    // ── 事件命中（绘制坐标，槽位内） ──
    bool containsPoint(float mx, float my) const;
    // 事件转发给前置控件（返回控件是否消费）
    bool handleEvent(const std::shared_ptr<Event>& ev) const;

    // ── 序列化辅助 ──
    // 对齐字符串（"top-left" / "mid-left" / "bottom-left" ...，与 Label 一致）
    const char* alignmentString() const;
    static bool parseAlignmentString(const char* value, AlignmentMode& out);

private:
    std::shared_ptr<Control> m_control;
    float m_gap = 6.0f;
    AlignmentMode m_align = AlignmentMode::AM_MID_LEFT;
    float m_fallback = 24.0f;
    bool m_attached = false;
};