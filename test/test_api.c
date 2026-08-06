#include "UICornerstoneAPI.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>   /* GetTickCount64：无人值守自动退出计时 */

/* 静态后端回调表（src/backend/<backend>/BackendPlugin.cpp 定义，编入 UICornerstone 静态库）。
   静态构建下必须用静态回调绕过插件 DLL：避免目录残留的旧版 UIBackend_*.dll vtable 错位
   （旧 DLL 无新虚方法 → 后端配置/Surface 工厂均失效）。空声明 → C linkage，与 extern "C" 定义匹配。 */
extern UIBackendCallbacks* GetUIBackendCallbacks(void);

static void onBtnClick(UIControlHandle ctl, void* user) {
    (void)ctl; (void)user;
    static int count = 0;
    printf("  Button clicked! (%d)\n", ++count); fflush(stdout);
}

static const char* LAYOUT_JSON =
"{"
"  \"controls\": ["
"    {"
"      \"type\": \"Panel\","
"      \"id\": \"root\","
"      \"rect\": {\"x\": 0, \"y\": 0, \"w\": 800, \"h\": 480},"
"      \"children\": ["
"        {"
"          \"type\": \"Label\","
"          \"id\": \"title\","
"          \"rect\": {\"x\": 20, \"y\": 10, \"w\": 760, \"h\": 30},"
"          \"font\": {\"size\": 18},"
"          \"colors\": { \"text\": { \"normal\": \"#FFFFFFFF\" } },"
"          \"caption\": \"UICornerstone C ABI Controls Demo\""
"        },"
"        {\"type\":\"Label\",\"id\":\"hint_button\",\"rect\":{\"x\":20,\"y\":42,\"w\":240,\"h\":16},\"colors\":{\"text\":{\"normal\":\"#AAAAAAFF\"}},\"caption\":\"Button (click)\"},"
"        {\"type\":\"Label\",\"id\":\"hint_label\",\"rect\":{\"x\":280,\"y\":42,\"w\":240,\"h\":16},\"colors\":{\"text\":{\"normal\":\"#AAAAAAFF\"}},\"caption\":\"Label (multiline)\"},"
"        {\"type\":\"Label\",\"id\":\"hint_checkbox\",\"rect\":{\"x\":540,\"y\":42,\"w\":240,\"h\":16},\"colors\":{\"text\":{\"normal\":\"#AAAAAAFF\"}},\"caption\":\"CheckBox (toggle)\"},"
"        {\"type\":\"Button\",\"id\":\"btn_demo\",\"caption\":\"Button\",\"rect\":{\"x\":20,\"y\":66,\"w\":240,\"h\":95},\"events\":{\"onClick\":\"onBtnClick\"}},"
"        {\"type\":\"Label\",\"id\":\"lbl_demo\",\"caption\":\"Label\\nLine 2\\nLine 3\",\"rect\":{\"x\":280,\"y\":66,\"w\":240,\"h\":95},\"colors\":{\"background\":{\"normal\":\"#3A3A3AFF\"},\"text\":{\"normal\":\"#CCCCCCFF\"}}},"
"        {\"type\":\"CheckBox\",\"id\":\"cb_demo\",\"caption\":\"Check Option\",\"rect\":{\"x\":540,\"y\":66,\"w\":240,\"h\":95},\"colors\":{\"text\":{\"normal\":\"#FFFFFFFF\"}}},"
"        {\"type\":\"Label\",\"id\":\"hint_editbox\",\"rect\":{\"x\":20,\"y\":169,\"w\":240,\"h\":16},\"colors\":{\"text\":{\"normal\":\"#AAAAAAFF\"}},\"caption\":\"EditBox (type here)\"},"
"        {\"type\":\"Label\",\"id\":\"hint_progress\",\"rect\":{\"x\":280,\"y\":169,\"w\":240,\"h\":16},\"colors\":{\"text\":{\"normal\":\"#AAAAAAFF\"}},\"caption\":\"ProgressBar\"},"
"        {\"type\":\"Label\",\"id\":\"hint_panel\",\"rect\":{\"x\":540,\"y\":169,\"w\":240,\"h\":16},\"colors\":{\"text\":{\"normal\":\"#AAAAAAFF\"}},\"caption\":\"Panel (sub-controls)\"},"
"        {\"type\":\"EditBox\",\"id\":\"eb_demo\",\"rect\":{\"x\":20,\"y\":193,\"w\":240,\"h\":95},\"colors\":{\"background\":{\"normal\":\"#2A2A2AFF\"},\"text\":{\"normal\":\"#FFFFFFFF\"}},\"text\":\"EditBox sample\"},"
"        {\"type\":\"ProgressBar\",\"id\":\"pb_demo\",\"rect\":{\"x\":280,\"y\":193,\"w\":240,\"h\":95}},"
"        {\"type\":\"Panel\",\"id\":\"panel_demo\",\"rect\":{\"x\":540,\"y\":193,\"w\":240,\"h\":95},\"colors\":{\"background\":{\"normal\":\"#3A3A5AFF\"}},\"children\":["
"          {\"type\":\"Label\",\"id\":\"panel_label\",\"rect\":{\"x\":10,\"y\":10,\"w\":220,\"h\":30},\"colors\":{\"text\":{\"normal\":\"#FFFFFFFF\"}},\"caption\":\"Panel Label\"},"
"          {\"type\":\"Button\",\"id\":\"panel_btn\",\"caption\":\"Panel Btn\",\"rect\":{\"x\":10,\"y\":50,\"w\":220,\"h\":35},\"events\":{\"onClick\":\"onBtnClick\"}}"
"        ]},"
"        {\"type\":\"Label\",\"id\":\"hint_textarea\",\"rect\":{\"x\":20,\"y\":296,\"w\":760,\"h\":16},\"colors\":{\"text\":{\"normal\":\"#AAAAAAFF\"}},\"caption\":\"TextArea (scrollable)\"},"
"        {\"type\":\"TextArea\",\"id\":\"ta_demo\",\"rect\":{\"x\":20,\"y\":320,\"w\":760,\"h\":155},\"colors\":{\"background\":{\"normal\":\"#2A2A2AFF\"},\"text\":{\"normal\":\"#CCCCCCFF\"}},"
"          \"text\":\"This is a multi-line TextArea.\\n\\nIt supports:\\n- Arrow keys to navigate\\n- Shift+Arrow to select\\n- Ctrl+C/X/V\\n- Ctrl+A select all\\n- Word wrap\","
"          \"wordWrap\":true,\"scrollBarThickness\":10}"
"      ]"
"    }"
"  ]"
"}";

