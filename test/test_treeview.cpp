#include <iostream>
#include <filesystem>
#include <memory>
#include "TreeView.h"
#include "Panel.h"
#include "Label.h"
#include "Button.h"
#include "CheckBox.h"
#include "Actor.h"
#include "MainWindow.h"
#include "Bench.h"
#include "AppCallbacks.h"
#include "LayoutParser.h"
#include "PlatformUtils.h"
#include "TestUtils.h"
#include "TestInstance.h"
#include "StateMachine.h"

using namespace std;

shared_ptr<Panel> g_rootPanel;
shared_ptr<TreeView> g_treeView;
shared_ptr<Label> g_statusLabel;
shared_ptr<TreeView> g_treeView2x;
shared_ptr<TreeView> g_treeViewCollapsed;
shared_ptr<TreeView> g_treeViewStyled;
shared_ptr<TreeView> g_treeViewEnhanced;
shared_ptr<Label> g_dataLabel;

int g_selectCount = 0;
int g_expandCount = 0;
int g_collapseCount = 0;
string g_lastSelectedId;
int g_clearNodeCount = 0;
int g_enhancedCheckCount = 0;

void onEnhancedCheckChanged(shared_ptr<CheckBox>, CheckState, CheckState) {
    g_enhancedCheckCount++;
}

void onTreeSelect(shared_ptr<TreeView>, const string& nodeId) {
    g_selectCount++;
    g_lastSelectedId = nodeId;
    char buf[128];
    snprintf(buf, sizeof(buf), "Selected: %s (count: %d)", nodeId.c_str(), g_selectCount);
    if (g_statusLabel) g_statusLabel->setCaption(buf);
    cout << buf << endl;
}

void onTreeExpand(shared_ptr<TreeView>, const string& nodeId) {
    g_expandCount++;
    cout << "Expanded: " << nodeId << endl;
}

void onTreeCollapse(shared_ptr<TreeView>, const string& nodeId) {
    g_collapseCount++;
    cout << "Collapsed: " << nodeId << endl;
}

void onTreeSelectData(shared_ptr<TreeView>, const string& nodeId, void* userData) {
    char buf[256];
    auto* s = static_cast<const string*>(userData);
    const char* data = s ? s->c_str() : "(null)";
    snprintf(buf, sizeof(buf), "Styled select: %s, data: %s", nodeId.c_str(), data);
    if (g_dataLabel) g_dataLabel->setCaption(buf);
    cout << buf << endl;
}

void onClearNode(shared_ptr<TreeView>, void* userData) {
    g_clearNodeCount++;
    const auto* s = static_cast<const string*>(userData);
    const char* data = s ? s->c_str() : "(null)";
    cout << "ClearNode called, userData: " << data << endl;
    delete s;
}

void initTestTree() {
    auto subChild2_1 = makeNode("btn1", "Button (btn1)");
    auto subChild2_2 = makeNode("subPnl2", "Panel (subPanel2)", false,
        { makeNode("inner1", "Inner 1") });
    auto subPanel = makeNode("subPnl", "Sub Panel", true,
        { subChild2_1, subChild2_2 });
    auto lbl1 = makeNode("lbl1", "Label (lbl1)");
    auto rootPanel = makeNode("rootPnl", "Root Panel", false,
        { subPanel, lbl1 });

    function<shared_ptr<TreeNode>(int)> deepChain;
    deepChain = [&](int level) -> shared_ptr<TreeNode> {
        if (level >= 20) return nullptr;
        auto child = deepChain(level + 1);
        vector<shared_ptr<TreeNode>> children;
        if (child) children.push_back(child);
        string label = "Level " + to_string(level);
        if (level < 3) {
            children.push_back(makeNode("sib_" + to_string(level), "Sibling " + to_string(level)));
        }
        return makeNode("deep_" + to_string(level), label, false, children);
    };
    auto deepRoot = deepChain(0);

    auto items = vector{ rootPanel, deepRoot };
    g_treeView->setItems(items);
}

