// ============================================================================
// RenderDevice.cpp -- 形状图元基类默认实现（design/Shape_Design.md §4.4）
// CPU 顶点生成 + 既有纯虚提交（drawTriangles/drawQuad/drawLine）；
// 三后端与 CallbackRenderDevice 零改动自动获得；后端可 override 原生加速。
// ============================================================================
#include "RenderDevice.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

constexpr float kPi = 3.14159265358979f;
constexpr int   kEllipseSegments = 32;   // 椭圆/圆扇形逼近段数
constexpr int   kCornerSegments = 8;     // 圆角矩形每角弧段数

using Vtx = RenderDevice::Vertex;

void submit(RenderDevice* dev, const std::vector<Vtx>& tris) {
    if (!tris.empty()) dev->drawTriangles(tris.data(), static_cast<int>(tris.size()));
}

// 椭圆周界点列（seg 个点，不含重复首点）
std::vector<SPoint> ellipsePts(float cx, float cy, float rx, float ry, int seg) {
    std::vector<SPoint> pts;
    pts.reserve(seg);
    for (int i = 0; i < seg; ++i) {
        const float a = 2.f * kPi * i / seg;
        pts.push_back({cx + rx * std::cos(a), cy + ry * std::sin(a)});
    }
    return pts;
}

// 凸形扇形填充（interior 点为扇心）
void fanFill(RenderDevice* dev, const std::vector<SPoint>& rim, float cx, float cy, SColor c) {
    std::vector<Vtx> t;
    t.reserve(rim.size() * 3);
    for (size_t i = 0; i < rim.size(); ++i) {
        const SPoint& a = rim[i];
        const SPoint& b = rim[(i + 1) % rim.size()];
        t.push_back({cx, cy, c});
        t.push_back({a.x, a.y, c});
        t.push_back({b.x, b.y, c});
    }
    submit(dev, t);
}

// 环带：外/内周界（同起点同方向、同点数）连成三角形条带
void ringBand(RenderDevice* dev, const std::vector<SPoint>& outer,
              const std::vector<SPoint>& inner, SColor c) {
    const size_t n = std::min(outer.size(), inner.size());
    if (n < 2) return;
    std::vector<Vtx> t;
    t.reserve(n * 6);
    for (size_t i = 0; i < n; ++i) {
        const size_t j = (i + 1) % n;
        t.push_back({outer[i].x, outer[i].y, c});
        t.push_back({inner[i].x, inner[i].y, c});
        t.push_back({outer[j].x, outer[j].y, c});
        t.push_back({inner[i].x, inner[i].y, c});
        t.push_back({inner[j].x, inner[j].y, c});
        t.push_back({outer[j].x, outer[j].y, c});
    }
    submit(dev, t);
}

// 圆角矩形周界（顺时针；inset>0 向内缩、<0 向外扩；radius 随 inset 钳制）
std::vector<SPoint> roundRectPts(const SRect& r, float radius, float inset) {
    const float l = r.left + inset, t = r.top + inset;
    const float w = r.width - inset * 2.f, h = r.height - inset * 2.f;
    if (w <= 0.f || h <= 0.f) return {};
    radius = std::max(0.f, std::min(radius - inset, std::min(w, h) * 0.5f));
    std::vector<SPoint> pts;
    pts.reserve(4 * (kCornerSegments + 1));
    auto arc = [&](float cx, float cy, float a0, float a1) {
        for (int i = 0; i <= kCornerSegments; ++i) {
            const float a = a0 + (a1 - a0) * i / kCornerSegments;
            pts.push_back({cx + radius * std::cos(a), cy + radius * std::sin(a)});
        }
    };
    arc(l + w - radius, t + radius, -kPi / 2.f, 0.f);            // 上边末 → 右（TR）
    arc(l + w - radius, t + h - radius, 0.f, kPi / 2.f);         // 右 → 下（BR）
    arc(l + radius, t + h - radius, kPi / 2.f, kPi);             // 下 → 左（BL）
    arc(l + radius, t + radius, kPi, 1.5f * kPi);                // 左 → 上（TL）
    return pts;
}

