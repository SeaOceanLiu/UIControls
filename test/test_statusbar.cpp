// ============================================================================
// test_statusbar.cpp -- StatusBar 状态栏控件测试
// 断言：数据模型（增删改/右对齐）/ 属性回环 / 弹窗绑定
// 可视化：底部状态栏（左：分支+问题数；右：编码/行尾/缩进）+ 点击分支弹出菜单
// ============================================================================
#include <iostream>
#include <memory>
#include <cmath>
#include "StatusBar.h"
#include "Menu.h"
#include "Label.h"
#include "MainWindow.h"
#include "Bench.h"
#include "AppCallbacks.h"
#include "TestUtils.h"
#include "TestInstance.h"
#include "UICornerstoneAPI.h"
#include "EventTypes.h"

using namespace std;

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { ++g_pass; TestUtil::log("OK   %s", msg); } \
    else      { ++g_fail; TestUtil::log("FAIL %s", msg); } \
} while (0)

static shared_ptr<StatusBar> g_probe;   // 数据模型断言探针（不挂树）
static shared_ptr<StatusBar> g_bar;     // 可视化控件（供 App 阶段点击弹出）

static shared_ptr<Event> makeMouse(EventType type, float x, float y) {
    auto ev = make_shared<Event>(type);
    if (type == EventType::MouseMove) ev->mousePos = {x, y};
    else ev->mouseButton = {x, y, MouseButton::Left};
    return ev;
}

static void runAssertions() {
    TestUtil::log("---- StatusBar assertions ----");

    g_probe->addStatusItem("branch", u8"main", false);
    g_probe->addStatusItem("problems", u8"0 问题", false);
    g_probe->addStatusItem("encoding", u8"UTF-8", true);
    g_probe->addStatusItem("eol", u8"LF", true);
    CHECK(g_probe->getStatusItem("branch") != nullptr, "addStatusItem x4");
    CHECK(g_probe->getStatusItem("branch")->rightAlign == false, "left item rightAlign=false");
    CHECK(g_probe->getStatusItem("encoding")->rightAlign == true, "right item rightAlign=true");

    // 改文本
    g_probe->updateStatusItemText("encoding", u8"GBK");
    CHECK(g_probe->getStatusItem("encoding")->text == u8"GBK", "updateStatusItemText");

    // 移除
    g_probe->removeStatusItem("eol");
    CHECK(g_probe->getStatusItem("eol") == nullptr, "removeStatusItem");

    // 弹窗面板初始为空
    CHECK(g_probe->getPopupPanel() == nullptr, "popup panel null initially");

    // 属性回环
    g_probe->setFloatProperty("font-size", 14.f);
    float f = 0.f;
    CHECK(g_probe->getFloatProperty("font-size", f) == 1 && f == 14.f, "font-size roundtrip");
    g_probe->setFloatProperty("item-height", 28.f);
    CHECK(g_probe->getFloatProperty("item-height", f) == 1 && f == 28.f, "item-height roundtrip");

    // 图标控件绑定（API 接受）
    auto icon = make_shared<Label>(nullptr, SRect(0, 0, 16, 16));
    g_probe->setStatusItemLeadingControl("branch", icon);
    CHECK(g_probe->getStatusItem("branch")->leadingControl == icon, "setStatusItemLeadingControl");

    TestUtil::log("---- assertions done: pass=%d fail=%d ----", g_pass, g_fail);
}

// ── C ABI 抽查 ──
static void runCabiChecks() {
    TestUtil::log("---- StatusBar CABI checks ----");
    UIControlHandle h = UICornerstone_CreateStatusBar(
        g_uiInstance, 700.f, 820.f, 360.f, 24.f, 1.f, 1.f);
    CHECK(h != nullptr, "CreateStatusBar");

    CHECK(UICornerstone_StatusBarAddItem(g_uiInstance, h, "a", u8"左段", 0) == 1 &&
          UICornerstone_StatusBarAddItem(g_uiInstance, h, "b", u8"右段", 1) == 1,
          "StatusBarAddItem x2");

    CHECK(UICornerstone_StatusBarSetItemText(g_uiInstance, h, "a", u8"已改") == 1,
          "StatusBarSetItemText");

    // 图标（Label 句柄）
    UIControlHandle lbl = UICornerstone_CreateLabel(g_uiInstance, u8"@", 14.f, 0.f, 0.f, 16.f, 16.f, 1.f, 1.f);
    CHECK(UICornerstone_StatusBarSetItemIcon(g_uiInstance, h, "a", lbl) == 1,
          "StatusBarSetItemIcon");

    // 弹窗（MenuPanel 句柄）
    UIControlHandle mp = UICornerstone_CreateMenuPanel(g_uiInstance, 1.f, 1.f);
    CHECK(UICornerstone_StatusBarSetItemMenu(g_uiInstance, h, "a", mp) == 1,
          "StatusBarSetItemMenu");

    CHECK(UICornerstone_StatusBarRemoveItem(g_uiInstance, h, "b") == 1,
          "StatusBarRemoveItem");

    TestUtil::log("---- CABI checks done: pass=%d fail=%d ----", g_pass, g_fail);
}

