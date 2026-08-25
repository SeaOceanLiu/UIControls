#include "FocusManager.h"
#include "ControlBase.h"
#include <algorithm>

FocusManager::FocusManager()
{
}

FocusManager::~FocusManager()
{
}

void FocusManager::registerControl(Control* ctl)
{
    if (!ctl) return;
    if (std::find(m_controls.begin(), m_controls.end(), ctl) != m_controls.end())
        return;
    m_controls.push_back(ctl);
}

void FocusManager::unregisterControl(Control* ctl)
{
    if (!ctl) return;
    auto it = std::remove(m_controls.begin(), m_controls.end(), ctl);
    if (it != m_controls.end())
        m_controls.erase(it, m_controls.end());

    if (m_currentFocused == ctl)
        m_currentFocused = nullptr;

    // Also remove from boundaries if present
    auto bit = std::remove(m_boundaries.begin(), m_boundaries.end(), ctl);
    if (bit != m_boundaries.end())
        m_boundaries.erase(bit, m_boundaries.end());
}

void FocusManager::registerBoundary(Control* boundary)
{
    if (!boundary) return;
    if (std::find(m_boundaries.begin(), m_boundaries.end(), boundary) != m_boundaries.end())
        return;
    m_boundaries.push_back(boundary);
}

void FocusManager::unregisterBoundary(Control* boundary)
{
    if (!boundary) return;
    auto it = std::remove(m_boundaries.begin(), m_boundaries.end(), boundary);
    if (it != m_boundaries.end())
        m_boundaries.erase(it, m_boundaries.end());
}

int FocusManager::getVisibleBoundaryCount() const
{
    int count = 0;
    for (Control* b : m_boundaries) {
        if (b && b->getVisible()) count++;
    }
    return count;
}

void FocusManager::notifyControlFocused(Control* ctl, bool byKeyboard) {
    if (!ctl) return;
    if (ctl->getFocused()) {
        if (m_currentFocused == ctl) return;
        if (m_currentFocused)
            m_currentFocused->setFocused(false, false);
        m_currentFocused = ctl;
        // 任何路径下焦点进入某 scope（如 WinFrame）时，激活该 scope（提升到顶层）
        if (Control* s = findFocusScope(ctl)) s->onFocusScopeActivated();
    } else {
        if (m_currentFocused == ctl)
            m_currentFocused = nullptr;
    }
}

void FocusManager::clearFocus() {
    if (m_currentFocused) {
        m_currentFocused->setFocused(false, false);
        m_currentFocused = nullptr;
    }
}

bool FocusManager::focusControl(Control* ctl) {
    if (!ctl || !ctl->isFocusable() || !ctl->getVisible() || !ctl->getEnable())
        return false;
    if (m_currentFocused && m_currentFocused != ctl)
        m_currentFocused->setFocused(false, false);
    ctl->setFocused(true, true);
    m_currentFocused = ctl;
    return true;
}

Control* FocusManager::findFocusScope(Control* ctl)
{
    // 从父级起查：边界容器自身（如 TabControl 页签条）属于【父作用域】，
    // 其内部控件才属于以该容器为边界的子作用域（Tab=层内循环，Ctrl+Tab=跨层）
    ctl = ctl ? ctl->getParent() : nullptr;
    while (ctl) {
        if (ctl->isFocusBoundary()) return ctl;
        ctl = ctl->getParent();
    }
    return nullptr;
}

bool FocusManager::isInScope(Control* candidate, Control* scope)
{
    if (!scope) return true;
    Control* cs = findFocusScope(candidate);
    return cs == scope;
}

bool FocusManager::isEffectivelyVisible(Control* ctl)
{
    // 自身 + 任一祖先隐藏 → 视为隐藏（如 TabControl 隐藏页内的控件）
    while (ctl) {
        if (!ctl->getVisible()) return false;
        ctl = ctl->getParent();
    }
    return true;
}

bool FocusManager::isDescendantOf(Control* ancestor, Control* candidate)
{
    if (!ancestor || !candidate) return false;
    Control* p = candidate->getParent();
    while (p) {
        if (p == ancestor) return true;
        p = p->getParent();
    }
    return false;
}

bool FocusManager::focusNext(Control* current)
{
    Control* scope = current ? findFocusScope(current) : nullptr;

    auto it = current ? std::find(m_controls.begin(), m_controls.end(), current)
                      : m_controls.end();
    if (it != m_controls.end()) ++it;

    for (size_t i = 0; i < m_controls.size(); i++) {
        if (it == m_controls.end()) it = m_controls.begin();

        Control* c = *it;
        if (isInScope(c, scope) && isEffectivelyVisible(c) && c->getEnable()) {
            if (c != current) {
                if (current) current->setFocused(false, false);
                c->setFocused(true, true);
                m_currentFocused = c;
                if (Control* newScope = findFocusScope(c)) newScope->onFocusScopeActivated();
                return true;
            }
            break;
        }
        ++it;
    }
    return false;
}

