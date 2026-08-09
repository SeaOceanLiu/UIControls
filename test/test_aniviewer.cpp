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
    printf("用法: %s [动画jsonc路径] [loop=0|1] [auto=<秒>] [vsync=0|1]\n", argv0);
    printf("  路径: 任意位置、可省略（缺省 assets/animations/rotateBtn/rotateBtn.jsonc）；\n");
    printf("        绝对路径原样使用；相对路径按 exe 同目录解析\n");
    printf("  参数: 任意顺序、可省略；loop 缺省 1，auto 缺省 0（不自动退出）\n");
    printf("  兼容: 旧式位置参数 [loop 0|1]（纯 0/1 参数）仍可用\n");
    printf("  窗口大小取 jsonc 的 overview.view 画布尺寸\n");
    printf("  示例: %s auto=5 vsync=1 assets/animations/rotateBtn/rotateBtn.jsonc\n", argv0);
}

static std::string resolvePath(const char* arg) {
    std::string s = arg;
    if (s.size() >= 2 && s[1] == ':') return s;        // Windows 绝对（盘符）
    if (!s.empty() && s[0] == '/') return s;           // POSIX 绝对
    return (fs::path(Platform::GetBasePath()) / s).string();  // 相对 → exe 同目录
}

static bool loadCanvasInfo(const std::string& path, int& winW, int& winH, std::string& name, int& frameRate) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false;
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    try {
        json doc = json::parse(content, nullptr, false, true);   // jsonc 允许注释
        const json& overview = doc.at("overview");
        winW = overview.at("view").at("width").get<int>();
        winH = overview.at("view").at("height").get<int>();
        name = overview.value("name", "test_aniviewer");
        frameRate = overview.value("frameRate", 0);
        return true;
    } catch (...) {
        return false;
    }
}

int main(int argc, char** argv) {
    // 路径参数任意位置：非 "key=" 键值且非纯 "0"/"1" 的参数视为 jsonc 路径；
    // 未提供时使用缺省路径 assets/animations/rotateBtn/rotateBtn.jsonc
    std::string jsoncReal;
    int loop = 1;
    int autoSec = 0;
    int vsync = -1;
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strncmp(a, "loop=", 5) == 0) {
            loop = (a[5] == '0') ? 0 : 1;
        } else if (strncmp(a, "auto=", 5) == 0) {
            autoSec = std::atoi(a + 5);
        } else if (strncmp(a, "vsync=", 6) == 0) {
            vsync = std::atoi(a + 6);
        } else if (a[0] == '0' && a[1] == '\0') {
            loop = 0;                       // 旧式位置参数 loop=0
        } else if (a[0] == '1' && a[1] == '\0') {
            loop = 1;                       // 旧式位置参数 loop=1
        } else if (std::strchr(a, '=') == nullptr) {
            jsoncReal = resolvePath(a);     // jsonc 路径（任意位置）
        } else {
            printf("WARN: 忽略无法识别的参数: %s\n", a);
        }
    }
    if (jsoncReal.empty()) {
        jsoncReal = resolvePath("assets/animations/rotateBtn/rotateBtn.jsonc");
        printf("提示: 未提供动画 jsonc 路径，使用缺省: %s\n", jsoncReal.c_str());
    }

    int winW = 0, winH = 0;
    std::string title;
    int setFrameRate = 0;
    if (!loadCanvasInfo(jsoncReal, winW, winH, title, setFrameRate)) {
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

    UIControlHandle ani = UICornerstone_CreateAnimation(inst, jsoncReal.c_str(), 0, 0, 0, 0, 1.0f, 1.0f);
    if (!ani) {
        printf("FAIL: 动画加载/prepare 失败 ('%s')\n", jsoncReal.c_str());
        UICornerstone_DestroyInstance(inst);
        return 1;
    }
    UICornerstone_SetBool(inst, ani, "playing", 1);
    UICornerstone_SetBool(inst, ani, "loop", loop);

    // 信息覆盖层：左上=动画设定帧率，右上=实际帧率（每秒刷新）。
    // label 宽度随窗口自适应，避免小画布（如 256px）两组 fps 重叠。
    float fpsLblW = (float)(winW - 24) / 2.0f;
    if (fpsLblW < 120.0f) fpsLblW = 120.0f;
    char setFpsText[64] = {0};
    snprintf(setFpsText, sizeof(setFpsText), "set fps: %d", setFrameRate);
    UIControlHandle lblSetFps = UICornerstone_CreateLabel(inst, setFpsText, 18.0f, 8.0f, 4.0f, fpsLblW, 26.0f, 1.0f, 1.0f);
    UIControlHandle lblRealFps = UICornerstone_CreateLabel(inst, "real fps: -", 18.0f, (float)winW - 8.0f - fpsLblW, 4.0f, fpsLblW, 26.0f, 1.0f, 1.0f);

    uint64_t t0 = Platform::GetTicks();
    uint64_t fpsT0 = t0;
    int fpsFrames = 0;
    char fpsText[64] = {0};
    while (!UICornerstone_IsQuitRequested(inst)) {
        if (autoSec && Platform::GetTicks() - t0 >= (uint64_t)autoSec * 1000) break;
        UICornerstone_ProcessEvents(inst);
        UICornerstone_Update(inst, 1.0 / 60.0);
        UICornerstone_Clear(inst);
        UICornerstone_Render(inst);
        UICornerstone_Present(inst);
        // 实际 fps：每秒更新一次右上角 label
        fpsFrames++;
        uint64_t now = Platform::GetTicks();
        if (now - fpsT0 >= 1000) {
            snprintf(fpsText, sizeof(fpsText), "real fps: %d", (int)(fpsFrames * 1000 / (now - fpsT0)));
            if (lblRealFps) UICornerstone_SetString(inst, lblRealFps, "caption", fpsText);
            fpsFrames = 0;
            fpsT0 = now;
        }
    }

    printf("视图已关闭（%s）\n", autoSec ? "自动超时" : "人工");
    UICornerstone_DestroyInstance(inst);
    return 0;
}