void initTest2xTree() {
    auto items = g_treeView->getItems();
    vector<shared_ptr<TreeNode>> clonedItems;
    int tailIndex = 0;
    for (const auto& item : items) {
        auto cloned = cloneNode(item, "2x_", "2x ");
        int rootIdx = tailIndex;

        function<shared_ptr<TreeNode>(int)> deepTail;
        deepTail = [&](int level) -> shared_ptr<TreeNode> {
            if (level >= 8) return nullptr;
            auto child = deepTail(level + 1);
            vector<shared_ptr<TreeNode>> children;
            if (child) children.push_back(child);
            string label = "Lv" + to_string(level) + " long label for h-scroll test";
            return makeNode("2x_tail_" + to_string(rootIdx) + "_" + to_string(level), label, true, children);
        };
        cloned->children.push_back(deepTail(0));
        clonedItems.push_back(cloned);
        tailIndex++;
    }

    g_treeView2x->setItems(clonedItems);
}

void initTestCollapsed() {
    auto node = makeNode("coll_root", "Collapsed Root", false,
        { makeNode("coll_child", "Collapsed Child", false,
            { makeNode("coll_grand", "Collapsed Grandchild") }) });
    g_treeViewCollapsed->setDefaultExpand(false);
    g_treeViewCollapsed->setItems({ node });
}

void initTestStyled() {
    function<shared_ptr<TreeNode>(int)> deep;
    deep = [&](int level) -> shared_ptr<TreeNode> {
        if (level >= 5) return nullptr;
        auto child = deep(level + 1);
        vector<shared_ptr<TreeNode>> children;
        if (child) children.push_back(child);
        // Attach custom data (heap string) to each node
        auto node = makeNode("sty_" + to_string(level),
            "Sty " + to_string(level) + " (custom data)", true, children);
        string* data = new string("CustomData_Lv" + to_string(level));
        node->userData = data;
        return node;
    };
    vector<shared_ptr<TreeNode>> items;
    items.push_back(deep(0));

    // Additional root nodes with custom data
    for (int i = 0; i < 3; i++) {
        auto node = makeNode("sty_extra_" + to_string(i),
            "Styled Item " + to_string(i) + " with long label for horizontal scroll", false);
        string* data = new string("ExtraData_" + to_string(i));
        node->userData = data;
        items.push_back(node);
    }

    g_treeViewStyled->setItems(items);
}

