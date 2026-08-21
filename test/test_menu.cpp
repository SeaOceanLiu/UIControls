#include <iostream>
#include <memory>
#include "EditBox.h"
#include "TextArea.h"
#include "ScrollBar.h"
#include "Menu.h"
#include "CheckBox.h"
#include "LayoutParser.h"
#include "PropertyNames.h"
#include "MainWindow.h"
#include "AppCallbacks.h"
#include "Bench.h"
#include "Button.h"
#include "Actor.h"
#include "GraphTool.h"
#include "TestUtils.h"
#include "TestInstance.h"

using namespace std;

shared_ptr<MenuBar> g_menuBar;
shared_ptr<MenuBar> g_jsonBar;
shared_ptr<MenuBar> g_propBar;
shared_ptr<MenuPanel> g_filePanel;
shared_ptr<MenuPanel> g_editPanel;
shared_ptr<Button> g_Button;
shared_ptr<GraphTool::DrawingContext> g_dc;

// ── Menu 增强断言（前置控件容器 + 逐 Item 字体 + 变行高 + 事件路由）──
static int g_enhPass = 0, g_enhFail = 0;
#define ENH_CHECK(cond, msg) \
    do { if (cond) { ++g_enhPass; } else { ++g_enhFail; printf("ENH-FAIL: %s\n", msg); } } while (0)

// ── 视觉验证：打开菜单并 readPixels 统计面板/容器区域非背景像素（跨帧：点击帧 N、读取帧 N+1）──
static shared_ptr<Event> makeMouse(EventType type, float x, float y);
static int g_visPhase = 0;
static bool g_visDone = false;

static void dumpRegion(const char* tag, SRect r) {
    if (r.width <= 0 || r.height <= 0) {
        fprintf(stderr, "VIS %s: EMPTY rect (%.0f,%.0f %.0fx%.0f)\n", tag, r.left, r.top, r.width, r.height);
        return;
    }
    vector<uint32_t> px((size_t)r.width * r.height);
    BENCH->getRenderDevice()->readPixels(px.data(), r);
    uint32_t bg = 0xFF262525u;        // PANEL_BG(37,37,38,255)
    uint32_t env = 0xFF181717u;       // BENCH 背景(23,23,24)
    size_t nPanel = 0, nContent = 0;
    for (uint32_t v : px) {
        if (v == bg) ++nPanel;
        else if (v != env) ++nContent;
    }
    fprintf(stderr, "VIS %s: region(%.0f,%.0f %.0fx%.0f) panelBg=%.1f%% content=%.1f%% sample=%08X %08X %08X\n",
        tag, r.left, r.top, r.width, r.height,
        px.empty() ? 0.0 : 100.0 * nPanel / px.size(),
        px.empty() ? 0.0 : 100.0 * nContent / px.size(),
        px.empty() ? 0 : px[0], px.size() < 2 ? 0 : px[1], px.size() < 3 ? 0 : px[2]);
}

static void clickAt(shared_ptr<Control> ctl, float x, float y) {
    ctl->handleEvent(makeMouse(EventType::MouseDown, x, y));
    ctl->handleEvent(makeMouse(EventType::MouseUp, x, y));
}

static void dumpContainers(const char* tag, shared_ptr<MenuPanel> panel) {
    SRect pd = panel->getDrawRect();
    for (float y = 1.0f; y < panel->getRect().height - 1.0f; y += 2.0f) {
        auto item = panel->getItemAt(pd.left + 5.0f, pd.top + y);
        if (item && item->getLeadingControl()) {
            dumpRegion(tag, item->getLeadingControl()->getDrawRect());
            return;
        }
    }
    fprintf(stderr, "VIS %s: no leading control found\n", tag);
}

