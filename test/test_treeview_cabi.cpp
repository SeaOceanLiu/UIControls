// =========================================================================
// test_treeview_cabi.cpp -- C ABI integration test for TreeView
// Tests: deep hierarchy, scrollbars, selection display with custom data
// Build from build/{backend}_dll/, output to build/{backend}_dll/test/Debug/
// =========================================================================

#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "../../include/UICornerstoneAPI.h"

extern "C" UIBackendCallbacks* GetUIBackendCallbacks(void);

typedef int   (*UIInitFn)(void*);
typedef void  (*UISetViewportFn)(float,float,float,float);
typedef void  (*UIProcessEventsFn)(void);
typedef void  (*UIUpdateFn)(double);
typedef void  (*UIClearFn)(void);
typedef void  (*UIRenderFn)(void);
typedef void  (*UIPresentFn)(void);
typedef int   (*UIIsQuitFn)(void);
typedef void  (*UIShutdownFn)(void);
typedef int   (*UILoadLayoutFn)(const char*);
typedef void* (*UIFindControlFn)(const char*);
typedef void  (*UIRegisterActionFn)(const char*, void(*)(void*,void*), void*);
typedef int   (*UISetStringFn)(void*, const char*, const char*);
typedef const char* (*UIGetSelectedIdFn)(void*);
typedef const char* (*UIGetSelectedUserDataFn)(void*);

static UIInitFn               uiInit               = nullptr;
static UISetViewportFn        uiSetViewport        = nullptr;
static UIProcessEventsFn      uiProcessEvents      = nullptr;
static UIUpdateFn             uiUpdate             = nullptr;
static UIClearFn              uiClear              = nullptr;
static UIRenderFn             uiRender             = nullptr;
static UIPresentFn            uiPresent            = nullptr;
static UIIsQuitFn             uiIsQuit             = nullptr;
static UIShutdownFn           uiShutdown           = nullptr;
static UILoadLayoutFn         uiLoadLayout         = nullptr;
static UIFindControlFn        uiFindControl        = nullptr;
static UIRegisterActionFn     uiRegisterAction     = nullptr;
static UISetStringFn          uiSetString          = nullptr;
static UIGetSelectedIdFn      uiTreeGetSelId       = nullptr;
static UIGetSelectedUserDataFn uiTreeGetSelUserData = nullptr;
static void*                  g_labelHandle        = nullptr;
static void*                  g_treeHandle         = nullptr;

static HMODULE g_uiDll = nullptr;