// TreeView 增强（leadingControl + 逐节点字体）：演示 + 自动断言
void initTestEnhanced() {
    g_treeViewEnhanced = TreeViewBuilder(nullptr, SRect(280, 320, 300, 200))
        .setFontSize(12)
        .setOnSelect(onTreeSelect)
        .setId(104)
        .build();
    g_rootPanel->addControl(g_treeViewEnhanced);

    auto node1 = makeNode("enh_cb", "Checkbox Row");
    auto cb = CheckBoxBuilder(nullptr, SRect(0, 0, 16, 16))
        .setOnCheckChanged(onEnhancedCheckChanged)
        .build();
    node1->leadingControl = cb;

    auto node2 = makeNode("enh_big", "Big Font Row");
    node2->fontSize = 18;

    auto node3 = makeNode("enh_img", "Image Row");
    auto img = make_shared<Actor>(nullptr, fs::path("assets/images/cross_down.png"), false);
    node3->leadingControl = img;

    g_treeViewEnhanced->setItems({ node1, node2, node3 });

    int pass = 0, fail = 0;
    auto check = [&](bool ok, const char* name) {
        if (ok) { pass++; cout << "PASS: " << name << endl; }
        else { fail++; cout << "FAIL: " << name << endl; }
    };

    // 1. 行控件挂树（addControl 进 TreeView）
    check(cb->getParent() == g_treeViewEnhanced.get(), "enh row checkbox attached");
    check(img->getParent() == g_treeViewEnhanced.get(), "enh row image attached");

    // 先跑一帧 draw：刷新 m_frameDrawRect + 行控件 setRect（行高自适应）
    g_treeViewEnhanced->draw();

    // 5. 槽高 = 文字高度：图片与 CheckBox 等高、小于行高 24，行间留空隙；宽按 1:1 等比
    check(img->getTexture() != nullptr, "enh image texture loaded");
    check(img->getRect().height > 0 && img->getRect().height < 24.0f, "enh image height follows text height");
    check(img->getRect().width == img->getRect().height, "enh image scaled to text height");
    check(cb->getRect().height == img->getRect().height && cb->getRect().width == cb->getRect().height,
          "enh checkbox scaled to text height");
    // 6. 槽起点 = 原文本起点（LEFT_PADDING 4 + arrowGap 16 = 20），局部坐标非负（缩进不越界）
    check(img->getRect().left == 20.0f && cb->getRect().left == 20.0f,
          "enh slot aligned with original text indent");

    // 2. 点击 CheckBox 中心 → 选中该行 + 勾选翻转（MouseDown 不消费落子控件分发，MouseUp 触发翻转）
    SRect cbRect = cb->getDrawRect();
    float cx = cbRect.left + cbRect.width / 2;
    float cy = cbRect.top + cbRect.height / 2;
    auto down = make_shared<Event>(EventType::MouseDown);
    down->mouseButton = { cx, cy, MouseButton::Left };
    g_treeViewEnhanced->handleEvent(down);
    auto up = make_shared<Event>(EventType::MouseUp);
    up->mouseButton = { cx, cy, MouseButton::Left };
    g_treeViewEnhanced->handleEvent(up);
    check(g_lastSelectedId == "enh_cb", "enh click selects row");
    check(cb->getCheckState() == CheckState::Checked, "enh click toggles checkbox");
    check(g_enhancedCheckCount == 1, "enh check callback fired once");

    // 3. 移除节点 → 行控件摘除（事件不再响应：点击树内空白区不选中、不勾选）
    int selBefore = g_selectCount;
    int cbBefore = g_enhancedCheckCount;
    g_treeViewEnhanced->removeNode("enh_cb");
    auto down2 = make_shared<Event>(EventType::MouseDown);
    down2->mouseButton = { cx, cbRect.top + 150, MouseButton::Left };  // 树下方空白（第 3 行之后）
    auto up2 = make_shared<Event>(EventType::MouseUp);
    up2->mouseButton = { cx, cbRect.top + 150, MouseButton::Left };
    g_treeViewEnhanced->handleEvent(down2);
    g_treeViewEnhanced->handleEvent(up2);
    check(g_selectCount == selBefore, "enh detached row no longer selects");
    check(g_enhancedCheckCount == cbBefore, "enh detached row no longer toggles");

    // 4. cloneNode：leadingControl 置空 + 字体字段照常复制
    auto cloned = cloneNode(node1);
    check(cloned->leadingControl == nullptr, "enh cloneNode nulls leadingControl");
    check(cloned->fontSize == node1->fontSize && cloned->fontName == node1->fontName,
          "enh cloneNode copies font fields");

    cout << (fail ? "Enhanced assertions: FAILURES = " + to_string(fail)
                  : "Enhanced assertions: ALL PASS") << " (" << pass << " passed)" << endl;
}

// JSON 用例：TreeView item 增加前置控件容器（CheckBox/Image）+ 逐 Item 字体（粗体）+ leadingGap
static const char* ENH_JSON = R"({
  "controls": [{
    "type": "panel",
    "id": "rootJsonEnh",
    "rect": { "x": 620, "y": 10, "w": 170, "h": 180 },
    "children": [{
      "type": "tree-view",
      "id": "jsonTreeEnh",
      "rect": { "x": 10, "y": 10, "w": 150, "h": 160 },
      "items": [
        { "id": "j1", "label": "JSON CB row",
          "leadingControl": { "type": "check-box", "checkState": "checked" },
          "leadingGap": 10 },
        { "id": "j2", "label": "JSON img row",
          "leadingControl": { "type": "image", "image": "assets/images/cross_down.png" },
          "font": "harmonyos-sans-sc-bold", "size": 16 }
      ]
    }]
  }]
})";

