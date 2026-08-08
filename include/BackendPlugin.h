#ifndef BackendPluginH
#define BackendPluginH

#include <string>
#include "Window.h"
#include "RenderDevice.h"
#include "TextRenderer.h"
#include "InputBackend.h"
#include "UICornerstoneAPI.h"

// sdl3 后端内部状态查询：SDL 子系统是否仍活跃（sdl3Init 置 true，sdl3Destroy/SDL_Quit 后置 false）。
// 用途：静态/全局残留 UI 对象（shared_ptr 逃逸树外）在进程退出阶段析构时，
// 必须跳过对已退出 SDL 子系统的资源释放调用（SDL_DestroyTexture/Cursor/Surface 等），
// 否则在 SDL_Quit 之后调用属于未定义行为（段错误）。
bool SDL3BackendIsActive(void);

// BackendAPI - C ABI compatible function table for backend plugin DLLs.
// Each backend DLL exports a GetBackendAPI() function returning a pointer
// to a static BackendAPI instance.
struct BackendAPI {
    unsigned version;
    bool (*init)();
    Window* (*createWindow)(const char* title, int w, int h, uint32_t flags);
    RenderDevice* (*createRenderDevice)(Window* window);
    TextRenderer* (*createTextRenderer)(RenderDevice* device);
    InputBackend* (*createInputBackend)(Window* window);
    void (*destroy)();
    uint32_t capabilities;   // UICORN_BACKEND_CAP_* 位，0 = 无声明能力
};

// BackendManager - owns one instance's backend lifecycle.
// Each UIInstance (owner) holds its own BackendManager; child viewports
// share the owner's instance. The registered backend table stays process-wide.
class BackendManager {
public:
    BackendManager() = default;
    ~BackendManager();

    bool initialize(const std::string& backendName = "sdl3",
                    const char* title = "UICornerstone",
                    int width = 1024, int height = 768, uint32_t flags = 0);

    // 从 C ABI 回调查表初始化（R6 新增）；title/width/height 为窗口参数覆盖，nullptr/0 用默认；
    // flags 为跨后端统一窗口标志（UIWindowFlags），透传给 createWindow 回调
    bool initialize(const UIBackendCallbacks* callbacks,
                    const char* title = nullptr, int width = 0, int height = 0,
                    uint32_t flags = 0);

    void shutdown();

    Window* window() const { return m_window; }
    RenderDevice* renderDevice() const { return m_renderDevice; }
    TextRenderer* textRenderer() const { return m_textRenderer; }
    InputBackend* inputBackend() const { return m_inputBackend; }
    // 后端能力位（UICORN_BACKEND_CAP_*），initialize 后有效
    uint32_t capabilities() const { return m_capabilities; }

    // Register a backend statically (called from backend init functions).
    // s_registeredAPI 保留为静态（进程级后端注册表，只读）
    static void registerBackend(const BackendAPI& api);

private:
    BackendAPI m_api = {};
    Window* m_window = nullptr;
    RenderDevice* m_renderDevice = nullptr;
    TextRenderer* m_textRenderer = nullptr;
    InputBackend* m_inputBackend = nullptr;
    uint32_t m_capabilities = 0;
    bool m_initialized = false;

    static BackendAPI s_registeredAPI;
};

#endif // BackendPluginH
