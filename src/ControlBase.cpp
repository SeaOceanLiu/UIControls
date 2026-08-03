#include "ControlBase.h"
#include "UICornerstoneAPI.h"
#include "PlatformUtils.h"
#include "MainWindow.h"
#include "PropertyNames.h"
#include <cstring>
ControlImpl::ControlImpl(Control *parent, float xScale, float yScale):
    // m_weakThis(this),
    // m_sharedThis(nullptr),
    m_isCreated(false),
    m_isTransparent(false),
    m_isBorderVisible(false),
    m_state(ControlState::Normal),
    m_id(INT_MAX),
    m_parent(parent),
    m_enable(true),
    m_visible(false),
    m_xScale(xScale),
    m_yScale(yScale),
    m_xxScale(parent==nullptr?xScale:xScale*parent->getScaleXX()),
    m_yyScale(parent==nullptr?yScale:yScale*parent->getScaleYY()),

    m_bgColor(StateColor()),
    m_borderColor(StateColor(StateColor::Type::Border)),
    m_textColor(StateColor(StateColor::Type::Text)),
    m_textShadowColor(StateColor(StateColor::Type::TextShadow)),

    m_surface(nullptr),
    m_renderDevice(nullptr),
    m_textRenderer(nullptr),
    m_inputBackend(nullptr),
    m_resourceProvider(nullptr),
    m_texture(nullptr),
    m_rect({0, 0, 0, 0}),
    m_mouseInside(false)
{
    // 构造时从父控件继承实例上下文：浮层控件（Dialog/Popup 等）以 parent 构造但
    // 不在父的 children 中，无 setContext 传播路径，必须在此继承，否则 open() 时
    // BENCH/GET_CONTEXT 为 null
    if (parent != nullptr && m_context == nullptr) {
        m_context = parent->getContext();
    }
    m_eventQueueInstance = m_context ? m_context->eventQueue : nullptr;
    // if (m_parent != nullptr){
        inheritRenderer();
    // }
}

ControlImpl::ControlImpl(const ControlImpl &other):
    m_isCreated(other.m_isCreated),
    m_isTransparent(other.m_isTransparent),
    m_isBorderVisible(other.m_isBorderVisible),
    m_state(other.m_state),
    m_id(other.m_id),
    m_parent(other.m_parent),
    m_enable(other.m_enable),
    m_visible(other.m_visible),
    m_xScale(other.m_xScale),
    m_yScale(other.m_yScale),
    m_xxScale(other.m_xxScale),
    m_yyScale(other.m_yyScale),

    m_bgColor(other.m_bgColor),
    m_borderColor(other.m_borderColor),
    m_textColor(other.m_textColor),

    m_renderDevice(other.m_renderDevice),
    m_surface(other.m_surface),
    m_texture(other.m_texture),
    m_rect(other.m_rect),
    m_mouseInside(other.m_mouseInside)
{
    m_eventQueueInstance = other.m_eventQueueInstance;

    for(const auto& child : other.m_children){
        // shared_ptr<ControlImpl> newChild = make_shared<ControlImpl>(&child); // Todo: 这里需要深拷贝
        // addControl(newChild);
    }

}
ControlImpl& ControlImpl::operator=(const ControlImpl &other){
    if (this == &other) return *this;
    m_isCreated = other.m_isCreated;
    m_isTransparent = other.m_isTransparent;
    m_isBorderVisible = other.m_isBorderVisible;
    m_state = other.m_state;
    m_id = other.m_id;
    m_parent = other.m_parent;
    m_enable = other.m_enable;
    m_visible = other.m_visible;
    m_xScale = other.m_xScale;
    m_yScale = other.m_yScale;
    m_xxScale = other.m_xxScale;
    m_yyScale = other.m_yyScale;
    m_renderDevice = other.m_renderDevice;
    m_surface = other.m_surface;
    m_texture = other.m_texture;
    m_rect = other.m_rect;
    m_mouseInside = other.m_mouseInside;
    m_eventQueueInstance = other.m_eventQueueInstance;

    for(const auto& child : other.m_children){
        // shared_ptr<ControlImpl>  newChild = make_shared<ControlImpl>(child); // 这里需要深拷贝
        // addControl(newChild);
    }

    return *this;
}

void ControlImpl::setContext(UIContext* ctx) {
    Control::setContext(ctx);
    if (ctx) {
        // 补注册焦点：两阶段创建期间 setFocusable(true) 时 context 为空，
        // FocusManager::registerControl 未执行，Tab 遍历不到该控件
        if (m_focusable && ctx->focusManager)
            ctx->focusManager->registerControl(this);
        // 自身两阶段创建：Builder::build 在 context 未就绪时可能已提前 create（m_isCreated=true），
        // 故此处无条件 recreate，由各派生 create 内部的 GET_CONTEXT 守卫决定是否真正补建
        recreate();
    }
    // 递归传播：子控件以 null context 创建（两阶段），挂树后统一重建
    for (auto& child : m_children) {
        child->setContext(ctx);
        if (ctx) {
            auto impl = dynamic_pointer_cast<ControlImpl>(child);
            if (impl) impl->recreate();
        }
    }
}

