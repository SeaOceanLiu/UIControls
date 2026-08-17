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
#include <string>

#include "../../include/UICornerstoneAPI.h"

extern "C" UIBackendCallbacks* GetUIBackendCallbacks(void);

typedef UIInstance (*UICreateInstanceFn)(const UIBackendCallbacks*, const UIInstanceConfig*);
typedef void       (*UIDestroyInstanceFn)(UIInstance);
typedef void  (*UISetViewportFn)(UIInstance,float,float,float,float);
typedef void  (*UIProcessEventsFn)(UIInstance);
typedef void  (*UIUpdateFn)(UIInstance,double);
typedef void  (*UIClearFn)(UIInstance);
typedef void  (*UIRenderFn)(UIInstance);
typedef void  (*UIPresentFn)(UIInstance);
typedef int   (*UIIsQuitFn)(UIInstance);
typedef void  (*UIPushUIEventFn)(UIInstance,const UIEvent*);
typedef int   (*UILoadLayoutFn)(UIInstance,const char*);
typedef void* (*UIFindControlFn)(UIInstance,const char*);
typedef void  (*UIRegisterActionFn)(UIInstance, const char*, void(*)(void*,void*), void*);
typedef int   (*UISetStringFn)(UIInstance, void*, const char*, const char*);
typedef int   (*UIGetStringFn)(UIInstance, void*, const char*, char*, int);
typedef int   (*UIGetPtrFn)(UIInstance, void*, const char*, void**);
typedef int   (*UISetFloatFn)(UIInstance, void*, const char*, float);
typedef int   (*UIGetFloatFn)(UIInstance, void*, const char*, float*);
typedef int   (*UISetIntFn)(UIInstance, void*, const char*, int);
typedef int   (*UIGetIntFn)(UIInstance, void*, const char*, int*);
typedef int   (*UISetBoolFn)(UIInstance, void*, const char*, int);
typedef int   (*UIGetEnumFn)(UIInstance, void*, const char*, char*, int);

static UICreateInstanceFn      uiCreateInstance       = nullptr;
static UISetViewportFn         uiSetViewport          = nullptr;
static UIProcessEventsFn       uiProcessEvents        = nullptr;
static UIUpdateFn              uiUpdate               = nullptr;
static UIClearFn               uiClear                = nullptr;
static UIRenderFn              uiRender               = nullptr;
static UIPresentFn             uiPresent              = nullptr;
static UIIsQuitFn              uiIsQuit               = nullptr;
static UIPushUIEventFn     uiPushUIEvent          = nullptr;
static UIDestroyInstanceFn     uiDestroyInstance      = nullptr;
static UILoadLayoutFn         uiLoadLayout         = nullptr;
static UIFindControlFn        uiFindControl        = nullptr;
static UIRegisterActionFn     uiRegisterAction     = nullptr;
static UISetStringFn          uiSetString          = nullptr;
static UIGetStringFn          uiGetString          = nullptr;
static UIGetPtrFn             uiGetPtr             = nullptr;
static UISetFloatFn           uiSetFloat           = nullptr;
static UIGetFloatFn           uiGetFloat           = nullptr;
static UISetIntFn             uiSetInt             = nullptr;
static UIGetIntFn             uiGetInt             = nullptr;
static UISetBoolFn            uiSetBool            = nullptr;
static UIGetEnumFn            uiGetEnum            = nullptr;
static void*                  g_labelHandle        = nullptr;
static void*                  g_treeHandle         = nullptr;

