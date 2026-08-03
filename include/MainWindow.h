#ifndef MainWindowH
#define MainWindowH
#include "ConstDef.h"
#include "EventQueue.h"
#include "Utility.h"
#include "BackendPlugin.h"
#include "ResourceProvider.h"
#include "AppCallbacks.h"
#include "FocusManager.h"
#include "UIContext.h"
#include <memory>


class MainWindow {
public:
    explicit MainWindow(UIContext* ctx);
    ~MainWindow();

    // 窗口/后端资源经 m_context 解析（子视口共享 owner 后端）
    Window* getWindow(void) { return m_context ? m_context->window : nullptr; }
    RenderDevice* getRenderDevice(void) { return m_context ? m_context->renderDevice : nullptr; }
    TextRenderer* getTextRenderer(void) { return m_context ? m_context->textRenderer : nullptr; }
    InputBackend* getInputBackend(void) { return m_context ? m_context->inputBackend : nullptr; }
    ResourceProvider* getResourceProvider(void) { return m_resourceProvider.get(); }
    SSize getWindowSize(void) { return m_size; }
    SPoint getWindowPos(void) { return m_pos; }
    float getDisplayWidth(void) { return m_displayWidth; }
    float getDisplayHeight(void) { return m_displayHeight; }
    SSize getDisplaySize(void) { return SSize{m_displayWidth, m_displayHeight}; }

    // Set window title
    void setTitle(const std::string& title);

    void onWindowResized(int w, int h) { m_size = SSize{(float)w, (float)h}; }
    void onWindowMoved(int x, int y) { m_pos = SPoint{(float)x, (float)y}; }

    // Request graceful quit from within callbacks (e.g. "Exit" menu item)
    void quit() { m_quitRequested = true; }

    // === Mode 1: Owned loop ===
    // Runs the entire main loop internally.
    // Returns 0 on normal exit, 1 on init failure.
    int run(AppCallbacks* app);

    // === Mode 2: Tick-based API ===
    bool init(AppCallbacks* app);
    bool processEvents(AppCallbacks* app);
    void update(AppCallbacks* app);
    void render(AppCallbacks* app);
    void shutdown(AppCallbacks* app);

private:
    UIContext* m_context;
    std::unique_ptr<ResourceProvider> m_resourceProvider;
    SSize m_size;
    SPoint m_pos;
    float m_displayWidth;
    float m_displayHeight;

    bool m_quitRequested;
    uint64_t m_nextTick;
    uint64_t m_nextRepeatTick;
    shared_ptr<Event> m_lastAction;
    unordered_map<EventType, uint64_t> m_eventJitter;
    int m_pendingResizeW = -1, m_pendingResizeH = -1;
    uint64_t m_lastResizeArrival = 0;
};
#endif // MainWindowH