// ── 可视化矩阵 ──
static void testStatusBarVisualize(Bench* bench) {
    g_probe = make_shared<StatusBar>(nullptr, SRect(0, 0, 100, 24));   // 断言探针不挂树
    runAssertions();

    auto bar = make_shared<StatusBar>(nullptr, SRect(0, 876, 1400, 24));
    bar->addStatusItem("branch", u8"main", false);
    bar->addStatusItem("problems", u8"0 问题 0 警告", false);
    bar->addStatusItem("encoding", u8"UTF-8", true);
    bar->addStatusItem("eol", u8"LF", true);
    bar->addStatusItem("indent", u8"空格: 4", true);

    // 分支段绑定弹窗菜单
    auto branchMenu = make_shared<MenuPanel>(nullptr, 1.0f, 1.0f);
    branchMenu->addItem(MenuItemBuilder(u8"master").build());
    branchMenu->addItem(MenuItemBuilder(u8"develop").build());
    branchMenu->addItem(MenuItemBuilder(u8"feature/login").build());
    bar->setStatusItemMenu("branch", branchMenu);

    // 分支段绑定图标（小 Label）
    auto icon = make_shared<Label>(nullptr, SRect(0, 0, 16, 16));
    bar->setStatusItemLeadingControl("branch", icon);

    bar->create();
    bench->addControl(bar);

    runCabiChecks();

    // 记录控件供 App 阶段模拟点击弹出
    g_bar = bar;
}

class StatusBarApp : public AppCallbacks {
public:
    bool onInit() override {
        MAINWIN->setTitle("test_statusbar");
        BENCH->setOnInitial([](shared_ptr<Bench> b) { testStatusBarVisualize(b.get()); });
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

        if (m_frames == 20 && !m_savedNormal) {
            const int saved = cap ? UICornerstone_SavePixelsToFile(pixels, w, h, "Temp/statusbar_normal.bmp") : 0;
            TestUtil::log("capture normal: cap=%d saved=%d -> %s", cap, saved, saved ? "Temp/statusbar_normal.bmp" : "FAILED");
            m_savedNormal = true;
        }

        // 第 30 帧：模拟点击分支段，弹出菜单
        if (m_frames == 30 && g_bar) {
            auto* it = g_bar->getStatusItem("branch");
            if (it) {
                const float cx = g_bar->getRect().left + it->hitRect.left + it->hitRect.width / 2.f;
                const float cy = g_bar->getRect().top + it->hitRect.top + it->hitRect.height / 2.f;
                g_bar->handleEvent(makeMouse(EventType::MouseDown, cx, cy));
                g_bar->handleEvent(makeMouse(EventType::MouseUp, cx, cy));
                TestUtil::log("clicked branch at (%.0f,%.0f) popupOpen=%d", cx, cy,
                              g_bar->isPopupOpen() ? 1 : 0);
            }
        }

        if (m_frames == 45 && !m_savedPopup) {
            const int saved = cap ? UICornerstone_SavePixelsToFile(pixels, w, h, "Temp/statusbar_popup.bmp") : 0;
            TestUtil::log("capture popup: cap=%d saved=%d -> %s", cap, saved, saved ? "Temp/statusbar_popup.bmp" : "FAILED");
            m_savedPopup = true;
            TestUtil::log("---- StatusBar test result: pass=%d fail=%d ----", g_pass, g_fail);
        }
        ++m_frames;
    }
    void onQuit() override { TestUtil::log("StatusBar test quit"); }

private:
    int m_frames = 0;
    bool m_savedNormal = false;
    bool m_savedPopup = false;
};

int main(int argc, char* argv[]) {
    return TestRunMain<StatusBarApp>(argc, argv);
}
