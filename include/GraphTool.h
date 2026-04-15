#ifndef GRAPHTOOL_H
#define GRAPHTOOL_H

#include "SDL3/SDL.h"
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define GRAPH_TOOL_DEBUG

#include "Utility.h"

// 首先声明全局命名空间中的类
// Utility.h中的类在全局命名空间中

namespace GraphTool {

// ==================== 颜色系统 ====================

// 基础颜色类 - 抽象的颜色表示（使用SColor避免命名冲突）
class SColor {
public:
    // 构造函数 - 支持多种格式
    SColor() : m_r(0.0f), m_g(0.0f), m_b(0.0f), m_a(1.0f) {} // 默认黑色

    SColor(float r, float g, float b, float a = 1.0f) :
        m_r(clamp(r)), m_g(clamp(g)), m_b(clamp(b)), m_a(clamp(a)) {}

    SColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) :
        m_r(r / 255.0f), m_g(g / 255.0f), m_b(b / 255.0f), m_a(a / 255.0f) {}

    SColor(uint32_t rgba) {
        m_r = ((rgba >> 24) & 0xFF) / 255.0f;
        m_g = ((rgba >> 16) & 0xFF) / 255.0f;
        m_b = ((rgba >> 8) & 0xFF) / 255.0f;
        m_a = (rgba & 0xFF) / 255.0f;
    }

    // 静态工厂方法 - 常用颜色
    static SColor Black(float alpha = 1.0f) { return SColor(0.0f, 0.0f, 0.0f, alpha); }
    static SColor White(float alpha = 1.0f) { return SColor(1.0f, 1.0f, 1.0f, alpha); }
    static SColor Red(float alpha = 1.0f) { return SColor(1.0f, 0.0f, 0.0f, alpha); }
    static SColor Green(float alpha = 1.0f) { return SColor(0.0f, 1.0f, 0.0f, alpha); }
    static SColor Blue(float alpha = 1.0f) { return SColor(0.0f, 0.0f, 1.0f, alpha); }
    static SColor Yellow(float alpha = 1.0f) { return SColor(1.0f, 1.0f, 0.0f, alpha); }
    static SColor Cyan(float alpha = 1.0f) { return SColor(0.0f, 1.0f, 1.0f, alpha); }
    static SColor Magenta(float alpha = 1.0f) { return SColor(1.0f, 0.0f, 1.0f, alpha); }
    static SColor Gray(float brightness = 0.5f, float alpha = 1.0f) {
        return SColor(brightness, brightness, brightness, alpha);
    }
    static SColor Transparent() { return SColor(0.0f, 0.0f, 0.0f, 0.0f); }

    // 获取颜色分量
    float red() const { return m_r; }
    float green() const { return m_g; }
    float blue() const { return m_b; }
    float alpha() const { return m_a; }

    uint8_t redByte() const { return static_cast<uint8_t>(m_r * 255); }
    uint8_t greenByte() const { return static_cast<uint8_t>(m_g * 255); }
    uint8_t blueByte() const { return static_cast<uint8_t>(m_b * 255); }
    uint8_t alphaByte() const { return static_cast<uint8_t>(m_a * 255); }

    // 颜色操作
    SColor withAlpha(float alpha) const {
        return SColor(m_r, m_g, m_b, clamp(alpha));
    }

    SColor brighter(float factor = 0.1f) const {
        float f = 1.0f + clamp(factor);
        return SColor(clamp(m_r * f), clamp(m_g * f), clamp(m_b * f), m_a);
    }

    SColor darker(float factor = 0.1f) const {
        float f = 1.0f - clamp(factor);
        return SColor(clamp(m_r * f), clamp(m_g * f), clamp(m_b * f), m_a);
    }

    SColor blend(const SColor& other, float ratio = 0.5f) const {
        float r = clamp(ratio);
        float invR = 1.0f - r;
        return SColor(
            m_r * invR + other.m_r * r,
            m_g * invR + other.m_g * r,
            m_b * invR + other.m_b * r,
            m_a * invR + other.m_a * r
        );
    }

    // 转换为平台特定格式
    SDL_Color toSDLColor() const {
        return SDL_Color{redByte(), greenByte(), blueByte(), alphaByte()};
    }
    SDL_FColor toSDLFColor() const {
        return SDL_FColor{m_r, m_g, m_b, m_a};
    }

    // 运算符重载
    bool operator==(const SColor& other) const {
        return m_r == other.m_r && m_g == other.m_g &&
               m_b == other.m_b && m_a == other.m_a;
    }

    bool operator!=(const SColor& other) const {
        return !(*this == other);
    }

private:
    float m_r, m_g, m_b, m_a; // 0-1范围的浮点数值

    static float clamp(float value) {
        return std::max<float>(0.0f, std::min<float>(1.0f, value));
    }
};

// ==================== 圆角半径结构 ====================
struct SRoundedCorners {
    float topLeft;
    float topRight;
    float bottomRight;
    float bottomLeft;

    SRoundedCorners(float allCorners) :
        topLeft(allCorners), topRight(allCorners),
        bottomRight(allCorners), bottomLeft(allCorners) {}

    SRoundedCorners(float tl, float tr, float br, float bl) :
        topLeft(tl), topRight(tr), bottomRight(br), bottomLeft(bl) {}
};

// ==================== 线段矩形点结构 ====================
struct SLineRectPoints {
    ::SPoint startLeft;    // 起点左旋转90度平移半线宽的点
    ::SPoint startRight;   // 起点右旋转90度平移半线宽的点
    ::SPoint endLeft;      // 终点左旋转90度平移半线宽的点
    ::SPoint endRight;     // 终点右旋转90度平移半线宽的点

    // 构造函数
    SLineRectPoints() = default;

    SLineRectPoints(const ::SPoint& sl, const ::SPoint& sr,
                   const ::SPoint& el, const ::SPoint& er) :
        startLeft(sl), startRight(sr), endLeft(el), endRight(er) {}

    // 转换为向量（保持向后兼容）
    std::vector<::SPoint> toVector() const {
        return {startLeft, startRight, endLeft, endRight};
    }

    // 获取起点两个点
    std::pair<::SPoint, ::SPoint> getStartPoints() const {
        return {startLeft, startRight};
    }

    // 获取终点两个点
    std::pair<::SPoint, ::SPoint> getEndPoints() const {
        return {endLeft, endRight};
    }

    // 检查是否有效（所有点都有效）
    bool isValid() const {
        return !(startLeft.x == 0 && startLeft.y == 0 &&
                startRight.x == 0 && startRight.y == 0 &&
                endLeft.x == 0 && endLeft.y == 0 &&
                endRight.x == 0 && endRight.y == 0);
    }
};

// ==================== 文本对齐枚举 ====================
enum class TextAlignment {
    Left,
    Center,
    Right
};

// ==================== 拐角样式枚举 ====================
enum class CornerStyle {
    Hard,    // 硬角（直接连接）
    Round    // 圆弧角（使用圆弧连接）
};

// ==================== 实用工具函数 ====================
namespace Utils {
    // 颜色工具
    inline SColor interpolateColor(const SColor& c1, const SColor& c2, float t) {
        return c1.blend(c2, t);
    }

    inline SColor multiplyColor(const SColor& color, float factor) {
        return SColor(
            color.red() * factor,
            color.green() * factor,
            color.blue() * factor,
            color.alpha()
        );
    }

    // 几何工具
    inline float distance(const ::SPoint& p1, const ::SPoint& p2) {
        float dx = p2.x - p1.x;
        float dy = p2.y - p1.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    inline ::SPoint interpolate(const ::SPoint& p1, const ::SPoint& p2, float t) {
        return ::SPoint(
            p1.x + (p2.x - p1.x) * t,
            p1.y + (p2.y - p1.y) * t
        );
    }

    inline bool pointInPolygon(const ::SPoint& point, const std::vector<::SPoint>& polygon) {
        if (polygon.size() < 3) return false;

        bool inside = false;
        for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
            if (((polygon[i].y > point.y) != (polygon[j].y > point.y)) &&
                (point.x < (polygon[j].x - polygon[i].x) * (point.y - polygon[i].y) /
                          (polygon[j].y - polygon[i].y) + polygon[i].x)) {
                inside = !inside;
            }
        }
        return inside;
    }

    inline ::SRect boundingBox(const std::vector<::SPoint>& points) {
        if (points.empty()) return ::SRect();

        float minX = points[0].x, maxX = points[0].x;
        float minY = points[0].y, maxY = points[0].y;

        for (const auto& p : points) {
            if (p.x < minX) minX = p.x;
            if (p.x > maxX) maxX = p.x;
            if (p.y < minY) minY = p.y;
            if (p.y > maxY) maxY = p.y;
        }

        return ::SRect(minX, minY, maxX - minX, maxY - minY);
    }

    // 图形生成
    inline std::vector<::SPoint> generateCirclePoints(const ::SPoint& center, float radius, int segments = 36) {
        std::vector<::SPoint> points;
        if (radius <= 0 || segments < 3) return points;

        points.reserve(segments);
        float angleStep = 2.0f * M_PI / segments;

        for (int i = 0; i < segments; ++i) {
            float angle = i * angleStep;
            points.push_back(::SPoint(
                center.x + radius * std::cos(angle),
                center.y + radius * std::sin(angle)
            ));
        }

        return points;
    }

    // 根据线段起点、终点和线宽生成矩形的四个点
    // 返回SLineRectPoints结构体，包含起点左右旋转点和终点左右旋转点
    inline SLineRectPoints generateLineRectPoints(const ::SPoint& start, const ::SPoint& end, float lineWidth) {
        if (lineWidth <= 0.0f) return SLineRectPoints();

        // 计算线段方向向量
        float dx = end.x - start.x;
        float dy = end.y - start.y;
        float length = std::sqrt(dx * dx + dy * dy);

        // 如果起点和终点相同，返回空结构体
        if (length < 0.001f) return SLineRectPoints();

        // 计算单位方向向量
        float ux = dx / length;
        float uy = dy / length;

        // 计算半线宽
        float halfWidth = lineWidth / 2.0f;

        // 计算左旋转90度的向量（逆时针旋转90度）：(-uy, ux)
        float leftX = -uy;
        float leftY = ux;

        // 计算右旋转90度的向量（顺时针旋转90度）：(uy, -ux)
        float rightX = uy;
        float rightY = -ux;

        // 生成四个点并返回结构体
        return SLineRectPoints(
            // 起点左旋转90度平移半线宽的点
            ::SPoint(start.x + halfWidth * leftX, start.y + halfWidth * leftY),
            // 起点右旋转90度平移半线宽的点
            ::SPoint(start.x + halfWidth * rightX, start.y + halfWidth * rightY),
            // 终点左旋转90度平移半线宽的点
            ::SPoint(end.x + halfWidth * leftX, end.y + halfWidth * leftY),
            // 终点右旋转90度平移半线宽的点
            ::SPoint(end.x + halfWidth * rightX, end.y + halfWidth * rightY)
        );
    }

    // 向后兼容版本：返回向量
    inline std::vector<::SPoint> generateLineRectPointsVector(const ::SPoint& start, const ::SPoint& end, float lineWidth) {
        auto points = generateLineRectPoints(start, end, lineWidth);
        return points.toVector();
    }

    inline std::vector<::SPoint> generateEllipsePoints(const ::SPoint& center, float radiusX, float radiusY, int segments = 36) {
        std::vector<::SPoint> points;
        if (radiusX <= 0 || radiusY <= 0 || segments < 3) return points;

        points.reserve(segments);
        float angleStep = 2.0f * M_PI / segments;

        for (int i = 0; i < segments; ++i) {
            float angle = i * angleStep;
            points.push_back(::SPoint(
                center.x + radiusX * std::cos(angle),
                center.y + radiusY * std::sin(angle)
            ));
        }

        return points;
    }

    inline std::vector<::SPoint> generateArcPoints(const ::SPoint& center, float radius,
                                                float startAngle, float endAngle, int segments = 36) {
        std::vector<::SPoint> points;
        if (radius <= 0 || segments < 2) return points;

        // 确保角度在合理范围内
        while (endAngle < startAngle) {
            endAngle += 2.0f * M_PI;
        }

        float angleRange = endAngle - startAngle;
        int actualSegments = std::max<int>(2, static_cast<int>(segments * angleRange / (2.0f * M_PI)));
        float angleStep = angleRange / (actualSegments - 1);

        points.reserve(actualSegments);
        for (int i = 0; i < actualSegments; ++i) {
            float angle = startAngle + i * angleStep;
            points.push_back(::SPoint(
                center.x + radius * std::cos(angle),
                center.y + radius * std::sin(angle)
            ));
        }

        return points;
    }

