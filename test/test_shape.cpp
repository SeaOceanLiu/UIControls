// ============================================================================
// test_shape.cpp -- Shape 形状控件测试
// 断言：属性四层回环 / points 缩放 / mapToDrawPoint / ringWidth·lineWidth 负值钳制 / 未知形状拒绝
// 可视化：7 形状矩阵（空心/实心/环/凹多边形耳切）挂树，供视觉验收
// ============================================================================
#include <iostream>
#include <memory>
#include <cmath>
#include "Shape.h"
#include "Label.h"
#include "MainWindow.h"
#include "Bench.h"
#include "AppCallbacks.h"
#include "TestUtils.h"
#include "TestInstance.h"
#include "UICornerstoneAPI.h"

using namespace std;

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { ++g_pass; TestUtil::log("OK   %s", msg); } \
    else      { ++g_fail; TestUtil::log("FAIL %s", msg); } \
} while (0)

static shared_ptr<Shape> g_probe;    // 属性断言探针

static void runAssertions() {
    TestUtil::log("---- Shape assertions ----");

    // 1) C++ setter 回环
    g_probe->setShape(ShapeType::Circle);
    CHECK(g_probe->getShape() == ShapeType::Circle, "setShape/getShape Circle");

    // 2) CABI 同键：SetEnum("shape","ellipse")
    CHECK(g_probe->setEnumProperty("shape", "ellipse") == 1 &&
          g_probe->getShape() == ShapeType::Ellipse, "SetEnum(shape)=ellipse");

    // 3) 负例：未知形状拒绝且保持原值
    CHECK(g_probe->setEnumProperty("shape", "hexagon") == 0 &&
          g_probe->getShape() == ShapeType::Ellipse, "unknown shape rejected, keep ellipse");

    // 4) 颜色属性回环
    g_probe->setFillColor(SColor(59, 130, 246));
    SColor c;
    CHECK(g_probe->getColorProperty("fill", c) == 1 && c.alphaByte() == 255,
          "fill color roundtrip (alphaByte=255)");

    // 5) 数值属性回环 + 负值钳制
    CHECK(g_probe->setFloatProperty("line-width", 3.5f) == 1, "SetFloat(line-width)");
    float f = 0.f;
    CHECK(g_probe->getFloatProperty("line-width", f) == 1 && f == 3.5f,
          "line-width roundtrip 3.5");
    g_probe->setRingWidth(-5.f);
    CHECK(g_probe->getRingWidth() == 0.f, "negative ringWidth clamped to 0");
    g_probe->setLineWidth(-1.f);
    CHECK(g_probe->getLineWidth() == 0.f, "negative lineWidth clamped to 0");

    // 6) points 缩放（决策 3.3：以写入时 rect 为基准等比缩放）
    auto probe2 = make_shared<Shape>(nullptr, SRect(100, 100, 200, 100));
    probe2->setPoints({{10.f, 10.f}, {190.f, 90.f}});
    probe2->setRect(SRect(100, 100, 400, 200));            // x/y 各 2 倍缩放
    auto scaled = probe2->getPoints();
    CHECK(scaled.size() == 2 && fabs(scaled[1].x - 380.f) < 0.01f &&
          fabs(scaled[1].y - 180.f) < 0.01f, "points scale x2 after setRect");

    // 7) mapToDrawPoint / getDrawPoint（本地→全局 = rect.left/top + 缩放后本地）
    const SPoint g = probe2->mapToDrawPoint(10.f, 10.f);
    CHECK(fabs(g.x - 110.f) < 0.01f && fabs(g.y - 110.f) < 0.01f,
          "mapToDrawPoint local->global");
    const SPoint gp = probe2->getDrawPoint(0);
    CHECK(fabs(gp.x - 120.f) < 0.01f && fabs(gp.y - 120.f) < 0.01f,
          "getDrawPoint(0)=mapToDrawPoint(scaled[0])");

    TestUtil::log("---- assertions done: pass=%d fail=%d ----", g_pass, g_fail);
}

static void addShape(Bench* bench, const shared_ptr<Shape>& s) {
    s->create();
    bench->addControl(s);
}

static void addCaption(Bench* bench, int x, int y, const char* txt) {
    auto lbl = LabelBuilder(nullptr, SRect(x, y, 240, 22))
                   .setCaption(txt).build();
    lbl->create();
    bench->addControl(lbl);
}

