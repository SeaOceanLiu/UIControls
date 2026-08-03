#include "MainWindow.h"
#include "Bench.h"
#include "AppCallbacks.h"
#include "BackendPlugin.h"
#include "PlatformUtils.h"

// ============================================================
// MainWindow
// ============================================================
MainWindow::MainWindow(UIContext* ctx):
    m_context(ctx),
    m_resourceProvider(ResourceProvider::createFilesystem(
        (ctx && !ctx->resourceRoot.empty()) ? ctx->resourceRoot : ConstDef::pathPrefix.string())),
    m_size({INITIAL_WIDTH, INITIAL_HEIGHT}),
    m_pos({INITIAL_POSX, INITIAL_POSY}),
    m_displayWidth(0),
    m_displayHeight(0),
    m_quitRequested(false),
    m_nextTick(0),
    m_nextRepeatTick(0),
    m_lastAction(nullptr)
{
    Window* w = getWindow();
    if (w) {
        m_size = w->getSize();
        m_pos = w->getPosition();
        m_displayWidth = w->getDisplayWidth();
        m_displayHeight = w->getDisplayHeight();
    }
}

MainWindow::~MainWindow() = default;

void MainWindow::setTitle(const std::string& title) {
    Window* w = getWindow();
    if (w) w->setTitle(title);
}

// ============================================================
// Mode 1: Owned loop — delegates to the tick-based API below
// ============================================================
int MainWindow::run(AppCallbacks* app) {
    if (!init(app)) return 1;

    bool running = true;
    while (running) {
        if (!processEvents(app))
            break;
        update(app);
        render(app);
    }

    shutdown(app);
    return 0;
}

// ============================================================
// Mode 2: Tick-based API
// ============================================================
bool MainWindow::init(AppCallbacks* app) {
    return app->onInit();
}

bool MainWindow::processEvents(AppCallbacks* app) {
    if (!m_context || !m_context->inputBackend) return true;
    auto* inputBackend = m_context->inputBackend;
    inputBackend->newFrame();

    Event event;
    bool running = true;
    int eventGuard = 0;
    while (inputBackend->pollEvent(event)) {
        if (++eventGuard > 500) break; // Safety: prevent infinite event loop
        switch (event.m_type) {
            case EventType::WindowClose:
                running = false;
                break;
            case EventType::WindowResize: {
                int w = event.resizeEvent.width;
                int h = event.resizeEvent.height;
                m_pendingResizeW = w;
                m_pendingResizeH = h;
                m_lastResizeArrival = Platform::GetTicks();
                onWindowResized(w, h);
                if (m_context->window) m_context->window->onResized(w, h);
                break;
            }
            case EventType::WindowMoved:
                onWindowMoved(event.windowMoved.x, event.windowMoved.y);
                break;
            default:
                {
                    // Dispatch all events (new-style and old-style) to controls
                    auto sharedEvent = make_shared<Event>(event);
                    m_context->bench->inputControl(sharedEvent);
                }
                // Notify app of events via new-style API
                app->onEvent(event);
                break;
        }
    }
    // Check for programmatic quit request (e.g. from "Exit" menu item)
    if (m_quitRequested) {
        m_quitRequested = false;
        running = false;
    }
    return running;
}

void MainWindow::update(AppCallbacks* app) {
    // Debounce resize: only call BENCH->resized() once the user has stopped
    // dragging (200 ms since the last WindowResize event).  During the drag
    // itself the viewport is updated (window->onResized) so the GL surface
    // tracks the new size, but control layouts are deferred until settle.
    uint64_t now = Platform::GetTicks();
    if (m_pendingResizeW >= 0 && m_pendingResizeH >= 0 &&
        now - m_lastResizeArrival >= 200) {
        m_context->bench->resized({0, 0, (float)m_pendingResizeW, (float)m_pendingResizeH});
        m_pendingResizeW = -1;
        m_pendingResizeH = -1;
    }
    app->onUpdate();
}

void MainWindow::render(AppCallbacks* app) {
    app->onRender();
    if (m_context && m_context->renderDevice) m_context->renderDevice->present();
}

void MainWindow::shutdown(AppCallbacks* app) {
    app->onQuit();
}