    inline std::vector<::SPoint> generateRoundedRectPoints(const ::SRect& rect, const SRoundedCorners& corners,
                                                        int segmentsPerCorner = 8) {
        std::vector<::SPoint> points;
        if (rect.width <= 0 || rect.height <= 0 || segmentsPerCorner < 1) return points;

        // 限制圆角半径不超过矩形尺寸的一半
        float maxRadiusX = rect.width / 2.0f;
        float maxRadiusY = rect.height / 2.0f;

        float tl = std::min<float>(corners.topLeft, std::min<float>(maxRadiusX, maxRadiusY));
        float tr = std::min<float>(corners.topRight, std::min<float>(maxRadiusX, maxRadiusY));
        float br = std::min<float>(corners.bottomRight, std::min<float>(maxRadiusX, maxRadiusY));
        float bl = std::min<float>(corners.bottomLeft, std::min<float>(maxRadiusX, maxRadiusY));

        // 生成四个角的点
        // 左上角
        if (tl > 0) {
            float centerX = rect.left + tl;
            float centerY = rect.top + tl;
            for (int i = 0; i <= segmentsPerCorner; ++i) {
                float angle = M_PI + i * (M_PI / 2) / segmentsPerCorner;
                points.push_back(::SPoint(
                    centerX + tl * std::cos(angle),
                    centerY + tl * std::sin(angle)
                ));
            }
        } else {
            points.push_back(::SPoint(rect.left, rect.top));
        }

        // 右上角
        if (tr > 0) {
            float centerX = rect.left + rect.width - tr;
            float centerY = rect.top + tr;
            for (int i = 0; i <= segmentsPerCorner; ++i) {
                float angle = M_PI * 1.5f + i * (M_PI / 2) / segmentsPerCorner;
                points.push_back(::SPoint(
                    centerX + tr * std::cos(angle),
                    centerY + tr * std::sin(angle)
                ));
            }
        } else {
            points.push_back(::SPoint(rect.left + rect.width, rect.top));
        }

        // 右下角
        if (br > 0) {
            float centerX = rect.left + rect.width - br;
            float centerY = rect.top + rect.height - br;
            for (int i = 0; i <= segmentsPerCorner; ++i) {
                float angle = i * (M_PI / 2) / segmentsPerCorner;
                points.push_back(::SPoint(
                    centerX + br * std::cos(angle),
                    centerY + br * std::sin(angle)
                ));
            }
        } else {
            points.push_back(::SPoint(rect.left + rect.width, rect.top + rect.height));
        }

        // 左下角
        if (bl > 0) {
            float centerX = rect.left + bl;
            float centerY = rect.top + rect.height - bl;
            for (int i = 0; i <= segmentsPerCorner; ++i) {
                float angle = M_PI / 2 + i * (M_PI / 2) / segmentsPerCorner;
                points.push_back(::SPoint(
                    centerX + bl * std::cos(angle),
                    centerY + bl * std::sin(angle)
                ));
            }
        } else {
            points.push_back(::SPoint(rect.left, rect.top + rect.height));
        }

        return points;
    }

    inline std::vector<::SPoint> generateRoundedRectPointsSimple(const ::SRect& rect, float radius, int segmentsPerCorner = 8) {
        return generateRoundedRectPoints(rect, SRoundedCorners(radius), segmentsPerCorner);
    }

    // 检查点是否在圆角矩形内
    inline bool pointInRoundedRect(const ::SPoint& point, const ::SRect& rect, const SRoundedCorners& corners) {
        // 首先检查是否在矩形边界内
        if (point.x < rect.left || point.x > rect.left + rect.width ||
            point.y < rect.top || point.y > rect.top + rect.height) {
            return false;
        }

        // 检查四个角区域
        // 左上角
        if (point.x < rect.left + corners.topLeft && point.y < rect.top + corners.topLeft) {
            float dx = point.x - (rect.left + corners.topLeft);
            float dy = point.y - (rect.top + corners.topLeft);
            return (dx * dx + dy * dy) <= (corners.topLeft * corners.topLeft);
        }

        // 右上角
        if (point.x > rect.left + rect.width - corners.topRight && point.y < rect.top + corners.topRight) {
            float dx = point.x - (rect.left + rect.width - corners.topRight);
            float dy = point.y - (rect.top + corners.topRight);
            return (dx * dx + dy * dy) <= (corners.topRight * corners.topRight);
        }

        // 右下角
        if (point.x > rect.left + rect.width - corners.bottomRight &&
            point.y > rect.top + rect.height - corners.bottomRight) {
            float dx = point.x - (rect.left + rect.width - corners.bottomRight);
            float dy = point.y - (rect.top + rect.height - corners.bottomRight);
            return (dx * dx + dy * dy) <= (corners.bottomRight * corners.bottomRight);
        }

        // 左下角
        if (point.x < rect.left + corners.bottomLeft &&
            point.y > rect.top + rect.height - corners.bottomLeft) {
            float dx = point.x - (rect.left + corners.bottomLeft);
            float dy = point.y - (rect.top + rect.height - corners.bottomLeft);
            return (dx * dx + dy * dy) <= (corners.bottomLeft * corners.bottomLeft);
        }

        // 不在角区域，但在矩形内
        return true;
    }

    inline bool pointInRoundedRectSimple(const ::SPoint& point, const ::SRect& rect, float radius) {
        return pointInRoundedRect(point, rect, SRoundedCorners(radius));
    }

    // 计算两条线段的交点或延长线的交点
    // 输入：两条线段的起点和终点 (p1, p2) 和 (p3, p4)
    // 输出：交点坐标，如果线段平行则返回无效点 (0, 0)
    // 参数 segmentOnly: 如果为true，只计算线段实际交点；如果为false，计算延长线交点
    inline ::SPoint lineIntersection(const ::SPoint& p1, const ::SPoint& p2,
                                    const ::SPoint& p3, const ::SPoint& p4,
                                    bool segmentOnly = false) {
        // 计算两条线的方向向量
        float dx1 = p2.x - p1.x;
        float dy1 = p2.y - p1.y;
        float dx2 = p4.x - p3.x;
        float dy2 = p4.y - p3.y;

        // 计算分母（叉积）
        float denominator = dx1 * dy2 - dy1 * dx2;

        // 如果分母接近0，说明两条线平行或重合
        if (std::abs(denominator) < 0.0001f) {
            return ::SPoint(0.0f, 0.0f); // 平行线，无交点
        }

        // 计算参数t和u
        float t = ((p3.x - p1.x) * dy2 - (p3.y - p1.y) * dx2) / denominator;
        float u = ((p3.x - p1.x) * dy1 - (p3.y - p1.y) * dx1) / denominator;

        // 如果只需要线段交点，检查t和u是否在[0,1]范围内
        if (segmentOnly) {
            if (t < 0.0f || t > 1.0f || u < 0.0f || u > 1.0f) {
                return ::SPoint(0.0f, 0.0f); // 交点不在线段上
            }
        }

        // 计算交点坐标
        float intersectX = p1.x + t * dx1;
        float intersectY = p1.y + t * dy1;

        return ::SPoint(intersectX, intersectY);
    }

    // 计算两条线段的交点（仅当线段实际相交时）
    inline ::SPoint segmentIntersection(const ::SPoint& p1, const ::SPoint& p2,
                                       const ::SPoint& p3, const ::SPoint& p4) {
        return lineIntersection(p1, p2, p3, p4, true);
    }

    // 计算两条线的延长线交点（不考虑线段范围）
    inline ::SPoint lineExtensionIntersection(const ::SPoint& p1, const ::SPoint& p2,
                                             const ::SPoint& p3, const ::SPoint& p4) {
        return lineIntersection(p1, p2, p3, p4, false);
    }

    // 检查点是否在线段上（包括端点）
    inline bool pointOnSegment(const ::SPoint& point, const ::SPoint& segStart,
                              const ::SPoint& segEnd, float epsilon = 0.001f) {
        // 首先检查点是否在由线段端点形成的边界框内
        float minX = std::min<float>(segStart.x, segEnd.x) - epsilon;
        float maxX = std::max<float>(segStart.x, segEnd.x) + epsilon;
        float minY = std::min<float>(segStart.y, segEnd.y) - epsilon;
        float maxY = std::max<float>(segStart.y, segEnd.y) + epsilon;

        if (point.x < minX || point.x > maxX || point.y < minY || point.y > maxY) {
            return false;
        }

        // 检查点是否在直线上（使用叉积）
        float cross = (segEnd.x - segStart.x) * (point.y - segStart.y) -
                     (segEnd.y - segStart.y) * (point.x - segStart.x);

        return std::abs(cross) <= epsilon;
    }

    // 检查两条线段是否相交（包括端点）
    inline bool segmentsIntersect(const ::SPoint& p1, const ::SPoint& p2,
                                 const ::SPoint& p3, const ::SPoint& p4) {
        // 快速排斥实验
        if (std::max<float>(p1.x, p2.x) < std::min<float>(p3.x, p4.x) ||
            std::max<float>(p3.x, p4.x) < std::min<float>(p1.x, p2.x) ||
            std::max<float>(p1.y, p2.y) < std::min<float>(p3.y, p4.y) ||
            std::max<float>(p3.y, p4.y) < std::min<float>(p1.y, p2.y)) {
            return false;
        }

        // 跨立实验
        auto cross = [](const ::SPoint& a, const ::SPoint& b, const ::SPoint& c) {
            return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        };

        float c1 = cross(p1, p2, p3);
        float c2 = cross(p1, p2, p4);
        float c3 = cross(p3, p4, p1);
        float c4 = cross(p3, p4, p2);

        // 允许在端点上相交
        if (std::abs(c1) < 0.0001f || std::abs(c2) < 0.0001f ||
            std::abs(c3) < 0.0001f || std::abs(c4) < 0.0001f) {
            // 检查是否在端点上
            return pointOnSegment(p3, p1, p2) || pointOnSegment(p4, p1, p2) ||
                   pointOnSegment(p1, p3, p4) || pointOnSegment(p2, p3, p4);
        }

        return (c1 * c2 < 0) && (c3 * c4 < 0);
    }

    // 判断点是否位于矩形内或矩形边线上
    // 参数：
    //   point: 要检查的点
    //   rect: 矩形
    //   epsilon: 容差，用于判断点是否在边线上
    // 返回值：
    //   true: 点在矩形内或边线上
    //   false: 点在矩形外
    inline bool pointInRect(const ::SPoint& point, const ::SRect& rect, float epsilon = 0.001f) {
        // 计算矩形的边界，处理负宽度和负高度的情况
        float left = rect.width >= 0 ? rect.left : rect.left + rect.width;
        float right = rect.width >= 0 ? rect.left + rect.width : rect.left;
        float top = rect.height >= 0 ? rect.top : rect.top + rect.height;
        float bottom = rect.height >= 0 ? rect.top + rect.height : rect.top;

        // 检查点是否在矩形边界内（包括容差）
        // 点在矩形内：left <= x <= right 且 top <= y <= bottom
        // 考虑容差：left - epsilon <= x <= right + epsilon 且 top - epsilon <= y <= bottom + epsilon
        bool inside = (point.x >= left - epsilon) && (point.x <= right + epsilon) &&
                      (point.y >= top - epsilon) && (point.y <= bottom + epsilon);

        return inside;
    }

    // 判断点是否严格位于矩形内部（不在边线上）
    // 参数：
    //   point: 要检查的点
    //   rect: 矩形
    //   epsilon: 容差，用于判断点是否在边线上
    // 返回值：
    //   true: 点在矩形内部（不在边线上）
    //   false: 点在矩形外部或边线上
    inline bool pointInsideRect(const ::SPoint& point, const ::SRect& rect, float epsilon = 0.001f) {
        // 计算矩形的边界，处理负宽度和负高度的情况
        float left = rect.width >= 0 ? rect.left : rect.left + rect.width;
        float right = rect.width >= 0 ? rect.left + rect.width : rect.left;
        float top = rect.height >= 0 ? rect.top : rect.top + rect.height;
        float bottom = rect.height >= 0 ? rect.top + rect.height : rect.top;

        // 检查点是否在矩形内部（排除边线）
        // 点在矩形内部：left < x < right 且 top < y < bottom
        // 考虑容差：left + epsilon < x < right - epsilon 且 top + epsilon < y < bottom - epsilon
        bool inside = (point.x > left + epsilon) && (point.x < right - epsilon) &&
                      (point.y > top + epsilon) && (point.y < bottom - epsilon);

        return inside;
    }

    // 判断点是否在矩形边线上
    // 参数：
    //   point: 要检查的点
    //   rect: 矩形
    //   epsilon: 容差，用于判断点是否在边线上
    // 返回值：
    //   true: 点在矩形边线上
    //   false: 点不在矩形边线上（可能在内部或外部）
    inline bool pointOnRectBorder(const ::SPoint& point, const ::SRect& rect, float epsilon = 0.001f) {
        // 计算矩形的边界，处理负宽度和负高度的情况
        float left = rect.width >= 0 ? rect.left : rect.left + rect.width;
        float right = rect.width >= 0 ? rect.left + rect.width : rect.left;
        float top = rect.height >= 0 ? rect.top : rect.top + rect.height;
        float bottom = rect.height >= 0 ? rect.top + rect.height : rect.top;

        // 检查点是否在矩形边线上
        // 点在边线上需要满足以下条件之一：
        // 1. 在左边界或右边界上，且y在top和bottom之间
        // 2. 在上边界或下边界上，且x在left和right之间

        // 检查是否在左边界或右边界上
        bool onVerticalEdge = ((std::abs(point.x - left) <= epsilon) || (std::abs(point.x - right) <= epsilon)) &&
                              (point.y >= top - epsilon) && (point.y <= bottom + epsilon);

        // 检查是否在上边界或下边界上
        bool onHorizontalEdge = ((std::abs(point.y - top) <= epsilon) || (std::abs(point.y - bottom) <= epsilon)) &&
                                (point.x >= left - epsilon) && (point.x <= right + epsilon);

        return onVerticalEdge || onHorizontalEdge;
    }