void ControlImpl::recreate(void) {
    if(!m_isCreated) {
        // 两阶段创建：此前因无实例上下文被守卫延迟，现在 context 就绪，直接创建
        create();
        return;
    }
    m_isCreated = false;
    // 虚调用：派生类的 create override（Button 状态 Actor、EditBox 字体、
    // CheckBox caption 等）在首次 create 时因 context 未就绪被守卫延迟，
    // 挂树后需重跑补建
    create();
}

void ControlImpl::create(void){
    if(!m_isCreated) {
        m_isCreated = true;
        setVisible(true);
    }
}

void ControlImpl::update(void){
    if(!getEnable()) return;

    // 检测鼠标进入/退出状态
    if (getVisible() && getEnable()) {
        // 获取当前鼠标位置
        float mouseX = 0, mouseY = 0;
        if (m_context && m_context->window) {
            m_context->window->getMousePosition(mouseX, mouseY);
        }

        SRect drawRect = getDrawRect();
        bool isInside = drawRect.contains(mouseX, mouseY);

        // 检测鼠标进入/退出状态变化
        if (isInside && !m_mouseInside) {
            // 鼠标进入控件区域
            m_mouseInside = true;
            // 转换为控件内坐标系
            float localX = mouseX - drawRect.left;
            float localY = mouseY - drawRect.top;
            onMouseEnter(localX, localY);
        } else if (!isInside && m_mouseInside) {
            // 鼠标退出控件区域
            m_mouseInside = false;
            // 转换为控件内坐标系
            float localX = mouseX - drawRect.left;
            float localY = mouseY - drawRect.top;
            onMouseLeave(localX, localY);
        }
    }

    for (auto& child : m_children){
        child->update();
    }
}

void ControlImpl::draw(void){
    if (!getVisible()) return;

    inheritRenderer();

    // draw the control
    // ...
    // draw the children
    for (auto& child : m_children){
        child->draw();
    }
}
void ControlImpl::beforeDraw() {
    if (!getVisible()) return;

    m_frameDrawRect = getDrawRect();
    m_frameDrawRectValid = true;
    drawBackground(&m_frameDrawRect);
}

void ControlImpl::afterDraw() {
    if (!getVisible()) return;

    drawBorder(&m_frameDrawRect);
    drawFocusRing();
    m_frameDrawRectValid = false;
}
void ControlImpl::drawBackground(const SRect *pDrawRect){
    SRect drawRect;

    if(!getTransparent()) {
        if (pDrawRect == nullptr){
            drawRect = getDrawRect();
        } else {
            drawRect = *pDrawRect;
        }

        // 背景色
        SColor bgColor;
        switch (m_state){
            case ControlState::Disabled:
                bgColor = m_bgColor.getDisabled();
                break;
            case ControlState::Hover:
                bgColor = m_bgColor.getHover();
                break;
            case ControlState::Pressed:
                bgColor = m_bgColor.getPressed();
                break;
            case ControlState::Normal:
            default:
                bgColor = m_bgColor.getNormal();
                break;
        }
        getRenderDevice()->setDrawColor(bgColor);
        getRenderDevice()->fillRect(drawRect);
    }
}

void ControlImpl::drawBorder(const SRect *pDrawRect){
    SRect drawRect;

    if(getBorderVisible()) {
        if (pDrawRect == nullptr){
            drawRect = getDrawRect();
        } else {
            drawRect = *pDrawRect;
        }

        SColor borderColor;
        switch (m_state){
            case ControlState::Disabled:
                borderColor = m_borderColor.getDisabled();
                break;
            case ControlState::Hover:
                borderColor = m_borderColor.getHover();
                break;
            case ControlState::Pressed:
                borderColor = m_borderColor.getPressed();
                break;
            case ControlState::Normal:
            default:
                borderColor = m_borderColor.getNormal();
                break;
        }
        getRenderDevice()->setDrawColor(borderColor);
        getRenderDevice()->drawRect(drawRect);
    }
}

