// ============================================================================
// test_tabcontrol.cpp -- TabControl 选项卡控件测试
// 断言：addTab/setCurrentIndex/onTabChange/页面显隐/键盘导航/removeTab 钳制
// CABI：Create/TabAddPage/TabSetTitle/TabSetPage/TabSetTabLeadingControl + 通用属性
// JSON：tab-control + tabs[] + currentIndex
// ============================================================================
#include <iostream>
#include <memory>
#include "TabControl.h"
#include "Panel.h"
#include "Label.h"
#include "MainWindow.h"
#include "Bench.h"
#include "AppCallbacks.h"
#include "TestUtils.h"
#include "TestInstance.h"
#include "UICornerstoneAPI.h"
#include "EventTypes.h"
#include "PropertyNames.h"
#include "LayoutParser.h"

using namespace std;

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { ++g_pass; TestUtil::log("OK   %s", msg); } \
    else      { ++g_fail; TestUtil::log("FAIL %s", msg); } \
} while (0)

static shared_ptr<Event> makeKey(KeyCode k) {
    auto ev = make_shared<Event>(EventType::KeyDown);
    ev->keyEvent.keycode = k;
    return ev;
}

static shared_ptr<Panel> makePage(const string& label) {
    auto p = make_shared<Panel>(nullptr, SRect(0, 0, 100, 100));
    p->create();
    return p;
}

// ── 断言 ──
static void runAssertions() {
    TestUtil::log("---- TabControl assertions ----");
    auto tc = make_shared<TabControl>(nullptr, SRect(0, 0, 400, 300));
    tc->setVisible(true);

    int i0 = tc->addTab("A", makePage("a"));
    int i1 = tc->addTab("B", makePage("b"));
    int i2 = tc->addTab("C", makePage("c"));
    CHECK(i0 == 0 && i1 == 1 && i2 == 2, "addTab returns sequential indices");
    CHECK(tc->getTabCount() == 3, "getTabCount == 3");
    CHECK(tc->getCurrentIndex() == 0, "initial current index == 0");

    // 页面显隐：仅当前页可见
    CHECK(tc->getTabs()[0].page->getVisible(), "page0 visible at start");

    // setCurrentIndex 切换 + onTabChange
    int fired = -1;
    tc->setOnTabChange([&](shared_ptr<TabControl>, int idx) { fired = idx; });
    tc->setCurrentIndex(2);
    CHECK(tc->getCurrentIndex() == 2, "setCurrentIndex(2)");
    CHECK(fired == 2, "onTabChange fired with index 2");
    CHECK(tc->getTabs()[2].page->getVisible(), "page2 visible after switch");
    CHECK(!tc->getTabs()[0].page->getVisible(), "page0 hidden after switch");

    // 键盘导航：Right 循环 → 0 之后回绕
    tc->setCurrentIndex(0);
    tc->handleEvent(makeKey(KeyCode::Right));
    CHECK(tc->getCurrentIndex() == 1, "Right -> next tab");
    tc->handleEvent(makeKey(KeyCode::Left));
    CHECK(tc->getCurrentIndex() == 0, "Left -> prev tab");
    tc->handleEvent(makeKey(KeyCode::End));
    CHECK(tc->getCurrentIndex() == 2, "End -> last tab");
    tc->handleEvent(makeKey(KeyCode::Home));
    CHECK(tc->getCurrentIndex() == 0, "Home -> first tab");

    // removeTab 钳制
    tc->setCurrentIndex(2);
    tc->removeTab(2);
    CHECK(tc->getTabCount() == 2, "removeTab reduces count");
    CHECK(tc->getCurrentIndex() <= 1, "current index clamped after removeTab");

    // setPosition / fontSize 属性
    tc->setPosition(TabPosition::Bottom);
    CHECK(tc->getPosition() == TabPosition::Bottom, "setPosition Bottom");
    tc->setFontSize(16.f);
    CHECK(tc->getFontSize() == 16.f, "setFontSize");

    TestUtil::log("---- assertions done: pass=%d fail=%d ----", g_pass, g_fail);
}