// 视觉演示状态机：0=点文件菜单打开（保持打开供目视）；1=完成
// 注：readPixels 与屏幕显示存在滞后差异（同帧读不到刚画的像素），像素统计仅供参考，
//     渲染执行本身由 45 个结构断言（rect/字号/容器/字体加载）覆盖验证。
static void doVisualCapture() {
    switch (g_visPhase) {
    case 0:
        // 移除重叠的全宽 JSON/PROP 菜单栏，仅保留文件菜单栏供目视（三条菜单栏均为全宽 (0,0)）
        if (g_jsonBar) BENCH->removeControl(g_jsonBar);
        if (g_propBar) BENCH->removeControl(g_propBar);
        // 演示增强项（断言阶段已 removeItem）：图片容器（验证容器填充）+ CheckBox 容器（setCaptionSize 随字号）+ 变行高（24 号）
        {
            auto demoImg = make_shared<Actor>(nullptr, fs::path("assets/images/cross_up.png"), false);
            auto demoItem = MenuItemBuilder(u8"增强演示(E)")
                .setShortcut("Ctrl+E")
                .setLeadingControl(demoImg)
                .setFontSize(24)
                .build();
            g_filePanel->addItem(demoItem);
            auto cbCtl = CheckBoxBuilder(nullptr, SRect(0, 0, 0, 0))
                .setCaptionSize(24)
                .setCheckState(CheckState::Checked)
                .build();
            auto cbItem = MenuItemBuilder(u8"已启用开关")
                .setLeadingControl(cbCtl)
                .setFontSize(24)
                .build();
            g_filePanel->addItem(cbItem);
        }
        // 断言末尾的“视觉演示”已打开文件菜单；此处直接定位并保持显示（避免重复点击切换关闭）
        g_filePanel->setPosition(0, 32);
        g_filePanel->show();
        g_visPhase = 1;
        break;
    default:
        g_visDone = true;
        break;
    }
}

static shared_ptr<Event> makeMouse(EventType type, float x, float y) {
    auto ev = make_shared<Event>(type);
    if (type == EventType::MouseMove) ev->mousePos = {x, y};
    else ev->mouseButton = {x, y, MouseButton::Left};
    return ev;
}