// ── 可视化矩阵 ──
static void testShapeVisualize(Bench* bench) {
    g_probe = make_shared<Shape>(nullptr, SRect(0, 0, 40, 40));   // 探针不挂树
    runAssertions();

    int row = 40, col = 30;

    // 行1：rect 描边 / filled-rect 实心 / round-rect 圆角
    addCaption(bench, col, row, "rect(stroke) / filled-rect / round-rect");
    row += 28;
    {
        auto s = make_shared<Shape>(nullptr, SRect(col, row, 90, 90));
        s->setStrokeColor(SColor(30, 41, 59)); s->setLineWidth(2);
        addShape(bench, s);
    }
    {
        auto s = make_shared<Shape>(nullptr, SRect(col + 120, row, 90, 90));
        s->setShape(ShapeType::FilledRect); s->setFillColor(SColor(59, 130, 246));
        addShape(bench, s);
    }
    {
        auto s = make_shared<Shape>(nullptr, SRect(col + 240, row, 90, 90));
        s->setShape(ShapeType::RoundRect); s->setRadius(18);
        s->setFillColor(SColor(16, 185, 129)); s->setStrokeColor(SColor(6, 78, 59)); s->setLineWidth(3);
        addShape(bench, s);
    }

    // 行2：circle 实心 / circle 空心 / circle 环(ringWidth)
    addCaption(bench, col, row + 110, "circle solid / hollow / ring");
    row += 138;
    {
        auto s = make_shared<Shape>(nullptr, SRect(col, row, 90, 90));
        s->setShape(ShapeType::Circle); s->setFillColor(SColor(239, 68, 68));
        addShape(bench, s);
    }
    {
        auto s = make_shared<Shape>(nullptr, SRect(col + 120, row, 90, 90));
        s->setShape(ShapeType::Circle); s->setStrokeColor(SColor(30, 41, 59)); s->setLineWidth(2);
        addShape(bench, s);
    }
    {
        auto s = make_shared<Shape>(nullptr, SRect(col + 240, row, 90, 90));
        s->setShape(ShapeType::Circle); s->setFillColor(SColor(59, 130, 246)); s->setRingWidth(14);
        addShape(bench, s);
    }

    // 行3：ellipse 拉伸 / polyline 粗折线 / polygon 凹多边形(箭头,验证耳切)
    addCaption(bench, col, row + 110, "ellipse stretch / polyline w4 / polygon concave(earclip)");
    row += 138;
    {
        auto s = make_shared<Shape>(nullptr, SRect(col, row, 140, 70));
        s->setShape(ShapeType::Ellipse); s->setFillColor(SColor(168, 85, 247));
        s->setStrokeColor(SColor(59, 7, 100)); s->setLineWidth(2);
        addShape(bench, s);
    }
    {
        auto s = make_shared<Shape>(nullptr, SRect(col + 160, row, 150, 80));
        s->setShape(ShapeType::Polyline);
        s->setPoints({{5.f, 75.f}, {45.f, 15.f}, {85.f, 55.f}, {125.f, 5.f}});
        s->setStrokeColor(SColor(234, 88, 12)); s->setLineWidth(4);
        addShape(bench, s);
    }
    {
        auto s = make_shared<Shape>(nullptr, SRect(col + 330, row, 150, 90));
        s->setShape(ShapeType::Polygon);                       // 凹多边形（箭头）
        s->setPoints({{0.f, 0.f}, {70.f, 0.f}, {70.f, 20.f}, {145.f, 35.f},
                      {70.f, 50.f}, {70.f, 70.f}, {0.f, 70.f}});
        s->setFillColor(SColor(245, 158, 11)); s->setStrokeColor(SColor(124, 45, 18)); s->setLineWidth(2);
        addShape(bench, s);
    }

    // 行4：JSON 声明式路径抽查（经 LayoutParser 的控件由 test_layout 覆盖；此处补 CABI 属性路径）
    addCaption(bench, col, row + 110, "CABI: SetEnum(shape)+SetColor(fill)+SetFloat(ring-width)");
    row += 138;
    {
        auto s = make_shared<Shape>(nullptr, SRect(col, row, 90, 90));
        s->create(); bench->addControl(s);
        s->setEnumProperty("shape", "ellipse");
        s->setColorProperty("fill", SColor(34, 197, 94));
        s->setFloatProperty("ring-width", 10.f);
    }
}

class ShapeApp : public AppCallbacks {
public:
    bool onInit() override {
        MAINWIN->setTitle("test_shape");
        BENCH->setOnInitial([](shared_ptr<Bench> b) { testShapeVisualize(b.get()); });
        return true;
    }
    void onUpdate() override {
        BENCH->eventLoopEntry();
        BENCH->update();
    }
    void onRender() override {
        GET_RENDERDEVICE->setDrawColor(SColor(40, 40, 44));
        GET_RENDERDEVICE->clear();
        BENCH->draw();
        // 视觉验证：帧内读回（Render 后、Present 前，design/Capture_API_Design.md §3.6）
        if (++m_frames == 30 && !m_saved) {
            static uint8_t pixels[1400 * 900 * 4];
            int w = 0, h = 0;
            const int cap = UICornerstone_CaptureViewport(g_uiInstance, pixels, &w, &h);
            const std::string out = "Temp/shape_capture.bmp";
            const int saved = cap ? UICornerstone_SavePixelsToFile(pixels, w, h, out.c_str()) : 0;
            TestUtil::log("capture: cap=%d saved=%d (%dx%d) -> %s", cap, saved, w, h,
                          saved ? out.c_str() : "FAILED");
            m_saved = true;
        }
    }
    void onQuit() override { TestUtil::log("Shape test quit"); }

private:
    int m_frames = 0;
    bool m_saved = false;
};

int main(int argc, char* argv[]) {
    return TestRunMain<ShapeApp>(argc, argv);
}