int main(int argc, char* argv[]) {
    printf("=== UICornerstone C ABI Controls Demo ===\n"); fflush(stdout);

    /* 无人值守：argv[1]="auto=<秒>" → 到时投递 WINDOW_CLOSE 自行退出 */
    int autoSec = 0; DWORD t0 = 0;
    if (argc >= 2 && strncmp(argv[1], "auto=", 5) == 0) {
        autoSec = atoi(argv[1] + 5);
        t0 = GetTickCount64();
        printf("  auto-quit in %ds\n", autoSec); fflush(stdout);
    }

    /* 后端配置冒烟：全局默认在创建前设置 → CreateInstance 应用 → 运行期查询/切换。
       vsync=1 全局默认在创建前生效：raylib 为创建期参数（InitWindow 前 FLAG_VSYNC_HINT，
       回读应为 1）；sdl3 创建时强制关闭 vsync（见 CreateSDL3Window）故回读 0 属预期，
       其后运行期 setConfig 可切换。非 sdl3/sfml/raylib 后端返回 0 属能力子集差异。 */
    UICornerstone_SetBackendConfigBool(NULL, "vsync", 1);
    #ifdef UICORNERSTONE_BUILD_SHARED
    UIInstance inst = UICornerstone_CreateInstanceFromPlugin(UICORNERSTONE_BACKEND_NAME, NULL);
#else
    UIInstance inst = UICornerstone_CreateInstance(GetUIBackendCallbacks(), NULL);
#endif
    if (!inst) {
        printf("FAIL: CreateInstance\n"); return 1;
    }

    int vb = -1, r = UICornerstone_GetBackendConfigBool(inst, "vsync", &vb);
    printf("  backend vsync after create: r=%d v=%d\n", r, vb); fflush(stdout);
    char rn[64] = {0};
    if (UICornerstone_GetBackendConfig(inst, "renderer-name", rn, sizeof(rn)) == 1)
        printf("  backend renderer-name: %s\n", rn); fflush(stdout);
    r = UICornerstone_SetBackendConfigInt(inst, "vsync", 1);
    if (r) {
        int vb2 = -1;
        if (UICornerstone_GetBackendConfigBool(inst, "vsync", &vb2) && vb2 != 1) {
            printf("FAIL: SetBackendConfig(vsync=1) not reflected (v=%d)\n", vb2);
            UICornerstone_DestroyInstance(inst); return 1;
        }
    } else {
        printf("  backend config vsync runtime set unsupported (ok if not sdl3/sfml)\n"); fflush(stdout);
    }

    UICornerstone_SetViewport(inst, 0, 0, 800, 480);
    UICornerstone_RegisterAction(inst, "onBtnClick", onBtnClick, NULL);

    printf("Loading layout...\n"); fflush(stdout);
    if (!UICornerstone_LoadLayout(inst, LAYOUT_JSON)) {
        printf("FAIL: LoadLayout\n");
        UICornerstone_DestroyInstance(inst); return 1;
    }
    printf("Layout loaded\n"); fflush(stdout);

    /* Verify all IDs */
    {int nF = 0;
    const char* ids[] = {"root","title",
        "hint_button","hint_label","hint_checkbox",
        "btn_demo","lbl_demo","cb_demo",
        "hint_editbox","hint_progress","hint_panel",
        "eb_demo","pb_demo","panel_demo",
        "panel_label","panel_btn",
        "hint_textarea","ta_demo"};
    for (int i = 0; i < (int)(sizeof(ids)/sizeof(ids[0])); i++) {
        if (UICornerstone_FindControl(inst, ids[i])) nF++;
        else printf("  Missing: %s\n", ids[i]);
    }
    printf("  FindControl: %d/%zu found\n", nF, sizeof(ids)/sizeof(ids[0])); fflush(stdout);}

    printf("  Frame loop running (close window to exit)...\n"); fflush(stdout);
    while (!UICornerstone_IsQuitRequested(inst)) {
        if (autoSec && (GetTickCount64() - t0) >= (DWORD)autoSec * 1000) {
            UIEvent ue;
            memset(&ue, 0, sizeof(ue));
            ue.type = UI_EVENT_WINDOW_CLOSE;
            UICornerstone_PushUIEvent(inst, &ue);
        }
        UICornerstone_ProcessEvents(inst);
        UICornerstone_Update(inst, 1.0 / 60.0);
        UICornerstone_Clear(inst);
        UICornerstone_Render(inst);
        UICornerstone_Present(inst);
    }

    printf("  Window closed%s\n",
        (autoSec && (GetTickCount64() - t0) >= (DWORD)autoSec * 1000) ? " (auto)" : " by user"); fflush(stdout);
    UICornerstone_DestroyInstance(inst);
    printf("  === PASS ===\n"); fflush(stdout);
    return 0;
}
