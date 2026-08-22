// ============================================================================
// test_listview.cpp -- ListView 列表控件测试
// 断言：数据模型（补足/列对齐反例/同步迁移）/ 选择钳制 / 排序跟随 / 四层属性回环 / CABI 抽查
// 可视化：multi 三列表（列头/排序/网格线/选中） + single 单列（ListBox 替代视觉）
// ============================================================================
#include <iostream>
#include <memory>
#include <cmath>
#include "ListView.h"
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

static shared_ptr<ListView> g_probe;   // 数据模型断言探针（不挂树）

static void runAssertions() {
    TestUtil::log("---- ListView assertions ----");

    // ── 列定义 ──
    g_probe->addColumn(u8"名称", 160.f, true);
    g_probe->addColumn(u8"类型", 90.f);
    g_probe->addColumn(u8"大小", 70.f, true);
    CHECK(g_probe->getColumnCount() == 3, "addColumn x3");

    // ── 行补足语义（§5.0.3）：不足补空 ──
    g_probe->addRow("r1", {"A"});
    CHECK(g_probe->getRowCells(0).size() == 3 && g_probe->getCell(0, 1).empty(),
          "addRow pads cells to column count");

    // ── 反例核对：["C"] 落 A 列（位置映射），不是 C 列 ──
    g_probe->addRow("r2", {"C"});
    auto colA = g_probe->getColumnValues(0);
    CHECK(colA.size() == 2 && colA[1] == "C", "[\"C\"] lands in column A (position mapping)");
    auto colC = g_probe->getColumnValues(2);
    CHECK(colC.size() == 2 && colC[0].empty() && colC[1].empty(),
          "getColumnValues(C) empty for tail-omitted rows");

    // ── getColumnValues 恒 = 行数（对齐语义）──
    g_probe->setCell(1, 2, "512 B");
    CHECK(g_probe->getColumnValues(2).size() == 2 &&
          g_probe->getColumnValues(2)[1] == "512 B", "getColumnValues aligned to row count");

    // ── setCell 越界自动扩 ──
    g_probe->setCell(0, 5, "X");
    CHECK(g_probe->getCell(0, 4).empty() && g_probe->getCell(0, 5) == "X",
          "setCell beyond columns auto-expands row");

    // ── insertColumn 同步迁移 cellControls；removeColumn 还原 ──
    auto cb = make_shared<Label>(nullptr, SRect(0, 0, 40, 20));
    g_probe->setCellLeadingControl(0, 1, cb);
    CHECK(g_probe->getCellLeadingControl(0, 1) == cb, "setCellLeadingControl");
    g_probe->insertColumn(0, u8"插入", 60.f);
    CHECK(g_probe->getColumnCount() == 4 && g_probe->getRowCount() == 2,
          "insertColumn shifts all rows");
    CHECK(g_probe->getCellLeadingControl(0, 2) == cb,
          "cellControls migrate col1 -> col2 after insertColumn(0)");
    g_probe->removeColumn(0);
    CHECK(g_probe->getColumnCount() == 3 && g_probe->getCellLeadingControl(0, 1) == cb,
          "removeColumn restores & migrates back");

    // ── removeRow 后选中钳制 ──
    g_probe->setSelectedRow(1);
    CHECK(g_probe->getSelectedRow() == 1, "setSelectedRow");
    g_probe->removeRow(1);
    CHECK(g_probe->getSelectedRow() == -1, "selection clamped after removeRow");

    // ── 排序：字典序升序 + 稳定 + 选中按 id 跟随 ──
    g_probe->addRow("aaa", {"aaa", "x", "1"});
    g_probe->addRow("ccc", {"ccc", "y", "3"});
    const int n = g_probe->getRowCount();                 // r1, aaa, ccc → 排序后 aaa,r1,ccc
    g_probe->setSelectedRow(n - 1);                       // 选中 "ccc"
    g_probe->sortByColumn(0, true);
    bool sortedAsc = true;
    for (int i = 1; i < n; ++i)
        if (g_probe->getRowCells(i)[0] < g_probe->getRowCells(i - 1)[0]) sortedAsc = false;
    CHECK(sortedAsc, "sortByColumn lexicographic ascending");
    CHECK(g_probe->getRowCells(g_probe->getSelectedRow())[0] == "ccc",
          "selection follows row by id after sort");

    // ── 自定义排序回调（数值比较降序）──
    g_probe->setColumnSorter(2, [](const string& a, const string& b) {
        return atof(a.c_str()) < atof(b.c_str());
    });
    g_probe->sortByColumn(2, false);
    CHECK(atof(g_probe->getRowCells(0)[2].c_str()) >=
          atof(g_probe->getRowCells(n - 1)[2].c_str()), "custom numeric comparator descending");
    g_probe->clearColumnSorter(2);

    // ── 四层属性回环 ──
    CHECK(g_probe->setEnumProperty("mode", "single") == 1 &&
          g_probe->getMode() == ListView::Mode::Single, "SetEnum(mode)=single");
    g_probe->setEnumProperty("mode", "multi");
    g_probe->setBoolProperty("gridlines", 0);
    int bi = 1;
    CHECK(g_probe->getBoolProperty("gridlines", bi) == 1 && bi == 0, "gridlines roundtrip false");
    g_probe->setFloatProperty("row-height", 30.f);
    float f = 0.f;
    CHECK(g_probe->getFloatProperty("row-height", f) == 1 && f == 30.f, "row-height roundtrip 30");
    g_probe->setIntProperty("selected-index", 0);
    int ii = -2;
    CHECK(g_probe->getIntProperty("selected-index", ii) == 1 && ii == 0, "selected-index roundtrip 0");

    TestUtil::log("---- assertions done: pass=%d fail=%d ----", g_pass, g_fail);
}