void initTestJsonEnh(Bench* bench) {
    LayoutParser parser;
    auto root = parser.parseLayout(ENH_JSON);
    if (!root) {
        TestUtil::log("json enh: FAILED to parse ENH_JSON");
        return;
    }
    bench->addControl(root);

    auto tv = dynamic_pointer_cast<TreeView>(parser.findControlById("jsonTreeEnh"));
    int pass = 0, fail = 0;
    auto check = [&](bool cond, const char* name) {
        if (cond) { pass++; cout << "PASS: " << name << endl; }
        else      { fail++; cout << "FAIL: " << name << endl; }
    };
    check(tv != nullptr, "json enh treeview parsed");
    if (!tv) {
        cout << (fail ? "json enh: FAILURES = " + to_string(fail) : "json enh: ALL PASS")
             << " (" << pass << " passed)" << endl;
        return;
    }

    // item 级字段
    auto j1 = tv->findNodeById("j1");
    auto j2 = tv->findNodeById("j2");
    check(j1 != nullptr && j2 != nullptr, "json enh items found");
    check(j1 && j1->leadingGap == 10.0f, "json enh item leadingGap");
    check(j2 && j2->fontName == FontName::HarmonyOS_Sans_SC_Bold, "json enh item bold font");
    check(j2 && j2->fontSize == 16, "json enh item font size");

    // 前置控件容器：挂树 + 类型 + 勾选态
    check(j1 && j1->leadingControl != nullptr, "json enh j1 leadingControl attached");
    if (j1 && j1->leadingControl) {
        check(j1->leadingControl->getParent() == tv.get(), "json enh j1 leadingControl attached to tree");
        auto cb = dynamic_pointer_cast<CheckBox>(j1->leadingControl);
        check(cb != nullptr && cb->getCheckState() == CheckState::Checked, "json enh j1 checkbox checked");
    }
    check(j2 && j2->leadingControl != nullptr, "json enh j2 leadingControl attached");
    if (j2 && j2->leadingControl) {
        auto img = dynamic_pointer_cast<Actor>(j2->leadingControl);
        tv->draw();
        check(img != nullptr && img->getTexture() != nullptr, "json enh j2 image texture loaded");
    }

    // CABI item 级属性（item-id 定位 → item-leading-gap / item-font-size / item-font）
    check(tv->setStringProperty(PropertyNames::kTreeItemId, "j2"), "json enh item-id set");
    float gap = 0; check(tv->getFloatProperty(PropertyNames::kTreeItemLeadingGap, gap) && gap == 6.0f,
                         "json enh item-leading-gap default 6");
    check(tv->setFloatProperty(PropertyNames::kTreeItemLeadingGap, 14.0f) &&
          tv->getFloatProperty(PropertyNames::kTreeItemLeadingGap, gap) && gap == 14.0f,
          "json enh item-leading-gap set/get");
    check(j2 && j2->leadingGap == 14.0f, "json enh item-leading-gap applied to node");
    check(tv->setIntProperty(PropertyNames::kTreeItemFontSize, 18), "json enh item-font-size set");
    int fs = 0;
    check(tv->getIntProperty(PropertyNames::kTreeItemFontSize, fs) && fs == 18 && j2->fontSize == 18,
          "json enh item-font-size applied");

    cout << (fail ? "json enh: FAILURES = " + to_string(fail) : "json enh: ALL PASS")
         << " (" << pass << " passed)" << endl;
}