void ControlImpl::resized(SRect newRect){
    m_rect.width = newRect.width;
    m_rect.height = newRect.height;
}
void ControlImpl::moved(SRect newRect){
    m_rect.left = newRect.left;
    m_rect.top = newRect.top;
}
//事件处理，返回值表示是否处理了该事件，true表示处理了，false表示未处理
bool ControlImpl::handleEvent(shared_ptr<Event> event){
    // 检查当前控件是否可见且启用
    if (getVisible() && getEnable()){
        // 提取事件坐标（仅对有位置的事件做遮挡检测）
        float mx = 0, my = 0;
        bool hasPos = false;
        if (event->m_type == EventType::MouseMove) {
            mx = event->mousePos.x; my = event->mousePos.y; hasPos = true;
        } else if (event->m_type == EventType::MouseDown || event->m_type == EventType::MouseUp) {
            mx = event->mouseButton.x; my = event->mouseButton.y; hasPos = true;
        } else if (event->m_type == EventType::MouseWheel) {
            mx = event->mouseWheel.x; my = event->mouseWheel.y; hasPos = true;
        } else if (event->m_type == EventType::FingerDown || event->m_type == EventType::FingerUp || event->m_type == EventType::FingerMotion) {
            mx = event->mousePos.x; my = event->mousePos.y; hasPos = true;
        }

        // 逆向遍历当前控件的所有子控件，保证后添加的控件先处理事件，因为后添加的控件在屏幕上位于上层
        // 使用副本遍历，防止子控件的 bringToFront 等操作修改 m_children 导致迭代器失效
        auto childrenCopy = m_children;
        for (auto it = childrenCopy.rbegin(); it != childrenCopy.rend(); ++it){
            // 对有位置的事件：检查当前子控件是否被更高层级的兄弟控件遮挡
            if (hasPos) {
                bool covered = false;
                for (auto coverIt = childrenCopy.rbegin(); coverIt != it; ++coverIt) {
                    if ((*coverIt)->getVisible() && (*coverIt)->isContainsPoint(mx, my)) {
                        covered = true;
                        break;
                    }
                }
                if (covered) continue;
            }
            if ((*it)->handleEvent(event)){
                return true;
            }
        }

    }
    return false;
}

bool ControlImpl::beforeEventHandlingWatcher(shared_ptr<Event> event){
    return false;
}

bool ControlImpl::afterEventHandlingWatcher(shared_ptr<Event> event){
    return false;
}

void ControlImpl::addControl(shared_ptr<Control> child){
    if (child == nullptr) return;

    // 如果控件已经存在，则直接返回
    if (std::find(m_children.begin(), m_children.end(), child) != m_children.end()){
        return;
    }
    // 继承父控件上下文（同时同步 m_eventQueueInstance）
    if (!child->getContext()) {
        child->setContext(m_context);
    }
    m_children.push_back(child);

    child->setParent(this);
    // 两阶段创建：父未挂树（m_context 无 render device）时不传播（避免触发
    // getRenderDevice 错误日志与空值缓存），由 getRenderDevice() 在 context
    // 就绪后经 parent 链重查
    if (m_context && m_context->renderDevice) {
        child->setRenderDevice(m_context->renderDevice);
    }

    stabilizeTopmostChildren();
}

void ControlImpl::stabilizeTopmostChildren() {
    // 收集所有 alwaysOnTop 子控件，移至末尾以保持 Z-order 顶层
    vector<shared_ptr<Control>> topmost;
    for (auto it = m_children.begin(); it != m_children.end(); ) {
        auto* impl = dynamic_cast<ControlImpl*>(it->get());
        if (impl && impl->m_alwaysOnTop) {
            topmost.push_back(std::move(*it));
            it = m_children.erase(it);
        } else {
            ++it;
        }
    }
    for (auto& child : topmost)
        m_children.push_back(std::move(child));
}

void ControlImpl::removeControl(shared_ptr<Control> child){
    m_children.erase(std::remove(m_children.begin(), m_children.end(), child), m_children.end());
}
// 只调用setParent的话，是不会添加到父控件的children中的，用于自行控制绘制逻辑和事件处理逻辑
// 如果要自动绘制和处理事件，需要调用addControl
//
// override 注意：
// - 可以加 if (m_parent == parent) return 阻止重复 setParent 触发的 recreate，
//   但仍需调用 ControlImpl::setParent(parent) 以确保 Renderer 继承等更新
//   （缩放传播由 updateChildScale() 处理，不走 setParent 路径）
// - 示例如 Label::setParent
void ControlImpl::setParent(Control *parent){
    m_parent = parent;
    // 继承父控件的实例上下文：非 children 挂载（如按钮的状态 Actor）无法经 setContext 传播，
    // 需在 setParent 时补齐，否则两阶段加载永远得不到触发时机
    if (parent != nullptr && m_context == nullptr) {
        m_context = parent->getContext();
    }
    inheritRenderer();
    m_xxScale = (parent==nullptr?m_xScale:m_xScale*parent->getScaleXX());
    m_yyScale = (parent==nullptr?m_yScale:m_yScale*parent->getScaleYY());
}