    // 判断点是否在矩形角上
    // 参数：
    //   point: 要检查的点
    //   rect: 矩形
    //   epsilon: 容差，用于判断点是否在角上
    // 返回值：
    //   true: 点在矩形角上
    //   false: 点不在矩形角上
    inline bool pointOnRectCorner(const ::SPoint& point, const ::SRect& rect, float epsilon = 0.001f) {
        // 计算矩形的边界，处理负宽度和负高度的情况
        float left = rect.width >= 0 ? rect.left : rect.left + rect.width;
        float right = rect.width >= 0 ? rect.left + rect.width : rect.left;
        float top = rect.height >= 0 ? rect.top : rect.top + rect.height;
        float bottom = rect.height >= 0 ? rect.top + rect.height : rect.top;

        // 计算矩形的四个角
        ::SPoint topLeft(left, top);
        ::SPoint topRight(right, top);
        ::SPoint bottomRight(right, bottom);
        ::SPoint bottomLeft(left, bottom);

        // 检查点是否接近任何一个角
        return (distance(point, topLeft) <= epsilon) ||
               (distance(point, topRight) <= epsilon) ||
               (distance(point, bottomRight) <= epsilon) ||
               (distance(point, bottomLeft) <= epsilon);
    }

    // 判断点是否在矩形边线上（包括角）
    // 这是pointOnRectBorder的别名，提供更直观的命名
    inline bool pointOnRectEdge(const ::SPoint& point, const ::SRect& rect, float epsilon = 0.001f) {
        return pointOnRectBorder(point, rect, epsilon);
    }
}

// ==================== 绘图上下文 ====================
class DrawingContext {
public:
    DrawingContext(SDL_Renderer* renderer) :
        m_renderer(renderer),
        m_penColor(SColor::Black()),
        m_fillColor(SColor::White()),
        m_penWidth(1.0f),
        m_fontSize(12.0f),
        m_cornerStyle(CornerStyle::Round) {
        if (!renderer) {
            SDL_Log("DrawingContext: Invalid renderer provided");
        }
    }

    // 画笔设置
    void setPenColor(const SColor& color) { m_penColor = color; }
    void setPenWidth(float width) { m_penWidth = width > 0 ? width : 1.0f; }
    void setFillColor(const SColor& color) { m_fillColor = color; }
    void setFont(const std::string& fontName, float size) {
        m_fontName = fontName;
        m_fontSize = size > 0 ? size : 12.0f;
    }

    // 基本图形绘制
    void drawPoint(const ::SPoint& point) {
        drawPoint(point.x, point.y);
    }

    void drawPoint(float x, float y) {
        if (!m_renderer) return;
        SDL_Color color = m_penColor.toSDLColor();
        SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
        SDL_RenderPoint(m_renderer, x, y);
    }

    void drawLine(const ::SPoint& start, const ::SPoint& end) {
        drawLine(start.x, start.y, end.x, end.y);
    }

    void drawLine(float x1, float y1, float x2, float y2) {
        if (!m_renderer || m_penWidth <= 0) return;

        // 如果线宽为1或更小，使用SDL的默认绘制
        if (m_penWidth <= 1.0f) {
            SDL_Color color = m_penColor.toSDLColor();
            SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
            SDL_RenderLine(m_renderer, x1, y1, x2, y2);
            return;
        }

        // 对于粗线，使用generateLineRectPoints工具函数生成矩形四个点
        ::SPoint start(x1, y1);
        ::SPoint end(x2, y2);

        auto rectPoints = Utils::generateLineRectPoints(start, end, m_penWidth);

        if (!rectPoints.isValid()) {
            // 如果生成的矩形点无效（例如起点终点相同），绘制一个点
            drawPoint(x1, y1);
            return;
        }

        // 使用生成的矩形点绘制矩形
        drawLine(rectPoints);
    }

    void drawLine(int x1, int y1, int x2, int y2){
        /*
        使用Bresenham算法绘制直线，会有锯齿适用于像素级绘制
        :param x1, y1: 起点坐标
        :param x2, y2: 终点坐标
        :param draw_point_func: 画点函数，接受(x, y)参数
        */
        int dx = abs(x2 - x1);
        int dy = abs(y2 - y1);
        int sx = x1 < x2 ? 1 : -1;
        int sy = y1 < y2 ? 1 : -1;
        int err = dx - dy;

        while (true) {
            drawPoint(x1, y1);  // 调用已有的画点函数
            if (x1 == x2 && y1 == y2)
                break;
            int e2 = 2 * err;
            if (e2 > -dy){
                err -= dy;
                x1 += sx;
            }
            if (e2 < dx){
                err += dx;
                y1 += sy;
            }
        }
    }

    // 新增：使用SLineRectPoints作为参数绘制粗线
    void drawLine(const SLineRectPoints& rectPoints) {
        if (!m_renderer || !rectPoints.isValid()) return;

        // 使用生成的矩形点绘制矩形
        SDL_Color color = m_penColor.toSDLColor();
        SDL_FColor fcolor = {m_penColor.red(), m_penColor.green(), m_penColor.blue(), m_penColor.alpha()};

        // 创建四个顶点
        SDL_Vertex vertices[4];

        // 顶点顺序：startLeft, startRight, endRight, endLeft
        // 这样形成的四边形可以正确绘制矩形
        vertices[0].position = SDL_FPoint{rectPoints.startLeft.x, rectPoints.startLeft.y};
        vertices[0].color = fcolor;
        vertices[0].tex_coord = SDL_FPoint{0, 0};

        vertices[1].position = SDL_FPoint{rectPoints.startRight.x, rectPoints.startRight.y};
        vertices[1].color = fcolor;
        vertices[1].tex_coord = SDL_FPoint{0, 0};

        vertices[2].position = SDL_FPoint{rectPoints.endRight.x, rectPoints.endRight.y};
        vertices[2].color = fcolor;
        vertices[2].tex_coord = SDL_FPoint{0, 0};

        vertices[3].position = SDL_FPoint{rectPoints.endLeft.x, rectPoints.endLeft.y};
        vertices[3].color = fcolor;
        vertices[3].tex_coord = SDL_FPoint{0, 0};

        // 使用两个三角形绘制矩形
        // 三角形1: startLeft -> startRight -> endRight
        // 三角形2: startLeft -> endRight -> endLeft
        int indices[6] = {0, 1, 2, 0, 2, 3};
        SDL_RenderGeometry(m_renderer, nullptr, vertices, 4, indices, 6);
    }