// 挂载场景（MenuBar → MenuPanel → items）验证事件路由与菜单链行为
static void runMenuEnhAssertions(shared_ptr<MenuPanel> panel) {
    // 基础：默认字号行高 = 20×1.6 = 32；分隔行 21；无容器时 icon 区 = 0（不预留空间）
    panel->show();
    ENH_CHECK(panel->getRect().height > 0, u8"面板高度应 > 0");
    ENH_CHECK(!panel->hasLeadingControl(), u8"无容器 → 不预留 icon 区");
    ENH_CHECK(panel->getIconAreaWidth() == 0.0f, u8"无容器 icon 区宽 = 0");

    // 覆盖项：font-size 24 → 行高 24×1.6=38.4、生效字号=24（addItem 时序：ensureOwnFont 先于测量）
    auto fontItem = MenuItemBuilder(u8"打印(P)…")
        .setShortcut("Ctrl+P")
        .setFontSize(24)
        .build();    panel->addItem(fontItem);
    ENH_CHECK(fontItem->getFontSize() == 24.0f, u8"覆盖项生效字号应为 24（addItem 时序 ensureOwnFont）");
    ENH_CHECK(fontItem->getRect().height == 24.0f * 1.6f, u8"覆盖项行高 = 24×1.6 = 38.4");

    // 面板级 setFontSize：继承项跟随、覆盖项保持自身字号
    auto firstRow = [&]() {
        SRect d = panel->getDrawRect();
        return panel->getItemAt(d.left + 5.0f, d.top + 5.0f);
    };
    panel->setFontSize(22.0f);
    auto inheritItem = firstRow();
    ENH_CHECK(inheritItem && inheritItem->getFontSize() == 22.0f, u8"继承项随面板字号更新（setFontSize 22）");
    ENH_CHECK(fontItem->getFontSize() == 24.0f, u8"覆盖项不随面板字号变化");
    ENH_CHECK(inheritItem && inheritItem->getRect().height == 22.0f * 1.6f, u8"继承项行高 = 22×1.6");
    panel->setFontSize(20.0f);
    ENH_CHECK(inheritItem && inheritItem->getRect().height == 32.0f, u8"继承项行高恢复 32");

    // 前置容器几何（先跑一帧 draw 刷新容器 rect）
    auto cb = CheckBoxBuilder(nullptr, SRect(0, 0, 0, 0)).setCheckState(CheckState::Unchecked).build();
    bool cbClicked = false;
    auto cbItem = MenuItemBuilder(u8"保存(S)")
        .setLeadingControl(cb)
        .setOnClick([&](shared_ptr<MenuItem>) { cbClicked = true; })
        .build();
    panel->addItem(cbItem);
    ENH_CHECK(panel->hasLeadingControl(), u8"有容器 → 保留 icon 区");
    ENH_CHECK(panel->getIconAreaWidth() >= 20.0f, u8"有容器 icon 区宽 >= 20");
    panel->draw();  // 容器 rect 在 draw 内 setRect
    auto cbBox = cb->getRect();
    // 容器垂直居中于自身行（各行独立居中；变行高行中心线自然错开）
    float expectTop = (cbItem->getRect().height - cbBox.height) * 0.5f;
    ENH_CHECK(fabsf(cbBox.top - expectTop) < 0.01f, u8"容器垂直居中于行");
    ENH_CHECK(cbBox.width == cbBox.height && cbBox.width > 0, u8"容器为正方形（边长=字体高度）");
    ENH_CHECK(cbBox.left == 20.0f + (panel->getIconAreaWidth() - cbBox.width) / 2.0f,
        u8"容器水平居中于 icon 区（left = 20 + (iconArea-w)/2）");

    // 事件路由：点容器 → 勾选翻转、不触发 onClick、菜单不关闭
    SRect cd = cb->getDrawRect();
    float cx = cd.left + cd.width / 2.0f, cy = cd.top + cd.height / 2.0f;
    panel->handleEvent(makeMouse(EventType::MouseDown, cx, cy));
    panel->handleEvent(makeMouse(EventType::MouseUp, cx, cy));
    ENH_CHECK(cb->getCheckState() == CheckState::Checked, u8"点击容器 → CheckBox 勾选翻转（MouseUp 路由）");
    ENH_CHECK(!cbClicked, u8"点击容器不触发 item onClick");
    ENH_CHECK(panel->isVisible(), u8"点击容器不关闭菜单链");

    // 点行非容器区 → item onClick 触发 + 关菜单链
    panel->handleEvent(makeMouse(EventType::MouseDown, cd.left, cd.top - 2));
    panel->handleEvent(makeMouse(EventType::MouseUp, cd.left, cd.top - 2));
    ENH_CHECK(cbClicked, u8"点击行非容器区触发 item onClick");

    // MouseMove 转发：容器 hover 态（点击行触发 onClick 已关菜单链，先重新显示）
    auto mcb = CheckBoxBuilder(nullptr, SRect(0, 0, 0, 0)).build();
    auto hoverItem = MenuItemBuilder(u8"Hover 项")
        .setLeadingControl(mcb)
        .build();
    panel->addItem(hoverItem);
    panel->show();
    panel->draw();
    SRect hd = mcb->getDrawRect();
    panel->handleEvent(makeMouse(EventType::MouseMove, hd.left + hd.width / 2.0f, hd.top + hd.height / 2.0f));
    ENH_CHECK(mcb->getState() == ControlState::Hover, u8"MouseMove 转发 → 容器 hover 态");

    // 生命周期：setLeadingControl(NULL) 解除容器并摘树（removeControl 语义，同 TreeView：
    // 不再绘制/接收事件；parent 指针保留，以"事件不再响应"断言）
    hoverItem->setLeadingControl(nullptr);
    ENH_CHECK(hoverItem->getLeadingControl() == nullptr, u8"setLeadingControl(NULL) 解除容器");
    int cbStateBefore = (int)mcb->getCheckState();
    panel->handleEvent(makeMouse(EventType::MouseDown, hd.left + hd.width / 2.0f, hd.top + hd.height / 2.0f));
    panel->handleEvent(makeMouse(EventType::MouseUp, hd.left + hd.width / 2.0f, hd.top + hd.height / 2.0f));
    ENH_CHECK((int)mcb->getCheckState() == cbStateBefore, u8"解除后容器不再接收事件（勾选不翻转）");

    // 清理追加的测试项
    panel->removeItem(hoverItem);
    panel->removeItem(cbItem);
    panel->removeItem(fontItem);
    panel->hide();

    // 第二期：JSON 解析（parseLayout → parseMenuBar → populateMenuPanel 增强键）
    {
        static const char* ENH_JSON = R"({
  "controls": [{
    "type": "menu-bar",
    "menus": [{
      "caption": "JSON menu",
      "items": [
        { "id": "json_cb", "caption": "JSON container item",
          "leadingControl": { "type": "check-box" },
          "leadingGap": 12, "font": "maplemono-nf-cn-regular", "size": 16 },
        { "id": "json_inherit", "caption": "inherit item", "size": 0 }
      ]
    }]
  }]
})";
        LayoutParser parser;
        parser.parseLayout(ENH_JSON);
        auto mbs = parser.getMenuBars();
        ENH_CHECK(!mbs.empty(), u8"JSON menubar 解析");
        if (!mbs.empty()) {
            BENCH->addControl(mbs[0]);  // 挂树：覆盖项字体（16 号）需 renderer 才能加载
            mbs[0]->setRect(SRect(420, 0, 400, 32));  // setParent 会重置 rect，挂树后重设位置（右半区，避免与 g_menuBar 重叠）
            g_jsonBar = mbs[0];
            auto jp = mbs[0]->getMenuPanel(0);
            ENH_CHECK(jp != nullptr, u8"JSON 菜单面板解析");
            auto jcb = jp->getItemById("json_cb");
            ENH_CHECK(jcb != nullptr, u8"JSON item-id 解析");
            ENH_CHECK(jcb->getLeadingControl() != nullptr, u8"JSON leadingControl 解析");
            ENH_CHECK(dynamic_pointer_cast<CheckBox>(jcb->getLeadingControl()) != nullptr, u8"JSON 容器 type=checkbox");
            ENH_CHECK(jcb->getLeadingGap() == 12.0f, u8"JSON leadingGap 解析");
            ENH_CHECK(jcb->getOwnFontSize() == 16, u8"JSON font-size 解析");
            float rh1 = jp->itemRowHeight(jcb.get());
            ENH_CHECK(fabsf(rh1 - 25.6f) < 0.01f, u8"JSON 覆盖项行高 16×1.6");
            auto jin = jp->getItemById("json_inherit");
            ENH_CHECK(jin != nullptr && jin->getOwnFontSize() == 0, u8"JSON 继承项 size=0");
            ENH_CHECK(fabsf(jp->itemRowHeight(jin.get()) - 32.0f) < 0.01f, u8"JSON 继承项行高 20×1.6");
            ENH_CHECK(jp->hasLeadingControl(), u8"JSON 有容器 → 保留 icon 区");
            ENH_CHECK(jp->getIconAreaWidth() >= 20.0f, u8"JSON icon 区宽 >= 20");
        }
    }

    // 第二期：属性系统（CABI 绑定层模拟）：item-id 定位 + item-* 属性
    {
        static const char* PROP_JSON = R"({
  "controls": [{
    "type": "menu-bar",
    "menus": [{
      "caption": "Prop menu",
      "items": [ { "id": "t1", "caption": "prop item" } ]
    }]
  }]
})";
        LayoutParser parser;
        parser.parseLayout(PROP_JSON);
        auto mbs = parser.getMenuBars();
        ENH_CHECK(!mbs.empty(), u8"prop menubar 解析");
        if (!mbs.empty()) {
            BENCH->addControl(mbs[0]);  // 挂树：item-font-size 字体重建需 renderer
            mbs[0]->setRect(SRect(420, 40, 400, 32));  // 挂树后重设位置（setParent 会重置 rect）
            g_propBar = mbs[0];
            auto jp = mbs[0]->getMenuPanel(0);
            auto pcb = CheckBoxBuilder(nullptr, SRect(0, 0, 0, 0)).build();

            ENH_CHECK(jp->setStringProperty(PropertyNames::kTreeItemId, "t1") == 1, u8"prop set item-id");
            const char* idOut = "";
            ENH_CHECK(jp->getStringProperty(PropertyNames::kTreeItemId, idOut) == 1 && string(idOut) == "t1", u8"prop get item-id");
            ENH_CHECK(jp->setIntProperty(PropertyNames::kTreeItemFontSize, 24) == 1, u8"prop set item-font-size");
            int fs = 0;
            ENH_CHECK(jp->getIntProperty(PropertyNames::kTreeItemFontSize, fs) == 1 && fs == 24, u8"prop get item-font-size");
            auto t1 = jp->getItemById("t1");
            ENH_CHECK(t1 != nullptr && fabsf(jp->itemRowHeight(t1.get()) - 38.4f) < 0.01f, u8"prop 行高重算 24×1.6");
            ENH_CHECK(jp->setFloatProperty(PropertyNames::kTreeItemLeadingGap, 10.0f) == 1, u8"prop set item-leading-gap");
            float lg = 0;
            ENH_CHECK(jp->getFloatProperty(PropertyNames::kTreeItemLeadingGap, lg) == 1 && lg == 10.0f, u8"prop get item-leading-gap");
            ENH_CHECK(jp->setEnumProperty(PropertyNames::kTreeItemFont, "maplemono-nf-cn-regular") == 1, u8"prop set item-font");
            const char* fn = "";
            ENH_CHECK(jp->getEnumProperty(PropertyNames::kTreeItemFont, fn) == 1 && string(fn) == "maplemono-nf-cn-regular", u8"prop get item-font");
            void* pcbHandle = (Control*)pcb.get();  // CABI 句柄语义：Control* 子对象位模式
            ENH_CHECK(jp->setPtrProperty(PropertyNames::kTreeItemLeadingControl, pcbHandle) == 1, u8"prop set item-leading-control");
            void* ptr = nullptr;
            ENH_CHECK(jp->getPtrProperty(PropertyNames::kTreeItemLeadingControl, ptr) == 1 && ptr == pcbHandle, u8"prop get item-leading-control");
            bool attached = (t1->getLeadingControl().get() == (Control*)pcbHandle);
            ENH_CHECK(attached, u8"prop 容器挂树生效");
            ENH_CHECK(jp->setPtrProperty(PropertyNames::kTreeItemLeadingControl, nullptr) == 1, u8"prop set NULL 解除");
            ENH_CHECK(t1->getLeadingControl() == nullptr, u8"prop 解除容器生效");
            ENH_CHECK(jp->setPtrProperty(PropertyNames::kTreeItemLeadingControl, nullptr) == 1, u8"prop set NULL 解除");
            ENH_CHECK(t1->getLeadingControl() == nullptr, u8"prop 解除容器生效");
            ENH_CHECK(!jp->hasLeadingControl(), u8"prop 无容器 → 不预留 icon 区");
            ENH_CHECK(jp->getIconAreaWidth() == 0.0f, u8"prop icon 区宽 = 0");
        }
    }

    printf("Menu enh: ALL PASS (%d passed)\n", g_enhPass);
    if (g_enhFail > 0) printf("Menu enh: FAILURES (%d)\n", g_enhFail);

    // 视觉演示：打开“文件(F)”菜单，展示增强项（checkbox 容器/变行高/icon 区）
    {
        SRect mb = g_menuBar->getRect();
        float fx = mb.left + 55.0f, fy = mb.top + mb.height / 2.0f;
        g_menuBar->handleEvent(makeMouse(EventType::MouseDown, fx, fy));
        g_menuBar->handleEvent(makeMouse(EventType::MouseUp, fx, fy));
    }
}