Control* ControlImpl::getParent(void){
    return m_parent;
}

float ControlImpl::getScaleXX(void){
    return m_xxScale;
}
float ControlImpl::getScaleYY(void){
    return m_yyScale;
}
void ControlImpl::setScaleX(float xScale){
    m_xScale = xScale;
    for (auto& child : m_children){
        updateChildScale(child.get()); // 直接更新复合缩放，不触发 setParent 的开销和脏标记问题
    }
}
void ControlImpl::setScaleY(float yScale){
    m_yScale = yScale;
    for (auto& child : m_children){
        updateChildScale(child.get()); // 直接更新复合缩放，不触发 setParent 的开销和脏标记问题
    }
}

// 注：override 时建议在最前面加 if (m_rect == rect) return;
// 以防止不必要的 recreate() cascade（参见 CheckBox::setRect, Label::setRect）
void ControlImpl::setRect(SRect rect){
    if (m_rect == rect) return;
    m_rect = rect;
}

SRect ControlImpl::getRect(void){
    return m_rect;
}
void ControlImpl::setMargin(Margin margin){
    m_margin = margin;

    recreate();
}
Margin ControlImpl::getMargin(void) const{
    return m_margin;
}
SRect ControlImpl::getMarginedRect(void) {
    Margin margin = getMargin();
    SRect marginRect = {
        margin.left,
        margin.top,
        getRect().width - margin.left - margin.right,
        getRect().height - margin.top - margin.bottom
    };
    return marginRect.normalize();
}

void ControlImpl::show(void){
    m_visible = true;
}

void ControlImpl::hide(void){
    m_visible = false;
}

void ControlImpl::setVisible(bool visible) {
    m_visible = visible;
}

bool ControlImpl::getVisible(void){
    return m_visible;
}

void ControlImpl::setEnable(bool enable){
    m_enable = enable;
    setState(enable ? ControlState::Normal : ControlState::Disabled);
}
bool ControlImpl::getEnable(void){
    return m_enable;
}

shared_ptr<Control> ControlImpl::getThis(void){
    return shared_from_this();
}

RenderDevice* ControlImpl::getRenderDevice(void) {
    if (m_renderDevice != nullptr) {
        return m_renderDevice;
    }
    if (m_parent != nullptr) {
        m_renderDevice = m_parent->getRenderDevice();
        return m_renderDevice;
    }
    // 不缓存 null：两阶段创建下 context 可能尚未就绪，后续可重查
    if (!UIContext::isActive(m_context)) return nullptr;
    auto rd = m_context->renderDevice;
    if (rd == nullptr) {
        Platform::Log("ControlImpl::getRenderDevice: No render device found! [%s ctx=%p]", typeid(*this).name(), (void*)m_context);
        return nullptr;
    }
    m_renderDevice = rd;
    return m_renderDevice;
}

void ControlImpl::setRenderDevice(RenderDevice* device) {
    if (m_renderDevice == device) return;

    m_renderDevice = device;
    for (auto& child : m_children){
        child->setRenderDevice(device);
    }
}

TextRenderer* ControlImpl::getTextRenderer(void) {
    if (m_textRenderer != nullptr) {
        return m_textRenderer;
    }
    if (m_parent != nullptr) {
        m_textRenderer = m_parent->getTextRenderer();
        return m_textRenderer;
    }
    // 不缓存 null：context 可能尚未就绪（两阶段创建），后续可重查
    if (!UIContext::isActive(m_context)) return nullptr;
    m_textRenderer = m_context->textRenderer;
    if (m_textRenderer == nullptr) {
        Platform::Log("ControlImpl::getTextRenderer: No text renderer found!");
        return nullptr;
    }
    return m_textRenderer;
}

void ControlImpl::setTextRenderer(TextRenderer* renderer) {
    if (m_textRenderer == renderer) return;

    m_textRenderer = renderer;
    for (auto& child : m_children){
        child->setTextRenderer(renderer);
    }
}

InputBackend* ControlImpl::getInputBackend(void) {
    if (m_inputBackend != nullptr) {
        return m_inputBackend;
    }
    if (m_parent != nullptr) {
        m_inputBackend = m_parent->getInputBackend();
        return m_inputBackend;
    }
    // 不缓存 null：context 可能尚未就绪（两阶段创建），后续可重查
    if (!UIContext::isActive(m_context)) return nullptr;
    m_inputBackend = m_context->inputBackend;
    if (m_inputBackend == nullptr) {
        Platform::Log("ControlImpl::getInputBackend: No input backend found!");
        return nullptr;
    }
    return m_inputBackend;
}

