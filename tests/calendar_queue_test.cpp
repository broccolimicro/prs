#include <gtest/gtest.h>
#include <prs/calendar_queue.h>
#include "test_helpers.h"
#include <string>
#include <limits>
#include <vector>
#include <algorithm>
#include <random>

// Simple struct to use as test event with defined time
struct TestEvent {
	uint64_t time;
	std::string name;
	
	TestEvent() : time(0), name("") {}
	TestEvent(uint64_t t, const std::string& n) : time(t), name(n) {}
	
	// For debugging
	std::string to_string() const {
		return name + ":" + std::to_string(time);
	}
};

// Custom priority function for TestEvent
struct TestEventPriority {
	uint64_t operator()(const TestEvent &e) {
		return e.time;
	}
};

// Type aliases for our calendar queue types
using TestQueue = calendar_queue<TestEvent, TestEventPriority>;

// Helper to add a test event with given time and name
TestQueue::event* addEvent(TestQueue* queue, uint64_t time, const std::string& name) {
	return queue->push(TestEvent(time, name));
}

// Helper to add many events to trigger resizing
void addManyEvents(TestQueue* queue, uint64_t start_time, uint64_t count, uint64_t stride = 1) {
	for (uint64_t i = 0; i < count; i++) {
		addEvent(queue, start_time + i * stride, "Event" + std::to_string(i));
	}
}

// Helper to verify queue order is correct
void verifyQueueOrder(TestQueue* queue) {
	if (queue->empty()) return;
	
	uint64_t prev_time = 0;
	while (!queue->empty()) {
		TestEvent e = queue->pop();
		if (prev_time > 0) {
			EXPECT_GE(e.time, prev_time) << "Events out of order: " << prev_time << " followed by " << e.time;
		}
		prev_time = e.time;
	}
}

/******************************************************************************
 * BASIC CALENDAR QUEUE TESTS
 *****************************************************************************/

// 1. Empty Queue Operations Tests

TEST(CalendarQueue, EmptyQueueOperations) {
	TestQueue queue;
	
	// Verify queue is initially empty
	EXPECT_TRUE(queue.empty());
	EXPECT_EQ(queue.count, 0u);
	
	// Verify next() returns nullptr on empty queue
	EXPECT_EQ(queue.next(), nullptr);
	
	// Verify pop() returns default-constructed value
	TestEvent empty_event = queue.pop();
	EXPECT_EQ(empty_event.time, 0u);
	EXPECT_EQ(empty_event.name, "");
}

TEST(CalendarQueue, EmptyQueueAfterClear) {
	TestQueue queue;
	
	// Add several events
	addEvent(&queue, 10u, "Event1");
	addEvent(&queue, 20u, "Event2");
	addEvent(&queue, 30u, "Event3");
	
	EXPECT_FALSE(queue.empty());
	
	// Clear all events
	while (!queue.empty()) {
		queue.pop();
	}
	
	// Verify queue is empty
	EXPECT_TRUE(queue.empty());
	EXPECT_EQ(queue.next(), nullptr);
}

// 2. Single Event Handling Tests

TEST(CalendarQueue, AddSingleEvent) {
	TestQueue queue;
	
	// Add single event
	TestQueue::event* event_ptr = addEvent(&queue, 100u, "SingleEvent");
	
	// Verify queue is not empty
	EXPECT_FALSE(queue.empty());
	EXPECT_EQ(queue.count, 1u);
	
	// Verify next() returns this event
	TestQueue::event* next_event = queue.next();
	EXPECT_EQ(next_event, event_ptr);
	EXPECT_EQ(next_event->value.time, 100u);
	EXPECT_EQ(next_event->value.name, "SingleEvent");
	
	// Pop the event
	TestEvent popped = queue.pop();
	EXPECT_EQ(popped.time, 100u);
	EXPECT_EQ(popped.name, "SingleEvent");
	
	// Verify queue is empty again
	EXPECT_TRUE(queue.empty());
}

TEST(CalendarQueue, SingleEventTimeVerification) {
	TestQueue queue;
	
	// Add event with specific time
	addEvent(&queue, 500u, "TimedEvent");
	
	// Verify now value is updated
	EXPECT_EQ(queue.now, 500u);
	
	// Verify repeated next() calls return same event
	TestQueue::event* event1 = queue.next();
	TestQueue::event* event2 = queue.next();
	
	EXPECT_EQ(event1, event2);
	EXPECT_EQ(event1->value.time, 500u);
}

// 3. Multiple Event Ordering Tests

