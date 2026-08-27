// #include "EventQueue.h"
#include "ControlBase.h"

std::mutex EventQueue::m_mtxForEventQueue;
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
    shared_ptr<Event> event = popEventFromQueue();
    while(event != nullptr){
        event = popEventFromQueue();
    }

    for(auto& it : m_beforeEventHandlingWatcherMap){
        it.second.clear();
    }
    m_beforeEventHandlingWatcherMap.clear();
    for(auto& it : m_afterEventHandlingWatcherMap){
        it.second.clear();
    }
    m_afterEventHandlingWatcherMap.clear();
}

bool EventQueue::addBeforeEventHandlingWatcher(EventType eventType, shared_ptr<Control> control){
    if (control == nullptr || eventType == EventType::None) return false;

    m_mtxForBeforeEventHandlingWatcher.lock();
    auto& vec = m_beforeEventHandlingWatcherMap[eventType];
    auto it = std::find_if(vec.begin(), vec.end(),
        [&control](const weak_ptr<Control>& wp) {
            auto sp = wp.lock();
            return sp && sp == control;
        });
    if (it == vec.end()) {
        vec.push_back(weak_ptr<Control>(control));
    }
    m_mtxForBeforeEventHandlingWatcher.unlock();

    return true;
}

bool EventQueue::removeBeforeEventHandlingWatcher(EventType eventType, shared_ptr<Control> control){
    if (control == nullptr || eventType == EventType::None) return false;

    m_mtxForBeforeEventHandlingWatcher.lock();
    auto it = m_beforeEventHandlingWatcherMap.find(eventType);
    if (it != m_beforeEventHandlingWatcherMap.end()){
        auto it2 = std::find_if(it->second.begin(), it->second.end(),
            [&control](const weak_ptr<Control>& wp) {
                auto sp = wp.lock();
                return sp && sp == control;
            });
        if (it2 != it->second.end()){
            it->second.erase(it2);
            m_mtxForBeforeEventHandlingWatcher.unlock();
            return true;
        }
    }
    m_mtxForBeforeEventHandlingWatcher.unlock();
    return false;
}

bool EventQueue::notifyBeforeEventHandlingWatchers(shared_ptr<Event> event){
    if (event->m_type == EventType::None) return false;

    // 锁内仅拷贝快照；回调在锁外执行——watcher 回调可能注销自身/他人
    // 注册（如菜单点击关闭），锁内回调会与注销锁互斥死锁，
    // 且遍历期间修改 vector 会迭代器失效
    vector<weak_ptr<Control>> snapshots;
    {
        m_mtxForBeforeEventHandlingWatcher.lock();
        auto it = m_beforeEventHandlingWatcherMap.find(event->m_type);
        if (it != m_beforeEventHandlingWatcherMap.end()){
            snapshots = it->second;
        }
        m_mtxForBeforeEventHandlingWatcher.unlock();
    }
    bool consumed = false;
    for (auto& weakControl : snapshots){
        auto control = weakControl.lock();
        if (control && control->beforeEventHandlingWatcher(event)){
            consumed = true;
            break;
        }
    }
    return consumed;
}


bool EventQueue::addAfterEventHandlingWatcher(EventType eventType, shared_ptr<Control> control){
    if (control == nullptr || eventType == EventType::None) return false;

    m_mtxForAfterEventHandlingWatcher.lock();
    auto& vec = m_afterEventHandlingWatcherMap[eventType];
    auto it = std::find_if(vec.begin(), vec.end(),
        [&control](const weak_ptr<Control>& wp) {
            auto sp = wp.lock();
            return sp && sp == control;
        });
    if (it == vec.end()) {
        vec.push_back(weak_ptr<Control>(control));
    }
    m_mtxForAfterEventHandlingWatcher.unlock();

    return true;
}

bool EventQueue::removeAfterEventHandlingWatcher(EventType eventType, shared_ptr<Control> control){
    if (control == nullptr || eventType == EventType::None) return false;

    m_mtxForAfterEventHandlingWatcher.lock();
    auto it = m_afterEventHandlingWatcherMap.find(eventType);
    if (it != m_afterEventHandlingWatcherMap.end()){
        auto it2 = std::find_if(it->second.begin(), it->second.end(),
            [&control](const weak_ptr<Control>& wp) {
                auto sp = wp.lock();
                return sp && sp == control;
            });
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
    if (event->m_type == EventType::None) return;

    vector<weak_ptr<Control>> snapshots;
    {
        m_mtxForAfterEventHandlingWatcher.lock();
        auto it = m_afterEventHandlingWatcherMap.find(event->m_type);
        if (it != m_afterEventHandlingWatcherMap.end()){
            snapshots = it->second;
        }
        m_mtxForAfterEventHandlingWatcher.unlock();
    }
    // 回调在锁外执行（同 notifyBefore：回调内注销注册不锁死、不改坏遍历）
    for (auto& weakControl : snapshots){
        auto control = weakControl.lock();
        if (control && control->afterEventHandlingWatcher(event))
            break;
    }
}