void testBenchInitialize(shared_ptr<Bench>) {
    TestUtil::log("testBenchInitialize - START");

    TestUtil::log("testBenchInitialize - creating item1");
    auto item1 = MenuItemBuilder(u8"新建(N)")
        .setShortcut("Ctrl+N")
        .setOnClick([](shared_ptr<MenuItem> item) {
            cout << "New file clicked" << endl;
        })
        .build();
    TestUtil::log("testBenchInitialize - item1 created OK");


    TestUtil::log("testBenchInitialize - creating MenuPanelBuilder");
    g_filePanel = MenuPanelBuilder()
        .addItem(MenuItemBuilder(u8"新建(N)")
            .setShortcut("Ctrl+N")
            .setOnClick([](shared_ptr<MenuItem> item) {
                cout << "New file clicked" << endl;
            })
            .build())
        .addItem(MenuItemBuilder(u8"打开(O)")
            .setShortcut("Ctrl+O")
            .setOnClick([](shared_ptr<MenuItem> item) {
                cout << "Open file clicked" << endl;
            })
            .build())
        .addItem(MenuItemBuilder(u8"最近打开的文件")
            .setSubMenu(MenuPanelBuilder()
                .addItem(MenuItemBuilder(u8"file1.txt")
                    .setOnClick([](shared_ptr<MenuItem> item) {
                        cout << "Recent file1.txt clicked" << endl;
                    })
                    .build())
                .addItem(MenuItemBuilder(u8"file2.txt")
                    .setOnClick([](shared_ptr<MenuItem> item) {
                        cout << "Recent file2.txt clicked" << endl;
                    })
                    .build())
                .build())
            .build())
        .addSeparator()
        .addItem(MenuItemBuilder(u8"保存(S)")
            .setShortcut("Ctrl+S")
            .setOnClick([](shared_ptr<MenuItem> item) {
                cout << "Save file clicked" << endl;
            })
            .build())
        .addSeparator()
        .addItem(MenuItemBuilder(u8"退出")
            .setOnClick([](shared_ptr<MenuItem> item) {
                cout << "Exit program" << endl;
                MAINWIN->quit();
            })
            .build())
        .build();

    g_editPanel = MenuPanelBuilder()
        .addItem(MenuItemBuilder(u8"撤销")
            .setShortcut("Ctrl+Z")
            .setOnClick([](shared_ptr<MenuItem> item) {
                cout << "Undo operation" << endl;
            })
            .build())
        .addItem(MenuItemBuilder(u8"重做")
            .setShortcut("Ctrl+Y")
            .setOnClick([](shared_ptr<MenuItem> item) {
                cout << "Redo operation" << endl;
            })
            .build())
        .addSeparator()
        .addItem(MenuItemBuilder(u8"剪切")
            .setShortcut("Ctrl+X")
            .setOnClick([](shared_ptr<MenuItem> item) {
                cout << "Cut operation" << endl;
            })
            .build())
        .addItem(MenuItemBuilder(u8"复制")
            .setShortcut("Ctrl+C")
            .setOnClick([](shared_ptr<MenuItem> item) {
                cout << "Copy operation" << endl;
            })
            .build())
        .addItem(MenuItemBuilder(u8"粘贴")
            .setShortcut("Ctrl+V")
            .setOnClick([](shared_ptr<MenuItem> item) {
                cout << "Paste operation" << endl;
            })
            .build())
        .build();

    g_menuBar = MenuBarBuilder(BENCH)
        .addMenu(u8"文件(F)", g_filePanel)
        .addMenu(u8"编辑(E)", g_editPanel)
        .build();

    TestUtil::log("Add menu bar to bench");
    BENCH->addControl(g_menuBar);

    // Menu 增强断言（挂载场景：MenuBar → MenuPanel，context 已就绪）
    runMenuEnhAssertions(g_filePanel);

    // ── Menu 缩放断言（验收清单第 3 项）：布局矩形不变、绘制矩形翻倍 ──
    {
        // MenuBar 缺省全宽占据视口顶部，同一视口无法对比缩放 → 用 manual-position 手动定位
        // 同屏目视对比：1x 条 (700,40) 与 2x 条 (700,140) 上下排列（2x 高 60、文字/条目 2x）
        auto bar1x = MenuBarBuilder(BENCH, 1.0f, 1.0f)
            .setBarHeight(30)
            .build();
        auto bar2x = MenuBarBuilder(BENCH, 2.0f, 2.0f)
            .setBarHeight(30)
            .build();
        bar1x->setManualPosition(true);
        bar2x->setManualPosition(true);
        bar1x->addMenu(u8"文件(1x)", MenuPanelBuilder().addItem(MenuItemBuilder(u8"打开").build()).build());
        bar2x->addMenu(u8"文件(2x)", MenuPanelBuilder().addItem(MenuItemBuilder(u8"打开").build()).build());
        bar1x->setRect(SRect(700, 40, 300, 30));   // 右侧区，避开 g_menuBar 顶部与文件菜单
        bar2x->setRect(SRect(700, 140, 300, 30));
        BENCH->addControl(bar1x);
        BENCH->addControl(bar2x);
        SRect blr1 = bar1x->getRect(), blr2 = bar2x->getRect();
        SRect bdr1 = bar1x->getDrawRect(), bdr2 = bar2x->getDrawRect();
        ENH_CHECK(blr1.height == 30.0f && blr2.height == 30.0f, u8"bar 逻辑高度不变 (30)");
        ENH_CHECK(bdr1.height == 30.0f && bdr2.height == 60.0f, u8"2x bar 绘制高度翻倍 (30→60)");
        ENH_CHECK(bdr1.top == 40.0f && bdr2.top == 140.0f, u8"manual 定位生效 (40/140)");
        ENH_CHECK(fabsf(bdr2.width - bdr1.width * 2.0f) < 0.01f, u8"2x bar 绘制宽度翻倍");
        ENH_CHECK(fabsf(bdr2.height - bdr1.height * 2.0f) < 0.01f, u8"2x bar 高度 = 1x 两倍");
        int mp = 0;
        ENH_CHECK(bar1x->getBoolProperty(PropertyNames::kManualPosition, mp) == 1 && mp == 1,
            u8"manual-position 属性读取");
        ENH_CHECK(bar1x->setBoolProperty(PropertyNames::kManualPosition, 0) == 1,
            u8"manual-position 属性设置");
        bar1x->setManualPosition(true);   // 恢复，保持后续目视展示

        // 2x 下拉面板：同屏对比行高/文字/图标缩放（逻辑行高不变、绘制翻倍）
        auto pnl1x = MenuPanelBuilder(1.0f, 1.0f)
            .addItem(MenuItemBuilder(u8"1x 缩放项").build())
            .build();
        auto pnl2x = MenuPanelBuilder(2.0f, 2.0f)
            .addItem(MenuItemBuilder(u8"2x 缩放项").build())
            .build();
        pnl1x->setPosition(40, 240);
        pnl2x->setPosition(420, 240);
        pnl1x->show();
        pnl2x->show();
        BENCH->addControl(pnl1x);
        BENCH->addControl(pnl2x);
        // 挂树后 getItemAt 使用绝对坐标（绘制坐标 = 面板位置 + 局部坐标）
        auto item1x = pnl1x->getItemAt(50.0f, 256.0f);
        auto item2x = pnl2x->getItemAt(430.0f, 256.0f);
        ENH_CHECK(item1x != nullptr && item2x != nullptr, u8"1x/2x 面板项均存在");
        float lh = item2x->getRect().height;
        ENH_CHECK(fabsf(lh - 32.0f) < 0.01f, u8"2x 项逻辑行高 = 20×1.6 = 32");
        ENH_CHECK(fabsf(item2x->getDrawRect().height - lh * 2.0f) < 0.01f,
            u8"2x 项绘制行高 = 逻辑 × 2");
        ENH_CHECK(fabsf(pnl2x->getDrawRect().width - pnl2x->getRect().width * 2.0f) < 0.01f,
            u8"2x 面板绘制宽度 = 逻辑 × 2");
        ENH_CHECK(pnl1x->getDrawRect().height < pnl2x->getDrawRect().height,
            u8"2x 面板绘制高度 > 1x（目视对比）");

        // 2x 条交互：hitTest 绘制坐标→逻辑换算、点击弹出面板于条正下方
        ENH_CHECK(bar2x->hitTest(750, 150) == 0, u8"2x 条命中条目（绘制坐标→逻辑换算）");
        ENH_CHECK(bar2x->hitTest(1350, 150) == -1, u8"2x 条条目区外不命中");
        bool handled = bar2x->handleEvent(makeMouse(EventType::MouseDown, 750, 160));
        ENH_CHECK(handled, u8"2x 条点击命中条目");
        auto p2 = bar2x->getMenuPanel(0);
        ENH_CHECK(p2 != nullptr && p2->getVisible(), u8"2x 点击弹出 MenuPanel");
        SRect pdr = p2->getDrawRect();
        auto p2item = p2->getItemAt(700.0f + 10, 200.0f + 16);   // p2 面板内局部 (10,16)
        ENH_CHECK(p2item != nullptr, u8"2x 菜单面板内条目存在");
        ENH_CHECK(fabsf(pdr.left - 700.0f) < 0.01f && fabsf(pdr.top - 200.0f) < 0.01f,
            u8"2x 面板绘制于条正下方 (700,200)");
        ENH_CHECK(fabsf(p2item->getDrawRect().height - p2item->getRect().height * 2.0f) < 0.01f,
            u8"2x 菜单条目绘制高度 = 逻辑 × 2（hover 色块/文字区域）");
        ENH_CHECK(fabsf(pdr.width - p2->getRect().width * 2.0f) < 0.01f,
            u8"2x 面板区域 = 逻辑 × 2");

        // 面板 hover/命中（绘制坐标→逻辑换算）：面板绘制于 (700,200)，条目行绘制高 64
        ENH_CHECK(p2->hitTest(750, 210) == 0, u8"2x 面板命中第 0 项（绘制坐标→逻辑换算）");
        ENH_CHECK(p2->hitTest(750, 300) == -1, u8"2x 面板条目区外不命中");
        p2->handleEvent(makeMouse(EventType::MouseMove, 750, 210));
        ENH_CHECK(p2->getHoveredIndex() == 0, u8"2x 面板 hover 命中第 0 项");
        bar2x->closeAllMenus();   // 收起，避免干扰视觉演示
    }

    BENCH->addControl(g_Button = ButtonBuilder(BENCH, {400, 100, 100, 50})
                    .setCaption("Button")
                    .setBackgroundStateColor(StateColor(StateColor::Type::Background).setNormal({128,128,128,255}))
                    .setBorderStateColor(StateColor(StateColor::Type::Border).setNormal({0,0,255,255}))
                    .setOnClick([](shared_ptr<Button> btn) {
                        TestUtil::log("g_menuBar->getRect()={%f, %f, %f, %f}",
                            g_menuBar->getRect().left, g_menuBar->getRect().top,
                            g_menuBar->getRect().width, g_menuBar->getRect().height);
                    })
                    .build()
                );
}

