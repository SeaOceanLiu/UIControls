#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <algorithm>
#include <cassert>

using namespace std;

// ============================================================
// TreeNode structure (same as design doc)
// ============================================================
struct TreeNode {
    string id;
    string label;
    bool expanded = false;
    vector<shared_ptr<TreeNode>> children;
};

// ============================================================
// TreeView data mock (extracts relevant paths for measurement)
// ============================================================
struct TreeViewData {
    vector<shared_ptr<TreeNode>> m_rootItems;
    unordered_map<string, shared_ptr<TreeNode>> m_nodeMap;

    struct FlatRow {
        shared_ptr<TreeNode> node;
        int depth;
    };
    vector<FlatRow> m_flatRows;

    string m_selectedId;
    int m_selectedRow = -1;

    void rebuildFlatRowsWithMap() {
        m_flatRows.clear();
        m_nodeMap.clear();
        m_selectedRow = -1;
        function<void(const shared_ptr<TreeNode>&, int)> flatten;
        flatten = [&](const shared_ptr<TreeNode>& node, int depth) {
            m_nodeMap[node->id] = node;
            m_flatRows.push_back({node, depth});
            if (node->id == m_selectedId)
                m_selectedRow = (int)m_flatRows.size() - 1;
            if (node->expanded)
                for (auto& child : node->children)
                    flatten(child, depth + 1);
        };
        for (auto& root : m_rootItems)
            flatten(root, 0);
    }

    void rebuildFlatRowsOnly() {
        m_flatRows.clear();
        m_selectedRow = -1;
        function<void(const shared_ptr<TreeNode>&, int)> flatten;
        flatten = [&](const shared_ptr<TreeNode>& node, int depth) {
            m_flatRows.push_back({node, depth});
            if (node->id == m_selectedId)
                m_selectedRow = (int)m_flatRows.size() - 1;
            if (node->expanded)
                for (auto& child : node->children)
                    flatten(child, depth + 1);
        };
        for (auto& root : m_rootItems)
            flatten(root, 0);
    }

    shared_ptr<TreeNode> findNodeByIdDFS(const string& id) {
        shared_ptr<TreeNode> result;
        function<void(const shared_ptr<TreeNode>&)> search;
        search = [&](const shared_ptr<TreeNode>& node) {
            if (result) return;
            if (node->id == id) { result = node; return; }
            for (auto& child : node->children) search(child);
        };
        for (auto& root : m_rootItems) search(root);
        return result;
    }

    shared_ptr<TreeNode> findNodeByIdMap(const string& id) {
        auto it = m_nodeMap.find(id);
        return it != m_nodeMap.end() ? it->second : nullptr;
    }
};

// ============================================================
// Helpers
// ============================================================
string makeId(int prefix, int index) {
    return "node_" + to_string(prefix) + "_" + to_string(index);
}

string makeLabel(int prefix, int index) {
    return "Node " + to_string(prefix) + "." + to_string(index);
}

// Current process memory in KB
size_t getMemoryKB() {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return pmc.WorkingSetSize / 1024;
    return 0;
}

// ============================================================
// Build a tree: 100k nodes, depth ~10, fanout ~10
// ============================================================
void buildTree(TreeViewData& data, int totalNodes) {
    // Build a balanced tree
    // Root: node_0_0
    // Level 1: node_1_0 ... node_1_9 (10 children)
    // Level 2: each has 10 children, etc.
    // Depth ~log10(totalNodes)
    int counter = 0;
    function<shared_ptr<TreeNode>(int, int, int)> createTree;
    createTree = [&](int depth, int maxDepth, int prefix) -> shared_ptr<TreeNode> {
        if (counter >= totalNodes) return nullptr;
        auto node = make_shared<TreeNode>();
        node->id = makeId(prefix, counter);
        node->label = makeLabel(prefix, counter);
        node->expanded = (depth < maxDepth);  // only top levels expanded
        counter++;

        if (depth < maxDepth && counter < totalNodes) {
            int childrenPerNode = max(1, (totalNodes - counter) / (maxDepth - depth + 1));
            childrenPerNode = min(childrenPerNode, 12);
            for (int i = 0; i < childrenPerNode && counter < totalNodes; i++) {
                auto child = createTree(depth + 1, maxDepth, prefix * 10 + i + 1);
                if (child) node->children.push_back(child);
            }
        }
        return node;
    };

    int maxDepth = (int)log10(totalNodes) + 1;
    auto root = createTree(0, maxDepth, 0);
    if (root) data.m_rootItems.push_back(root);
}