static HMODULE g_uiDll = nullptr;
static UIInstance g_inst = nullptr;
static int g_autoSec = 0;   // auto=<秒>：到时注入 WINDOW_CLOSE 自行退出（无人值守）

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
    "    \"type\": \"panel\","
    "    \"id\": \"rootPanel\","
    "    \"rect\": { \"x\": 10, \"y\": 10, \"w\": 760, \"h\": 560 },"
    "    \"children\": ["
    "      {"
    "        \"type\": \"tree-view\","
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
"          { \"id\": \"n2\",  \"label\": \"EEE - Root level 2\",            \"expanded\": true,  \"userData\": \"root2 data\"," 
            "          \"leadingControl\": { \"type\": \"check-box\", \"checkState\": \"checked\" } },"
            "          { \"id\": \"n3\",  \"label\": \"FFF - Root level 3\",            \"expanded\": false, \"userData\": \"root3 data\"," 
            "          \"font\": \"harmonyos-sans-sc-bold\", \"size\": 18, \"leadingGap\": 8,  \"children\": ["
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
    "          { \"id\": \"n6\",  \"label\": \"QQQ - Root level 6\", \"leadingGap\": 12 },"
    "          { \"id\": \"n7\",  \"label\": \"RRR - Root level 7\" },"
    "          { \"id\": \"n8\",  \"label\": \"SSS - Root level 8\" },"
    "          { \"id\": \"n9\",  \"label\": \"TTT - Root level 9\" },"
    "          { \"id\": \"n10\", \"label\": \"UUU - Root level 10\" }"
    "        ],"
    "        \"events\": { \"onSelect\": \"onTreeSelect\" }"
    "      },"
    "      {"
    "        \"type\": \"label\","
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

    char idBuf[128] = "";
    char udBuf[128] = "";
    if (uiGetString) {
        uiGetString(g_inst, ctl, "selected-id", idBuf, (int)sizeof(idBuf));
    }
    if (uiGetPtr) {
        void* ud = nullptr;
        if (uiGetPtr(g_inst, ctl, "selected-user-data", &ud) && ud) {
            auto* s = static_cast<std::string*>(ud);
            _snprintf_s(udBuf, sizeof(udBuf), _TRUNCATE, "%s", s->c_str());
        }
    }

    char buf[512];
    if (idBuf[0] && udBuf[0])
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "Selected: %s\nCustom data: %s", idBuf, udBuf);
    else if (idBuf[0])
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "Selected: %s", idBuf);
    else
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "(deselected)");

    uiSetString(g_inst, g_labelHandle, "caption", buf);
}