static void addCaption(Bench* bench, int x, int y, const char* txt) {
    auto lbl = LabelBuilder(nullptr, SRect(x, y, 320, 22)).setCaption(txt).build();
    lbl->create();
    bench->addControl(lbl);
}

// ── C ABI 抽查（g_uiInstance 有效期内的可视化阶段执行）──
static void runCabiChecks() {
    TestUtil::log("---- ListView CABI checks ----");
    UIControlHandle h = UICornerstone_CreateListView(
        g_uiInstance, 700.f, 60.f, 300.f, 150.f, 1.f, 1.f);
    CHECK(h != nullptr, "CreateListView");

    const char* cols[] = {u8"名称", u8"大小"};
    CHECK(UICornerstone_ListViewAddColumn(g_uiInstance, h, cols[0], 160.f, 1) == 1 &&
          UICornerstone_ListViewAddColumn(g_uiInstance, h, cols[1], 90.f, 0) == 1,
          "ListViewAddColumn x2");

    const char* cells0[] = {"main.cpp", "2.1 KB"};
    const char* cells1[] = {"build.bat", "512 B"};
    CHECK(UICornerstone_ListViewAddRow(g_uiInstance, h, "f1", 2, cells0) == 1 &&
          UICornerstone_ListViewAddRow(g_uiInstance, h, "f2", 2, cells1) == 1,
          "ListViewAddRow x2");

    char buf[128] = "";
    CHECK(UICornerstone_ListViewGetCellText(g_uiInstance, h, 0, 0, buf, sizeof(buf)) == 1 &&
          strcmp(buf, "main.cpp") == 0, "ListViewGetCellText roundtrip");

    CHECK(UICornerstone_ListViewSetCellText(g_uiInstance, h, 1, 1, "1 KB") == 1 &&
          UICornerstone_ListViewGetCellText(g_uiInstance, h, 1, 1, buf, sizeof(buf)) == 1 &&
          strcmp(buf, "1 KB") == 0, "ListViewSetCellText roundtrip");

    CHECK(UICornerstone_ListViewSetColumnWidth(g_uiInstance, h, 0, 200.f) == 1,
          "ListViewSetColumnWidth");

    // 非 list-view 句柄拒绝
    CHECK(UICornerstone_ListViewAddRow(g_uiInstance, nullptr, "x", 0, nullptr) == 0,
          "null handle rejected");

    TestUtil::log("---- CABI checks done: pass=%d fail=%d ----", g_pass, g_fail);
}

// ── 可视化矩阵 ──
static void testListViewVisualize(Bench* bench) {
    g_probe = make_shared<ListView>(nullptr, SRect(0, 0, 100, 100));   // 断言探针不挂树
    runAssertions();

    addCaption(bench, 30, 30, "multi (Report): header/sort/gridlines/selection");
    {
        auto lv = make_shared<ListView>(nullptr, SRect(30, 60, 420, 240));
        lv->addColumn(u8"名称", 180.f, true);
        lv->addColumn(u8"类型", 120.f);
        lv->addColumn(u8"大小", 90.f, true);
        lv->addRow("f1", {"main.cpp", u8"C++ 源文件", "2.1 KB"});
        lv->addRow("f2", {"build.bat", u8"批处理", "512 B"});
        lv->addRow("f3", {"logo.svg", u8"矢量图", "8 KB"});
        lv->addRow("f4", {"readme.md", u8"文档", "3 KB"});
        lv->addRow("f5", {"app.exe", u8"可执行", "96 KB"});
        lv->setSelectedRow(0);
        lv->create();
        bench->addControl(lv);
    }

    addCaption(bench, 480, 30, "single (= ListBox)");
    {
        auto lv = make_shared<ListView>(nullptr, SRect(480, 60, 220, 200));
        lv->setMode(ListView::Mode::Single);
        lv->addItem(u8"选项 一", u8"选项 一");
        lv->addItem(u8"选项 二", u8"选项 二");
        lv->addItem(u8"选项 三", u8"选项 三");
        lv->addItem(u8"选项 四", u8"选项 四");
        lv->setSelectedRow(1);
        lv->create();
        bench->addControl(lv);
    }

    runCabiChecks();
}

class ListViewApp : public AppCallbacks {
public:
    bool onInit() override {
        MAINWIN->setTitle("test_listview");
        BENCH->setOnInitial([](shared_ptr<Bench> b) { testListViewVisualize(b.get()); });
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
        // 视觉验证：帧内读回（Render 后、Present 前）
        if (++m_frames == 30 && !m_saved) {
            static uint8_t pixels[1400 * 900 * 4];
            int w = 0, h = 0;
            const int cap = UICornerstone_CaptureViewport(g_uiInstance, pixels, &w, &h);
            const std::string out = "Temp/listview_capture.bmp";
            const int saved = cap ? UICornerstone_SavePixelsToFile(pixels, w, h, out.c_str()) : 0;
            TestUtil::log("capture: cap=%d saved=%d (%dx%d) -> %s", cap, saved, w, h,
                          saved ? out.c_str() : "FAILED");
            m_saved = true;
        }
    }
    void onQuit() override { TestUtil::log("ListView test quit"); }

private:
    int m_frames = 0;
    bool m_saved = false;
};

int main(int argc, char* argv[]) {
    return TestRunMain<ListViewApp>(argc, argv);
}
