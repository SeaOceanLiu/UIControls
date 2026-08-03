#ifndef WINDOW_H
#define WINDOW_H

#include <string>
#include "Utility.h"

class RenderDevice;

// 跨后端统一窗口标志（值对齐 SDL_WINDOW_*，各后端自行映射：
// raylib 按 SDL_WINDOW_RESIZABLE=0x20 约定，sfml 按 0x01=Fullscreen 约定）
namespace UIWindowFlags {
    constexpr uint32_t None       = 0x00000000;
    constexpr uint32_t Fullscreen = 0x00000001; // SDL_WINDOW_FULLSCREEN
    constexpr uint32_t Resizable  = 0x00000020; // SDL_WINDOW_RESIZABLE
}

class Window {
public:
    virtual ~Window() = default;

    virtual SSize getSize() const = 0;
    virtual SPoint getPosition() const = 0;
    virtual float getDisplayWidth() const = 0;
    virtual float getDisplayHeight() const = 0;
    virtual float getDpiScale() const = 0;
    virtual void setTitle(const std::string& title) = 0;
    virtual void* nativeHandle() = 0;
    virtual RenderDevice* renderDevice() = 0;
    virtual bool getMousePosition(float& x, float& y) = 0;
    virtual void setResizable(bool resizable) { (void)resizable; }
    virtual void onResized(int width, int height) {}
};

Window* CreateSDL3Window(const char* title, int width, int height, uint32_t flags);
Window* CreateSDL3WindowFromExisting(void* nativeWindow, void* nativeRenderer);

#endif