void ControlImpl::setInputBackend(InputBackend* backend) {
    if (m_inputBackend == backend) return;

    m_inputBackend = backend;
    for (auto& child : m_children){
        child->setInputBackend(backend);
    }
}

ResourceProvider* ControlImpl::getResourceProvider(void) {
    if (m_resourceProvider != nullptr) {
        return m_resourceProvider;
    }
    if (m_parent != nullptr) {
        m_resourceProvider = m_parent->getResourceProvider();
        return m_resourceProvider;
    }
    // 不缓存 null：context 可能尚未就绪（两阶段创建），后续可重查
    if (!UIContext::isActive(m_context)) return nullptr;
    m_resourceProvider = m_context->resourceProvider;
    return m_resourceProvider;
}

void ControlImpl::setResourceProvider(ResourceProvider* provider) {
    if (m_resourceProvider == provider) return;

    m_resourceProvider = provider;
    for (auto& child : m_children){
        child->setResourceProvider(provider);
    }
}

SRect ControlImpl::getDrawRect(void){
    Control *parent = getParent();
    SRect parentDrawRect;
    if (parent != nullptr){
        parentDrawRect = parent->getDrawRect();
        return {m_rect.left * parent->getScaleXX() + parentDrawRect.left,
            m_rect.top * parent->getScaleYY() + parentDrawRect.top,
            m_rect.width * getScaleXX(),
            m_rect.height * getScaleYY()};
    }
    return {m_rect.left, m_rect.top, m_rect.width * getScaleXX(), m_rect.height * getScaleYY()};
}

SRect ControlImpl::mapToDrawRect(SRect rect){
    SRect drawRect = m_frameDrawRectValid ? m_frameDrawRect : getDrawRect();
    return {rect.left * getScaleXX() + drawRect.left,
        rect.top * getScaleYY() + drawRect.top,
        rect.width * getScaleXX(),
        rect.height * getScaleYY()};
}
SPoint ControlImpl::mapToDrawPoint(SPoint point){
    SRect drawRect = m_frameDrawRectValid ? m_frameDrawRect : getDrawRect();
    return {point.x * getScaleXX() + drawRect.left,
        point.y * getScaleYY() + drawRect.top};
}
bool ControlImpl::isContainsPoint(float x, float y){
    SRect drawRect = getDrawRect();
    return drawRect.contains(x, y);
}

void ControlImpl::onMouseEnter(float x, float y){
    // 默认不做任何处理，子类可重写此方法
}

void ControlImpl::onMouseLeave(float x, float y){
    // 默认不做任何处理，子类可重写此方法
}

void ControlImpl::setTransparent(bool isTransparent){
    m_isTransparent = isTransparent;
}

void ControlImpl::setState(ControlState state){
    m_state = state;
}

void ControlImpl::setBackgroundStateColor(StateColor stateColor){
    m_bgColor = stateColor;
}
void ControlImpl::setBorderStateColor(StateColor stateColor){
    m_borderColor = stateColor;
    setBorderVisible(true);
}
void ControlImpl::setTextStateColor(StateColor stateColor){
    m_textColor = stateColor;
}
void ControlImpl::setTextShadowStateColor(StateColor stateColor){
    m_textShadowColor = stateColor;
}
StateColor ControlImpl::getBackgroundStateColor(void){
    return m_bgColor;
}
StateColor ControlImpl::getBorderStateColor(void){
    return m_borderColor;
}
StateColor ControlImpl::getTextStateColor(void){
    return m_textColor;
}
StateColor ControlImpl::getTextShadowStateColor(void){
    return m_textShadowColor;
}

void ControlImpl::setNormalStateBGColor(SColor color){
    m_bgColor.setNormal(color);
}
void ControlImpl::setHoverStateBGColor(SColor color){
    m_bgColor.setHover(color);
}
void ControlImpl::setPressedStateBGColor(SColor color){
    m_bgColor.setPressed(color);
}
void ControlImpl::setDisabledStateBGColor(SColor color){
    m_bgColor.setDisabled(color);
}
void ControlImpl::setNormalStateBDColor(SColor color){
    m_borderColor.setNormal(color);
}
void ControlImpl::setHoverStateBDColor(SColor color){
    m_borderColor.setHover(color);
}
void ControlImpl::setPressedStateBDColor(SColor color){
    m_borderColor.setPressed(color);
}
void ControlImpl::setDisabledStateBDColor(SColor color){
    m_borderColor.setDisabled(color);
}
void ControlImpl::setTextNormalStateColor(SColor color){
    m_textColor.setNormal(color);
}
void ControlImpl::setTextHoverStateColor(SColor color){
    m_textColor.setHover(color);
}
void ControlImpl::setTextPressedStateColor(SColor color){
    m_textColor.setPressed(color);
}
void ControlImpl::setTextDisabledStateColor(SColor color){
    m_textColor.setDisabled(color);
}
void ControlImpl::setTextShadowNormalStateColor(SColor color){
    m_textShadowColor.setNormal(color);
}
void ControlImpl::setTextShadowHoverStateColor(SColor color){
    m_textShadowColor.setHover(color);
}
void ControlImpl::setTextShadowPressedStateColor(SColor color){
    m_textShadowColor.setPressed(color);
}
void ControlImpl::setTextShadowDisabledStateColor(SColor color){
    m_textShadowColor.setDisabled(color);
}