// ---- JSON layout ----
// TreeView with deep hierarchy + long labels (horizontal scroll trigger)
// + a selection status label below.
static const char* LAYOUT_JSON =
    "{"
    "  \"theme\": {"
    "    \"colors\": {"
    "      \"treeview\": {"
    "        \"background\": { \"normal\": \"#2D2D2D\" },"
    "        \"border\":    { \"normal\": \"#555555\" }"
    "      }"
    "    }"
    "  },"
    "  \"controls\": [{"
    "    \"type\": \"Panel\","
    "    \"id\": \"rootPanel\","
    "    \"rect\": { \"x\": 10, \"y\": 10, \"w\": 760, \"h\": 560 },"
    "    \"children\": ["
    "      {"
    "        \"type\": \"TreeView\","
    "        \"id\": \"controlTree\","
    "        \"rect\": { \"x\": 10, \"y\": 10, \"w\": 280, \"h\": 360 },"
    "        \"indentWidth\": 16,"
    "        \"rowHeight\": 24,"
    "        \"cycleNavigation\": true,"
    "        \"items\": ["
    "          { \"id\": \"n1\",  \"label\": \"AAA - Root level 1\",            \"expanded\": true,  \"userData\": \"root1 data\",  \"children\": ["
    "            { \"id\": \"n1a\", \"label\": \"BBB - Level 2 item\",          \"expanded\": true,  \"userData\": \"lvl2 data\",    \"children\": ["
    "              { \"id\": \"n1ai\", \"label\": \"CCC - Level 3 - very deep nesting to test horizontal scrollbar\", \"userData\": \"deep data\" }"
    "            ]},"
    "            { \"id\": \"n1b\", \"label\": \"DDD - Another level 2 item with long name for scroll\", \"userData\": \"lvl2b data\" }"
    "          ]},"
    "          { \"id\": \"n2\",  \"label\": \"EEE - Root level 2\",            \"expanded\": true,  \"userData\": \"root2 data\" },"
    "          { \"id\": \"n3\",  \"label\": \"FFF - Root level 3\",            \"expanded\": false, \"userData\": \"root3 data\",  \"children\": ["
    "            { \"id\": \"n3a\", \"label\": \"GGG - collapsed child\" }"
    "          ]},"
    "          { \"id\": \"n4\",  \"label\": \"HHH - More items for vertical scroll testing\", \"expanded\": true, \"userData\": \"scroll test\", \"children\": ["
    "            { \"id\": \"n4a\", \"label\": \"III - Sub item A\" },"
    "            { \"id\": \"n4b\", \"label\": \"JJJ - Sub item B\" },"
    "            { \"id\": \"n4c\", \"label\": \"KKK - Sub item C\" },"
    "            { \"id\": \"n4d\", \"label\": \"LLL - Sub item D\" }"
    "          ]},"
    "          { \"id\": \"n5\",  \"label\": \"MMM - Root level 5\",            \"expanded\": true,  \"userData\": \"item5\", \"children\": ["
    "            { \"id\": \"n5a\", \"label\": \"NNN - Sub 5A\" },"
    "            { \"id\": \"n5b\", \"label\": \"OOO - Sub 5B\" },"
    "            { \"id\": \"n5c\", \"label\": \"PPP - Sub 5C\" }"
    "          ]},"
    "          { \"id\": \"n6\",  \"label\": \"QQQ - Root level 6\" },"
    "          { \"id\": \"n7\",  \"label\": \"RRR - Root level 7\" },"
    "          { \"id\": \"n8\",  \"label\": \"SSS - Root level 8\" },"
    "          { \"id\": \"n9\",  \"label\": \"TTT - Root level 9\" },"
    "          { \"id\": \"n10\", \"label\": \"UUU - Root level 10\" }"
    "        ],"
    "        \"events\": { \"onSelect\": \"onTreeSelect\" }"
    "      },"
    "      {"
    "        \"type\": \"Label\","
    "        \"id\": \"lblSelection\","
    "        \"rect\": { \"x\": 10, \"y\": 380, \"w\": 740, \"h\": 170 },"
    "        \"caption\": \"(click a tree node)\","
    "        \"fontSize\": 16,"
    "        \"textColor\": [180, 200, 220]"
    "      }"
    "    ]"
    "  }]"
    "}";

// ---- callback for tree node selection ----
static void onTreeNodeSelected(void* ctl, void* userData) {
    (void)userData;
    if (!g_labelHandle || !ctl) return;

    const char* id  = uiTreeGetSelId ? uiTreeGetSelId(ctl) : nullptr;
    const char* ud  = uiTreeGetSelUserData ? uiTreeGetSelUserData(ctl) : nullptr;

    char buf[512];
    if (id && ud)
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "Selected: %s\nCustom data: %s", id, ud);
    else if (id)
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "Selected: %s", id);
    else
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "(deselected)");

    uiSetString(g_labelHandle, "text", buf);
}