// ============================================================
// Benchmark
// ============================================================
int main() {
    const int NODES[] = { 1000, 10000, 100000 };
    const int MAX_ITERATIONS = 100;

    cout << "==============================================" << endl;
    cout << "  TreeView Performance & Memory Benchmark" << endl;
    cout << "==============================================" << endl << endl;

    for (int ni = 0; ni < 3; ni++) {
        int totalNodes = NODES[ni];
        cout << "--- " << totalNodes << " nodes ---" << endl;

        // Build tree
        TreeViewData data;
        buildTree(data, totalNodes);

        // Memory before rebuild
        size_t memBefore = getMemoryKB();
        data.rebuildFlatRowsWithMap();
        size_t memAfter = getMemoryKB();

        size_t flatCount = data.m_flatRows.size();
        size_t memUsed = memAfter > memBefore ? memAfter - memBefore : 0;
        cout << "  FlatRows (expanded): " << flatCount << endl;
        cout << "  Memory used:         " << memUsed << " KB ("
             << (memUsed * 1024 / max(1, totalNodes)) << " bytes/node)" << endl;

        // ---- rebuildFlatRows timing ----
        auto t0 = chrono::high_resolution_clock::now();
        for (int i = 0; i < MAX_ITERATIONS; i++) {
            data.rebuildFlatRowsWithMap();
        }
        auto t1 = chrono::high_resolution_clock::now();
        double rebuildMs = chrono::duration<double, milli>(t1 - t0).count() / MAX_ITERATIONS;
        cout << "  rebuildFlatRows+map: " << rebuildMs << " ms" << endl;

        // ---- rebuildFlatRows without map for comparison ----
        auto t2 = chrono::high_resolution_clock::now();
        for (int i = 0; i < MAX_ITERATIONS; i++) {
            data.rebuildFlatRowsOnly();
        }
        auto t3 = chrono::high_resolution_clock::now();
        double rebuildOnlyMs = chrono::duration<double, milli>(t3 - t2).count() / MAX_ITERATIONS;
        cout << "  rebuildFlatRows only: " << rebuildOnlyMs << " ms" << endl;
        cout << "  map overhead:          " << (rebuildMs - rebuildOnlyMs) << " ms" << endl;

        // ---- findNodeById: DFS vs Map ----
        const int LOOKUPS = 1000;

        // DFS
        auto t4 = chrono::high_resolution_clock::now();
        for (int i = 0; i < LOOKUPS; i++) {
            string id = makeId(0, i * (totalNodes / LOOKUPS));
            volatile auto result = data.findNodeByIdDFS(id);
            (void)result;
        }
        auto t5 = chrono::high_resolution_clock::now();
        double dfsTime = chrono::duration<double, milli>(t5 - t4).count();

        // Map
        auto t6 = chrono::high_resolution_clock::now();
        for (int i = 0; i < LOOKUPS; i++) {
            string id = makeId(0, i * (totalNodes / LOOKUPS));
            volatile auto result = data.findNodeByIdMap(id);
            (void)result;
        }
        auto t7 = chrono::high_resolution_clock::now();
        double mapTime = chrono::duration<double, milli>(t7 - t6).count();

        cout << "  findNodeById x" << LOOKUPS << ":" << endl;
        cout << "    DFS:    " << dfsTime << " ms ("
             << (dfsTime / LOOKUPS * 1000) << " us/lookup)" << endl;
        cout << "    Map:    " << mapTime << " ms ("
             << (mapTime / LOOKUPS * 1000) << " us/lookup)" << endl;
        cout << "    speedup: " << (dfsTime / max(0.001, mapTime)) << "x" << endl;

        // ---- selectNode: valid vs invalid id ----
        // Valid
        string validId = makeId(0, totalNodes / 2);
        auto t8 = chrono::high_resolution_clock::now();
        for (int i = 0; i < MAX_ITERATIONS; i++) {
            data.m_selectedId = validId;
            data.rebuildFlatRowsWithMap();
        }
        auto t9 = chrono::high_resolution_clock::now();
        double validSelect = chrono::duration<double, milli>(t9 - t8).count() / MAX_ITERATIONS;

        // Invalid (map allows O(1) rejection)
        string invalidId = "nonexistent";
        auto t10 = chrono::high_resolution_clock::now();
        for (int i = 0; i < MAX_ITERATIONS; i++) {
            if (data.m_nodeMap.find(invalidId) != data.m_nodeMap.end()) {
                data.m_selectedId = invalidId;
                data.rebuildFlatRowsWithMap();
            }
        }
        auto t11 = chrono::high_resolution_clock::now();
        double invalidReject = chrono::duration<double, milli>(t11 - t10).count() / MAX_ITERATIONS;

        cout << "  selectNode (valid):   " << validSelect << " ms" << endl;
        cout << "  selectNode (invalid): " << invalidReject << " ms (O(1) reject)" << endl;

        cout << endl;
    }

    // ---- draw() simulation: O(N) scan vs O(1) jump ----
    cout << "--- draw() simulation ---" << endl;
    int viewRows = 20;
    int flatRows = 100000;
    int scrollPos = 95000;  // near end

    // O(N) scan: iterate from 0 until break
    auto td0 = chrono::high_resolution_clock::now();
    for (int iter = 0; iter < 1000; iter++) {
        for (int i = 0; i < flatRows; i++) {
            float y = i * 24.0f - scrollPos;
            if (y + 24 < 0) continue;
            if (y > viewRows * 24) break;
        }
    }
    auto td1 = chrono::high_resolution_clock::now();
    double scanMs = chrono::duration<double, milli>(td1 - td0).count() / 1000;

    // O(1) jump: calculate firstVisible directly
    auto td2 = chrono::high_resolution_clock::now();
    for (int iter = 0; iter < 1000; iter++) {
        int firstVisible = max(0, scrollPos / 24);
        for (int i = firstVisible; i < flatRows; i++) {
            float y = (i - firstVisible) * 24.0f;
            if (y > viewRows * 24) break;
        }
    }
    auto td3 = chrono::high_resolution_clock::now();
    double jumpMs = chrono::duration<double, milli>(td3 - td2).count() / 1000;

    cout << "  FlatRows=100k, scroll at 95000, view=20 rows" << endl;
    cout << "  O(N) scan:     " << scanMs * 1000 << " us/draw" << endl;
    cout << "  O(1) jump:     " << jumpMs * 1000 << " us/draw" << endl;
    cout << "  speedup:       " << (scanMs / max(0.0001, jumpMs)) << "x" << endl;

    cout << endl << "Benchmark complete." << endl;
    return 0;
}
