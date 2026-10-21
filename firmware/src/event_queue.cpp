#include "event_queue.h"

EventQueue::EventQueue(size_t max_capacity)
    : m_max_capacity(max_capacity), m_dropped_heartbeats(0) {}

bool EventQueue::push(const TelemetryEvent& event) {
    if (m_queue.size() < m_max_capacity) {
        m_queue.push_back(event);
        return true;
    }

    // Queue is full. Apply policy:
    if (event.event == "heartbeat") {
        // Discard incoming heartbeat to prioritize existing events
        m_dropped_heartbeats++;
        return false;
    }

    // Incoming event is a critical state transition (alarm_started, alarm_cleared, sensor_fault)
    // Find oldest heartbeat in the queue to drop
    for (auto it = m_queue.begin(); it != m_queue.end(); ++it) {
        if (it->event == "heartbeat") {
            m_queue.erase(it);
            m_dropped_heartbeats++;
            m_queue.push_back(event);
            return true;
        }
    }

    // No heartbeat found to evict. Evict oldest non-alarm event if any, or oldest element
    m_queue.erase(m_queue.begin());
    m_queue.push_back(event);
    return true;
}

bool EventQueue::pop(TelemetryEvent& event) {
    if (m_queue.empty()) {
        return false;
    }
    event = m_queue.front();
    m_queue.erase(m_queue.begin());
    return true;
}

bool EventQueue::peek(TelemetryEvent& event) const {
    if (m_queue.empty()) {
        return false;
    }
    event = m_queue.front();
    return true;
}

void EventQueue::removeFront() {
    if (!m_queue.empty()) {
        m_queue.erase(m_queue.begin());
    }
}

void EventQueue::clear() {
    m_queue.clear();
    m_dropped_heartbeats = 0;
}
