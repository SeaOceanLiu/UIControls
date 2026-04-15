#ifndef PanelH
#define PanelH

#include <functional>
#include <vector>
#include <unordered_map>
#include "ControlBase.h"
#include "Actor.h"

using namespace std;

class Panel: public ControlImpl {
    friend class PanelBuilder;
    using OnClickHandler = std::function<void (shared_ptr<Control>)>;
private:
    unordered_map<shared_ptr<Actor>, SPoint> m_actors;
public:
    Panel(Control *parent, SRect rect, float xScale=1.0f, float yScale=1.0f);
    void update(void) override;
    void draw(void) override;
    bool handleEvent(shared_ptr<Event> event) override;

    void addControl(shared_ptr<Control> control) override;;
};
class PanelBuilder {
private:
    shared_ptr<Panel> m_panel;
public:
    PanelBuilder(Control *parent, SRect rect, float xScale=1.0f, float yScale=1.0f);
    PanelBuilder& setBGColor(SDL_Color color);
    PanelBuilder& setBorderColor(SDL_Color color);
    PanelBuilder& setTransparent(bool isTransparent);
    PanelBuilder& setBorderVisible(bool isBorderVisible);
    PanelBuilder& addControl(shared_ptr<Control> control);
    shared_ptr<Panel> build(void);
};
#endif