TEST(CalendarQueue, LinearTimeOrderingTest) {
	TestQueue queue;
	
	// Add events with increasing timestamps
	addEvent(&queue, 100u, "Event1");
	addEvent(&queue, 200u, "Event2");
	addEvent(&queue, 300u, "Event3");
	
	// Verify they come out in time order
	TestEvent e1 = queue.pop();
	TestEvent e2 = queue.pop();
	TestEvent e3 = queue.pop();
	
	EXPECT_EQ(e1.time, 100u);
	EXPECT_EQ(e2.time, 200u);
	EXPECT_EQ(e3.time, 300u);
	
	// Add events with decreasing timestamps
	addEvent(&queue, 300u, "Event3");
	addEvent(&queue, 200u, "Event2");
	addEvent(&queue, 100u, "Event1");
	
	// Verify they still come out in time order
	e1 = queue.pop();
	e2 = queue.pop();
	e3 = queue.pop();
	
	EXPECT_EQ(e1.time, 100u);
	EXPECT_EQ(e2.time, 200u);
	EXPECT_EQ(e3.time, 300u);
}

TEST(CalendarQueue, MixedTimeOrderingTest) {
	TestQueue queue;
	
	// Add events in random time order
	addEvent(&queue, 500u, "Event5");
	addEvent(&queue, 100u, "Event1");
	addEvent(&queue, 300u, "Event3");
	addEvent(&queue, 200u, "Event2");
	addEvent(&queue, 400u, "Event4");
	
	// Verify they come out in time order
	for (uint64_t expected_time = 100u; expected_time <= 500u; expected_time += 100u) {
		TestEvent e = queue.pop();
		EXPECT_EQ(e.time, expected_time);
		EXPECT_EQ(e.name, "Event" + std::to_string(expected_time/100u));
	}
}

// 4. Time-based Event Retrieval Tests

TEST(CalendarQueue, NextEventTimeTest) {
	TestQueue queue;
	
	// Add events with specific times
	addEvent(&queue, 10u, "Event10");
	addEvent(&queue, 20u, "Event20");
	addEvent(&queue, 30u, "Event30");
	
	// Find next event after time 15
	TestQueue::event* next_after_15 = queue.next(15u);
	ASSERT_NE(next_after_15, nullptr);
	EXPECT_EQ(next_after_15->value.time, 20u);
	
	// Find next event after time 5
	TestQueue::event* next_after_5 = queue.next(5u);
	ASSERT_NE(next_after_5, nullptr);
	EXPECT_EQ(next_after_5->value.time, 10u);
	
	// Find next event after time 35 (should be nullptr)
	TestQueue::event* next_after_35 = queue.next(35u);
	EXPECT_EQ(next_after_35, nullptr);
}

// 5. Time Overflow Testing

TEST(CalendarQueue, NearMaximumTimeTest) {
	TestQueue queue;
	
	uint64_t max_time = std::numeric_limits<uint64_t>::max();
	uint64_t near_max = max_time - 100u;
	
	// Add events near the maximum time value
	addEvent(&queue, near_max, "NearMax");
	addEvent(&queue, max_time, "Max");
	
	// Verify correct ordering
	TestEvent e1 = queue.pop();
	TestEvent e2 = queue.pop();
	
	EXPECT_EQ(e1.time, near_max);
	EXPECT_EQ(e2.time, max_time);
}

// 6. Persistence of Events

TEST(CalendarQueue, EventPointerValidityTest) {
	TestQueue queue;
	
	// Push events and save pointers
	TestQueue::event* ptr1 = addEvent(&queue, 100u, "Event1");
	TestQueue::event* ptr2 = addEvent(&queue, 200u, "Event2");
	TestQueue::event* ptr3 = addEvent(&queue, 300u, "Event3");
	
	// Verify pointers remain valid
	EXPECT_EQ(ptr1->value.name, "Event1");
	EXPECT_EQ(ptr2->value.name, "Event2");
	EXPECT_EQ(ptr3->value.name, "Event3");
	
	// Pop first event
	queue.pop();
	
	// Verify other pointers are still valid
	EXPECT_EQ(ptr2->value.name, "Event2");
	EXPECT_EQ(ptr3->value.name, "Event3");
}

TEST(CalendarQueue, EventContentModificationTest) {
	TestQueue queue;
	
	// Add event to queue
	TestQueue::event* ptr = addEvent(&queue, 100u, "OriginalName");
	
	// Modify event content (not time)
	ptr->value.name = "ModifiedName";
	
	// Verify modification persists
	TestEvent e = queue.pop();
	EXPECT_EQ(e.name, "ModifiedName");
}

/******************************************************************************
 * COMPLEX CALENDAR QUEUE TESTS
 *****************************************************************************/

// 1. Queue Resizing Tests