void testAppInitialize(shared_ptr<Bench>) {
    TestUtil::log("testTreeViewInitialize");

    g_rootPanel = make_shared<Panel>(nullptr, SRect(10, 10, 780, 560));
    g_rootPanel->setNormalStateBGColor(SColor(30, 30, 30, 255));
    g_rootPanel->create();
    BENCH->addControl(g_rootPanel);

    // Normal TreeView
    g_treeView = TreeViewBuilder(nullptr, SRect(10, 10, 250, 300))
        .setOnSelect(onTreeSelect)
        .setOnExpand(onTreeExpand)
        .setOnCollapse(onTreeCollapse)
        .setFontSize(12)
        .setId(100)
        .build();
    g_rootPanel->addControl(g_treeView);

    // 2x scale TreeView — same rect as 1x for visual comparison
    g_treeView2x = TreeViewBuilder(nullptr,
        SRect(280, 10, 250, 300), 2.0f, 2.0f)
        .setFontSize(12)
        .setId(101)
        .build();
    g_rootPanel->addControl(g_treeView2x);

    // Collapsed TreeView (defaultExpand=false)
    g_treeViewCollapsed = TreeViewBuilder(nullptr, SRect(10, 320, 250, 100))
        .setDefaultExpand(false)
        .setId(102)
        .build();
    g_rootPanel->addControl(g_treeViewCollapsed);

    // Styled TreeView — custom colors, spacing, arrow gap, custom data
    g_treeViewStyled = TreeViewBuilder(nullptr, SRect(10, 430, 250, 110))
        .setBgColor(SColor(20, 30, 40, 255))
        .setBorderColor(SColor(0, 120, 215, 255))
        .setHoverColor(SColor(40, 60, 80, 255))
        .setSelectedColor(SColor(0, 100, 180, 255))
        .setTextColor(SColor(200, 230, 255, 255))
        .setRowHeight(28)
        .setLineSpacing(4)
        .setArrowGap(24)
        .setIndentWidth(20)
        .setFontSize(12)
        .setOnSelectData(onTreeSelectData)
        .setOnClearNode(onClearNode)
        .setId(103)
        .build();
    g_rootPanel->addControl(g_treeViewStyled);

    // Status labels
    g_statusLabel = make_shared<Label>(nullptr, SRect(10, 550, 270, 20));
    g_statusLabel->setCaption("TreeView test — click nodes to select, arrows to expand/collapse");
    g_statusLabel->setFontSize(11);
    g_statusLabel->setTextNormalStateColor(SColor(180, 180, 180, 255));
    g_statusLabel->create();
    BENCH->addControl(g_statusLabel);

    g_dataLabel = make_shared<Label>(nullptr, SRect(10, 575, 270, 20));
    g_dataLabel->setCaption("Custom data display");
    g_dataLabel->setFontSize(11);
    g_dataLabel->setTextNormalStateColor(SColor(180, 220, 180, 255));
    g_dataLabel->create();
    BENCH->addControl(g_dataLabel);

    TestUtil::log("TreeView test controls created");

    // Initialize tree data
    initTestTree();
    initTest2xTree();
    initTestCollapsed();
    initTestStyled();
    initTestEnhanced();
    initTestJsonEnh(BENCH);

    TestUtil::log("TreeView test data initialized");
}

class TreeViewApp : public AppCallbacks {
public:
    bool onInit() override {
        MAINWIN->setTitle("test_treeview");
        BENCH->setOnInitial(testAppInitialize);
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
    }

    void onQuit() override {
        TestUtil::log("TreeView test quit");
        cout << "Select count: " << g_selectCount << endl;
        cout << "Expand count: " << g_expandCount << endl;
        cout << "Collapse count: " << g_collapseCount << endl;
        cout << "ClearNode count: " << g_clearNodeCount << endl;
    }
};

int main(int argc, char* argv[]) {
    return TestRunMain<TreeViewApp>(argc, argv);
}
