#pragma once

#include "calendar_queue.h"
#include "production_rule.h"
#include <common/standard.h>

namespace prs {

// Represents a scheduled transition/event in the simulation
// An enabled transition contains all information about an event that will occur at a specific time
struct enabled_transition {
	enabled_transition();
	enabled_transition(uint64_t fire_at, boolean::cube assume, boolean::cube guard, int net, int value, int strength, bool stable);
	~enabled_transition();

	uint64_t fire_at;  // Time at which this transition should fire

	boolean::cube assume;  // Conditions that must be true for this transition to happen
	boolean::cube guard;   // Guard condition that activates this transition
	int net;              // The net (signal) this transition affects
	int value;            // New value: 1=high, 0=low, -1=unstable/interference
	int strength;         // Signal strength: 0=floating, 1=weak, 2=normal, 3=power
	bool stable;          // Whether this transition produces a stable value

	string to_string(const production_rule_set *base);
};

struct enabled_priority {
	uint64_t operator()(const enabled_transition &value) {
		return value.fire_at;
	}
};

bool operator<(enabled_transition t0, enabled_transition t1);
bool operator>(enabled_transition t0, enabled_transition t1);

// Core simulation engine for Production Rule Sets (PRS)
//
// The simulator class provides a framework for simulating asynchronous digital circuits
// represented as Production Rule Sets (PRS). It handles:
//
// 1. Event Scheduling: Transitions are scheduled based on timing delays
// 2. Signal Propagation: Changes propagate through the circuit with appropriate delays
// 3. Signal Resolution: Conflicts are resolved based on driving strengths
// 4. State Tracking: Both instantaneous and target states are maintained
//
// Typical usage pattern:
// ```
// simulator sim(&prs);          // Initialize with production rule set
// sim.reset();                  // Reset to initial state
// sim.set("input", 1);          // Set an input signal
// while(!sim.enabled.empty()) { // Process all scheduled events
//   sim.fire();                 // Fire next scheduled event
// }
// ```
//
// Key simulation concepts:
// - Values: 0=low, 1=high, -1=interference/unstable, 2=undriven
// - Strengths: 0=floating, 1=weak, 3=power (strongest)
// - Events: Scheduled state changes with specific timing
// - Assumptions: Constraints on signal values that prevent contradicting events
struct simulator {
	simulator();
	simulator(const production_rule_set *base, bool debug=false);
	~simulator();

	using queue=calendar_queue<enabled_transition, enabled_priority>;

	bool debug;  // Enable verbose debug output

	const production_rule_set *base;  // The circuit being simulated

	// Signal value representations:
	// 2 = undriven or unknown
	// 1 = stable one (high)
	// 0 = stable zero (low)
	// -1 = unstable/interference (X)
	// 
	// These are two different representations of circuit state:
	// - encoding: The current state after all instantaneous events
	// - global: The target state towards which encoding is converging
	// The simulator works to make encoding converge toward global
	boolean::cube encoding;  // Current state of the circuit
	boolean::cube global;    // Target state of the circuit

	// Signal strength levels for each net:
	// -1 = power (strongest)
	// 0 = normal
	// 1 = weak
	// 2 = undriven/floating (weakest)
	// Note: internally represented as (2-strength) to match boolean cube storage
	boolean::cube strength;

	// Queue of all pending/scheduled events ordered by firing time
	queue enabled;

	// Array indexed by net ID pointing to events in the enabled queue
	// Each net can have at most one pending event
	vector<queue::event*> nets;

	// Access the event scheduled for a specific net
	queue::event* &at(int net);

	// Schedule a new event/transition with specified parameters
	void schedule(uint64_t delay_max, boolean::cube assume, boolean::cube guard, int net, int value, int strength, bool stable=true);
	
	// Propagate changes from one net to others through connected devices
	void propagate(deque<int> &q, int net, bool vacuous=false);
	
	// Model the behavior of a device during evaluation
	void model(int i, bool reverse, boolean::cube &assume, boolean::cube &guard, int &value, int &drive_strength, int &glitch_value, int &glitch_strength, uint64_t &delay_max);
	
	// Evaluate all instantaneous effects of changes to specified nets
	void evaluate(deque<int> net);
	
	// Fire the next event or a specific event, advancing simulation time
	// @param net Specific net to fire, or std::numeric_limits<int>::max() for next chronological event
	// @return The transition that was fired
	enabled_transition fire(int net=std::numeric_limits<int>::max());

	// Apply assumptions about signal values to the simulation
	// NOTE: Does NOT set signal values directly, only cancels contradicting events
	// @param assume Boolean cube representing the assumed signal values
	void assume(boolean::cube assume);

	// Set a value on a specific net in the simulation
	void set(int net, int value, int strength=3, bool stable=true, deque<int> *q=nullptr);
	
	// Set multiple values simultaneously using a boolean cube
	void set(boolean::cube action, int strength=3, bool stable=true, deque<int> *q=nullptr);

	// Reset the simulation to initial state
	void reset();
	
	// Schedule events to make the circuit state converge to a stable state
	void wait();
	
	// Begin normal operation by deasserting reset signals
	void run();
};

}

