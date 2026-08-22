// ============================================================================
// Shape.h -- 形状控件（纯绘制：矩形/圆角/圆/椭圆/折线/多边形，空心与环带）
// 设计：design/Shape_Design.md（决策 3.1-3.13）
// 定位：最简绘制控件——无子控件/无布局引擎/无事件；每个参数的修改均触发
//       rebuildGeometry() 几何重算（§4.8 缓存链路）。
// ============================================================================
#pragma once

#include "ControlBase.h"

#include <vector>

// 形状类型（JSON/CABI 枚举值见 PropertyNames::kShape*）
enum class ShapeType {
    Rect,        // 描边矩形（stroke/lineWidth）
    FilledRect,  // 填充矩形（fill）
    RoundRect,   // 圆角矩形（fill/stroke/lineWidth/radius）
    Circle,      // 圆（内切整个 rect，各向异性拉伸；ringWidth>0 画环带）
    Ellipse,     // 椭圆（同 circle 语义）
    Polyline,    // 折线（开放路径，仅描边）
    Polygon      // 多边形（闭合：凸顶点扇/凹耳切 + 描边）
};

class Shape : public ControlImpl {
public:
    using SPointF = SPoint; // 点 = 本地像素 float 坐标（决策 3.3）

    Shape(Control* parent, const SRect& rect, float xScale = 1.0f, float yScale = 1.0f);

    // ── 参数设置（全部触发 rebuildGeometry）──
    void setShape(ShapeType type);
    void setFillColor(SColor color);          // 透明 = 空心
    void setStrokeColor(SColor color);
    void setLineWidth(float width);           // px，0 = 无描边
    void setRadius(float radius);             // round-rect 生效
    void setRingWidth(float width);           // circle/ellipse 生效；0 = 实心；≥内切半径钳制实心
    // points 以当前 rect 为基准写入（决策 3.3）；resize 时按基准等比缩放
    void setPoints(const std::vector<SPointF>& pts);

    // ── 查询 ──
    ShapeType getShape() const { return m_shape; }
    SColor getFillColor() const { return m_fillColor; }
    SColor getStrokeColor() const { return m_strokeColor; }
    float getLineWidth() const { return m_lineWidth; }
    float getRadius() const { return m_radius; }
    float getRingWidth() const { return m_ringWidth; }
    // 当前缩放后的本地点集（resize 后为等比缩放结果）
    std::vector<SPointF> getPoints() const;
    // 本地 → 全局绘制坐标（主映射 API，决策 3.3）
    SPoint mapToDrawPoint(float lx, float ly) const;
    // 第 index 个顶点的全局坐标 = mapToDrawPoint(points[index])
    SPoint getDrawPoint(int index) const;

    // ── 引擎接口 ──
    void draw(void) override;

    // ── 属性系统 override（CABI 分发；points 走专用 CABI 不走字符串属性） ──
    int setEnumProperty(const char* prop, const char* value) override;   // "shape"
    int setFloatProperty(const char* prop, float value) override;        // "line-width"/"radius"/"ring-width"
    int setColorProperty(const char* prop, SColor color) override;       // "fill"/"stroke"
    int getEnumProperty(const char* prop, const char*& out) override;
    int getFloatProperty(const char* prop, float& out) override;
    int getColorProperty(const char* prop, SColor& out) override;

protected:
    void rebuildGeometry();                                   // 参数变更统一入口（§4.8）

private:
    friend class ShapeBuilder;

    ShapeType m_shape = ShapeType::Rect;
    SColor m_fillColor{0, 0, 0, 0};            // 缺省透明 = 空心
    SColor m_strokeColor{0, 0, 0, 255};        // 缺省黑
    float m_lineWidth = 1.0f;
    float m_radius = 0.0f;
    float m_ringWidth = 0.0f;
    std::vector<SPointF> m_points;              // 基准本地坐标（setPoints 写入时的值）
    SRect m_baseRect;                           // setPoints 时的 rect（缩放基准）
    bool m_hasBaseRect = false;
};