TEST(CalendarQueue, GrowOperationTest) {
	// Use a small initial calendar size to trigger resizing easily
	// year=8 means 2^8 = 256 days, mindiff=2 means minimum 2^2 = 4 days
	TestQueue queue(8, 2);
	
	// Record original days before growing
	uint64_t original_days = queue.days();
	
	// Add events with timestamps that will spread across many calendar days
	addManyEvents(&queue, 0u, 600u, 100u); // 600 events with stride of 100 to spread them out
	
	// Verify the calendar grew
	EXPECT_GT(queue.days(), original_days);
	
	// Verify events are still in order
	verifyQueueOrder(&queue);
}

TEST(CalendarQueue, ShrinkOperationTest) {
	// Use a small initial calendar size to trigger resizing easily
	TestQueue queue(8, 2);
	
	// Record original days before growing
	uint64_t original_days = queue.days();

	// First add enough to trigger a grow
	addManyEvents(&queue, 0u, 600u, 100u);
	
	uint64_t current_days = queue.days();
	EXPECT_GT(current_days, original_days); // Should have grown from initial 256 days
	
	// Now remove most events to trigger shrink
	// We'll leave fewer than days/4 events
	uint64_t eventsToLeave = current_days / 8u;
	uint64_t eventsToRemove = 600u - eventsToLeave;
	
	for (uint64_t i = 0u; i < eventsToRemove; i++) {
		queue.pop();
	}
	
	// Add one more event to potentially trigger the shrink
	addEvent(&queue, 100000u, "TriggerShrink");
	
	// Verify calendar shrunk
	EXPECT_LT(queue.days(), current_days);
	
	// Verify events are still in order
	verifyQueueOrder(&queue);
}

TEST(CalendarQueue, MultipleResizeOperationsTest) {
	// Use a small initial calendar size to trigger resizing easily
	TestQueue queue(8, 2);
	
	// First grow by adding many events
	addManyEvents(&queue, 0u, 600u, 50u);
	uint64_t size_after_grow = queue.days();
	
	// Remove most to trigger shrink
	uint64_t eventsToRemove = 550u;
	for (uint64_t i = 0u; i < eventsToRemove; i++) {
		queue.pop();
	}
	uint64_t size_after_shrink = queue.days();
	EXPECT_LT(size_after_shrink, size_after_grow);
	
	// Grow again
	addManyEvents(&queue, 10000u, 600u, 50u);
	uint64_t size_after_second_grow = queue.days();
	EXPECT_GT(size_after_second_grow, size_after_shrink);
	
	// Verify events are in order
	verifyQueueOrder(&queue);
}

// 2. Bucket Management Tests

TEST(CalendarQueue, EventsInSingleBucketTest) {
	TestQueue queue(8, 2);
	
	// Add many events that all fall in the same calendar day/bucket
	uint64_t base_time = 1000000u;  // All events will be in the same "day"
	
	// Add events with small time differences
	for (uint64_t i = 0u; i < 100u; i++) {
		addEvent(&queue, base_time + i, "BucketEvent" + std::to_string(i));
	}
	
	// Verify events come out in correct order
	uint64_t prev_time = 0u;
	while (!queue.empty()) {
		TestEvent e = queue.pop();
		if (prev_time > 0u) {
			EXPECT_GT(e.time, prev_time);
		}
		prev_time = e.time;
	}
}

TEST(CalendarQueue, EventsSpreadAcrossBucketsTest) {
	TestQueue queue(8, 2);
	
	// Add events that span multiple days/years in the calendar
	
	// Create time values that will span across multiple calendar "days"
	std::vector<uint64_t> times;
	
	// Add some events in current year
	for (uint64_t i = 0u; i < 50u; i++) {
		times.push_back(i * (1ull << queue.day) * 3u); // Spread across days
	}
	
	// Add some events in next year
	uint64_t next_year_time = (1ull << queue.year);
	for (uint64_t i = 0u; i < 50u; i++) {
		times.push_back(next_year_time + i * (1ull << queue.day) * 2u);
	}
	
	// Shuffle the times to insert in random order
	std::random_device rd;
	std::mt19937 g(rd());
	std::shuffle(times.begin(), times.end(), g);
	
	// Add events in shuffled order
	for (size_t i = 0; i < times.size(); i++) {
		addEvent(&queue, times[i], "Event" + std::to_string(i));
	}
	
	// Verify events come out in time order
	verifyQueueOrder(&queue);
}