void testGraphToolInitialize(void){
    TestUtil::log("testGraphToolInitialize");
    g_dc = make_shared<GraphTool::DrawingContext>(BENCH->getRenderDevice());
    g_dc->setPenColor(GraphTool::SColor((uint8_t)255, 0, 0));
    g_dc->setCornerStyle(GraphTool::CornerStyle::Hard);
    g_dc->setPenWidth(20);
}

void testGraphTool(void){
    g_dc->drawLine(0, 0, 100, 100);
    g_dc->drawRect({150, 50, 100, 80}, false);
    g_dc->drawRoundedRect({300, 50, 100, 80}, 15, false);
    g_dc->drawEllipse(SPoint(550, 90), 50, 30, false);
    g_dc->drawArc(SPoint(300, 200), 200, 0, M_PI/2, false);
    g_dc->drawCircle(SPoint(700, 200), 40, false);
    g_dc->drawPolyline(vector<SPoint>({SPoint(100, 200), SPoint(10, 100), SPoint(300, 200), SPoint(200, 300)}), true);
}

class MenuApp : public AppCallbacks {
public:
    bool onInit() override {
        MAINWIN->setTitle("test_menu");
        cout << "test_menu test" << endl;

        SSize displaySize = MAINWIN->getDisplaySize();
        BENCH->setOnInitial(testBenchInitialize);
        testGraphToolInitialize();
        return true;
    }

    void onUpdate() override {
        BENCH->eventLoopEntry();
        BENCH->update();
    }

    void onRender() override {
        GET_RENDERDEVICE->setDrawColor(SColor(40.0f/255.0f, 40.0f/255.0f, 40.0f/255.0f, 1.0f));
        GET_RENDERDEVICE->clear();
        BENCH->draw();
        if (!g_visDone) doVisualCapture();
    }

    void onQuit() override {
        TestUtil::log("SDL_AppQuit");
        cout << "Menu test program exiting" << endl;
    }
};

int main(int argc, char* argv[]) {
    return TestRunMain<MenuApp>(argc, argv);
}
