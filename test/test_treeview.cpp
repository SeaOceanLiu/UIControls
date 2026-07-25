#include <iostream>
#include <memory>
#include "TreeView.h"
#include "Panel.h"
#include "Label.h"
#include "Button.h"
#include "MainWindow.h"
#include "Bench.h"
#include "AppCallbacks.h"
#include "PlatformUtils.h"
#include "TestUtils.h"

using namespace std;

shared_ptr<Panel> g_rootPanel;
shared_ptr<TreeView> g_treeView;
shared_ptr<Label> g_statusLabel;
shared_ptr<TreeView> g_treeView2x;
shared_ptr<TreeView> g_treeViewCollapsed;
shared_ptr<TreeView> g_treeViewStyled;
shared_ptr<Label> g_dataLabel;

int g_selectCount = 0;
int g_expandCount = 0;
int g_collapseCount = 0;
string g_lastSelectedId;
int g_clearNodeCount = 0;

void onTreeSelect(const string& nodeId) {
    g_selectCount++;
    g_lastSelectedId = nodeId;
    char buf[128];
    snprintf(buf, sizeof(buf), "Selected: %s (count: %d)", nodeId.c_str(), g_selectCount);
    if (g_statusLabel) g_statusLabel->setCaption(buf);
    cout << buf << endl;
}

void onTreeExpand(const string& nodeId) {
    g_expandCount++;
    cout << "Expanded: " << nodeId << endl;
}

void onTreeCollapse(const string& nodeId) {
    g_collapseCount++;
    cout << "Collapsed: " << nodeId << endl;
}

void onTreeSelectData(const string& nodeId, void* userData) {
    char buf[256];
    const char* data = userData ? static_cast<const char*>(userData) : "(null)";
    snprintf(buf, sizeof(buf), "Styled select: %s, data: %s", nodeId.c_str(), data);
    if (g_dataLabel) g_dataLabel->setCaption(buf);
    cout << buf << endl;
}

void onClearNode(void* userData) {
    g_clearNodeCount++;
    const char* data = userData ? static_cast<const char*>(userData) : "(null)";
    cout << "ClearNode called, userData: " << data << endl;
    // Free the string allocated with strdup/new
    delete[] static_cast<const char*>(userData);
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
    for (const auto& item : items) {
        auto cloned = cloneNode(item, "2x_", "2x ");

        function<shared_ptr<TreeNode>(int)> deepTail;
        deepTail = [&](int level) -> shared_ptr<TreeNode> {
            if (level >= 8) return nullptr;
            auto child = deepTail(level + 1);
            vector<shared_ptr<TreeNode>> children;
            if (child) children.push_back(child);
            string label = "Lv" + to_string(level) + " long label for h-scroll test";
            return makeNode("2x_tail_" + to_string(level), label, true, children);
        };
        cloned->children.push_back(deepTail(0));
        clonedItems.push_back(cloned);
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

void testAppInitialize() {
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
    TreeViewApp app;
    return MAINWIN->run(&app);
}
