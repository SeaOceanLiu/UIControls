// =========================================================================
// test_aniviewer.cpp -- LuotiAni 命令行视觉校验工具
// 加载指定的动画 jsonc，按 jsonc 画布尺寸建立窗口并播放，供用户人工校验视觉效果。
//
// 用法: test_aniviewer <动画jsonc路径> [loop 0|1]
//   路径规则: 绝对路径原样使用；相对路径按 exe 同目录解析
//   (与 UICornerstone_CreateAnimation 的路径语义一致，源: UICornerstoneAPI.cpp:994)
//   窗口尺寸取自 jsonc overview.view；动画控件 w/h=0 自动回退画布尺寸。
// =========================================================================
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <fstream>
#include <iterator>
#include "UICornerstoneAPI.h"
#include "ConstDef.h"          // namespace fs = std::filesystem
#include "PlatformUtils.h"
#include "nlohmann/json.hpp"

// 静态后端回调表（src/backend/<backend>/BackendPlugin.cpp 定义，编入 UICornerstone 静态库）。
// 直接使用可避免误加载目录中残留的插件 DLL——其 Surface 工厂注册在独立 image，
// 核心库内仍为 null → Surface::loadFromMemory 失败。
extern "C" UIBackendCallbacks* GetUIBackendCallbacks(void);

using json = nlohmann::json;

static void printUsage(const char* argv0) {
    printf("LuotiAni 视觉校验工具\n");
    printf("用法: %s <动画jsonc路径> [loop 0|1]   (loop 缺省 1)\n", argv0);
    printf("  路径: 绝对路径原样使用；相对路径按 exe 同目录解析\n");
    printf("  窗口大小取 jsonc 的 overview.view 画布尺寸\n");
    printf("  示例: %s assets/animations/rotateBtn/rotateBtn.jsonc\n", argv0);
}

static std::string resolvePath(const char* arg) {
    std::string s = arg;
    if (s.size() >= 2 && s[1] == ':') return s;        // Windows 绝对（盘符）
    if (!s.empty() && s[0] == '/') return s;           // POSIX 绝对
    return (fs::path(Platform::GetBasePath()) / s).string();  // 相对 → exe 同目录
}

static bool loadCanvasInfo(const std::string& path, int& winW, int& winH, std::string& name) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false;
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    try {
        json doc = json::parse(content, nullptr, false, true);   // jsonc 允许注释
        const json& overview = doc.at("overview");
        winW = overview.at("view").at("width").get<int>();
        winH = overview.at("view").at("height").get<int>();
        name = overview.value("name", "test_aniviewer");
        return true;
    } catch (...) {
        return false;
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 2;
    }
    std::string jsoncReal = resolvePath(argv[1]);
    int loop = (argc >= 3 && argv[2][0] == '0') ? 0 : 1;
    // 无人值守：argv[3]="auto=<秒>" → 到时自动跳出循环（无需关闭窗口）
    int autoSec = 0;
    if (argc >= 4 && strncmp(argv[3], "auto=", 5) == 0) autoSec = std::atoi(argv[3] + 5);
    // vsync 开关：argv[4]="vsync=0|1" → 创建实例前设置后端全局配置
    int vsync = -1;
    if (argc >= 5 && strncmp(argv[4], "vsync=", 6) == 0) vsync = std::atoi(argv[4] + 6);

    int winW = 0, winH = 0;
    std::string title;
    if (!loadCanvasInfo(jsoncReal, winW, winH, title)) {
        printf("FAIL: 无法读取动画 jsonc: %s\n", jsoncReal.c_str());
        return 2;
    }
    printf("动画: %s  (%dx%d)  loop=%d%s%s —— 关闭窗口退出\n",
        title.c_str(), winW, winH, loop, autoSec ? "  auto" : "", vsync >= 0 ? (vsync ? "  vsync=1" : "  vsync=0") : "");

    // vsync 在实例创建前经全局后端配置生效（sdl3 支持；其他后端返回 0 忽略）
    if (vsync >= 0)
        UICornerstone_SetBackendConfigBool(NULL, "vsync", vsync);

    UIInstanceConfig cfg = UI_INSTANCE_CONFIG_DEFAULT;
    cfg.windowTitle = title.c_str();
    cfg.windowWidth = winW;
    cfg.windowHeight = winH;

    // 用静态回调创建（同 test_api 静态链接路径）：避免误加载目录中残留的
    // 后端插件 DLL（其 Surface 工厂在独立 image 注册，核心库内为 null → loadFromMemory 失败）
    UIInstance inst = UICornerstone_CreateInstance(GetUIBackendCallbacks(), &cfg);
    if (!inst) {
        printf("FAIL: 创建实例失败\n");
        return 1;
    }

    UIControlHandle ani = UICornerstone_CreateAnimation(inst, jsoncReal.c_str(), 0, 0, 0, 0);
    if (!ani) {
        printf("FAIL: 动画加载/prepare 失败 ('%s')\n", jsoncReal.c_str());
        UICornerstone_DestroyInstance(inst);
        return 1;
    }
    UICornerstone_SetBool(inst, ani, "playing", 1);
    UICornerstone_SetBool(inst, ani, "loop", loop);

    uint64_t t0 = Platform::GetTicks();
    while (!UICornerstone_IsQuitRequested(inst)) {
        if (autoSec && Platform::GetTicks() - t0 >= (uint64_t)autoSec * 1000) break;
        UICornerstone_ProcessEvents(inst);
        UICornerstone_Update(inst, 1.0 / 60.0);
        UICornerstone_Clear(inst);
        UICornerstone_Render(inst);
        UICornerstone_Present(inst);
    }

    printf("视图已关闭（%s）\n", autoSec ? "自动超时" : "人工");
    UICornerstone_DestroyInstance(inst);
    return 0;
}