TEST(CalendarQueue, YearBoundaryTest) {
	TestQueue queue(8, 2);
	
	// Test events that fall on year boundaries
	uint64_t year_boundary = (1ull << queue.year);
	
	// Add events before, at, and after year boundary
	addEvent(&queue, year_boundary - 100u, "BeforeYearBoundary");
	addEvent(&queue, year_boundary, "AtYearBoundary");
	addEvent(&queue, year_boundary + 100u, "AfterYearBoundary");
	
	// Verify correct order
	TestEvent e1 = queue.pop();
	TestEvent e2 = queue.pop();
	TestEvent e3 = queue.pop();
	
	EXPECT_EQ(e1.name, "BeforeYearBoundary");
	EXPECT_EQ(e2.name, "AtYearBoundary");
	EXPECT_EQ(e3.name, "AfterYearBoundary");
}

// 3. Reused Events Tests

TEST(CalendarQueue, EventRecyclingTest) {
	TestQueue queue(8, 2);
	
	// Add and remove many events to populate the unused list
	for (uint64_t i = 0u; i < 100u; i++) {
		addEvent(&queue, i * 100u, "RecycleEvent" + std::to_string(i));
	}
	
	while (!queue.empty()) {
		queue.pop();
	}
	
	// At this point, the unused list should have 100 events
	
	// Add some more events, which should reuse the ones in unused list
	// rather than allocating new ones
	size_t events_size_before = queue.events.size();
	
	for (uint64_t i = 0u; i < 50u; i++) {
		addEvent(&queue, i * 200u, "NewEvent" + std::to_string(i));
	}
	
	// The events deque size shouldn't have changed since we're reusing events
	EXPECT_EQ(queue.events.size(), events_size_before);
	
	// Verify events are in order
	verifyQueueOrder(&queue);
}

TEST(CalendarQueue, RapidPushPopCyclesTest) {
	TestQueue queue(8, 2);
	
	// Do many cycles of add/remove to stress event recycling
	for (uint64_t cycle = 0u; cycle < 10u; cycle++) {
		// Add a batch of events
		for (uint64_t i = 0u; i < 50u; i++) {
			addEvent(&queue, cycle * 1000u + i * 10u, "CycleEvent" + std::to_string(i));
		}
		
		// Remove half of them
		for (uint64_t i = 0u; i < 25u; i++) {
			queue.pop();
		}
	}
	
	// Verify remaining events are in order
	verifyQueueOrder(&queue);
}

// 4. Priority Modification Tests

TEST(CalendarQueue, SetPriorityTest) {
	TestQueue queue(8, 2);
	
	// Add some events
	TestQueue::event* event_ptr = addEvent(&queue, 1000u, "HighPriorityEvent");
	addEvent(&queue, 100u, "LowPriorityEvent");
	addEvent(&queue, 500u, "MediumPriorityEvent");
	
	// Event with time 1000 should come out last normally
	TestEvent first = queue.pop();
	EXPECT_EQ(first.time, 100u);
	
	// Now add a new event with a lower time
	TestEvent new_event(200u, "UpdatedPriorityEvent");
	queue.set(event_ptr, new_event);
	
	// It should now come out first
	TestEvent new_first = queue.pop();
	EXPECT_EQ(new_first.time, 200u);
	EXPECT_EQ(new_first.name, "UpdatedPriorityEvent");
}

// 5. Edge Case Tests

TEST(CalendarQueue, RemoveSpecificEventTest) {
	TestQueue queue(8, 2);
	
	// Add events
	addEvent(&queue, 100u, "Event1");
	TestQueue::event* middle_ptr = addEvent(&queue, 200u, "Event2");
	addEvent(&queue, 300u, "Event3");
	
	// Remove the middle event specifically
	TestEvent removed = queue.pop(middle_ptr);
	EXPECT_EQ(removed.time, 200u);
	EXPECT_EQ(removed.name, "Event2");
	
	// Verify remaining events are in order
	TestEvent e1 = queue.pop();
	TestEvent e2 = queue.pop();
	
	EXPECT_EQ(e1.time, 100u);
	EXPECT_EQ(e2.time, 300u);
}

TEST(CalendarQueue, EmptyDaysHandlingTest) {
	TestQueue queue(8, 2);
	
	// Add events spread out so many days are empty
	addEvent(&queue, 0u, "Day0");
	addEvent(&queue, 1ull << (queue.day + 3u), "Day8");  // 8 days later
	addEvent(&queue, 1ull << (queue.day + 5u), "Day32"); // 32 days later
	
	// Verify correct order even with gaps
	TestEvent e1 = queue.pop();
	TestEvent e2 = queue.pop();
	TestEvent e3 = queue.pop();
	
	EXPECT_EQ(e1.name, "Day0");
	EXPECT_EQ(e2.name, "Day8");
	EXPECT_EQ(e3.name, "Day32");
} 