// ── CABI ──
static void runCabiChecks() {
    TestUtil::log("---- TabControl CABI checks ----");
    UIControlHandle t = UICornerstone_CreateTabControl(g_uiInstance, 10.f, 10.f, 400.f, 300.f, 1.f, 1.f);
    CHECK(t != nullptr, "CreateTabControl");

    CHECK(UICornerstone_TabAddPage(g_uiInstance, t, "One") == 0, "TabAddPage #0");
    CHECK(UICornerstone_TabAddPage(g_uiInstance, t, "Two") == 1, "TabAddPage #1");

    // 页面句柄绑定
    UIControlHandle pg = UICornerstone_CreatePanel(g_uiInstance, 0, 0, 100, 100, 1.f, 1.f);
    CHECK(UICornerstone_TabSetPage(g_uiInstance, t, 0, pg) == 1, "TabSetPage");

    CHECK(UICornerstone_TabSetTitle(g_uiInstance, t, 1, "TwoB") == 1, "TabSetTitle");

    UIControlHandle ic = UICornerstone_CreateLabel(g_uiInstance, "ic", 12.f, 0, 0, 16, 16, 1.f, 1.f);
    CHECK(UICornerstone_TabSetTabLeadingControl(g_uiInstance, t, 0, ic) == 1, "TabSetTabLeadingControl");

    // 通用属性：position / current-index / font-size
    CHECK(UICornerstone_SetEnum(g_uiInstance, t, PropertyNames::kJsonPosition, "left") == 1, "SetEnum position=left");
    CHECK(UICornerstone_SetInt(g_uiInstance, t, PropertyNames::kJsonCurrentIndex, 1) == 1, "SetInt current-index=1");
    CHECK(UICornerstone_SetFloat(g_uiInstance, t, PropertyNames::kFontSize, 15.f) == 1, "SetFloat font-size=15");
    int cur = -1;
    CHECK(UICornerstone_GetInt(g_uiInstance, t, PropertyNames::kJsonCurrentIndex, &cur) == 1 && cur == 1, "GetInt current-index reads back");

    TestUtil::log("---- CABI checks done: pass=%d fail=%d ----", g_pass, g_fail);
}

// ── 可视化 + JSON ──
static shared_ptr<TabControl> g_tc;

static void testTabVisualize(Bench* bench) {
    auto tc = make_shared<TabControl>(bench, SRect(120, 80, 560, 360));
    auto p0 = make_shared<Panel>(tc.get(), SRect(0, 0, 100, 100)); p0->create();
    auto p1 = make_shared<Panel>(tc.get(), SRect(0, 0, 100, 100)); p1->create();
    auto p2 = make_shared<Panel>(tc.get(), SRect(0, 0, 100, 100)); p2->create();
    tc->addTab("主页", p0);
    tc->addTab("设置", p1);
    tc->addTab("关于", p2);
    tc->setCurrentIndex(0);
    tc->create();
    tc->show();                           // 默认 visible=false，显式显示
    bench->addControl(tc);
    g_tc = tc;

    // JSON 解析（tab-control + tabs + currentIndex）
    const string json = R"({
      "controls":[{
        "type":"tab-control","id":"tabs","rect":[0,0,480,320],
        "position":"top","fontSize":13,"currentIndex":1,
        "tabs":[
          {"title":"Home","page":{"type":"panel"}},
          {"title":"Settings","page":{"type":"panel"}},
          {"title":"About","page":{"type":"panel"}}
        ]
      }]
    })";
    LayoutParser parser;
    auto root = parser.parseLayout(json);
    auto* ci = root ? dynamic_cast<ControlImpl*>(root.get()) : nullptr;
    auto jtc = ci ? dynamic_pointer_cast<TabControl>(ci->getThis()) : nullptr;
    CHECK(jtc != nullptr, "JSON tab-control parsed");
    if (jtc) {
        CHECK(jtc->getTabCount() == 3, "JSON tabs parsed (3 pages)");
        CHECK(jtc->getCurrentIndex() == 1, "JSON currentIndex applied");
        CHECK(jtc->getPosition() == TabPosition::Top, "JSON position applied");
    }

    // 调用断言与 CABI（独立于 bench 的 probe）
    runAssertions();
    runCabiChecks();
}

class TabApp : public AppCallbacks {
public:
    bool onInit() override {
        MAINWIN->setTitle("test_tabcontrol");
        BENCH->setOnInitial([](shared_ptr<Bench> b) { testTabVisualize(b.get()); });
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

        static uint8_t pixels[1400 * 900 * 4];
        int w = 0, h = 0;
        const int cap = UICornerstone_CaptureViewport(g_uiInstance, pixels, &w, &h);

        if (m_frames == 10 && !m_cap0) {
            const int saved = cap ? UICornerstone_SavePixelsToFile(pixels, w, h, "Temp/tab_shown.bmp") : 0;
            TestUtil::log("capture shown: cap=%d saved=%d", cap, saved);
            m_cap0 = true;
        }

        // 第 20 帧：切到第 2 页，验证页面切换
        if (m_frames == 20 && g_tc) {
            g_tc->setCurrentIndex(2);
            TestUtil::log("switched to tab %d", g_tc->getCurrentIndex());
            CHECK(g_tc->getCurrentIndex() == 2, "visual switch to tab 2");
        }
        if (m_frames == 40 && !m_cap1) {
            const int saved = cap ? UICornerstone_SavePixelsToFile(pixels, w, h, "Temp/tab_switched.bmp") : 0;
            TestUtil::log("capture switched: cap=%d saved=%d", cap, saved);
            m_cap1 = true;
            TestUtil::log("---- TabControl test result: pass=%d fail=%d ----", g_pass, g_fail);
        }

        ++m_frames;
    }
    void onQuit() override { TestUtil::log("TabControl test quit"); }

private:
    int m_frames = 0;
    bool m_cap0 = false;
    bool m_cap1 = false;
};

int main(int argc, char* argv[]) {
    return TestRunMain<TabApp>(argc, argv);
}
