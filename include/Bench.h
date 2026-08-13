#ifndef BenchH
#define BenchH
#include "MainWindow.h"
#include "Panel.h"
#include "Label.h"
#include "WinFrame.h"
#include "LuotiAni.h"
#include "UIContext.h"

class Bench: public Panel, public TopControl
{
    using OnInitialHandler = std::function<void (shared_ptr<Bench>)>;
protected:
private:
    bool m_isLoading;
    bool m_isInitialed;
    SRect m_defaultArenaRect;
    SRect m_defaultBGRect;

    float m_N; // multiple of Arena
    float m_M; // multiple of BG

    uint64_t m_nextTick;
    unordered_map<EventType, uint64_t> m_eventJitter; // jitter for each event
    uint64_t m_nextRepeatTick;
    shared_ptr<Event> m_lastAction;
    int m_isExiting;
    OnInitialHandler m_onInitial;

public:
    explicit Bench(UIContext* ctx);
    void initial(void);
    void inputControl(shared_ptr<Event> event);
    void repeatTrigger(void);
    void draw(void) override;
    void update(void) override;
    bool handleEvent(shared_ptr<Event> event) override;
    int isExiting(void);

    void setOnInitial(OnInitialHandler handler);

    void resized(SRect newRect) override;
    void addControl(shared_ptr<Control> control) override;

    // ── 视口缩放（§ViewportScale_Design）：Off 兼容原语义 — Fit/Stretch 覆盖根变换 ──
    enum class ViewportScaleMode { Off = 0, Fit = 1, Stretch = 2 };
    void setViewportScaleMode(ViewportScaleMode mode);
    ViewportScaleMode getViewportScaleMode(void) const { return m_vpMode; }
    void setViewportAnchor(float ax, float ay);
    void recomputeViewportTransform(void);
    SRect getDrawRect(void) override;
    void setScaleX(float xScale=1.0f) override;
    void setScaleY(float yScale=1.0f) override;
    int setEnumProperty(const char* prop, const char* value) override;

private:
    ViewportScaleMode m_vpMode = ViewportScaleMode::Off;
    float m_anchorX = 0.0f;
    float m_anchorY = 0.0f;
};
#endif // BenchH