    void drawRect(const ::SRect& rect, bool filled = false) {
        if (!m_renderer) return;

        if (filled) {
            // 填充矩形
            SDL_Color color = m_fillColor.toSDLColor();
            SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
            SDL_FRect sdlRect = {rect.left, rect.top, rect.width, rect.height};
            SDL_RenderFillRect(m_renderer, &sdlRect);
        } else {
            // 绘制矩形边框，支持线宽
            if (m_penWidth <= 1.0f) {
                // 线宽为1或更小，使用SDL的默认绘制
                SDL_Color color = m_penColor.toSDLColor();
                SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
                SDL_FRect sdlRect = {rect.left, rect.top, rect.width, rect.height};
                SDL_RenderRect(m_renderer, &sdlRect);
            } else {
                // 绘制粗边框：绘制两个矩形（外矩形和内矩形），然后填充中间区域
                float halfWidth = m_penWidth / 2.0f;

                // 计算外矩形和内矩形
                ::SRect outerRect(
                    rect.left - halfWidth,
                    rect.top - halfWidth,
                    rect.width + m_penWidth,
                    rect.height + m_penWidth
                );

                ::SRect innerRect(
                    rect.left + halfWidth,
                    rect.top + halfWidth,
                    rect.width - m_penWidth,
                    rect.height - m_penWidth
                );

                // 如果内矩形的宽度或高度为0或负数，直接绘制填充外矩形
                if (innerRect.width <= 0 || innerRect.height <= 0) {
                    // 保存当前填充颜色
                    SColor oldFillColor = m_fillColor;
                    // 临时使用画笔颜色作为填充颜色
                    setFillColor(m_penColor);
                    drawRect(outerRect, true);
                    // 恢复填充颜色
                    setFillColor(oldFillColor);
                    return;
                }

                // 绘制外矩形和内矩形之间的区域
                // 使用四个矩形区域来填充
                SDL_Color color = m_penColor.toSDLColor();
                SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);

                // 上边区域
                SDL_FRect topRect = {
                    outerRect.left,
                    outerRect.top,
                    outerRect.width,
                    halfWidth * 2
                };
                SDL_RenderFillRect(m_renderer, &topRect);

                // 下边区域
                SDL_FRect bottomRect = {
                    outerRect.left,
                    outerRect.top + outerRect.height - halfWidth * 2,
                    outerRect.width,
                    halfWidth * 2
                };
                SDL_RenderFillRect(m_renderer, &bottomRect);

                // 左边区域（排除上下边已绘制的部分）
                SDL_FRect leftRect = {
                    outerRect.left,
                    outerRect.top + halfWidth * 2,
                    halfWidth * 2,
                    outerRect.height - halfWidth * 4
                };
                SDL_RenderFillRect(m_renderer, &leftRect);

                // 右边区域（排除上下边已绘制的部分）
                SDL_FRect rightRect = {
                    outerRect.left + outerRect.width - halfWidth * 2,
                    outerRect.top + halfWidth * 2,
                    halfWidth * 2,
                    outerRect.height - halfWidth * 4
                };
                SDL_RenderFillRect(m_renderer, &rightRect);
            }
        }
    }

    // 圆角矩形绘制
    void drawRoundedRect(const ::SRect& rect, float radius, bool filled = false) {
        drawRoundedRect(rect, SRoundedCorners(radius), filled);
    }

    void drawRoundedRect(const ::SRect& rect, const SRoundedCorners& corners, bool filled = false) {
        if (!m_renderer) return;

        if (filled) {
            // 填充圆角矩形
            auto points = Utils::generateRoundedRectPoints(rect, corners);
            if (points.empty()) return;

            SColor currentColor = m_fillColor;
            SDL_Color sdlColor = currentColor.toSDLColor();
            SDL_SetRenderDrawColor(m_renderer, sdlColor.r, sdlColor.g, sdlColor.b, sdlColor.a);

            // 使用三角形扇绘制填充
            if (points.size() >= 3) {
                for (size_t i = 1; i < points.size() - 1; ++i) {
                    // 使用三角形绘制填充
                    SDL_Vertex vertices[3];
                    vertices[0].position = SDL_FPoint{points[0].x, points[0].y};
                    vertices[0].color = SDL_FColor{currentColor.red(), currentColor.green(), currentColor.blue(), currentColor.alpha()};
                    vertices[0].tex_coord = SDL_FPoint{0, 0};

                    vertices[1].position = SDL_FPoint{points[i].x, points[i].y};
                    vertices[1].color = SDL_FColor{currentColor.red(), currentColor.green(), currentColor.blue(), currentColor.alpha()};
                    vertices[1].tex_coord = SDL_FPoint{0, 0};

                    vertices[2].position = SDL_FPoint{points[i + 1].x, points[i + 1].y};
                    vertices[2].color = SDL_FColor{currentColor.red(), currentColor.green(), currentColor.blue(), currentColor.alpha()};
                    vertices[2].tex_coord = SDL_FPoint{0, 0};

                    SDL_RenderGeometry(m_renderer, nullptr, vertices, 3, nullptr, 0);
                }
            }
        } else {
            // 绘制圆角矩形边框，支持线宽
            if (m_penWidth <= 1.0f) {
                // 线宽为1或更小，使用SDL的默认绘制
                auto points = Utils::generateRoundedRectPoints(rect, corners);
                if (points.empty()) return;

                SDL_Color color = m_penColor.toSDLColor();
                SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);

                for (size_t i = 0; i < points.size(); ++i) {
                    size_t next = (i + 1) % points.size();
                    SDL_RenderLine(m_renderer, points[i].x, points[i].y, points[next].x, points[next].y);
                }
            } else {
                // 绘制粗边框：使用两个圆角矩形，然后填充中间区域
                float halfWidth = m_penWidth / 2.0f;

                // 计算外矩形和内矩形
                ::SRect outerRect(
                    rect.left - halfWidth,
                    rect.top - halfWidth,
                    rect.width + m_penWidth,
                    rect.height + m_penWidth
                );

                ::SRect innerRect(
                    rect.left + halfWidth,
                    rect.top + halfWidth,
                    rect.width - m_penWidth,
                    rect.height - m_penWidth
                );

                // 调整圆角半径
                SRoundedCorners outerCorners(
                    corners.topLeft + halfWidth,
                    corners.topRight + halfWidth,
                    corners.bottomRight + halfWidth,
                    corners.bottomLeft + halfWidth
                );

                SRoundedCorners innerCorners(
                    std::max<float>(0.0f, corners.topLeft - halfWidth),
                    std::max<float>(0.0f, corners.topRight - halfWidth),
                    std::max<float>(0.0f, corners.bottomRight - halfWidth),
                    std::max<float>(0.0f, corners.bottomLeft - halfWidth)
                );

                // 如果内矩形的宽度或高度为0或负数，直接绘制填充外矩形
                if (innerRect.width <= 0 || innerRect.height <= 0) {
                    // 保存当前填充颜色
                    SColor oldFillColor = m_fillColor;
                    // 临时使用画笔颜色作为填充颜色
                    setFillColor(m_penColor);
                    drawRoundedRect(outerRect, outerCorners, true);
                    // 恢复填充颜色
                    setFillColor(oldFillColor);
                    return;
                }

                // 生成内外圆角矩形的顶点
                auto outerPoints = Utils::generateRoundedRectPoints(outerRect, outerCorners);
                auto innerPoints = Utils::generateRoundedRectPoints(innerRect, innerCorners);

                if (outerPoints.empty() || innerPoints.empty()) return;

                // 直接绘制圆角矩形环
                // 使用三角形带绘制内外多边形之间的区域
                SDL_Color color = m_penColor.toSDLColor();
                SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);

                // 对于每个角，绘制三角形带
                // 注意：这里简化处理，假设内外多边形的顶点数相同
                // 实际应用中，内外多边形的顶点数可能不同，需要更复杂的算法

                // 计算每个角的顶点数
                int segmentsPerCorner = 8; // 与Utils::generateRoundedRectPoints默认值一致
                int verticesPerCorner = segmentsPerCorner + 1;

                // 绘制四个角
                for (int corner = 0; corner < 4; ++corner) {
                    int startIdx = corner * verticesPerCorner;
                    int endIdx = startIdx + verticesPerCorner;

                    // 确保索引在范围内
                    if (endIdx > outerPoints.size() || endIdx > innerPoints.size()) {
                        continue;
                    }

                    // 绘制这个角的三角形带
                    for (int i = startIdx; i < endIdx - 1; ++i) {
                        int next = i + 1;

                        SDL_Vertex vertices[4];
                        SDL_FColor fcolor = {m_penColor.red(), m_penColor.green(), m_penColor.blue(), m_penColor.alpha()};

                        // 顶点1: 内多边形当前点
                        vertices[0].position = SDL_FPoint{innerPoints[i].x, innerPoints[i].y};
                        vertices[0].color = fcolor;
                        vertices[0].tex_coord = SDL_FPoint{0, 0};

                        // 顶点2: 外多边形当前点
                        vertices[1].position = SDL_FPoint{outerPoints[i].x, outerPoints[i].y};
                        vertices[1].color = fcolor;
                        vertices[1].tex_coord = SDL_FPoint{0, 0};

                        // 顶点3: 外多边形下一个点
                        vertices[2].position = SDL_FPoint{outerPoints[next].x, outerPoints[next].y};
                        vertices[2].color = fcolor;
                        vertices[2].tex_coord = SDL_FPoint{0, 0};

                        // 顶点4: 内多边形下一个点
                        vertices[3].position = SDL_FPoint{innerPoints[next].x, innerPoints[next].y};
                        vertices[3].color = fcolor;
                        vertices[3].tex_coord = SDL_FPoint{0, 0};

                        // 使用两个三角形绘制四边形
                        int indices[6] = {0, 1, 2, 0, 2, 3};
                        SDL_RenderGeometry(m_renderer, nullptr, vertices, 4, indices, 6);
                    }
                }

                // 绘制四条边
                // 上边
                for (int i = verticesPerCorner * 1 - 1; i < verticesPerCorner * 2; ++i) {
                    if (i + 1 >= outerPoints.size() || i + 1 >= innerPoints.size()) break;

                    SDL_Vertex vertices[4];
                    SDL_FColor fcolor = {m_penColor.red(), m_penColor.green(), m_penColor.blue(), m_penColor.alpha()};

                    vertices[0].position = SDL_FPoint{innerPoints[i].x, innerPoints[i].y};
                    vertices[0].color = fcolor;
                    vertices[0].tex_coord = SDL_FPoint{0, 0};

                    vertices[1].position = SDL_FPoint{outerPoints[i].x, outerPoints[i].y};
                    vertices[1].color = fcolor;
                    vertices[1].tex_coord = SDL_FPoint{0, 0};

                    vertices[2].position = SDL_FPoint{outerPoints[i + 1].x, outerPoints[i + 1].y};
                    vertices[2].color = fcolor;
                    vertices[2].tex_coord = SDL_FPoint{0, 0};

                    vertices[3].position = SDL_FPoint{innerPoints[i + 1].x, innerPoints[i + 1].y};
                    vertices[3].color = fcolor;
                    vertices[3].tex_coord = SDL_FPoint{0, 0};

                    int indices[6] = {0, 1, 2, 0, 2, 3};
                    SDL_RenderGeometry(m_renderer, nullptr, vertices, 4, indices, 6);
                }

                // 右边
                for (int i = verticesPerCorner * 2 - 1; i < verticesPerCorner * 3; ++i) {
                    if (i + 1 >= outerPoints.size() || i + 1 >= innerPoints.size()) break;

                    SDL_Vertex vertices[4];
                    SDL_FColor fcolor = {m_penColor.red(), m_penColor.green(), m_penColor.blue(), m_penColor.alpha()};

                    vertices[0].position = SDL_FPoint{innerPoints[i].x, innerPoints[i].y};
                    vertices[0].color = fcolor;
                    vertices[0].tex_coord = SDL_FPoint{0, 0};

                    vertices[1].position = SDL_FPoint{outerPoints[i].x, outerPoints[i].y};
                    vertices[1].color = fcolor;
                    vertices[1].tex_coord = SDL_FPoint{0, 0};

                    vertices[2].position = SDL_FPoint{outerPoints[i + 1].x, outerPoints[i + 1].y};
                    vertices[2].color = fcolor;
                    vertices[2].tex_coord = SDL_FPoint{0, 0};

                    vertices[3].position = SDL_FPoint{innerPoints[i + 1].x, innerPoints[i + 1].y};
                    vertices[3].color = fcolor;
                    vertices[3].tex_coord = SDL_FPoint{0, 0};

                    int indices[6] = {0, 1, 2, 0, 2, 3};
                    SDL_RenderGeometry(m_renderer, nullptr, vertices, 4, indices, 6);
                }

                // 下边
                for (int i = verticesPerCorner * 3 - 1; i < verticesPerCorner * 4; ++i) {
                    if (i + 1 >= outerPoints.size() || i + 1 >= innerPoints.size()) break;

                    SDL_Vertex vertices[4];
                    SDL_FColor fcolor = {m_penColor.red(), m_penColor.green(), m_penColor.blue(), m_penColor.alpha()};

                    vertices[0].position = SDL_FPoint{innerPoints[i].x, innerPoints[i].y};
                    vertices[0].color = fcolor;
                    vertices[0].tex_coord = SDL_FPoint{0, 0};

                    vertices[1].position = SDL_FPoint{outerPoints[i].x, outerPoints[i].y};
                    vertices[1].color = fcolor;
                    vertices[1].tex_coord = SDL_FPoint{0, 0};

                    vertices[2].position = SDL_FPoint{outerPoints[i + 1].x, outerPoints[i + 1].y};
                    vertices[2].color = fcolor;
                    vertices[2].tex_coord = SDL_FPoint{0, 0};

                    vertices[3].position = SDL_FPoint{innerPoints[i + 1].x, innerPoints[i + 1].y};
                    vertices[3].color = fcolor;
                    vertices[3].tex_coord = SDL_FPoint{0, 0};

                    int indices[6] = {0, 1, 2, 0, 2, 3};
                    SDL_RenderGeometry(m_renderer, nullptr, vertices, 4, indices, 6);
                }

                // 左边（连接最后一个点和第一个点）
                if (outerPoints.size() > 0 && innerPoints.size() > 0) {
                    int last = outerPoints.size() - 1;

                    SDL_Vertex vertices[4];
                    SDL_FColor fcolor = {m_penColor.red(), m_penColor.green(), m_penColor.blue(), m_penColor.alpha()};

                    vertices[0].position = SDL_FPoint{innerPoints[last].x, innerPoints[last].y};
                    vertices[0].color = fcolor;
                    vertices[0].tex_coord = SDL_FPoint{0, 0};

                    vertices[1].position = SDL_FPoint{outerPoints[last].x, outerPoints[last].y};
                    vertices[1].color = fcolor;
                    vertices[1].tex_coord = SDL_FPoint{0, 0};

                    vertices[2].position = SDL_FPoint{outerPoints[0].x, outerPoints[0].y};
                    vertices[2].color = fcolor;
                    vertices[2].tex_coord = SDL_FPoint{0, 0};

                    vertices[3].position = SDL_FPoint{innerPoints[0].x, innerPoints[0].y};
                    vertices[3].color = fcolor;
                    vertices[3].tex_coord = SDL_FPoint{0, 0};

                    int indices[6] = {0, 1, 2, 0, 2, 3};
                    SDL_RenderGeometry(m_renderer, nullptr, vertices, 4, indices, 6);
                }
            }
        }
    }

    // 带边框的圆角矩形
    void drawRoundedRectWithBorder(const ::SRect& rect, float radius,
                                  float borderWidth, const SColor& borderColor,
                                  const SColor& fillColor) {
        drawRoundedRectWithBorder(rect, SRoundedCorners(radius), borderWidth, borderColor, fillColor);
    }

    void drawRoundedRectWithBorder(const ::SRect& rect, const SRoundedCorners& corners,
                                  float borderWidth, const SColor& borderColor,
                                  const SColor& fillColor) {
        if (!m_renderer || borderWidth <= 0) return;

        // 保存当前颜色
        SColor oldPenColor = m_penColor;
        SColor oldFillColor = m_fillColor;

        // 绘制填充
        setFillColor(fillColor);
        drawRoundedRect(rect, corners, true);

        // 绘制边框
        setPenColor(borderColor);
        setPenWidth(borderWidth);
        drawRoundedRect(rect, corners, false);

        // 恢复颜色
        setPenColor(oldPenColor);
        setFillColor(oldFillColor);
    }

    void drawCircle(const ::SPoint& center, float radius, bool filled = false) {
        if (!m_renderer || radius <= 0) return;

        if (filled) {
            // 填充圆形
            auto points = Utils::generateCirclePoints(center, radius);
            if (points.empty()) return;

            SColor currentColor = m_fillColor;
            SDL_Color sdlColor = currentColor.toSDLColor();
            SDL_SetRenderDrawColor(m_renderer, sdlColor.r, sdlColor.g, sdlColor.b, sdlColor.a);

            // 使用三角形扇绘制填充
            if (points.size() >= 3) {
                // 绘制完整的圆形：从第一个点到最后一个点
                for (size_t i = 0; i < points.size(); ++i) {
                    size_t next = (i + 1) % points.size();

                    // 使用三角形绘制填充
                    SDL_Vertex vertices[3];
                    vertices[0].position = SDL_FPoint{center.x, center.y};
                    vertices[0].color = SDL_FColor{currentColor.red(), currentColor.green(), currentColor.blue(), currentColor.alpha()};
                    vertices[0].tex_coord = SDL_FPoint{0, 0};

                    vertices[1].position = SDL_FPoint{points[i].x, points[i].y};
                    vertices[1].color = SDL_FColor{currentColor.red(), currentColor.green(), currentColor.blue(), currentColor.alpha()};
                    vertices[1].tex_coord = SDL_FPoint{0, 0};

                    vertices[2].position = SDL_FPoint{points[next].x, points[next].y};
                    vertices[2].color = SDL_FColor{currentColor.red(), currentColor.green(), currentColor.blue(), currentColor.alpha()};
                    vertices[2].tex_coord = SDL_FPoint{0, 0};
                    SDL_RenderGeometry(m_renderer, nullptr, vertices, 3, nullptr, 0);
                }
            }
        } else {
            // 绘制圆形边框，支持线宽
            if (m_penWidth <= 1.0f) {
                // 线宽为1或更小，使用SDL的默认绘制
                auto points = Utils::generateCirclePoints(center, radius);
                if (points.empty()) return;

                SDL_Color color = m_penColor.toSDLColor();
                SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);

                for (size_t i = 0; i < points.size(); ++i) {
                    size_t next = (i + 1) % points.size();
                    SDL_RenderLine(m_renderer, points[i].x, points[i].y, points[next].x, points[next].y);
                }
            } else {
                // 绘制粗边框：绘制两个同心圆，然后填充中间区域
                float innerRadius = radius - m_penWidth / 2.0f;
                float outerRadius = radius + m_penWidth / 2.0f;

                if (innerRadius <= 0) {
                    // 如果内径为0或负数，直接绘制填充圆形
                    drawCircle(center, outerRadius, true);
                } else {
                    // 生成内外圆的点
                    auto innerPoints = Utils::generateCirclePoints(center, innerRadius, 72);
                    auto outerPoints = Utils::generateCirclePoints(center, outerRadius, 72);

                    if (innerPoints.size() != outerPoints.size() || innerPoints.empty()) return;

                    // 使用三角形带绘制圆环
                    for (size_t i = 0; i < innerPoints.size(); ++i) {
                        size_t next = (i + 1) % innerPoints.size();

                        SDL_Vertex vertices[4];
                        SDL_FColor fcolor = {m_penColor.red(), m_penColor.green(), m_penColor.blue(), m_penColor.alpha()};

                        // 顶点1: 内圆当前点
                        vertices[0].position = SDL_FPoint{innerPoints[i].x, innerPoints[i].y};
                        vertices[0].color = fcolor;
                        vertices[0].tex_coord = SDL_FPoint{0, 0};

                        // 顶点2: 外圆当前点
                        vertices[1].position = SDL_FPoint{outerPoints[i].x, outerPoints[i].y};
                        vertices[1].color = fcolor;
                        vertices[1].tex_coord = SDL_FPoint{0, 0};

                        // 顶点3: 外圆下一个点
                        vertices[2].position = SDL_FPoint{outerPoints[next].x, outerPoints[next].y};
                        vertices[2].color = fcolor;
                        vertices[2].tex_coord = SDL_FPoint{0, 0};

                        // 顶点4: 内圆下一个点
                        vertices[3].position = SDL_FPoint{innerPoints[next].x, innerPoints[next].y};
                        vertices[3].color = fcolor;
                        vertices[3].tex_coord = SDL_FPoint{0, 0};

                        // 使用两个三角形绘制四边形
                        int indices[6] = {0, 1, 2, 0, 2, 3};
                        SDL_RenderGeometry(m_renderer, nullptr, vertices, 4, indices, 6);
                    }
                }
            }
        }
    }

    void drawEllipse(const ::SPoint& center, float radiusX, float radiusY, bool filled = false) {
        if (!m_renderer || radiusX <= 0 || radiusY <= 0) return;

        if (filled) {
            // 填充椭圆
            auto points = Utils::generateEllipsePoints(center, radiusX, radiusY);
            if (points.empty()) return;

            SColor currentColor = m_fillColor;
            SDL_Color sdlColor = currentColor.toSDLColor();
            SDL_SetRenderDrawColor(m_renderer, sdlColor.r, sdlColor.g, sdlColor.b, sdlColor.a);

            // 使用三角形扇绘制填充
            if (points.size() >= 3) {
                for (size_t i = 1; i < points.size() - 1; ++i) {
                    // 使用三角形绘制填充
                    SDL_Vertex vertices[3];
                    vertices[0].position = SDL_FPoint{center.x, center.y};
                    vertices[0].color = SDL_FColor{currentColor.red(), currentColor.green(), currentColor.blue(), currentColor.alpha()};
                    vertices[0].tex_coord = SDL_FPoint{0, 0};

                    vertices[1].position = SDL_FPoint{points[i].x, points[i].y};
                    vertices[1].color = SDL_FColor{currentColor.red(), currentColor.green(), currentColor.blue(), currentColor.alpha()};
                    vertices[1].tex_coord = SDL_FPoint{0, 0};

                    vertices[2].position = SDL_FPoint{points[i + 1].x, points[i + 1].y};
                    vertices[2].color = SDL_FColor{currentColor.red(), currentColor.green(), currentColor.blue(), currentColor.alpha()};
                    vertices[2].tex_coord = SDL_FPoint{0, 0};
                    SDL_RenderGeometry(m_renderer, nullptr, vertices, 3, nullptr, 0);
                }
            }
        } else {
            // 绘制椭圆边框，支持线宽
            if (m_penWidth <= 1.0f) {
                // 线宽为1或更小，使用SDL的默认绘制
                auto points = Utils::generateEllipsePoints(center, radiusX, radiusY);
                if (points.empty()) return;

                SDL_Color color = m_penColor.toSDLColor();
                SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);

                for (size_t i = 0; i < points.size(); ++i) {
                    size_t next = (i + 1) % points.size();
                    SDL_RenderLine(m_renderer, points[i].x, points[i].y, points[next].x, points[next].y);
                }
            } else {
                // 绘制粗边框：绘制两个椭圆，然后填充中间区域
                float innerRadiusX = radiusX - m_penWidth / 2.0f;
                float outerRadiusX = radiusX + m_penWidth / 2.0f;
                float innerRadiusY = radiusY - m_penWidth / 2.0f;
                float outerRadiusY = radiusY + m_penWidth / 2.0f;

                if (innerRadiusX <= 0 || innerRadiusY <= 0) {
                    // 如果内椭圆为0或负数，直接绘制填充外椭圆
                    drawEllipse(center, outerRadiusX, outerRadiusY, true);
                } else {
                    // 生成内外椭圆的点
                    auto innerPoints = Utils::generateEllipsePoints(center, innerRadiusX, innerRadiusY, 72);
                    auto outerPoints = Utils::generateEllipsePoints(center, outerRadiusX, outerRadiusY, 72);

                    if (innerPoints.size() != outerPoints.size() || innerPoints.empty()) return;

                    // 使用三角形带绘制椭圆环
                    for (size_t i = 0; i < innerPoints.size(); ++i) {
                        size_t next = (i + 1) % innerPoints.size();

                        SDL_Vertex vertices[4];
                        SDL_FColor fcolor = {m_penColor.red(), m_penColor.green(), m_penColor.blue(), m_penColor.alpha()};

                        // 顶点1: 内椭圆当前点
                        vertices[0].position = SDL_FPoint{innerPoints[i].x, innerPoints[i].y};
                        vertices[0].color = fcolor;
                        vertices[0].tex_coord = SDL_FPoint{0, 0};

                        // 顶点2: 外椭圆当前点
                        vertices[1].position = SDL_FPoint{outerPoints[i].x, outerPoints[i].y};
                        vertices[1].color = fcolor;
                        vertices[1].tex_coord = SDL_FPoint{0, 0};

                        // 顶点3: 外椭圆下一个点
                        vertices[2].position = SDL_FPoint{outerPoints[next].x, outerPoints[next].y};
                        vertices[2].color = fcolor;
                        vertices[2].tex_coord = SDL_FPoint{0, 0};

                        // 顶点4: 内椭圆下一个点
                        vertices[3].position = SDL_FPoint{innerPoints[next].x, innerPoints[next].y};
                        vertices[3].color = fcolor;
                        vertices[3].tex_coord = SDL_FPoint{0, 0};

                        // 使用两个三角形绘制四边形
                        int indices[6] = {0, 1, 2, 0, 2, 3};
                        SDL_RenderGeometry(m_renderer, nullptr, vertices, 4, indices, 6);
                    }
                }
            }
        }
    }

    void drawArc(const ::SPoint& center, float radius, float startAngle, float endAngle, bool filled = false) {
        if (!m_renderer || radius <= 0) return;

        if (filled) {
            // 填充圆弧（扇形）
            auto points = Utils::generateArcPoints(center, radius, startAngle, endAngle);
            if (points.size() < 2) return;

            SColor currentColor = m_fillColor;
            SDL_Color sdlColor = currentColor.toSDLColor();
            SDL_SetRenderDrawColor(m_renderer, sdlColor.r, sdlColor.g, sdlColor.b, sdlColor.a);

            // 对于填充的圆弧，添加中心点形成扇形
            if (points.size() >= 2) {
                for (size_t i = 0; i < points.size() - 1; ++i) {
                    // 使用三角形绘制填充
                    SDL_Vertex vertices[3];
                    vertices[0].position = SDL_FPoint{center.x, center.y};
                    vertices[0].color = SDL_FColor{currentColor.red(), currentColor.green(), currentColor.blue(), currentColor.alpha()};
                    vertices[0].tex_coord = SDL_FPoint{0, 0};

                    vertices[1].position = SDL_FPoint{points[i].x, points[i].y};
                    vertices[1].color = SDL_FColor{currentColor.red(), currentColor.green(), currentColor.blue(), currentColor.alpha()};
                    vertices[1].tex_coord = SDL_FPoint{0, 0};

                    vertices[2].position = SDL_FPoint{points[i + 1].x, points[i + 1].y};
                    vertices[2].color = SDL_FColor{currentColor.red(), currentColor.green(), currentColor.blue(), currentColor.alpha()};
                    vertices[2].tex_coord = SDL_FPoint{0, 0};
                    SDL_RenderGeometry(m_renderer, nullptr, vertices, 3, nullptr, 0);
                }
            }
        } else {
            // 绘制圆弧轮廓，支持线宽
            if (m_penWidth <= 1.0f) {
                // 线宽为1或更小，使用SDL的默认绘制
                auto points = Utils::generateArcPoints(center, radius, startAngle, endAngle);
                if (points.size() < 2) return;

                SDL_Color color = m_penColor.toSDLColor();
                SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);

                for (size_t i = 0; i < points.size() - 1; ++i) {
                    SDL_RenderLine(m_renderer, points[i].x, points[i].y, points[i + 1].x, points[i + 1].y);
                }
            } else {
                // 绘制粗边框：使用两个圆弧，然后填充中间区域
                float innerRadius = radius - m_penWidth / 2.0f;
                float outerRadius = radius + m_penWidth / 2.0f;

                if (innerRadius <= 0) {
                    // 如果内径为0或负数，直接绘制填充圆弧
                    auto points = Utils::generateArcPoints(center, outerRadius, startAngle, endAngle);
                    if (points.size() < 2) return;

                    // 绘制填充扇形
                    for (size_t i = 0; i < points.size() - 1; ++i) {
                        SDL_Vertex vertices[3];
                        vertices[0].position = SDL_FPoint{center.x, center.y};
                        vertices[0].color = SDL_FColor{m_penColor.red(), m_penColor.green(), m_penColor.blue(), m_penColor.alpha()};
                        vertices[0].tex_coord = SDL_FPoint{0, 0};

                        vertices[1].position = SDL_FPoint{points[i].x, points[i].y};
                        vertices[1].color = SDL_FColor{m_penColor.red(), m_penColor.green(), m_penColor.blue(), m_penColor.alpha()};
                        vertices[1].tex_coord = SDL_FPoint{0, 0};

                        vertices[2].position = SDL_FPoint{points[i + 1].x, points[i + 1].y};
                        vertices[2].color = SDL_FColor{m_penColor.red(), m_penColor.green(), m_penColor.blue(), m_penColor.alpha()};
                        vertices[2].tex_coord = SDL_FPoint{0, 0};
                        SDL_RenderGeometry(m_renderer, nullptr, vertices, 3, nullptr, 0);
                    }
                } else {
                    // 生成内外圆弧的点
                    auto innerPoints = Utils::generateArcPoints(center, innerRadius, startAngle, endAngle, 72);
                    auto outerPoints = Utils::generateArcPoints(center, outerRadius, startAngle, endAngle, 72);

                    if (innerPoints.size() != outerPoints.size() || innerPoints.size() < 2) return;

                    // 使用三角形带绘制圆弧环
                    for (size_t i = 0; i < innerPoints.size() - 1; ++i) {
                        SDL_Vertex vertices[4];
                        SDL_FColor fcolor = {m_penColor.red(), m_penColor.green(), m_penColor.blue(), m_penColor.alpha()};

                        // 顶点1: 内圆弧当前点
                        vertices[0].position = SDL_FPoint{innerPoints[i].x, innerPoints[i].y};
                        vertices[0].color = fcolor;
                        vertices[0].tex_coord = SDL_FPoint{0, 0};

                        // 顶点2: 外圆弧当前点
                        vertices[1].position = SDL_FPoint{outerPoints[i].x, outerPoints[i].y};
                        vertices[1].color = fcolor;
                        vertices[1].tex_coord = SDL_FPoint{0, 0};

                        // 顶点3: 外圆弧下一个点
                        vertices[2].position = SDL_FPoint{outerPoints[i + 1].x, outerPoints[i + 1].y};
                        vertices[2].color = fcolor;
                        vertices[2].tex_coord = SDL_FPoint{0, 0};

                        // 顶点4: 内圆弧下一个点
                        vertices[3].position = SDL_FPoint{innerPoints[i + 1].x, innerPoints[i + 1].y};
                        vertices[3].color = fcolor;
                        vertices[3].tex_coord = SDL_FPoint{0, 0};

                        // 使用两个三角形绘制四边形
                        int indices[6] = {0, 1, 2, 0, 2, 3};
                        SDL_RenderGeometry(m_renderer, nullptr, vertices, 4, indices, 6);
                    }
                }
            }
        }
    }

    // 多边形绘制
    void drawPolygon(const std::vector<::SPoint>& points, bool filled = false,
                    bool debugCorner = false, const SColor& debugColor = SColor::Green()) {
#ifdef GRAPH_TOOL_DEBUG
        SDL_Log("GraphTool::drawPolygon: points.size=%d, penWidth=%f, cornerStyle=%d", points.size(), m_penWidth, static_cast<int>(m_cornerStyle));
#endif
        if (!m_renderer || points.size() < 3) return;

        if (filled) {
            // 填充多边形
            SColor currentColor = m_fillColor;
            SDL_Color sdlColor = currentColor.toSDLColor();
            SDL_SetRenderDrawColor(m_renderer, sdlColor.r, sdlColor.g, sdlColor.b, sdlColor.a);

            // 使用三角形扇绘制填充
            if (points.size() >= 3) {
                for (size_t i = 1; i < points.size() - 1; ++i) {
                    // 使用三角形绘制填充
                    SDL_Vertex vertices[3];
                    vertices[0].position = SDL_FPoint{points[0].x, points[0].y};
                    vertices[0].color = SDL_FColor{currentColor.red(), currentColor.green(), currentColor.blue(), currentColor.alpha()};
                    vertices[0].tex_coord = SDL_FPoint{0, 0};

                    vertices[1].position = SDL_FPoint{points[i].x, points[i].y};
                    vertices[1].color = SDL_FColor{currentColor.red(), currentColor.green(), currentColor.blue(), currentColor.alpha()};
                    vertices[1].tex_coord = SDL_FPoint{0, 0};

                    vertices[2].position = SDL_FPoint{points[i + 1].x, points[i + 1].y};
                    vertices[2].color = SDL_FColor{currentColor.red(), currentColor.green(), currentColor.blue(), currentColor.alpha()};
                    vertices[2].tex_coord = SDL_FPoint{0, 0};
                    SDL_RenderGeometry(m_renderer, nullptr, vertices, 3, nullptr, 0);
                }
            }
        } else {
            // 绘制多边形边框，支持线宽
            if (m_penWidth <= 1.0f) {
                // 线宽为1或更小，使用SDL的默认绘制
                SDL_Color color = m_penColor.toSDLColor();
                SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);

                for (size_t i = 0; i < points.size(); ++i) {
                    size_t next = (i + 1) % points.size();
                    SDL_RenderLine(m_renderer, points[i].x, points[i].y, points[next].x, points[next].y);
                }
            } else {
                // 绘制粗边框：绘制每条边，并处理所有拐角连接
                for (size_t i = 0; i < points.size(); ++i) {
                    size_t next = (i + 1) % points.size();
                    // 绘制当前边
                    // drawLine(points[i].x, points[i].y, points[next].x, points[next].y);
                    auto rectPoints = Utils::generateLineRectPoints(points[i], points[next], m_penWidth);
                    drawLine(rectPoints);
#ifdef GRAPH_TOOL_DEBUG
                    SDL_Log("GraphTool::drawPolygon(%d): rectPoints from (%f, %f) to (%f, %f), from (%f, %f) to (%f, %f)", static_cast<int>(i),
                             rectPoints.startLeft.x, rectPoints.startLeft.y, rectPoints.endLeft.x, rectPoints.endLeft.y,
                             rectPoints.startRight.x, rectPoints.startRight.y, rectPoints.endRight.x, rectPoints.endRight.y);
#endif
                             // 取前一段边
                    size_t prev = (i == 0) ? points.size() - 1 : i - 1;
                    auto prevRectPoints = Utils::generateLineRectPoints(points[prev], points[i], m_penWidth);
#ifdef GRAPH_TOOL_DEBUG
                    SDL_Log("GraphTool::drawPolygon(%d): prevRectPoints from (%f, %f) to (%f, %f), from (%f, %f) to (%f, %f)", static_cast<int>(i),
                             prevRectPoints.startLeft.x, prevRectPoints.startLeft.y, prevRectPoints.endLeft.x, prevRectPoints.endLeft.y,
                             prevRectPoints.startRight.x, prevRectPoints.startRight.y, prevRectPoints.endRight.x, prevRectPoints.endRight.y);
#endif
                    // 此时rectPoints和prevRectPoints分别是当前边和前一边的矩形四个顶点，相当于已知四条偏移线
                    // 现在获取偏移线的交点
                    SPoint intersectPoint1 = Utils::lineIntersection(rectPoints.startLeft, rectPoints.endLeft,
                                                                    prevRectPoints.startLeft, prevRectPoints.endLeft);
                    SPoint intersectPoint2 = Utils::lineIntersection(rectPoints.startLeft, rectPoints.endLeft,
                                                                    prevRectPoints.startRight, prevRectPoints.endRight);
                    SPoint intersectPoint3 = Utils::lineIntersection(rectPoints.startRight, rectPoints.endRight,
                                                                    prevRectPoints.startLeft, prevRectPoints.endLeft);
                    SPoint intersectPoint4 = Utils::lineIntersection(rectPoints.startRight, rectPoints.endRight,
                                                                    prevRectPoints.startRight, prevRectPoints.endRight);
#ifdef GRAPH_TOOL_DEBUG
                    SDL_Log("GraphTool::drawPolygon(%d): intersectPoints: (%f, %f), (%f, %f), (%f, %f), (%f, %f)", static_cast<int>(i),
                             intersectPoint1.x, intersectPoint1.y, intersectPoint2.x, intersectPoint2.y,
                             intersectPoint3.x, intersectPoint3.y, intersectPoint4.x, intersectPoint4.y);
#endif
                    // 判断哪个交点是在两个矩形外的，即为“外交点”
                    SRotatedRect currentRect = SRotatedRect(rectPoints.startLeft, rectPoints.startRight,
                                                            rectPoints.endRight, rectPoints.endLeft);
                    SRotatedRect prevRect = SRotatedRect(prevRectPoints.startLeft, prevRectPoints.startRight,
                                                            prevRectPoints.endRight, prevRectPoints.endLeft);
                    std::vector<SPoint> drawPolygon = {rectPoints.startLeft, rectPoints.startRight,
                                                    rectPoints.endRight, rectPoints.endLeft,
                                                    prevRectPoints.startLeft, prevRectPoints.startRight,
                                                    prevRectPoints.endRight, prevRectPoints.endLeft};
                    std::vector<SPoint> candidatePoints = {intersectPoint1, intersectPoint2, intersectPoint3, intersectPoint4};
                    SPoint outerPoint;
                    bool hasOuterPoint = false;
                    for (const auto& point : candidatePoints) {
                        // if (!Utils::pointInRect(point, SRect(rectPoints.startLeft, rectPoints.endRight))
                        //     && !Utils::pointInRect(point, SRect(prevRectPoints.startLeft, prevRectPoints.endRight))) {
                        //     hasOuterPoint = true;
                        //     outerPoint = point;
                        //     break; // 找到一个外交点即可
                        // }
                        SDL_Log("GraphTool::drawPolygon(%d): checking outerPoint candidate (%f, %f)", static_cast<int>(i), point.x, point.y);
                        // SDL_Log("    currentRect contains: %s, prevRect contains: %s",
                        //         currentRect.contains(point) ? "True" : "False",
                        //         prevRect.contains(point) ? "True" : "False");
                        // SDL_Log("    currentRect.rotation=%f", currentRect.rotation);
                        // SDL_Log("    prevRect.rotation=%f", prevRect.rotation);
                        SDL_Log("    isPointOnPolygonSide()=%s",
                                isPointOnPolygonSide(drawPolygon, point, false) ? "True" : "False");
                        // if (!currentRect.contains(point) && !prevRect.contains(point)) {
                        if (isPointOnPolygonSide(drawPolygon, point, false)){
                            hasOuterPoint = true;
                            outerPoint = point;
                            break; // 找到一个外交点即可
                        }
                    }

                    if (hasOuterPoint == false) {
                        SDL_Log("GraphTool::drawPolygon: not found outerPoint, maybe parallel lines or coinciding lines.");
                        throw("GraphTool::drawPolygon: not found outerPoint, maybe parallel lines or coinciding lines.");
                        return;
                    }

                    // 取外角点
                    SPoint cornerPoint1;
                    // if (!Utils::pointInRect(rectPoints.startLeft, SRect(prevRectPoints.startLeft, prevRectPoints.endRight))) {
                    //     cornerPoint1 = rectPoints.startLeft;
                    // } else if (!Utils::pointInRect(rectPoints.startRight, SRect(prevRectPoints.startLeft, prevRectPoints.endRight))) {
                    //     cornerPoint1 = rectPoints.startRight;
                    // } else {
                    //     throw("GraphTool::drawPolygon: not found cornerPoint1");
                    // }
                    if (!prevRect.contains(rectPoints.startLeft)) {
                        cornerPoint1 = rectPoints.startLeft;
                    } else if (!prevRect.contains(rectPoints.startRight)) {
                        cornerPoint1 = rectPoints.startRight;
                    } else {
                        SDL_Log("GraphTool::drawPolygon: not found cornerPoint1");
                        throw("GraphTool::drawPolygon: not found cornerPoint1");
                        return;
                    }

                    SPoint cornerPoint2;
                    // if (!Utils::pointInRect(prevRectPoints.endLeft, SRect(rectPoints.startLeft, rectPoints.endRight))) {
                    //     cornerPoint2 = prevRectPoints.endLeft;
                    // } else if (!Utils::pointInRect(prevRectPoints.endRight, SRect(rectPoints.startLeft, rectPoints.endRight))) {
                    //     cornerPoint2 = prevRectPoints.endRight;
                    // } else {
                    //     throw("GraphTool::drawPolygon: not found cornerPoint2");
                    // }
                    if (!currentRect.contains(prevRectPoints.endLeft)) {
                        cornerPoint2 = prevRectPoints.endLeft;
                    } else if (!currentRect.contains(prevRectPoints.endRight)) {
                        cornerPoint2 = prevRectPoints.endRight;
                    } else {
                        SDL_Log("GraphTool::drawPolygon: not found cornerPoint2");
                        throw("GraphTool::drawPolygon: not found cornerPoint2");
                    }


                    if (m_cornerStyle == CornerStyle::Round) {
                        // // 圆弧角：绘制扇形填充拐角缺口
                        // // 计算两条线之间的夹角
                        // float dot = ux1 * ux2 + uy1 * uy2;
                        // // 确保dot在[-1, 1]范围内
                        // if (dot > 1.0f) dot = 1.0f;
                        // if (dot < -1.0f) dot = -1.0f;
                        // float angle = std::acos(dot);

                        // // 确定圆弧方向（内角还是外角）
                        // float cross = ux1 * uy2 - uy1 * ux2;

                        // // 计算圆弧的起始角度和结束角度
                        // float angle1 = std::atan2(ny1, nx1);
                        // float angle2 = std::atan2(ny2, nx2);

                        // // 调整角度以确保正确的圆弧方向
                        // if (cross < 0) {
                        //     // 外角，需要交换角度
                        //     std::swap(angle1, angle2);
                        //     if (angle2 < angle1) angle2 += 2.0f * M_PI;
                        // }
                        SDL_Log("GraphTool::drawPolygon: CornerStyle::Round");
                        // // 生成圆弧点 - 使用更多的分段确保平滑
                        // int segments = (std::max)(8, static_cast<int>(angle * 360.0f / M_PI));

                        // // 直接绘制填充扇形
                        // for (int j = 0; j < segments; ++j) {
                        //     float t1 = static_cast<float>(j) / segments;
                        //     float t2 = static_cast<float>(j + 1) / segments;

                        //     float angle1_current = angle1 + (angle2 - angle1) * t1;
                        //     float angle2_current = angle1 + (angle2 - angle1) * t2;

                        //     // 计算圆弧点
                        //     float arcX1 = points[i].x + halfWidth * std::cos(angle1_current);
                        //     float arcY1 = points[i].y + halfWidth * std::sin(angle1_current);
                        //     float arcX2 = points[i].x + halfWidth * std::cos(angle2_current);
                        //     float arcY2 = points[i].y + halfWidth * std::sin(angle2_current);

                        //     // 绘制三角形（顶点-圆弧点1-圆弧点2）
                        //     SDL_Vertex vertices[3];
                        //     SDL_FColor fcolor = {debugColor.red(), debugColor.green(), debugColor.blue(), debugColor.alpha()};

                        //     // 顶点1: 顶点
                        //     vertices[0].position = SDL_FPoint{points[i].x, points[i].y};
                        //     vertices[0].color = fcolor;
                        //     vertices[0].tex_coord = SDL_FPoint{0, 0};

                        //     // 顶点2: 圆弧点1
                        //     vertices[1].position = SDL_FPoint{arcX1, arcY1};
                        //     vertices[1].color = fcolor;
                        //     vertices[1].tex_coord = SDL_FPoint{0, 0};

                        //     // 顶点3: 圆弧点2
                        //     vertices[2].position = SDL_FPoint{arcX2, arcY2};
                        //     vertices[2].color = fcolor;
                        //     vertices[2].tex_coord = SDL_FPoint{0, 0};

                        //     SDL_RenderGeometry(m_renderer, nullptr, vertices, 3, nullptr, 0);
                        // }
                    } else {
                        // 硬角：直接绘制四边形连接两个外角点

                        // 绘制四边形填充拐角缺口
                        // 使用两个三角形：顶点-外角点1-交点和顶点-交点-外角点2
                        SDL_Vertex vertices[4];
                        SDL_FColor fcolor;
                        if (debugCorner) {
                            fcolor = {debugColor.red(), debugColor.green(), debugColor.blue(), debugColor.alpha()};
                        } else {
                            fcolor = getPenColor().toSDLFColor();
                        }

#ifdef GRAPH_TOOL_DEBUG
                        SDL_Log("GraphTool::drawPolygon(%d): position:{%f, %f}, cornerPoint1:{%f, %f}, outerPoint:{%f, %f}, cornerPoint2:{%f, %f}",i,
                                 points[i].x, points[i].y,
                                 cornerPoint1.x, cornerPoint1.y,
                                 outerPoint.x, outerPoint.y,
                                 cornerPoint2.x, cornerPoint2.y);
#endif
                        // 顶点1: 交点
                        vertices[0].position = points[i].toSDLFPoint();
                        vertices[0].color = fcolor;
                        vertices[0].tex_coord = SDL_FPoint{0, 0};

                        // 顶点2: 外角点1
                        vertices[1].position = cornerPoint1.toSDLFPoint();
                        vertices[1].color = fcolor;
                        vertices[1].tex_coord = SDL_FPoint{0, 0};

                        // 顶点3: 外交点
                        vertices[2].position = outerPoint.toSDLFPoint();
                        vertices[2].color = fcolor;
                        vertices[2].tex_coord = SDL_FPoint{0, 0};

                        // 顶点4: 外角点2
                        vertices[3].position = cornerPoint2.toSDLFPoint();
                        vertices[3].color = fcolor;
                        vertices[3].tex_coord = SDL_FPoint{0, 0};

                        // 使用两个三角形绘制四边形
                        int indices[6] = {0, 1, 2, 0, 2, 3};
                        SDL_RenderGeometry(m_renderer, nullptr, vertices, 4, indices, 6);
                    }
                }
            }
        }
    }

    void drawPolyline(const std::vector<::SPoint>& points, bool debugCorner = false, const SColor& debugColor = SColor::Green()) {
        if (!m_renderer || points.size() < 2) return;

        // 绘制折线，支持线宽
        if (m_penWidth <= 1.0f) {
            // 线宽为1或更小，使用简单绘制
            SDL_Color color = m_penColor.toSDLColor();
            SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);

            for (size_t i = 0; i < points.size() - 1; ++i) {
                SDL_RenderLine(m_renderer, points[i].x, points[i].y, points[i + 1].x, points[i + 1].y);
            }
        } else {
            // 绘制粗折线，需要处理拐角连接
            for (size_t i = 0; i < points.size() - 1; ++i) {
                // 绘制当前线段
                drawLine(points[i].x, points[i].y, points[i + 1].x, points[i + 1].y);

                // 如果不是第一条线段，绘制拐角连接
                if (i > 0) {
                    // 计算前一线段的方向
                    float dx1 = points[i].x - points[i - 1].x;
                    float dy1 = points[i].y - points[i - 1].y;
                    float len1 = std::sqrt(dx1 * dx1 + dy1 * dy1);

                    // 计算当前线段的方向
                    float dx2 = points[i + 1].x - points[i].x;
                    float dy2 = points[i + 1].y - points[i].y;
                    float len2 = std::sqrt(dx2 * dx2 + dy2 * dy2);

                    if (len1 > 0.001f && len2 > 0.001f) {
                        // 计算单位方向向量
                        float ux1 = dx1 / len1;
                        float uy1 = dy1 / len1;
                        float ux2 = dx2 / len2;
                        float uy2 = dy2 / len2;

                        // 计算拐角的外角点
                        float halfWidth = m_penWidth / 2.0f;

                        // 计算两条线的外角交点
                        // 方法：计算两条线的外偏移线的交点

                        // 计算第一条线的外偏移点
                        float nx1 = -uy1;
                        float ny1 = ux1;
                        float offsetX1 = points[i].x + halfWidth * nx1;
                        float offsetY1 = points[i].y + halfWidth * ny1;

                        // 计算第二条线的外偏移点
                        float nx2 = -uy2;
                        float ny2 = ux2;
                        float offsetX2 = points[i].x + halfWidth * nx2;
                        float offsetY2 = points[i].y + halfWidth * ny2;

                        // 计算两条偏移线的交点
                        float a11 = ux1;
                        float a12 = -ux2;
                        float a21 = uy1;
                        float a22 = -uy2;

                        float det = a11 * a22 - a12 * a21;
                        float intersectX, intersectY;

                        if (std::abs(det) > 0.001f) {
                            float b1 = offsetX2 - offsetX1;
                            float b2 = offsetY2 - offsetY1;

                            float t1 = (b1 * a22 - a12 * b2) / det;
                            float t2 = (a11 * b2 - b1 * a21) / det;

                            intersectX = offsetX1 + t1 * ux1;
                            intersectY = offsetY1 + t1 * uy1;

                            // 验证交点
                            float intersectX2 = offsetX2 + t2 * ux2;
                            float intersectY2 = offsetY2 + t2 * uy2;

                            if (std::abs(intersectX - intersectX2) > 0.1f || std::abs(intersectY - intersectY2) > 0.1f) {
                                intersectX = (intersectX + intersectX2) / 2.0f;
                                intersectY = (intersectY + intersectY2) / 2.0f;
                            }
                        } else {
                            // 两条线平行或接近平行，使用简单的中点
                            intersectX = (offsetX1 + offsetX2) / 2.0f;
                            intersectY = (offsetY1 + offsetY2) / 2.0f;
                        }

                        // 根据拐角样式绘制拐角连接
                        if (debugCorner) {
                            // 调试模式：使用不同颜色绘制拐角连接
                            SColor oldPenColor = m_penColor;
                            SColor oldFillColor = m_fillColor;

                            // 保存当前颜色
                            setPenColor(debugColor);
                            setFillColor(debugColor);

                            if (m_cornerStyle == CornerStyle::Round) {
                                // 圆弧角：绘制扇形填充拐角缺口
                                // 计算两条线之间的夹角
                                float dot = ux1 * ux2 + uy1 * uy2;
                                // 确保dot在[-1, 1]范围内
                                if (dot > 1.0f) dot = 1.0f;
                                if (dot < -1.0f) dot = -1.0f;
                                float angle = std::acos(dot);

                                // 确定圆弧方向（内角还是外角）
                                float cross = ux1 * uy2 - uy1 * ux2;

                                // 计算圆弧的起始角度和结束角度
                                float angle1 = std::atan2(ny1, nx1);
                                float angle2 = std::atan2(ny2, nx2);

                                // 调整角度以确保正确的圆弧方向
                                if (cross < 0) {
                                    // 外角，需要交换角度
                                    std::swap(angle1, angle2);
                                    if (angle2 < angle1) angle2 += 2.0f * M_PI;
                                }

                                // 生成圆弧点 - 使用更多的分段确保平滑
                                int segments = (std::max)(8, static_cast<int>(angle * 360.0f / M_PI));

                                // 直接绘制填充扇形
                                for (int j = 0; j < segments; ++j) {
                                    float t1 = static_cast<float>(j) / segments;
                                    float t2 = static_cast<float>(j + 1) / segments;

                                    float angle1_current = angle1 + (angle2 - angle1) * t1;
                                    float angle2_current = angle1 + (angle2 - angle1) * t2;

                                    // 计算圆弧点
                                    float arcX1 = points[i].x + halfWidth * std::cos(angle1_current);
                                    float arcY1 = points[i].y + halfWidth * std::sin(angle1_current);
                                    float arcX2 = points[i].x + halfWidth * std::cos(angle2_current);
                                    float arcY2 = points[i].y + halfWidth * std::sin(angle2_current);

                                    // 绘制三角形（顶点-圆弧点1-圆弧点2）
                                    SDL_Vertex vertices[3];
                                    SDL_FColor fcolor = {debugColor.red(), debugColor.green(), debugColor.blue(), debugColor.alpha()};

                                    // 顶点1: 顶点
                                    vertices[0].position = SDL_FPoint{points[i].x, points[i].y};
                                    vertices[0].color = fcolor;
                                    vertices[0].tex_coord = SDL_FPoint{0, 0};

                                    // 顶点2: 圆弧点1
                                    vertices[1].position = SDL_FPoint{arcX1, arcY1};
                                    vertices[1].color = fcolor;
                                    vertices[1].tex_coord = SDL_FPoint{0, 0};

                                    // 顶点3: 圆弧点2
                                    vertices[2].position = SDL_FPoint{arcX2, arcY2};
                                    vertices[2].color = fcolor;
                                    vertices[2].tex_coord = SDL_FPoint{0, 0};

                                    SDL_RenderGeometry(m_renderer, nullptr, vertices, 3, nullptr, 0);
                                }
                            } else {
                                // 硬角：直接绘制四边形连接两个外角点
                                // 绘制四边形填充拐角缺口
                                // 使用两个三角形：顶点-外角点1-交点和顶点-交点-外角点2
                                SDL_Vertex vertices[4];
                                SDL_FColor fcolor = {debugColor.red(), debugColor.green(), debugColor.blue(), debugColor.alpha()};

                                // 顶点1: 顶点
                                vertices[0].position = SDL_FPoint{points[i].x, points[i].y};
                                vertices[0].color = fcolor;
                                vertices[0].tex_coord = SDL_FPoint{0, 0};

                                // 顶点2: 第一条线的外角点
                                vertices[1].position = SDL_FPoint{offsetX1, offsetY1};
                                vertices[1].color = fcolor;
                                vertices[1].tex_coord = SDL_FPoint{0, 0};

                                // 顶点3: 交点
                                vertices[2].position = SDL_FPoint{intersectX, intersectY};
                                vertices[2].color = fcolor;
                                vertices[2].tex_coord = SDL_FPoint{0, 0};

                                // 顶点4: 第二条线的外角点
                                vertices[3].position = SDL_FPoint{offsetX2, offsetY2};
                                vertices[3].color = fcolor;
                                vertices[3].tex_coord = SDL_FPoint{0, 0};

                                // 使用两个三角形绘制四边形
                                int indices[6] = {0, 1, 2, 0, 2, 3};
                                SDL_RenderGeometry(m_renderer, nullptr, vertices, 4, indices, 6);
                            }

                            // 恢复颜色
                            setPenColor(oldPenColor);
                            setFillColor(oldFillColor);
                        } else {
                            // 正常模式：使用画笔颜色绘制拐角连接
                            if (m_cornerStyle == CornerStyle::Round) {
                                // 圆弧角：绘制扇形填充拐角缺口
                                // 计算两条线之间的夹角
                                float dot = ux1 * ux2 + uy1 * uy2;
                                // 确保dot在[-1, 1]范围内
                                if (dot > 1.0f) dot = 1.0f;
                                if (dot < -1.0f) dot = -1.0f;
                                float angle = std::acos(dot);

                                // 确定圆弧方向（内角还是外角）
                                float cross = ux1 * uy2 - uy1 * ux2;

                                // 计算圆弧的起始角度和结束角度
                                float angle1 = std::atan2(ny1, nx1);
                                float angle2 = std::atan2(ny2, nx2);

                                // 调整角度以确保正确的圆弧方向
                                if (cross < 0) {
                                    // 外角，需要交换角度
                                    std::swap(angle1, angle2);
                                    if (angle2 < angle1) angle2 += 2.0f * M_PI;
                                }

                                // 生成圆弧点 - 使用更多的分段确保平滑
                                int segments = (std::max)(8, static_cast<int>(angle * 360.0f / M_PI));

                                // 直接绘制填充扇形
                                for (int j = 0; j < segments; ++j) {
                                    float t1 = static_cast<float>(j) / segments;
                                    float t2 = static_cast<float>(j + 1) / segments;

                                    float angle1_current = angle1 + (angle2 - angle1) * t1;
                                    float angle2_current = angle1 + (angle2 - angle1) * t2;

                                    // 计算圆弧点
                                    float arcX1 = points[i].x + halfWidth * std::cos(angle1_current);
                                    float arcY1 = points[i].y + halfWidth * std::sin(angle1_current);
                                    float arcX2 = points[i].x + halfWidth * std::cos(angle2_current);
                                    float arcY2 = points[i].y + halfWidth * std::sin(angle2_current);

                                    // 绘制三角形（顶点-圆弧点1-圆弧点2）
                                    SDL_Vertex vertices[3];
                                    SDL_FColor fcolor = {m_penColor.red(), m_penColor.green(), m_penColor.blue(), m_penColor.alpha()};

                                    // 顶点1: 顶点
                                    vertices[0].position = SDL_FPoint{points[i].x, points[i].y};
                                    vertices[0].color = fcolor;
                                    vertices[0].tex_coord = SDL_FPoint{0, 0};

                                    // 顶点2: 圆弧点1
                                    vertices[1].position = SDL_FPoint{arcX1, arcY1};
                                    vertices[1].color = fcolor;
                                    vertices[1].tex_coord = SDL_FPoint{0, 0};

                                    // 顶点3: 圆弧点2
                                    vertices[2].position = SDL_FPoint{arcX2, arcY2};
                                    vertices[2].color = fcolor;
                                    vertices[2].tex_coord = SDL_FPoint{0, 0};

                                    SDL_RenderGeometry(m_renderer, nullptr, vertices, 3, nullptr, 0);
                                }
                            } else {
                                // 硬角：严格按照用户算法实现
                                // 算法：四边形[B, A1, O, A2]，其中：
                                // B: 折线顶点 (points[i])
                                // A1: 第一条线的外角点 (offsetX1, offsetY1)
                                // A2: 第二条线的外角点 (offsetX2, offsetY2)
                                // O: 外交点（两条偏移线的外交点）

                                // 计算外交点O
                                // 我们需要计算两条偏移线的交点，并确保它是外交点
                                // 方法：计算两条偏移线的交点，然后判断哪个是外交点

                                // 计算两条偏移线的交点
                                float a11 = ux1;
                                float a12 = -ux2;
                                float a21 = uy1;
                                float a22 = -uy2;

                                float det = a11 * a22 - a12 * a21;
                                float intersectX, intersectY;

                                if (std::abs(det) > 0.001f) {
                                    float b1 = offsetX2 - offsetX1;
                                    float b2 = offsetY2 - offsetY1;

                                    float t1 = (b1 * a22 - a12 * b2) / det;
                                    float t2 = (a11 * b2 - b1 * a21) / det;

                                    // 计算交点
                                    intersectX = offsetX1 + t1 * ux1;
                                    intersectY = offsetY1 + t1 * uy1;

                                    // 验证交点
                                    float intersectX2 = offsetX2 + t2 * ux2;
                                    float intersectY2 = offsetY2 + t2 * uy2;

                                    if (std::abs(intersectX - intersectX2) > 0.1f || std::abs(intersectY - intersectY2) > 0.1f) {
                                        intersectX = (intersectX + intersectX2) / 2.0f;
                                        intersectY = (intersectY + intersectY2) / 2.0f;
                                    }

                                    // 关键：判断交点是否是外交点
                                    // 外交点应该位于两条线段的"外侧"
                                    // 方法：检查交点是否在两条线段形成的夹角的外部

                                    // 计算从顶点到交点的向量
                                    float vx = intersectX - points[i].x;
                                    float vy = intersectY - points[i].y;

                                    // 计算两条边的方向向量
                                    float edge1x = -ux1;  // 指向顶点的反方向
                                    float edge1y = -uy1;
                                    float edge2x = ux2;   // 指向下一个点
                                    float edge2y = uy2;

                                    // 计算叉积来判断方向
                                    float cross1 = edge1x * vy - edge1y * vx;
                                    float cross2 = edge2x * vy - edge2y * vx;

                                    // 如果交点在内角（夹角内部），则两个叉积应该同号
                                    // 如果交点在外角（夹角外部），则两个叉积应该异号
                                    bool isInsideAngle = (cross1 * cross2 > 0);

                                    // 如果交点在夹角内部（内交点），我们需要计算外交点
                                    if (isInsideAngle) {
                                        // 计算外交点：使用负的t值
                                        t1 = -t1;
                                        t2 = -t2;

                                        intersectX = offsetX1 + t1 * ux1;
                                        intersectY = offsetY1 + t1 * uy1;

                                        // 验证外交点
                                        intersectX2 = offsetX2 + t2 * ux2;
                                        intersectY2 = offsetY2 + t2 * uy2;

                                        if (std::abs(intersectX - intersectX2) > 0.1f || std::abs(intersectY - intersectY2) > 0.1f) {
                                            intersectX = (intersectX + intersectX2) / 2.0f;
                                            intersectY = (intersectY + intersectY2) / 2.0f;
                                        }
                                    }
                                } else {
                                    // 两条线平行或接近平行，使用简单的中点
                                    intersectX = (offsetX1 + offsetX2) / 2.0f;
                                    intersectY = (offsetY1 + offsetY2) / 2.0f;
                                }

                                // 绘制四边形填充拐角缺口 [B, A1, O, A2]
                                SDL_Vertex vertices[4];
                                SDL_FColor fcolor = {m_penColor.red(), m_penColor.green(), m_penColor.blue(), m_penColor.alpha()};

                                // 顶点B: 折线顶点
                                vertices[0].position = SDL_FPoint{points[i].x, points[i].y};
                                vertices[0].color = fcolor;
                                vertices[0].tex_coord = SDL_FPoint{0, 0};

                                // 顶点A1: 第一条线的外角点
                                vertices[1].position = SDL_FPoint{offsetX1, offsetY1};
                                vertices[1].color = fcolor;
                                vertices[1].tex_coord = SDL_FPoint{0, 0};

                                // 顶点O: 外交点
                                vertices[2].position = SDL_FPoint{intersectX, intersectY};
                                vertices[2].color = fcolor;
                                vertices[2].tex_coord = SDL_FPoint{0, 0};

                                // 顶点A2: 第二条线的外角点
                                vertices[3].position = SDL_FPoint{offsetX2, offsetY2};
                                vertices[3].color = fcolor;
                                vertices[3].tex_coord = SDL_FPoint{0, 0};

                                // 使用两个三角形绘制四边形 [B, A1, O] 和 [B, O, A2]
                                int indices[6] = {0, 1, 2, 0, 2, 3};
                                SDL_RenderGeometry(m_renderer, nullptr, vertices, 4, indices, 6);
                            }
                        }
                    }
                }
            }

            // 绘制线帽（起点和终点）
            if (points.size() >= 2) {
                // 保存当前填充颜色
                SColor oldFillColor = m_fillColor;

                // 使用画笔颜色作为填充颜色绘制圆帽
                setFillColor(m_penColor);

                // 绘制起点圆帽 - 确保使用完整的圆
                drawCircle(points[0], m_penWidth / 2.0f, true);

                // 绘制终点圆帽 - 确保使用完整的圆
                drawCircle(points[points.size() - 1], m_penWidth / 2.0f, true);

                // 恢复填充颜色
                setFillColor(oldFillColor);
            }
        }
    }

    // 文本绘制（简化版本，实际需要SDL_ttf支持）
    void drawText(const ::SPoint& position, const std::string& text) {
        // 这里需要SDL_ttf库来实现文本渲染
        // 暂时留空，后续可以集成SDL_ttf
        (void)position;
        (void)text;
    }

    void drawText(const ::SRect& bounds, const std::string& text, TextAlignment alignment = TextAlignment::Left) {
        // 这里需要SDL_ttf库来实现文本渲染
        // 暂时留空，后续可以集成SDL_ttf
        (void)bounds;
        (void)text;
        (void)alignment;
    }

    // 图像绘制
    void drawImage(const ::SPoint& position, SDL_Texture* texture) {
        if (!m_renderer || !texture) return;

        float width, height;
        SDL_GetTextureSize(texture, &width, &height);
        SDL_FRect destRect = {position.x, position.y, width, height};
        SDL_RenderTexture(m_renderer, texture, nullptr, &destRect);
    }

    void drawImage(const ::SRect& destRect, SDL_Texture* texture) {
        if (!m_renderer || !texture) return;
        SDL_FRect sdlDestRect = {destRect.left, destRect.top, destRect.width, destRect.height};
        SDL_RenderTexture(m_renderer, texture, nullptr, &sdlDestRect);
    }

    void drawImage(const ::SRect& destRect, SDL_Texture* texture, const ::SRect& srcRect) {
        if (!m_renderer || !texture) return;
        SDL_FRect sdlSrcRect = {srcRect.left, srcRect.top, srcRect.width, srcRect.height};
        SDL_FRect sdlDestRect = {destRect.left, destRect.top, destRect.width, destRect.height};
        SDL_RenderTexture(m_renderer, texture, &sdlSrcRect, &sdlDestRect);
    }

    // 裁剪区域管理
    void pushClipRect(const ::SRect& rect) {
        if (!m_renderer) return;
        SDL_Rect sdlRect = {
            static_cast<int>(rect.left),
            static_cast<int>(rect.top),
            static_cast<int>(rect.width),
            static_cast<int>(rect.height)
        };
        SDL_SetRenderClipRect(m_renderer, &sdlRect);
        m_clipStack.push_back(rect);
    }

    void popClipRect() {
        if (!m_renderer || m_clipStack.empty()) return;
        m_clipStack.pop_back();
        if (m_clipStack.empty()) {
            SDL_SetRenderClipRect(m_renderer, nullptr);
        } else {
            const ::SRect& rect = m_clipStack.back();
            SDL_Rect sdlRect = {
                static_cast<int>(rect.left),
                static_cast<int>(rect.top),
                static_cast<int>(rect.width),
                static_cast<int>(rect.height)
            };
            SDL_SetRenderClipRect(m_renderer, &sdlRect);
        }
    }

    // 变换管理
    void pushTransform() {
        if (!m_renderer) return;
        SDL_Renderer* renderer = m_renderer;
        m_transformStack.push_back(renderer);
    }

    void popTransform() {
        if (!m_renderer || m_transformStack.empty()) return;
        m_transformStack.pop_back();
    }

    // 获取颜色
    SColor getPenColor() const { return m_penColor; }
    SColor getFillColor() const { return m_fillColor; }

    // 拐角样式设置
    void setCornerStyle(CornerStyle style) { m_cornerStyle = style; }
    CornerStyle getCornerStyle() const { return m_cornerStyle; }

    // 获取渲染器
    SDL_Renderer* getRenderer() const { return m_renderer; }

    // 变换操作
    void scale(float sx, float sy) {
        if (!m_renderer) return;
        // SDL3中可能没有直接的scale函数，暂时留空
        // 可以使用SDL_RenderSetScale或SDL_RenderSetLogicalSize
        // 这里暂时不实现，因为SDL3 API可能不同
    }

