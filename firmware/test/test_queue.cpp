#include <cassert>
#include <iostream>
#include "../include/event_queue.h"

void test_queue_fifo_and_capacity() {
    EventQueue q(3);
    assert(q.empty());
    assert(q.size() == 0);

    TelemetryEvent ev1; ev1.event = "heartbeat"; ev1.seq = 1;
    TelemetryEvent ev2; ev2.event = "heartbeat"; ev2.seq = 2;
    TelemetryEvent ev3; ev3.event = "heartbeat"; ev3.seq = 3;

    assert(q.push(ev1));
    assert(q.push(ev2));
    assert(q.push(ev3));
    assert(q.size() == 3);

    // Queue is full, another heartbeat should be discarded
    TelemetryEvent ev4; ev4.event = "heartbeat"; ev4.seq = 4;
    assert(!q.push(ev4));
    assert(q.droppedHeartbeats() == 1);
    assert(q.size() == 3);

    // Pop check FIFO
    TelemetryEvent out;
    assert(q.pop(out));
    assert(out.seq == 1);
    assert(q.pop(out));
    assert(out.seq == 2);
    assert(q.size() == 1);
    std::cout << "[PASS] test_queue_fifo_and_capacity\n";
}

void test_queue_evicts_heartbeat_for_alarm() {
    EventQueue q(2);
    TelemetryEvent hb1; hb1.event = "heartbeat"; hb1.seq = 1;
    TelemetryEvent hb2; hb2.event = "heartbeat"; hb2.seq = 2;
    q.push(hb1);
    q.push(hb2);
    assert(q.size() == 2);

    // Critical alarm event comes in
    TelemetryEvent alarm; alarm.event = "alarm_started"; alarm.seq = 3;
    assert(q.push(alarm));
    assert(q.size() == 2);
    assert(q.droppedHeartbeats() == 1);

    TelemetryEvent first;
    q.pop(first);
    assert(first.seq == 2); // hb1 was evicted to preserve oldest hb or make room for alarm

    TelemetryEvent second;
    q.pop(second);
    assert(second.event == "alarm_started");
    assert(second.seq == 3);
    std::cout << "[PASS] test_queue_evicts_heartbeat_for_alarm\n";
}

int main() {
    std::cout << "Running EventQueue Unit Tests...\n";
    test_queue_fifo_and_capacity();
    test_queue_evicts_heartbeat_for_alarm();
    std::cout << "All EventQueue tests passed successfully!\n";
    return 0;
}
