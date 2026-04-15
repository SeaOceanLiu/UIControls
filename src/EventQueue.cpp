// #include "EventQueue.h"
#include "ControlBase.h"

std::mutex EventQueue::m_mtxForEventQueue; // 定义静态成员变量
std::mutex EventQueue::m_mtxForBeforeEventHandlingWatcher;
std::mutex EventQueue::m_mtxForAfterEventHandlingWatcher;

void EventQueue::pushEventIntoQueue(shared_ptr<Event> event){
    m_mtxForEventQueue.lock();
    m_eventQueue.push(event);
    m_mtxForEventQueue.unlock();
}

shared_ptr<Event> EventQueue::popEventFromQueue(void){
    m_mtxForEventQueue.lock();
    if (m_eventQueue.empty()){
        m_mtxForEventQueue.unlock();
        return nullptr;
    }
    shared_ptr<Event> event = m_eventQueue.front();
    m_eventQueue.pop();
    m_mtxForEventQueue.unlock();
    return event;
}
void EventQueue::clear(void){
    //m_mtxForEventQueue.lock();
    shared_ptr<Event> event = popEventFromQueue();
    while(event != nullptr){
        // 改为shared_ptr后，不需要手动释放内存
        event = popEventFromQueue();
    }
    //m_mtxForEventQueue.unlock();

    // 清理事件处理观察者
    for(auto& it : m_beforeEventHandlingWatcherMap){
        it.second.clear();
    }
    m_beforeEventHandlingWatcherMap.clear();
    for(auto& it : m_afterEventHandlingWatcherMap){
        it.second.clear();
    }
    m_afterEventHandlingWatcherMap.clear();
}

bool EventQueue::addBeforeEventHandlingWatcher(EventName eventName, shared_ptr<Control> control){
    if (control == nullptr || eventName == EventName::None) return false;

    m_mtxForBeforeEventHandlingWatcher.lock();
    if (m_beforeEventHandlingWatcherMap.find(eventName) == m_beforeEventHandlingWatcherMap.end() ||
        std::find(m_beforeEventHandlingWatcherMap[eventName].begin(), m_beforeEventHandlingWatcherMap[eventName].end(), control) ==
            m_beforeEventHandlingWatcherMap[eventName].end()){
        m_beforeEventHandlingWatcherMap[eventName].push_back(control);
    }
    m_mtxForBeforeEventHandlingWatcher.unlock();

    return true;
}

bool EventQueue::removeBeforeEventHandlingWatcher(EventName eventName, shared_ptr<Control> control){
    if (control == nullptr || eventName == EventName::None) return false;

    m_mtxForBeforeEventHandlingWatcher.lock();
    auto it = m_beforeEventHandlingWatcherMap.find(eventName);
    if (it != m_beforeEventHandlingWatcherMap.end()){
        auto it2 = std::find(it->second.begin(), it->second.end(), control);
        if (it2 != it->second.end()){
            it->second.erase(it2);
            m_mtxForBeforeEventHandlingWatcher.unlock();
            return true;
        }
    }
    m_mtxForBeforeEventHandlingWatcher.unlock();
    return false;
}

void EventQueue::notifyBeforeEventHandlingWatchers(shared_ptr<Event> event){
    if (event->m_eventName == EventName::None) return;

    m_mtxForBeforeEventHandlingWatcher.lock();
    auto it = m_beforeEventHandlingWatcherMap.find(event->m_eventName);
    if (it != m_beforeEventHandlingWatcherMap.end()){
        for (auto control : it->second){
            if(control->beforeEventHandlingWatcher(event))
                break; // 如果有控件申请吃掉事件，则不再通知其他控件
        }
    }
    m_mtxForBeforeEventHandlingWatcher.unlock();
}


bool EventQueue::addAfterEventHandlingWatcher(EventName eventName, shared_ptr<Control> control){
    if (control == nullptr || eventName == EventName::None) return false;

    m_mtxForAfterEventHandlingWatcher.lock();
    if (m_afterEventHandlingWatcherMap.find(eventName) == m_afterEventHandlingWatcherMap.end() ||
        std::find(m_afterEventHandlingWatcherMap[eventName].begin(), m_afterEventHandlingWatcherMap[eventName].end(), control) ==
            m_afterEventHandlingWatcherMap[eventName].end()){
        m_beforeEventHandlingWatcherMap[eventName].push_back(control);
    }
    m_mtxForAfterEventHandlingWatcher.unlock();

    return true;
}

bool EventQueue::removeAfterEventHandlingWatcher(EventName eventName, shared_ptr<Control> control){
    if (control == nullptr || eventName == EventName::None) return false;

    m_mtxForAfterEventHandlingWatcher.lock();
    auto it = m_beforeEventHandlingWatcherMap.find(eventName);
    if (it != m_beforeEventHandlingWatcherMap.end()){
        auto it2 = std::find(it->second.begin(), it->second.end(), control);
        if (it2 != it->second.end()){
            it->second.erase(it2);
            m_mtxForAfterEventHandlingWatcher.unlock();
            return true;
        }
    }
    m_mtxForAfterEventHandlingWatcher.unlock();
    return false;
}

void EventQueue::notifyAfterEventHandlingWatchers(shared_ptr<Event> event){
    if (event->m_eventName == EventName::None) return;

    m_mtxForAfterEventHandlingWatcher.lock();
    auto it = m_beforeEventHandlingWatcherMap.find(event->m_eventName);
    if (it != m_beforeEventHandlingWatcherMap.end()){
        for (auto control : it->second){
            if(control->afterEventHandlingWatcher(event))
                break; // 如果有控件申请吃掉事件，则不再通知其他控件
        }
    }
    m_mtxForAfterEventHandlingWatcher.unlock();
}
