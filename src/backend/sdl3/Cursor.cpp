#include "Cursor.h"
#include "BackendPlugin.h"
#include <SDL3/SDL_mouse.h>

class SDLCursor : public Cursor {
    SDL_Cursor* m_cursor;
    bool m_owned;
public:
    SDLCursor(SDL_Cursor* cursor, bool owned) : m_cursor(cursor), m_owned(owned) {}
    ~SDLCursor() override {
        // SDL_Quit 后（进程退出阶段残留对象析构）调用 SDL_DestroyCursor 为未定义行为
        if (m_owned && m_cursor && SDL3BackendIsActive()) {
            SDL_DestroyCursor(m_cursor);
        }
    }
    SDL_Cursor* get() const { return m_cursor; }
};

Cursor* sdl3CreateSystemCursor(SystemCursorType type) {
    static const SDL_SystemCursor mapping[] = {
        SDL_SYSTEM_CURSOR_DEFAULT,
        SDL_SYSTEM_CURSOR_TEXT,
        SDL_SYSTEM_CURSOR_POINTER,
        SDL_SYSTEM_CURSOR_WAIT,
        SDL_SYSTEM_CURSOR_CROSSHAIR,
        SDL_SYSTEM_CURSOR_PROGRESS,
        SDL_SYSTEM_CURSOR_NWSE_RESIZE,
        SDL_SYSTEM_CURSOR_NESW_RESIZE,
        SDL_SYSTEM_CURSOR_EW_RESIZE,
        SDL_SYSTEM_CURSOR_NS_RESIZE,
        SDL_SYSTEM_CURSOR_MOVE,
        SDL_SYSTEM_CURSOR_NOT_ALLOWED,
        SDL_SYSTEM_CURSOR_NW_RESIZE,
        SDL_SYSTEM_CURSOR_N_RESIZE,
        SDL_SYSTEM_CURSOR_NE_RESIZE,
        SDL_SYSTEM_CURSOR_E_RESIZE,
        SDL_SYSTEM_CURSOR_SE_RESIZE,
        SDL_SYSTEM_CURSOR_S_RESIZE,
        SDL_SYSTEM_CURSOR_SW_RESIZE,
        SDL_SYSTEM_CURSOR_W_RESIZE,
    };
    int idx = static_cast<int>(type);
    if (idx < 0 || idx >= static_cast<int>(sizeof(mapping) / sizeof(mapping[0]))) {
        return nullptr;
    }
    SDL_Cursor* sdlCursor = SDL_CreateSystemCursor(mapping[idx]);
    if (!sdlCursor) return nullptr;
    return new SDLCursor(sdlCursor, true);
}

Cursor* sdl3GetDefaultCursor() {
    // 注意：不可用 SDL_GetCursor()（返回"当前已设定的"光标——先经过 EW/NS 光标的
    // 分条后会把临时光标误缓存为默认，导致全局游标残留在 resize 箭头）；
    // 必须取系统默认光标。静态分配（进程中仅 1 个，避免 SDL 退出顺序析构问题）
    static Cursor* defaultCursorPtr = []() -> Cursor* {
        SDL_Cursor* c = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
        return c ? new SDLCursor(c, true) : nullptr;
    }();
    return defaultCursorPtr;
}

void sdl3SetCurrentCursor(Cursor* cursor) {
    if (!cursor) return;
    SDLCursor* sdlCursor = dynamic_cast<SDLCursor*>(cursor);
    if (sdlCursor && sdlCursor->get()) {
        SDL_SetCursor(sdlCursor->get());
    }
}

void RegisterSDL3CursorFactories() {
    Cursor::registerFactories(sdl3CreateSystemCursor, sdl3GetDefaultCursor, sdl3SetCurrentCursor);
}