static void loadFunctions() {
    uiInit           = (UIInitFn)             GetProcAddress(g_uiDll, "UICornerstone_Init");
    uiSetViewport    = (UISetViewportFn)      GetProcAddress(g_uiDll, "UICornerstone_SetViewport");
    uiProcessEvents  = (UIProcessEventsFn)    GetProcAddress(g_uiDll, "UICornerstone_ProcessEvents");
    uiUpdate         = (UIUpdateFn)           GetProcAddress(g_uiDll, "UICornerstone_Update");
    uiClear          = (UIClearFn)            GetProcAddress(g_uiDll, "UICornerstone_Clear");
    uiRender         = (UIRenderFn)           GetProcAddress(g_uiDll, "UICornerstone_Render");
    uiPresent        = (UIPresentFn)          GetProcAddress(g_uiDll, "UICornerstone_Present");
    uiIsQuit         = (UIIsQuitFn)           GetProcAddress(g_uiDll, "UICornerstone_IsQuitRequested");
    uiShutdown       = (UIShutdownFn)         GetProcAddress(g_uiDll, "UICornerstone_Shutdown");
    uiLoadLayout     = (UILoadLayoutFn)       GetProcAddress(g_uiDll, "UICornerstone_LoadLayout");
    uiFindControl    = (UIFindControlFn)      GetProcAddress(g_uiDll, "UICornerstone_FindControl");
    uiRegisterAction = (UIRegisterActionFn)   GetProcAddress(g_uiDll, "UICornerstone_RegisterAction");
    uiSetString      = (UISetStringFn)        GetProcAddress(g_uiDll, "UICornerstone_SetString");
    uiTreeGetSelId   = (UIGetSelectedIdFn)    GetProcAddress(g_uiDll, "UICornerstone_TreeViewGetSelectedId");
    uiTreeGetSelUserData = (UIGetSelectedUserDataFn) GetProcAddress(g_uiDll, "UICornerstone_TreeViewGetSelectedUserData");
}

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);

    g_uiDll = LoadLibraryA("UICornerstone.dll");
    if (!g_uiDll) { printf("FAIL: LoadLibrary\n"); return 1; }

    loadFunctions();
    if (!uiInit || !uiLoadLayout || !uiFindControl || !uiRegisterAction || !uiSetString || !uiIsQuit) {
        printf("FAIL: GetProcAddress\n");
        FreeLibrary(g_uiDll);
        return 1;
    }
    if (!uiTreeGetSelId || !uiTreeGetSelUserData) {
        printf("FAIL: missing TreeView C ABI exports\n");
        FreeLibrary(g_uiDll);
        return 1;
    }

    UIBackendCallbacks* callbacks = GetUIBackendCallbacks();
    if (!callbacks) { printf("FAIL: GetUIBackendCallbacks\n"); FreeLibrary(g_uiDll); return 1; }

    if (!uiInit(callbacks)) { printf("FAIL: UICornerstone_Init\n"); FreeLibrary(g_uiDll); return 1; }
    printf("Init OK\n");

    uiSetViewport(0, 0, 800, 600);

    // Register the onSelect action BEFORE loading layout
    uiRegisterAction("onTreeSelect", onTreeNodeSelected, nullptr);

    if (uiLoadLayout(LAYOUT_JSON) == 0) { printf("FAIL: LoadLayout\n"); uiShutdown(); FreeLibrary(g_uiDll); return 1; }
    printf("LoadLayout OK\n");

    g_treeHandle = uiFindControl("controlTree");
    if (!g_treeHandle) { printf("FAIL: FindControl(controlTree)\n"); uiShutdown(); FreeLibrary(g_uiDll); return 1; }
    printf("FindControl(controlTree) OK\n");

    g_labelHandle = uiFindControl("lblSelection");
    if (!g_labelHandle) { printf("FAIL: FindControl(lblSelection)\n"); uiShutdown(); FreeLibrary(g_uiDll); return 1; }
    printf("FindControl(lblSelection) OK\n");

    printf("Frame loop running - click nodes in the TreeView, close window to exit\n");
    while (!uiIsQuit()) {
        uiProcessEvents();
        uiUpdate(1.0 / 60.0);
        uiClear();
        uiRender();
        uiPresent();
    }

    uiShutdown();
    FreeLibrary(g_uiDll);
    printf("=== TreeView C ABI Test PASSED ===\n");
    return 0;
}