private:
    SDL_Renderer* m_renderer;
    SColor m_penColor;
    SColor m_fillColor;
    float m_penWidth;
    std::string m_fontName;
    float m_fontSize;
    CornerStyle m_cornerStyle;
    std::vector<::SRect> m_clipStack;
    std::vector<SDL_Renderer*> m_transformStack;
};

// ==================== 高级绘图功能 ====================
class AdvancedDrawing {
public:
    // 绘制渐变矩形
    static void drawGradientRect(DrawingContext& ctx, const ::SRect& rect,
                                const SColor& startColor, const SColor& endColor,
                                bool horizontal = true) {
        // 简化实现：使用多个矩形模拟渐变
        int steps = 20;
        float stepSize = horizontal ? rect.width / steps : rect.height / steps;

        for (int i = 0; i < steps; ++i) {
            float t = static_cast<float>(i) / steps;
            SColor color = Utils::interpolateColor(startColor, endColor, t);

            ::SRect segment;
            if (horizontal) {
                segment = ::SRect(rect.left + i * stepSize, rect.top,
                                 stepSize, rect.height);
            } else {
                segment = ::SRect(rect.left, rect.top + i * stepSize,
                                 rect.width, stepSize);
            }

            ctx.setFillColor(color);
            ctx.drawRect(segment, true);
        }
    }

