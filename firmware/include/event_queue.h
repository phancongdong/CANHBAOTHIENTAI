#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H

#include <vector>
#include <cstddef>
#include "event.h"

class EventQueue {
public:
    explicit EventQueue(size_t max_capacity = 50);

    bool push(const TelemetryEvent& event);
    bool pop(TelemetryEvent& event);
    bool peek(TelemetryEvent& event) const;
    void removeFront();

    size_t size() const { return m_queue.size(); }
    bool empty() const { return m_queue.empty(); }
    size_t capacity() const { return m_max_capacity; }
    size_t droppedHeartbeats() const { return m_dropped_heartbeats; }
    void clear();

private:
    size_t m_max_capacity;
    size_t m_dropped_heartbeats = 0;
    std::vector<TelemetryEvent> m_queue;
};

#endif // EVENT_QUEUE_H