static void loadFunctions() {
    uiCreateInstance = (UICreateInstanceFn)   GetProcAddress(g_uiDll, "UICornerstone_CreateInstance");
    uiSetViewport    = (UISetViewportFn)      GetProcAddress(g_uiDll, "UICornerstone_SetViewport");
    uiProcessEvents  = (UIProcessEventsFn)    GetProcAddress(g_uiDll, "UICornerstone_ProcessEvents");
    uiUpdate         = (UIUpdateFn)           GetProcAddress(g_uiDll, "UICornerstone_Update");
    uiClear          = (UIClearFn)            GetProcAddress(g_uiDll, "UICornerstone_Clear");
    uiRender         = (UIRenderFn)           GetProcAddress(g_uiDll, "UICornerstone_Render");
    uiPresent        = (UIPresentFn)          GetProcAddress(g_uiDll, "UICornerstone_Present");
    uiIsQuit         = (UIIsQuitFn)           GetProcAddress(g_uiDll, "UICornerstone_IsQuitRequested");
    uiPushUIEvent    = (UIPushUIEventFn)      GetProcAddress(g_uiDll, "UICornerstone_PushUIEvent");
    uiDestroyInstance= (UIDestroyInstanceFn)  GetProcAddress(g_uiDll, "UICornerstone_DestroyInstance");
    uiLoadLayout     = (UILoadLayoutFn)       GetProcAddress(g_uiDll, "UICornerstone_LoadLayout");
    uiFindControl    = (UIFindControlFn)      GetProcAddress(g_uiDll, "UICornerstone_FindControl");
    uiRegisterAction = (UIRegisterActionFn)   GetProcAddress(g_uiDll, "UICornerstone_RegisterAction");
    uiSetString      = (UISetStringFn)        GetProcAddress(g_uiDll, "UICornerstone_SetString");
    uiGetString      = (UIGetStringFn)        GetProcAddress(g_uiDll, "UICornerstone_GetString");
    uiGetPtr         = (UIGetPtrFn)           GetProcAddress(g_uiDll, "UICornerstone_GetPtr");
    uiSetFloat       = (UISetFloatFn)         GetProcAddress(g_uiDll, "UICornerstone_SetFloat");
    uiGetFloat       = (UIGetFloatFn)         GetProcAddress(g_uiDll, "UICornerstone_GetFloat");
    uiSetInt         = (UISetIntFn)           GetProcAddress(g_uiDll, "UICornerstone_SetInt");
    uiGetInt         = (UIGetIntFn)           GetProcAddress(g_uiDll, "UICornerstone_GetInt");
    uiSetBool        = (UISetBoolFn)          GetProcAddress(g_uiDll, "UICornerstone_SetBool");
    uiGetEnum        = (UIGetEnumFn)          GetProcAddress(g_uiDll, "UICornerstone_GetEnum");
    uiGetPtr         = (UIGetPtrFn)           GetProcAddress(g_uiDll, "UICornerstone_GetPtr");
}

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "auto=", 5) == 0) g_autoSec = atoi(argv[i] + 5);
    }
    setvbuf(stdout, NULL, _IONBF, 0);

    g_uiDll = LoadLibraryA("UICornerstone.dll");
    if (!g_uiDll) { printf("FAIL: LoadLibrary\n"); return 1; }

    loadFunctions();
    if (!uiCreateInstance || !uiLoadLayout || !uiFindControl || !uiRegisterAction || !uiSetString || !uiIsQuit || !uiPushUIEvent) {
        printf("FAIL: GetProcAddress\n");
        FreeLibrary(g_uiDll);
        return 1;
    }

    UIBackendCallbacks* callbacks = GetUIBackendCallbacks();
    if (!callbacks) { printf("FAIL: GetUIBackendCallbacks\n"); FreeLibrary(g_uiDll); return 1; }

    UIInstanceConfig cfg = UI_INSTANCE_CONFIG_DEFAULT;
    cfg.windowTitle = "test_treeview_cabi";
    cfg.windowWidth = 800;
    cfg.windowHeight = 600;
    g_inst = uiCreateInstance(callbacks, &cfg);
    if (!g_inst) { printf("FAIL: CreateInstance\n"); FreeLibrary(g_uiDll); return 1; }
    uiSetViewport(g_inst, 0, 0, 800, 600);
    printf("Init OK\n");

    // Register the onSelect action BEFORE loading layout
    uiRegisterAction(g_inst, "onTreeSelect", onTreeNodeSelected, nullptr);

    if (uiLoadLayout(g_inst, LAYOUT_JSON) == 0) { printf("FAIL: LoadLayout\n"); uiDestroyInstance(g_inst); FreeLibrary(g_uiDll); return 1; }
    printf("LoadLayout OK\n");

    g_treeHandle = uiFindControl(g_inst, "controlTree");
    if (!g_treeHandle) { printf("FAIL: FindControl(controlTree)\n"); uiDestroyInstance(g_inst); FreeLibrary(g_uiDll); return 1; }
    printf("FindControl(controlTree) OK\n");

    g_labelHandle = uiFindControl(g_inst, "lblSelection");
    if (!g_labelHandle) { printf("FAIL: FindControl(lblSelection)\n"); uiDestroyInstance(g_inst); FreeLibrary(g_uiDll); return 1; }
    printf("FindControl(lblSelection) OK\n");

    // ---- TreeView 增强：item 级 CABI（item-id 定位 + leadingGap/font/size/leadingControl）----
    // 1. 既有 CABI：indent-width（相对上一级的缩进宽度）
    float indent = 0;
    if (!uiGetFloat || !uiGetFloat(g_inst, g_treeHandle, "indent-width", &indent) || indent != 16.0f) {
        printf("FAIL: GetFloat(indent-width)\n"); return 1;
    }
    printf("GetFloat(indent-width)=%.0f OK\n", indent);

    // 2. item-id 定位 + item-leading-gap（JSON 默认 12 → CABI 改 10）
    if (!uiSetString || !uiSetString(g_inst, g_treeHandle, "item-id", "n6")) { printf("FAIL: SetString(item-id)\n"); return 1; }
    float gap = 0;
    if (!uiGetFloat(g_inst, g_treeHandle, "item-leading-gap", &gap) || gap != 12.0f) { printf("FAIL: GetFloat(item-leading-gap) default\n"); return 1; }
    if (!uiSetFloat(g_inst, g_treeHandle, "item-leading-gap", 10.0f) ||
        !uiGetFloat(g_inst, g_treeHandle, "item-leading-gap", &gap) || gap != 10.0f) { printf("FAIL: SetFloat(item-leading-gap)\n"); return 1; }
    printf("item-leading-gap 12->10 OK\n");

    // 3. item-font / item-font-size（JSON: n3 粗体 18 → CABI 改 20）
    if (!uiSetString(g_inst, g_treeHandle, "item-id", "n3")) { printf("FAIL: SetString(item-id n3)\n"); return 1; }
    char fbuf[64] = {0};
    if (!uiGetEnum || !uiGetEnum(g_inst, g_treeHandle, "item-font", fbuf, (int)sizeof(fbuf)) ||
        strcmp(fbuf, "harmonyos-sans-sc-bold") != 0) { printf("FAIL: GetEnum(item-font)\n"); return 1; }
    int fs = 0;
    if (!uiGetInt || !uiGetInt(g_inst, g_treeHandle, "item-font-size", &fs) || fs != 18) { printf("FAIL: GetInt(item-font-size)\n"); return 1; }
    if (!uiSetInt(g_inst, g_treeHandle, "item-font-size", 20) ||
        !uiGetInt(g_inst, g_treeHandle, "item-font-size", &fs) || fs != 20) { printf("FAIL: SetInt(item-font-size)\n"); return 1; }
    printf("item-font=bold, item-font-size 18->20 OK\n");

    // 4. item-leading-control：JSON 挂的 CheckBox 借用手柄，C 侧直接操作
    void* lc = NULL;
    if (!uiSetString(g_inst, g_treeHandle, "item-id", "n2") || !uiGetPtr(g_inst, g_treeHandle, "item-leading-control", &lc) || !lc) {
        printf("FAIL: GetPtr(item-leading-control)\n"); return 1;
    }
    if (!uiSetBool || uiSetBool(g_inst, lc, "checked", 1) != 1) { printf("FAIL: SetBool(leading checkbox, checked)\n"); return 1; }
    printf("item-leading-control (checkbox) borrowed & toggled OK\n");

    // 5. GetControlType：容器控件运行时类型查询
    typedef int (*UIGetControlTypeFn)(UIInstance, void*, char*, int);
    static UIGetControlTypeFn uiGetControlType = nullptr;
    if (!uiGetControlType) {
        uiGetControlType = (UIGetControlTypeFn)GetProcAddress(g_uiDll, "UICornerstone_GetControlType");
        if (!uiGetControlType) { printf("FAIL: GetProcAddress(GetControlType)\n"); return 1; }
    }
    char typeBuf[64] = {0};
    if (!uiGetControlType(g_inst, lc, typeBuf, (int)sizeof(typeBuf)) || strcmp(typeBuf, "check-box") != 0) {
        printf("FAIL: GetControlType(leading checkbox) got \"%s\"\n", typeBuf); return 1;
    }
    if (!uiGetControlType(g_inst, g_treeHandle, typeBuf, (int)sizeof(typeBuf)) || strcmp(typeBuf, "tree-view") != 0) {
        printf("FAIL: GetControlType(treeview) got \"%s\"\n", typeBuf); return 1;
    }
    printf("GetControlType: check-box / tree-view OK\n");

    printf("TreeView item CABI assertions PASSED\n");

    printf("Frame loop running - click nodes in the TreeView, close window to exit\n");
    ULONGLONG autoT0 = GetTickCount64();
    while (!uiIsQuit(g_inst)) {
        if (g_autoSec > 0 && (GetTickCount64() - autoT0) >= (ULONGLONG)g_autoSec * 1000) {
            UIEvent ue; memset(&ue, 0, sizeof(ue)); ue.type = UI_EVENT_WINDOW_CLOSE;
            uiPushUIEvent(g_inst, &ue);
        }
        uiProcessEvents(g_inst);
        uiUpdate(g_inst, 1.0 / 60.0);
        uiClear(g_inst);
        uiRender(g_inst);
        uiPresent(g_inst);
    }

    uiDestroyInstance(g_inst);
    g_inst = nullptr;
    FreeLibrary(g_uiDll);
    g_uiDll = nullptr;
    printf("=== TreeView C ABI Test PASSED ===\n");
    return 0;
}