void ControlImpl::setBorderVisible(bool isVisible){
    m_isBorderVisible = isVisible;
}
bool ControlImpl::getBorderVisible(void){
    return m_isBorderVisible;
}

void ControlImpl::triggerEvent(shared_ptr<Event> event){
    m_eventQueueInstance->pushEventIntoQueue(event);
}

ControlImpl::~ControlImpl() {
    // 静态/全局残留控件在进程退出阶段析构时 m_context 可能已随
    // DestroyInstance 释放（悬垂），必须经 isActive 确认实例存活后再访问。
    if (m_focusable && UIContext::isActive(m_context)) {
        FocusManager* fm = m_context->focusManager;
        if (fm) fm->unregisterControl(this);
    }
}

void ControlImpl::setFocused(bool focused, bool byKeyboard) {
    if (m_focused == focused) return;
    m_focused = focused;
    m_focusByKeyboard = byKeyboard && focused;
    if (focused) {
        onFocusGained(byKeyboard);
    } else {
        onFocusLost();
    }
    FocusManager* fm = m_context ? m_context->focusManager : nullptr;
    if (fm) fm->notifyControlFocused(this, byKeyboard);
}

void ControlImpl::setFocusable(bool focusable) {
    if (m_focusable == focusable) return;
    m_focusable = focusable;
    FocusManager* fm = m_context ? m_context->focusManager : nullptr;
    if (fm) {
        if (focusable)
            fm->registerControl(this);
        else
            fm->unregisterControl(this);
    }
}

void ControlImpl::setTabIndex(int index) {
    if (m_tabIndex == index) return;
    m_tabIndex = index;
}

void ControlImpl::onFocusGained(bool byKeyboard) {
}

void ControlImpl::onFocusLost() {
}

void ControlImpl::onFocusScopeActivated() {
}

void ControlImpl::drawFocusRing() {
    if (!m_focused || !m_showFocusRing) return;
    if (!m_focusRingAlwaysVisible && !m_focusByKeyboard) return;
    if (!m_frameDrawRectValid) return;

    SRect dr = m_frameDrawRect;
    auto* rd = getRenderDevice();
    if (!rd) return;

    if (m_focusRingStyle == FocusRingStyle::Solid) {
        // 3-layer ring: black outer (contrast on light bg), white middle (contrast on dark bg), color accent inner
        rd->setDrawColor(SColor(0, 0, 0, 150));
        rd->drawRect({dr.left, dr.top, dr.width, dr.height});
        rd->setDrawColor(SColor(255, 255, 255, 150));
        rd->drawRect({dr.left + 1, dr.top + 1, dr.width - 2, dr.height - 2});
        rd->setDrawColor(m_focusRingColor);
        rd->drawRect({dr.left + 2, dr.top + 2, dr.width - 4, dr.height - 4});
    } else {
        float dashLen = 6.0f;
        float gapLen = 4.0f;

        for (int pass = 0; pass < 3; pass++) {
            float inset = (float)(pass + 1);
            SColor c;
            switch (pass) {
                case 0: c = SColor(0, 0, 0, 150); break;
                case 1: c = SColor(255, 255, 255, 150); break;
                default: c = m_focusRingColor; break;
            }
            rd->setDrawColor(c);

            float left = dr.left + inset;
            float top = dr.top + inset;
            float right = dr.left + dr.width - inset;
            float bottom = dr.top + dr.height - inset;

            // Top edge
            for (float x = left; x < right; x += dashLen + gapLen) {
                float endX = std::min(x + dashLen, right);
                rd->drawLine(x, top, endX, top);
            }
            // Bottom edge
            for (float x = left; x < right; x += dashLen + gapLen) {
                float endX = std::min(x + dashLen, right);
                rd->drawLine(x, bottom, endX, bottom);
            }
            // Left edge
            for (float y = top; y < bottom; y += dashLen + gapLen) {
                float endY = std::min(y + dashLen, bottom);
                rd->drawLine(left, y, left, endY);
            }
            // Right edge
            for (float y = top; y < bottom; y += dashLen + gapLen) {
                float endY = std::min(y + dashLen, bottom);
                rd->drawLine(right, y, right, endY);
            }
        }
    }
}

