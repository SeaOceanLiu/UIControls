// =========================================================================
// test_treeview_cabi.cpp -- single fromsource C ABI test for TreeView
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
typedef void  (*UIRegisterActionFn)(const char*,void(*)(void*,void*),void*);

static UIInitFn             uiInit            = nullptr;
static UISetViewportFn      uiSetViewport     = nullptr;
static UIProcessEventsFn    uiProcessEvents   = nullptr;
static UIUpdateFn           uiUpdate          = nullptr;
static UIClearFn            uiClear           = nullptr;
static UIRenderFn           uiRender          = nullptr;
static UIPresentFn          uiPresent         = nullptr;
static UIIsQuitFn           uiIsQuitRequested = nullptr;
static UIShutdownFn         uiShutdown        = nullptr;
static UILoadLayoutFn       uiLoadLayout      = nullptr;
static UIFindControlFn      uiFindControl     = nullptr;
static UIRegisterActionFn   uiRegisterAction  = nullptr;

static HMODULE g_uiDll = nullptr;

static const char* TREEVIEW_JSON = R"({
    "theme": {
        "colors": {
            "treeview": {
                "background": { "normal": "#2D2D2D" },
                "border": { "normal": "#555555" }
            }
        }
    },
    "controls": [
        {
            "type": "Panel",
            "id": "rootPanel",
            "rect": { "x": 10, "y": 10, "w": 760, "h": 560 },
            "style": { "background": { "normal": "#1E1E1E" } },
            "controls": [
                {
                    "type": "TreeView",
                    "id": "controlTree",
                    "rect": { "x": 10, "y": 10, "w": 250, "h": 300 },
                    "indentWidth": 16,
                    "rowHeight": 24,
                    "cycleNavigation": true,
                    "defaultExpand": false,
                    "items": [
                        {
                            "id": "rootPnl",
                            "label": "Root Panel",
                            "expanded": true,
                            "children": [
                                { "id": "btn1", "label": "Button (btn1)" },
                                {
                                    "id": "subPnl",
                                    "label": "Sub Panel",
                                    "expanded": false,
                                    "children": [
                                        { "id": "innerBtn", "label": "Inner Button" }
                                    ]
                                }
                            ]
                        },
                        { "id": "lbl1", "label": "Label (lbl1)" }
                    ],
                    "events": { "onSelect": "onTreeSelect" }
                }
            ]
        }
    ]
})";

static int g_selectCount = 0;
static char g_lastSelected[256] = {0};

static void onTreeSelect(void* ctl, void* user) {
    (void)ctl; (void)user;
    g_selectCount++;
    printf("[C ABI] TreeView onSelect called (%d)\n", g_selectCount);
}

static void loadFunctions() {
    uiInit            = (UIInitFn)GetProcAddress(g_uiDll, "UICornerstone_Init");
    uiSetViewport     = (UISetViewportFn)GetProcAddress(g_uiDll, "UICornerstone_SetViewport");
    uiProcessEvents   = (UIProcessEventsFn)GetProcAddress(g_uiDll, "UICornerstone_ProcessEvents");
    uiUpdate          = (UIUpdateFn)GetProcAddress(g_uiDll, "UICornerstone_Update");
    uiClear           = (UIClearFn)GetProcAddress(g_uiDll, "UICornerstone_Clear");
    uiRender          = (UIRenderFn)GetProcAddress(g_uiDll, "UICornerstone_Render");
    uiPresent         = (UIPresentFn)GetProcAddress(g_uiDll, "UICornerstone_Present");
    uiIsQuitRequested = (UIIsQuitFn)GetProcAddress(g_uiDll, "UICornerstone_IsQuitRequested");
    uiShutdown        = (UIShutdownFn)GetProcAddress(g_uiDll, "UICornerstone_Shutdown");
    uiLoadLayout      = (UILoadLayoutFn)GetProcAddress(g_uiDll, "UICornerstone_LoadLayout");
    uiFindControl     = (UIFindControlFn)GetProcAddress(g_uiDll, "UICornerstone_FindControl");
    uiRegisterAction  = (UIRegisterActionFn)GetProcAddress(g_uiDll, "UICornerstone_RegisterAction");
}

int main() {
    printf("=== TreeView C ABI Test ===\n");

    g_uiDll = LoadLibraryA("UICornerstone.dll");
    if (!g_uiDll) {
        printf("ERROR: Failed to load UICornerstone.dll\n");
        return 1;
    }

    loadFunctions();
    if (!uiInit || !uiLoadLayout || !uiRegisterAction || !uiFindControl) {
        printf("ERROR: Missing required DLL exports\n");
        FreeLibrary(g_uiDll);
        return 1;
    }

    // Register callback
    uiRegisterAction("onTreeSelect", onTreeSelect, nullptr);
    printf("Registered onTreeSelect callback\n");

    // Init
    UIBackendCallbacks* backend = GetUIBackendCallbacks();
    if (!backend || uiInit(backend) != 0) {
        printf("ERROR: UICornerstone_Init failed\n");
        FreeLibrary(g_uiDll);
        return 1;
    }

    uiSetViewport(0, 0, 800, 600);

    // Load layout
    if (uiLoadLayout(TREEVIEW_JSON) != 0) {
        printf("ERROR: UICornerstone_LoadLayout failed\n");
        uiShutdown();
        FreeLibrary(g_uiDll);
        return 1;
    }
    printf("Layout loaded successfully\n");

    // Verify control
    void* ctrl = uiFindControl("controlTree");
    printf("FindControl(controlTree): %s\n", ctrl ? "FOUND" : "NOT FOUND");

    // Run a few frames
    for (int i = 0; i < 10; i++) {
        uiProcessEvents();
        uiUpdate(1.0 / 60.0);
        uiClear();
        uiRender();
        uiPresent();
    }

    printf("Select events fired: %d\n", g_selectCount);

    uiShutdown();
    FreeLibrary(g_uiDll);
    printf("=== TreeView C ABI Test PASSED ===\n");
    return 0;
}