bool FocusManager::focusPrev(Control* current)
{
    Control* scope = current ? findFocusScope(current) : nullptr;

    auto it = current ? std::find(m_controls.begin(), m_controls.end(), current)
                      : m_controls.begin();
    if (it == m_controls.begin()) it = m_controls.end();
    if (it != m_controls.begin()) --it;

    for (size_t i = 0; i < m_controls.size(); i++) {
        Control* c = *it;
        if (isInScope(c, scope) && isEffectivelyVisible(c) && c->getEnable()) {
            if (c != current) {
                if (current) current->setFocused(false, false);
                c->setFocused(true, true);
                m_currentFocused = c;
                if (Control* newScope = findFocusScope(c)) newScope->onFocusScopeActivated();
                return true;
            }
            break;
        }
        if (it == m_controls.begin()) it = m_controls.end();
        if (it != m_controls.begin()) --it;
    }
    return false;
}

bool FocusManager::focusNextScope()
{
    Control* current = m_currentFocused;
    if (!current) return focusFirstBoundaryInteriorNext(nullptr);

    // 层 1→2：当前在边界容器自身（如 TabControl 页签条）→ 进入其内部第一控件
    if (std::find(m_boundaries.begin(), m_boundaries.end(), current) != m_boundaries.end()) {
        if (focusFirstInScope(current)) return true;
    }

    Control* currentScope = findFocusScope(current);

    // 层 2→1：当前在某非根作用域内部 → 退出到该作用域的边界容器自身（页签条）
    if (currentScope && findFocusScope(currentScope) != nullptr) {
        // 非根边界容器：退出到容器自身
        if (current) current->setFocused(false, false);
        currentScope->setFocused(true, true);
        m_currentFocused = currentScope;
        return true;
    }

    // 根作用域（外部控件）→ 进入下一个边界容器内部
    return focusFirstBoundaryInteriorNext(currentScope);
}

bool FocusManager::focusFirstBoundaryInteriorNext(Control* fromScope)
{
    Control* current = m_currentFocused;
    Control* currentScope = fromScope;

    // Find current scope's index in boundaries
    int startIdx = -1;
    for (size_t i = 0; i < m_boundaries.size(); i++) {
        if (m_boundaries[i] == currentScope) {
            startIdx = (int)i;
            break;
        }
    }

    // Try each boundary from next to end, then wrap around
    for (size_t offset = 1; offset <= m_boundaries.size(); offset++) {
        int idx = (startIdx + (int)offset) % (int)m_boundaries.size();
        Control* boundary = m_boundaries[idx];
        if (!boundary->getVisible()) continue;

        if (focusFirstInScope(boundary))
            return true;
    }

    // Fallback: find first visible enabled control in the flat list
    for (Control* c : m_controls) {
        if (isEffectivelyVisible(c) && c->getEnable()) {
            if (current && current != c) current->setFocused(false, false);
            c->setFocused(true, true);
            m_currentFocused = c;
            return true;
        }
    }
    return false;
}

bool FocusManager::focusPrevScope()
{
    Control* current = m_currentFocused;
    if (!current) return focusFirstBoundaryInteriorPrev(nullptr);

    // 边界容器自身 → 进入其内部（反向：末个控件场景暂同向，取首控件）
    if (std::find(m_boundaries.begin(), m_boundaries.end(), current) != m_boundaries.end()) {
        if (focusFirstInScope(current)) return true;
    }

    Control* currentScope = findFocusScope(current);

    // 非根作用域内部 → 退出到容器自身
    if (currentScope && findFocusScope(currentScope) != nullptr) {
        if (current) current->setFocused(false, false);
        currentScope->setFocused(true, true);
        m_currentFocused = currentScope;
        return true;
    }

    return focusFirstBoundaryInteriorPrev(currentScope);
}

bool FocusManager::focusFirstBoundaryInteriorPrev(Control* fromScope)
{
    Control* current = m_currentFocused;
    Control* currentScope = fromScope;

    int startIdx = -1;
    for (size_t i = 0; i < m_boundaries.size(); i++) {
        if (m_boundaries[i] == currentScope) {
            startIdx = (int)i;
            break;
        }
    }

    for (size_t offset = 1; offset <= m_boundaries.size(); offset++) {
        int idx = (startIdx - (int)offset);
        while (idx < 0) idx += (int)m_boundaries.size();
        idx = idx % (int)m_boundaries.size();
        Control* boundary = m_boundaries[idx];
        if (!boundary->getVisible()) continue;

        if (focusFirstInScope(boundary))
            return true;
    }

    for (Control* c : m_controls) {
        if (isEffectivelyVisible(c) && c->getEnable()) {
            if (current && current != c) current->setFocused(false, false);
            c->setFocused(true, true);
            m_currentFocused = c;
            return true;
        }
    }
    return false;
}

bool FocusManager::focusFirstInScope(Control* scope)
{
    if (!scope) return false;
    // 键盘/焦点切换激活 scope 时通知（如 WinFrame 提升到顶层）
    scope->onFocusScopeActivated();
    for (Control* c : m_controls) {
        if (isDescendantOf(scope, c) && isEffectivelyVisible(c) && c->getEnable()) {
            if (m_currentFocused && m_currentFocused != c)
                m_currentFocused->setFocused(false, false);
            c->setFocused(true, true);
            m_currentFocused = c;
            return true;
        }
    }
    // Fallback: focus the scope itself if it's focusable and visible
    if (scope->isFocusable() && scope->getVisible()) {
        if (m_currentFocused && m_currentFocused != scope)
            m_currentFocused->setFocused(false, false);
        scope->setFocused(true, true);
        m_currentFocused = scope;
        return true;
    }
    return false;
}