void ControlImpl::inheritRenderer(void) {
    if (m_renderDevice == nullptr) {
        m_renderDevice = GET_RENDERDEVICE;
    }
    if (m_textRenderer == nullptr) {
        m_textRenderer = GET_CONTEXT ? GET_CONTEXT->textRenderer : nullptr;
    }
    if (m_inputBackend == nullptr) {
        m_inputBackend = GET_CONTEXT ? GET_CONTEXT->inputBackend : nullptr;
    }
    if (m_resourceProvider == nullptr) {
        m_resourceProvider = GET_CONTEXT ? GET_CONTEXT->resourceProvider : nullptr;
    }
}

// ── Property system ──
int ControlImpl::setColorProperty(const char* prop, SColor color) {
    if (strcmp(prop, PropertyNames::kBackground) == 0)           { setNormalStateBGColor(color);   return 1; }
    if (strcmp(prop, PropertyNames::kStateHover) == 0)          { setHoverStateBGColor(color);    return 1; }
    if (strcmp(prop, PropertyNames::kStatePressed) == 0)        { setPressedStateBGColor(color);  return 1; }
    if (strcmp(prop, PropertyNames::kStateDisabled) == 0)       { setDisabledStateBGColor(color); return 1; }
    if (strcmp(prop, PropertyNames::kBorder) == 0)               { setNormalStateBDColor(color);   return 1; }
    if (strcmp(prop, PropertyNames::kBorderHover) == 0)         { setHoverStateBDColor(color);    return 1; }
    if (strcmp(prop, PropertyNames::kBorderPressed) == 0)       { setPressedStateBDColor(color);  return 1; }
    if (strcmp(prop, PropertyNames::kBorderDisabled) == 0)      { setDisabledStateBDColor(color); return 1; }
    if (strcmp(prop, PropertyNames::kText) == 0)                 { setTextNormalStateColor(color); return 1; }
    if (strcmp(prop, PropertyNames::kTextHover) == 0)           { setTextHoverStateColor(color);  return 1; }
    if (strcmp(prop, PropertyNames::kTextPressed) == 0)         { setTextPressedStateColor(color);return 1; }
    if (strcmp(prop, PropertyNames::kTextDisabled) == 0)        { setTextDisabledStateColor(color);return 1; }
    if (strcmp(prop, PropertyNames::kTextShadow) == 0)          { setTextShadowNormalStateColor(color);return 1; }
    if (strcmp(prop, PropertyNames::kTextShadowHover) == 0)    { setTextShadowHoverStateColor(color);return 1; }
    if (strcmp(prop, PropertyNames::kTextShadowPressed) == 0)  { setTextShadowPressedStateColor(color);return 1; }
    if (strcmp(prop, PropertyNames::kTextShadowDisabled) == 0) { setTextShadowDisabledStateColor(color);return 1; }
    return 0;
}

int ControlImpl::setStateColorProperty(const char* prop, StateColor sc) {
    if (strcmp(prop, PropertyNames::kBackground) == 0)  { setBackgroundStateColor(sc); return 1; }
    if (strcmp(prop, PropertyNames::kBorder) == 0)      { setBorderStateColor(sc);     return 1; }
    if (strcmp(prop, PropertyNames::kText) == 0)        { setTextStateColor(sc);       return 1; }
    if (strcmp(prop, PropertyNames::kTextShadow) == 0)  { setTextShadowStateColor(sc); return 1; }
    return 0;
}

int ControlImpl::setIntProperty(const char* prop, int value) {
    return 0;
}

int ControlImpl::setFloatProperty(const char* prop, float value) {
    return 0;
}

int ControlImpl::setStringProperty(const char* prop, const char* value) {
    return 0;
}

int ControlImpl::setBoolProperty(const char* prop, int value) {
    bool b = value != 0;
    if (strcmp(prop, PropertyNames::kVisible) == 0)         { setVisible(b); return 1; }
    if (strcmp(prop, PropertyNames::kEnabled) == 0)         { setEnable(b);  return 1; }
    if (strcmp(prop, PropertyNames::kTransparent) == 0)     { setTransparent(b); return 1; }
    if (strcmp(prop, PropertyNames::kBorderVisible) == 0)  { setBorderVisible(b); return 1; }
    return 0;
}

int ControlImpl::setEnumProperty(const char* prop, const char* value) {
    return 0;
}

int ControlImpl::setPtrProperty(const char* prop, void* value) {
    return 0;
}

int ControlImpl::getPtrProperty(const char* prop, void*& out) {
    return 0;
}