    // 绘制阴影效果
    static void drawShadow(DrawingContext& ctx, const ::SRect& rect,
                          float shadowSize, const SColor& shadowColor,
                          float blurRadius = 5.0f) {
        // 简化实现：绘制多个逐渐变淡的矩形
        int layers = static_cast<int>(blurRadius);
        for (int i = layers; i > 0; --i) {
            float alpha = shadowColor.alpha() * (static_cast<float>(i) / layers);
            float offset = shadowSize * (static_cast<float>(i) / layers);

            SColor layerColor = shadowColor.withAlpha(alpha);
            ::SRect shadowRect = ::SRect(rect.left + offset, rect.top + offset, rect.width, rect.height);

            ctx.setFillColor(layerColor);
            ctx.drawRect(shadowRect, true);
        }
    }

    // 绘制发光效果
    static void drawGlow(DrawingContext& ctx, std::function<void()> drawFunc,
                        float glowRadius, const SColor& glowColor,
                        int layers = 5) {
        // 保存当前状态
        SColor oldPenColor = ctx.getPenColor();
        SColor oldFillColor = ctx.getFillColor();

        // 绘制多个逐渐变淡的轮廓
        for (int i = layers; i > 0; --i) {
            float currentRadius = glowRadius * (static_cast<float>(i) / layers);
            float alpha = glowColor.alpha() * (static_cast<float>(i) / layers);
            SColor currentColor = glowColor.withAlpha(alpha);

            // 应用缩放实现发光扩散
            ctx.pushTransform();
            // 应用缩放实现发光扩散
            ctx.scale(1.0f + currentRadius * 0.01f, 1.0f + currentRadius * 0.01f);

            // 设置发光颜色
            ctx.setPenColor(currentColor);
            ctx.setFillColor(currentColor);

            // 绘制发光效果
            drawFunc();

            // 恢复状态
            ctx.popTransform();
        }

        // 绘制原始内容
        drawFunc();

        // 恢复颜色
        ctx.setPenColor(oldPenColor);
        ctx.setFillColor(oldFillColor);
    }
};

}

#endif // GRAPHTOOL_H