// 单段宽线四边形（方角端帽）
void wideSegment(RenderDevice* dev, float x1, float y1, float x2, float y2, float w, SColor c) {
    const float dx = x2 - x1, dy = y2 - y1;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-6f) return;
    const float nx = -dy / len * w * 0.5f, ny = dx / len * w * 0.5f;
    dev->drawQuad(x1 + nx, y1 + ny, x2 + nx, y2 + ny, x2 - nx, y2 - ny, x1 - nx, y1 - ny, c);
}

// 路径描边（closed 时闭合回起点；w<=1 走像素对齐 1px drawLine）
void strokePath(RenderDevice* dev, const std::vector<SPoint>& pts, bool closed, float w, SColor c) {
    if (w <= 0.f || pts.size() < 2) return;
    if (w <= 1.f) {
        dev->setDrawColor(c);
        for (size_t i = 0; i + 1 < pts.size(); ++i)
            dev->drawLine(pts[i].x, pts[i].y, pts[i + 1].x, pts[i + 1].y);
        if (closed && pts.size() > 2)
            dev->drawLine(pts.back().x, pts.back().y, pts.front().x, pts.front().y);
        return;
    }
    for (size_t i = 0; i + 1 < pts.size(); ++i)
        wideSegment(dev, pts[i].x, pts[i].y, pts[i + 1].x, pts[i + 1].y, w, c);
    if (closed && pts.size() > 2)
        wideSegment(dev, pts.back().x, pts.back().y, pts.front().x, pts.front().y, w, c);
}

// 叉积 / 点在三角形内（耳切用）
float crossProduct(const SPoint& o, const SPoint& a, const SPoint& b) {
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}
bool ptInTri(const SPoint& p, const SPoint& a, const SPoint& b, const SPoint& c) {
    const float d1 = crossProduct(a, b, p), d2 = crossProduct(b, c, p), d3 = crossProduct(c, a, p);
    const bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    const bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(hasNeg && hasPos);
}

// 凹多边形耳切三角剖分 PhaseA（无自交无洞，O(n²)，UI 顶点 n≤64 成本可忽略）
std::vector<Vtx> earClip(const std::vector<SPoint>& pts, SColor c) {
    std::vector<Vtx> out;
    const int n = static_cast<int>(pts.size());
    if (n < 3) return out;
    std::vector<int> idx(n);
    for (int i = 0; i < n; ++i) idx[i] = i;
    int guard = 0;
    const int maxIter = n * n;
    while (static_cast<int>(idx.size()) > 3 && guard++ < maxIter) {
        bool clipped = false;
        const int m = static_cast<int>(idx.size());
        for (int i = 0; i < m; ++i) {
            const int ia = idx[(i + m - 1) % m], ib = idx[i], ic = idx[(i + 1) % m];
            const SPoint& a = pts[ia]; const SPoint& b = pts[ib]; const SPoint& cc = pts[ic];
            if (crossProduct(a, b, cc) <= 0.f) continue;              // 非凸顶点（顺时针序列取正）
            bool contain = false;
            for (int k = 0; k < m; ++k) {
                if (k == (i + m - 1) % m || k == i || k == (i + 1) % m) continue;
                if (ptInTri(pts[idx[k]], a, b, cc)) { contain = true; break; }
            }
            if (contain) continue;
            out.push_back({a.x, a.y, c});
            out.push_back({b.x, b.y, c});
            out.push_back({cc.x, cc.y, c});
            idx.erase(idx.begin() + i);
            clipped = true;
            break;
        }
        if (!clipped) break;                                    // 退化/自交：停止（一期不保证）
    }
    if (idx.size() == 3) {
        out.push_back({pts[idx[0]].x, pts[idx[0]].y, c});
        out.push_back({pts[idx[1]].x, pts[idx[1]].y, c});
        out.push_back({pts[idx[2]].x, pts[idx[2]].y, c});
    }
    return out;
}

} // namespace