int ControlImpl::setCallbackProperty(const char* event, void (*cb)(void*, const void*, void*), void* userData) {
    if (!event || !cb) return 0;
    m_cCallbacks[event] = {cb, userData};
    return 1;
}

void ControlImpl::fireCCallback(const char* eventName, CCallbackData data, const void* ptr) {
    auto it = m_cCallbacks.find(eventName);
    if (it == m_cCallbacks.end() || !it->second.cb) return;
    UIEventData evt;
    memset(&evt, 0, sizeof(evt));
    evt.eventName = eventName;
    switch (data) {
    case CCallbackData::Int:       if (ptr) evt.data.intVal = *static_cast<const int*>(ptr); break;
    case CCallbackData::Float:     if (ptr) evt.data.floatVal = *static_cast<const float*>(ptr); break;
    case CCallbackData::String:    if (ptr) evt.data.strVal = static_cast<const char*>(ptr); break;
    case CCallbackData::Ptr:       if (ptr) evt.data.ptrVal = const_cast<void*>(ptr); break;
    case CCallbackData::Selection: if (ptr) { auto* s = static_cast<const SelectionPayload*>(ptr); evt.data.selection = {s->idx, s->val}; } break;
    case CCallbackData::TreeNode:  if (ptr) { auto* t = static_cast<const TreeNodePayload*>(ptr); evt.data.treeNode = {t->id, t->userData}; } break;
    case CCallbackData::Color:     if (ptr) { auto* c = static_cast<const ColorPayload*>(ptr); evt.data.color = {c->r, c->g, c->b, c->a}; } break;
    default: break;
    }
    it->second.cb(reinterpret_cast<UIControlHandle>(this), &evt, it->second.userData);
}

int ControlImpl::getColorProperty(const char* prop, SColor& out) {
    StateColor bg = getBackgroundStateColor();
    StateColor bd = getBorderStateColor();
    StateColor txt = getTextStateColor();
    StateColor shd = getTextShadowStateColor();
    if (strcmp(prop, "background") == 0)          { out = bg.getNormal();  return 1; }
    if (strcmp(prop, "background.hover") == 0)    { out = bg.getHover();   return 1; }
    if (strcmp(prop, "background.pressed") == 0)  { out = bg.getPressed(); return 1; }
    if (strcmp(prop, "background.disabled") == 0) { out = bg.getDisabled();return 1; }
    if (strcmp(prop, "border") == 0)              { out = bd.getNormal();  return 1; }
    if (strcmp(prop, "border.hover") == 0)        { out = bd.getHover();   return 1; }
    if (strcmp(prop, "border.pressed") == 0)      { out = bd.getPressed(); return 1; }
    if (strcmp(prop, "border.disabled") == 0)     { out = bd.getDisabled();return 1; }
    if (strcmp(prop, "text") == 0)                { out = txt.getNormal();  return 1; }
    if (strcmp(prop, "text.hover") == 0)          { out = txt.getHover();   return 1; }
    if (strcmp(prop, "text.pressed") == 0)        { out = txt.getPressed(); return 1; }
    if (strcmp(prop, "text.disabled") == 0)       { out = txt.getDisabled();return 1; }
    if (strcmp(prop, "text-shadow") == 0)         { out = shd.getNormal();  return 1; }
    return 0;
}

int ControlImpl::getStateColorProperty(const char* prop, StateColor& out) {
    if (strcmp(prop, "background") == 0) { out = getBackgroundStateColor(); return 1; }
    if (strcmp(prop, "border") == 0)     { out = getBorderStateColor();     return 1; }
    if (strcmp(prop, "text") == 0)       { out = getTextStateColor();       return 1; }
    if (strcmp(prop, "text-shadow") == 0){ out = getTextShadowStateColor(); return 1; }
    return 0;
}

int ControlImpl::getBoolProperty(const char* prop, int& out) {
    if (strcmp(prop, "visible") == 0)         { out = getVisible() ? 1 : 0; return 1; }
    if (strcmp(prop, "enabled") == 0)         { out = getEnable()  ? 1 : 0; return 1; }
    if (strcmp(prop, "transparent") == 0)     { out = getTransparent() ? 1 : 0; return 1; }
    if (strcmp(prop, "border-visible") == 0)  { out = getBorderVisible() ? 1 : 0; return 1; }
    return 0;
}

int ControlImpl::getIntProperty(const char* prop, int& out) {
    return 0;
}

int ControlImpl::getFloatProperty(const char* prop, float& out) {
    return 0;
}

int ControlImpl::getStringProperty(const char* prop, const char*& out) {
    return 0;
}

int ControlImpl::getEnumProperty(const char* prop, const char*& out) {
    return 0;
}
