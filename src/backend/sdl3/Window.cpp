#include "Window.h"
#include "RenderDevice.h"
#include <SDL3/SDL.h>

// ============================================================
// SDL3Window
// ============================================================
class SDL3Window : public Window {
public:
    SDL3Window(SDL_Window* window, SDL_Renderer* renderer)
        : m_window(window)
        , m_renderer(renderer)
        , m_renderDevice(CreateSDL3RenderDevice(renderer))
    {
        SDL_DisplayID displayId = SDL_GetPrimaryDisplay();
        if (displayId != 0) {
            const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(displayId);
            if (mode) {
                m_displayWidth = (float)mode->w * mode->pixel_density;
                m_displayHeight = (float)mode->h * mode->pixel_density;
            }
        }
    }

    ~SDL3Window() override {
        // Render device is owned and deleted by BackendManager
        m_renderDevice = nullptr;
        if (m_renderer) SDL_DestroyRenderer(m_renderer);
        if (m_window) SDL_DestroyWindow(m_window);
    }

    SSize getSize() const override {
        int w = 0, h = 0;
        if (m_window) SDL_GetWindowSize(m_window, &w, &h);
        return SSize{(float)w, (float)h};
    }

    SPoint getPosition() const override {
        int x = 0, y = 0;
        if (m_window) SDL_GetWindowPosition(m_window, &x, &y);
        return SPoint{(float)x, (float)y};
    }

    float getDisplayWidth() const override { return m_displayWidth; }
    float getDisplayHeight() const override { return m_displayHeight; }

    float getDpiScale() const override {
        if (!m_window) return 1.0f;
        SDL_DisplayID displayId = SDL_GetDisplayForWindow(m_window);
        if (displayId == 0) return 1.0f;
        float scale = SDL_GetDisplayContentScale(displayId);
        return (scale > 0.0f) ? scale : 1.0f;
    }

    void setTitle(const std::string& title) override {
        if (m_window) SDL_SetWindowTitle(m_window, title.c_str());
    }

    void* nativeHandle() override { return (void*)m_window; }

    RenderDevice* renderDevice() override { return m_renderDevice; }

    void setResizable(bool resizable) override {
        if (m_window) SDL_SetWindowResizable(m_window, resizable);
    }

    bool getMousePosition(float& x, float& y) override {
        if (!m_window) return false;
        // SDL_GetMouseState 返回的是鼠标相对"当前鼠标焦点窗口"的坐标：
        // 鼠标在另一窗口（多实例）时拿到的坐标属于别的窗口，会造成 hover
        // 串扰。须用全局坐标判断鼠标是否在本窗口内，不在则返回 false
        // （调用方按"鼠标不在本窗口"处理，清除 hover 状态）。
        float gx, gy;
        SDL_GetGlobalMouseState(&gx, &gy);
        int wx, wy;
        SDL_GetWindowPosition(m_window, &wx, &wy);
        int ww, wh;
        SDL_GetWindowSize(m_window, &ww, &wh);
        if (gx < wx || gx >= wx + ww || gy < wy || gy >= wy + wh)
            return false;
        x = gx - wx;
        y = gy - wy;
        return true;
    }

    SDL_Window* getSDLWindow() const { return m_window; }
    SDL_Renderer* getSDLRenderer() const { return m_renderer; }

private:
    SDL_Window* m_window;
    SDL_Renderer* m_renderer;
    RenderDevice* m_renderDevice;
    float m_displayWidth = 0;
    float m_displayHeight = 0;
};

// ============================================================
// Factory Function
// ============================================================
Window* CreateSDL3Window(const char* title, int width, int height, uint32_t flags) {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    // flags 为跨后端统一窗口标志（UIWindowFlags，值对齐 SDL_WINDOW_*，可直接透传）。
    // 注意：Vsync 是应用层保留位（raylib 创建期专用），不属 SDL 窗口标志，须掩掉。
    if (!SDL_CreateWindowAndRenderer(title, width, height, flags & ~UIWindowFlags::Vsync, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return nullptr;
    }
    // VSync 退出默认关闭（开启与否由后端配置入口控制，不硬编码）
    SDL_SetRenderVSync(renderer, 0);
    return new SDL3Window(window, renderer);
}

Window* CreateSDL3WindowFromExisting(void* nativeWindow, void* nativeRenderer) {
    return new SDL3Window(static_cast<SDL_Window*>(nativeWindow),
                          static_cast<SDL_Renderer*>(nativeRenderer));
}