// ────────────────────────── 宽线 / 折线 ──────────────────────────

void RenderDevice::drawLine(float x1, float y1, float x2, float y2, float width, SColor color) {
    if (width <= 1.f) { setDrawColor(color); drawLine(x1, y1, x2, y2); return; }
    wideSegment(this, x1, y1, x2, y2, width, color);
}

void RenderDevice::drawPolyline(const SPoint* pts, int count, SColor color, float width) {
    if (!pts || count < 2) return;
    for (int i = 0; i + 1 < count; ++i)
        wideSegment(this, pts[i].x, pts[i].y, pts[i + 1].x, pts[i + 1].y, width, color);
}

// ────────────────────────── 椭圆 / 环 ──────────────────────────

void RenderDevice::drawEllipse(float cx, float cy, float rx, float ry,
                               SColor fill, SColor stroke, float lineWidth, float ringWidth) {
    rx = std::max(0.f, rx);
    ry = std::max(0.f, ry);
    if (rx < 0.5f && ry < 0.5f) return;

    if (fill.alpha() > 0) {
        if (ringWidth > 0.f) {
            const float irx = rx - ringWidth, iry = ry - ringWidth;
            if (irx < 0.5f || iry < 0.5f) {
                fanFill(this, ellipsePts(cx, cy, rx, ry, kEllipseSegments), cx, cy, fill); // 钳制实心
            } else {
                ringBand(this, ellipsePts(cx, cy, rx, ry, kEllipseSegments),
                              ellipsePts(cx, cy, irx, iry, kEllipseSegments), fill);
            }
        } else {
            fanFill(this, ellipsePts(cx, cy, rx, ry, kEllipseSegments), cx, cy, fill);
        }
    }
    if (stroke.alpha() > 0 && lineWidth > 0.f)
        strokePath(this, ellipsePts(cx, cy, rx, ry, kEllipseSegments), false, lineWidth, stroke);
}

// ────────────────────────── 圆角矩形 ──────────────────────────

void RenderDevice::drawRoundRect(const SRect& rect, float radius,
                                 SColor fill, SColor stroke, float lineWidth) {
    if (rect.width <= 0.f || rect.height <= 0.f) return;

    if (fill.alpha() > 0) {
        auto path = roundRectPts(rect, radius, 0.f);
        if (!path.empty())
            fanFill(this, path, rect.left + rect.width * 0.5f, rect.top + rect.height * 0.5f, fill);
    }
    if (stroke.alpha() > 0 && lineWidth > 0.f) {
        if (lineWidth <= 1.f) {
            auto path = roundRectPts(rect, radius, 0.f);
            strokePath(this, path, true, lineWidth, stroke);
        } else {
            auto outer = roundRectPts(rect, radius, -lineWidth * 0.5f);
            auto inner = roundRectPts(rect, radius, +lineWidth * 0.5f);
            ringBand(this, outer, inner, stroke);
        }
    }
}

// ────────────────────────── 多边形 ──────────────────────────

void RenderDevice::drawPolygon(const SPoint* pts, int count,
                               SColor fill, SColor stroke, float lineWidth) {
    if (!pts || count < 3) return;
    const std::vector<SPoint> v(pts, pts + count);

    if (fill.alpha() > 0) {
        // 凸性检测（O(n)：叉积符号一致 → 零成本顶点扇）；否则耳切 PhaseA
        bool convexPos = false, convexNeg = false, convex = true;
        for (int i = 0; i < count; ++i) {
            const float cr = crossProduct(v[i], v[(i + 1) % count], v[(i + 2) % count]);
            if (cr > 0.f) convexPos = true;
            if (cr < 0.f) convexNeg = true;
            if (convexPos && convexNeg) { convex = false; break; }
        }
        if (convex) {
            fanFill(this, v, v[0].x, v[0].y, fill);             // 顶点扇（v0 为扇心）
        } else {
            submit(this, earClip(v, fill));
        }
    }
    if (stroke.alpha() > 0 && lineWidth > 0.f)
        strokePath(this, v, true, lineWidth, stroke);
}
