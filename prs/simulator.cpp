#include "simulator.h"
#include <common/message.h>
#include <common/math.h>
#include <interpret_boolean/export.h>
#include <stdio.h>

namespace prs {

enabled_transition::enabled_transition() {
	this->fire_at = 0;
	this->net = 0;
	this->value = 2;
	this->strength = 0;
	this->stable = true;
}

enabled_transition::enabled_transition(uint64_t fire_at, boolean::cube assume, boolean::cube guard, int net, int value, int strength, bool stable) {
	this->fire_at = fire_at;
	
	this->assume = assume;
	this->guard = guard;
	this->net = net;
	this->value = value;
	this->strength = strength;
	this->stable = stable;
}

enabled_transition::~enabled_transition() {
}

string enabled_transition::to_string(const production_rule_set *base) {
	string result = export_expression(guard, *base).to_string() + "->" + base->nets[net].name;
	if (base->nets[net].region > 0) {
		result += "'" + ::to_string(base->nets[net].region);
	}
	// Value encoding in asynchronous circuit notation:
	// -1: Interference or instability (represented as ~)
	// 0: Low logic level (represented as -)
	// 1: High logic level (represented as +)
	switch (value) {
	case -1: result += "~"; break;
	case 0: result += "-"; break;
	case 1: result += "+"; break;
	}
	
	// Strength levels from strongest to weakest:
	// 0: Floating/undriven
	// 1: Weak driving
	// 3: Power (strongest driving)
	switch (strength) {
	case 0: result += " floating"; break;
	case 1: result += " weak"; break;
	case 3: result += " power"; break;
	}

	if (not stable) {
		result += " unstable";
	}

	if (not assume.is_tautology()) {
		result += " {" + export_expression(assume, *base).to_string() + "}";
	}

	return result;
}

bool operator<(enabled_transition t0, enabled_transition t1) {
	return t0.fire_at < t1.fire_at;
}

bool operator>(enabled_transition t0, enabled_transition t1) {
	return t0.fire_at > t1.fire_at;
}

simulator::simulator()
{
	base = NULL;
	debug = false;
}

simulator::simulator(const production_rule_set *base, bool debug)
{
	this->base = base;
	this->debug = debug;
	if (base != NULL) {
		for (int i = 0; i < (int)base->nets.size(); i++) {
			if (base->nets[i].driver == 1) {
				global.set(i, 1);
				encoding.set(i, 1);
			} else if (base->nets[i].driver == 0) {
				global.set(i, 0);
				encoding.set(i, 0);
			} else {
				global.set(i, -1);
				encoding.set(i, -1);
			}
		}
	}
}

simulator::~simulator()
{

}

simulator::queue::event* &simulator::at(int net) {
	return nets[net];
}

// The schedule() method adds a new event to the event queue to be processed in the future.
// 
// Events in the calendar queue are organized by their scheduled firing time. When an event
// is scheduled, it will be placed in the queue according to when it should execute. The
// actual firing time is determined using a Pareto distribution based on the maximum delay.
// 
// If an event is already scheduled for the same net:
// - For vacuous transitions (same value or mutex assumptions): The existing event is updated
// - For conflicting transitions: Special handling occurs for potential instability
//   (the event may be marked as unstable, strength may be updated, etc.)
// 
// Note: Events can be canceled by the assume() method if they contradict
// an assumption made after scheduling.
//
// @param delay_max Maximum delay for this transition
// @param assume Assumptions for this transition
// @param guard Guard condition for this transition
// @param net The target net
// @param value The new value to assign
// @param strength The driving strength
// @param stable Whether this is a stable transition
void simulator::schedule(uint64_t delay_max, boolean::cube assume, boolean::cube guard, int net, int value, int strength, bool stable) {
	if (net >= (int)nets.size()) {
		nets.resize(net+1, nullptr);
	}
	
	int prev_value = encoding.get(net);

	// Use Pareto distribution for delay variation
	// Pareto distribution provides a realistic model of circuit timing variations,
	// with most transitions happening near the minimum delay but with a long tail
	// to account for process variations and other physical effects
	uint64_t fire_at = enabled.now + pareto(delay_max, 5.0);
	
	if (at(net) == nullptr) {
		// No existing event for this net - create a new one
		at(net) = enabled.push(enabled_transition(fire_at, assume, guard, net, value, strength, stable));
	} else if (at(net)->value.strength == 0 or at(net)->value.value == prev_value or are_mutex(global.xoutnulls(), at(net)->value.assume)) {
		// It was a vacuous transition (doesn't cause actual change), so replace it
		at(net)->value.assume = assume;
		at(net)->value.guard = guard;
		at(net)->value.value = value;
		at(net)->value.strength = strength;
		at(net)->value.stable = stable;
	} else {
		// TODO(edward.bingham) maybe schedule a new time it fire_at is sooner than the previous event?
		//enabled.set(devs[dev], enabled_transition(dev, ((devs[dev]->value+1)&(value+1))-1, fire_at));

		// This is where we handle potential instability when multiple drivers affect the same net
		// Combine guards and assumptions with existing event
		at(net)->value.guard &= guard;
		at(net)->value.assume &= assume;

		// When values conflict, set to -1 (interference) and mark as unstable
		// This represents X in traditional HDLs
		if (value != at(net)->value.value) {
			at(net)->value.value = -1;  // -1 indicates interference/instability
			at(net)->value.stable = false;
		}

		// Take the stronger of the two driving strengths
		if (at(net)->value.strength < strength) {
			at(net)->value.strength = strength;
		}
	}
}

// The propagate() method drives changes from one net to others through production rules.
// This is part of the instantaneous evaluation process that determines which nets need to
// be updated after a change occurs on the specified net.
// 
// @param q Queue of nets to be evaluated
// @param net The net whose changes are being propagated
// @param vacuous Whether this is a vacuous transition (no actual value change)
void simulator::propagate(deque<int> &q, int net, bool vacuous) {
	// First, propagate through transistors where this net is a source terminal
	for (int driver = 0; driver < 2; driver++) {
		for (auto i = base->nets[net].sourceOf[driver].begin(); i != base->nets[net].sourceOf[driver].end(); i++) {
			auto dev = base->devs.begin()+*i;
			int local_value = encoding.get(dev->gate);
			if (local_value == 2 or local_value == dev->threshold) {
				q.push_back(dev->drain);
			}
		}
	}

	// Commented out code for reverse source propagation (not currently used)
	/*for (int driver = 0; driver < 2; driver++) {
		for (auto i = base->nets[net].rsourceOf[driver].begin(); i != base->nets[net].rsourceOf[driver].end(); i++) {
			auto dev = base->devs.begin()+*i;
			int local_value = encoding.get(dev->gate);
			if (local_value == 2 or local_value == dev->threshold) {
				q.push_back(dev->drain);
			}
		}
	}*/

	// For non-vacuous changes, also propagate through gates controlled by this net
	// This handles the case where this net controls other transistors as their gate
	for (int threshold = 0; threshold < 2 and not vacuous; threshold++) {
		for (auto i = base->nets[net].gateOf[threshold].begin(); i != base->nets[net].gateOf[threshold].end(); i++) {
			q.push_back(base->devs[*i].drain);
		}
	}
	
	// Sort and deduplicate the queue to avoid evaluating the same net multiple times
	sort(q.begin(), q.end());
	q.erase(unique(q.begin(), q.end()), q.end());
}

void simulator::model(int i, bool reverse, boolean::cube &assume, boolean::cube &guard, int &value, int &drive_strength, int &glitch_value, int &glitch_strength, uint64_t &delay_max) {
	auto dev = base->devs.begin()+i;
	
	// Check if this device's assumptions conflict with the current state
	// If they conflict, this device is disabled by its assumptions
	bool fail_assumption = are_mutex(global.xoutnulls(), dev->attr.assume);
	if (debug and fail_assumption) {
		cout << "\tfailed assumption " << export_composition(global, *base).to_string() << " & " << export_expression(dev->attr.assume, *base).to_string() << endl;
	}
	
	// Apply assumptions to the observed state
	boolean::cube observed = encoding;
	boolean::cube assume_action;
	if (not fail_assumption) {
		// Collect all compatible assumptions
		for (auto c = dev->attr.assume.cubes.begin(); c != dev->attr.assume.cubes.end(); c++) {
			if (not are_mutex(encoding, *c)) {
				assume_action &= *c;
			}
		}
		assume_action = assume_action.xoutnulls();
		observed &= assume_action;
	}

	// Handle both normal and reverse direction operations
	// (reverse is for bidirectional devices)
	int source = reverse ? dev->drain : dev->source;
	int drain = reverse ? dev->source : dev->drain;

	int prev_value = observed.get(drain)+1;
	int prev_strength = 2-strength.get(drain);

	//bool isremote = net != drain;

	// Get gate value (controls whether device is on or off)
	int local_value = observed.get(dev->gate);
	int global_value = global.get(dev->gate);

	// Calculate source value and strength
	// The "+1" adjustment handles our internal representation of X as -1
	int source_value = observed.get(source)+1;
	int source_strength = 2-strength.get(source);
	
	// Complex strength adjustment logic based on circuit conditions
	if (fail_assumption) {
		// Device assumptions fail, use previous values
		source_strength = prev_strength;
		source_value = prev_value;
	} else if (source_value-1 == 1-dev->driver) {
		// Source is opposite of what this device is designed to drive
		if (base->assume_nobackflow) {
			// No backflow allowed - set to undriven
			source_strength = 0;
			source_value = 3;
		} else if (source_strength > 1) {
			// Reduce strength for backflow
			source_strength = 1;
		}
	} else if (dev->attr.force and source_strength > 2) {
		// Force attribute means use power driving
		source_strength = 3;
	} else if (dev->attr.weak and source_strength > 1) {
		// Weak attribute means use weak driving
		source_strength = 1;
	} else if (source_strength > 2) {
		// Otherwise limit to normal driving
		source_strength = 2;
	}

	if (debug) cout << "\t@" << base->nets[source].name << ":" << source_value-1 << "*" << source_strength << "&" << (dev->threshold==0 ? "~" : "") << base->nets[dev->gate].name << ":" << local_value << "->" << base->nets[drain].name << (dev->driver==0 ? "-" : "+") << "*" << drive_strength;

	// Main logic for transistor operation
	// If gate value matches threshold (device is on) or gate is unknown but global is matching
	if (local_value == dev->threshold or (local_value == 2 and global_value == dev->threshold)) {
		if (source_value == 3 or drive_strength > source_strength) {
			// undriven or weaker than existing drive - no effect
			// TODO(edward.bingham) consider for backprop
			if (debug) cout << "\tundriven" << endl;
			return;
		} else if (drive_strength < source_strength) {
			// device is driving net stronger than other devices - take over
			value = source_value;
			drive_strength = source_strength;
			if (dev->attr.delay_max < delay_max) {
				delay_max = dev->attr.delay_max;
			} else {
				// TODO(edward.bingham) weak instability?
			}
			if (debug) cout << "\tstronger " << value << "*" << drive_strength << endl;
		} else if (not fail_assumption) /*if (drive_strength == source_strength)*/ {
			// Equal strength devices - use bitwise AND to resolve
			// This operation works because of how values are encoded:
			// When two equal-strength drivers conflict, result is interference (X)
			value &= source_value;
			if (dev->attr.delay_max < delay_max) {
				delay_max = dev->attr.delay_max;
			}
			// TODO(edward.bingham) this might also cause instability
			if (debug) cout << "\tdriven " << value << "*" << drive_strength << endl;
		}
		if (not fail_assumption and global_value != 2 and global_value != -1) {
			guard.set(dev->gate, global_value);
			assume &= assume_action;
		}
	} /*else if (local_value == 1-dev->threshold) {
		// this device is disabled, value remains unaffected
		// TODO(edward.bingham) check instability?
	}*/ else if (not fail_assumption and (source_value&prev_value) != prev_value and (local_value == -1 or (local_value == 2 and global_value != dev->threshold))) {
		// This handles potential glitches/instability
		// The condition (source_value&prev_value) != prev_value checks if we have conflicting values
		if (source_strength > glitch_strength) {
			glitch_value = source_value;
			glitch_strength = source_strength;
			if (dev->attr.delay_max < delay_max) {
				delay_max = dev->attr.delay_max;
			}
			if (debug) cout << "\tstronger glitch " << glitch_value << "*" << glitch_strength  << endl;
		} else if (source_strength == glitch_strength) {
			glitch_value &= source_value;
			if (dev->attr.delay_max < delay_max) {
				delay_max = dev->attr.delay_max;
			}
			if (debug) cout << "\tglitch " << glitch_value << "*" << glitch_strength << endl;
		} else {
			if (debug) cout << "\tweaker" << endl;
		}
	} else {
		if (debug) cout << "\tdisabled" << endl;
	}
}

// The evaluate() method propagates instantaneous events through the system.
// This function should be called after setting signal values or making assumptions
// to ensure that all zero-delay effects are immediately applied.
// 
// Unlike fire(), evaluate() does not advance simulation time. It only processes
// immediate logical consequences of the current state.
// 
// @param nets A collection of nets to evaluate changes on
void simulator::evaluate(deque<int> nets) {
	deque<int> q = nets;
	boolean::cube ack;
	while (not q.empty()) {
		int net = q.front();
		q.pop_front();

		int glitch_value = 3;
		int glitch_strength = 0;
		int drive_strength = 0;
		int value = 3;
		if (base->nets[net].keep) {
			drive_strength = 1;
			value = encoding.get(net)+1;
		}
		boolean::cube guard;
		boolean::cube assumed;
		uint64_t delay_max = std::numeric_limits<uint64_t>::max();

		if (debug) cout << "evaluating " << net << "/(" << base->nets.size() << ") " << base->nets[net].name << ":" << encoding.get(net) << (base->nets[net].keep ? " keep" : "") << endl;
		for (int driver = 0; driver < 2; driver++) {
			for (auto i = base->nets[net].drainOf[driver].begin(); i != base->nets[net].drainOf[driver].end(); i++) {
				model(*i, false, assumed, guard, value, drive_strength, glitch_value, glitch_strength, delay_max);
			}

			/*for (auto i = base->nets[net].rsourceOf[driver].begin(); i != base->nets[net].rsourceOf[driver].end(); i++) {
				model(*i, true, assumed, guard, value, drive_strength, glitch_value, glitch_strength, delay_max);
			}*/
		}

		if (delay_max == std::numeric_limits<uint64_t>::max()) {
			delay_max = 0;
		}

		if (debug) cout << "\tfinal value = ";
		bool stable = true;
		if (glitch_strength >= drive_strength and glitch_value != value) {
			// This net is unstable
			value = 0;
			drive_strength = glitch_strength;
			stable = false;
			
			if (debug) cout << "unstable ";
			// TODO(edward.bingham) schedule another event to resolve the glitch?
		}
		value -= 1;

		if (value == 2 and drive_strength == 0) {
			if (base->assume_static) {
				value = encoding.get(net);
			} else {
				value = -1;
			}
		}

		if (debug) cout << value << " strength = " << drive_strength << endl;

		// TODO(edward.bingham) we should only propagate instantly here if delay_max is 0, we need to handle the other condition in the import/export of production rules, not in the simulator
		if (delay_max == 0 or (base->nets[net].gateOf[0].empty() and base->nets[net].gateOf[1].empty() and (not base->nets[net].sourceOf[0].empty() or not base->nets[net].sourceOf[1].empty()))) {
			if (value >= 0) {
				ack &= guard & assumed;
				assume(assumed);
			}

			int avalue = assumed.get(net);
			if (avalue == 2 or avalue != 1-value) {
				set(net, value, drive_strength, stable, &q);
			}
		} else {
			int avalue = assumed.get(net);
			if (avalue == 2 or avalue != 1-value) {
				schedule(delay_max, assumed, guard, net, value, drive_strength, stable);
			}
		}
	}

	encoding = encoding & ack;
}

// The fire() method is the core mechanism for advancing simulation time and processing events.
// 
// When called:
// - Without parameters (or net=std::numeric_limits<int>::max()): selects and fires the next
//   event from the event queue based on timing order.
// - With a specific net index: fires the specific event propagating through that net.
// 
// Each time an event is fired:
// 1. The event is removed from the queue
// 2. The specified signal transition is applied
// 3. New events may be scheduled as consequences of this transition
// 4. The simulation state is updated accordingly
//
// The typical simulation flow is:
// 1. Set initial values with set()
// 2. Call fire() repeatedly until all events are processed (enabled.empty())
// 
// @param net The index of the net whose event should be fired, or std::numeric_limits<int>::max() to fire the next event chronologically
// @return The transition that was fired
enabled_transition simulator::fire(int net) {
	enabled_transition t;
	if (net == std::numeric_limits<int>::max()) {
		t = enabled.pop();
	} else if (net < 0 or net >= (int)nets.size()) {
		printf("error: attempting to fire transition on non-existent net\n");
		return t;
	} else if (nets[net] == nullptr) {
		printf("error: no transition to fire on this net\n");
		return t;
	} else {
		t = enabled.pop(at(net));
		at(net) = nullptr;
	}

	if (t.net < 0 or t.net >= (int)nets.size()) {
		printf("error: attempting to fire non-existent transition\n");
		return t;
	}

	at(t.net) = nullptr;
	
	if (debug) {
		printf("firing %s->%s%c:%d%s {%s}\n", export_expression(t.guard, *base).to_string().c_str(), base->nets[t.net].name.c_str(), t.value == 0 ? '-' : (t.value == 1 ? '+' : '~'), t.strength, t.stable ? "" : " unstable", export_expression(t.assume, *base).to_string().c_str());
	}

	if (t.value >= 0) {
		encoding &= t.guard & t.assume;
		assume(t.assume);
	}

	deque<int> q;
	set(t.net, t.value, t.strength, t.stable);
	return t;
}

// The assume() method applies assumptions about signal values to the simulation.
// 
// IMPORTANT: assume() does NOT directly set signal values. Instead, it:
// 1. Cancels any pending events that would contradict the assumption
// 2. Constrains future event propagation through the assume parameter
// 
// To actually set signal values, use set() method instead.
// 
// This is useful for modeling test conditions or exploring "what if" scenarios
// where certain signals are constrained to specific values.
//
// Example: assume(a=0) will cancel any pending events that would set a=1,
// but won't change a's current value if it's already 1.
// 
// @param assume Boolean cube representing the assumed signal values
void simulator::assume(boolean::cube assume) {
	for (int net = 0; net < (int)assume.values.size()*16; net++) {
		int value = assume.get(net);
		if (value != 2 and net < (int)nets.size()) {
			auto e = at(net);
			if (e != nullptr and (e->value.value != value or not e->value.stable)) {
				if (debug) printf("popping event %d\n", net);
				enabled.pop(e);
				at(net) = nullptr;
			}
		}
	}
}

// The set() method applies a value to a specific net in the simulation.
// 
// This is one of the primary ways to interact with the simulation, allowing you to:
// - Set input values
// - Force internal signal values for testing purposes
// - Apply changes that trigger event propagation
// 
// When a signal is set:
// 1. Any pending events for this net are canceled
// 2. The new value is applied immediately
// 3. If a queue pointer is provided, the net is added to it for later evaluation
// 4. Otherwise, propagation effects are computed immediately
// 
// The simulator can check various requirements during setting:
// - require_stable: Errors if attempting to set an unstable value
// - require_noninterfering: Errors if setting an interfering value (-1)
// - require_driven: Errors if a node is left floating
// 
// @param net The net to set
// @param value The value to set (0, 1, -1, or 2 for undriven)
// @param strength The driving strength (0-3)
// @param stable Whether this is a stable value
// @param q Optional queue to add this net to for later evaluation. If nullptr,
// then evaluation is automatically handled.
void simulator::set(int net, int value, int strength, bool stable, deque<int> *q) {
	// Check constraints and report errors if violated
	if (base->require_stable and not stable and strength > 0) {
		error("", "unstable rule " + base->nets[net].name + (value == 1 ? "+" : (value == 0 ? "-" : "~")), __FILE__, __LINE__);
	}
	if (base->require_noninterfering and stable and value == -1 and strength > 0) {
		error("", "interference " + base->nets[net].name, __FILE__, __LINE__);
	}
	if (base->require_driven and strength == 0 and net >= 0) {
		error("", "floating node " + base->nets[net].name, __FILE__, __LINE__);
	}

	// Cancel any pending events for this net
	if (net >= 0 and net < (int)nets.size() and at(net) != nullptr) {
		enabled.pop(at(net));
		at(net) = nullptr;
	}

	int prev_value = encoding.get(net);
	int prev_strength = 2-this->strength.get(net);

	//cout << "\tprev value = " << prev_value << " strength = " << prev_strength << endl;
	bool vacuous = false;
	if (value == prev_value) {
		if (strength == prev_strength) {
			// No actual change in value or strength - return early
			return;
		}
		vacuous = true;  // Only strength changes, not value
	}

	// Check for non-adiabatic transitions (energy-inefficient transitions in circuit)
	// These occur when a controlling gate changes while source and drain differ
	if (base->require_adiabatic and not vacuous and (value == 0 or value == 1)) {
		vector<int> viol;
		for (auto i = base->nets[net].gateOf[value].begin(); i != base->nets[net].gateOf[value].end(); i++) {
			int drain_value = encoding.get(base->devs[*i].drain);
			int source_value = encoding.get(base->devs[*i].source);
			// Non-adiabatic condition: gate changes while source and drain differ
			// This can cause energy inefficiency and glitches in physical circuits
			if ((not base->assume_nobackflow or source_value == base->devs[*i].driver)
				and drain_value != source_value) {
				//printf("assume_nobackflow %d source_value %d driver %d drain_value %d value %d threshold %d\n", (int)base->assume_nobackflow, source_value, base->devs[*i].driver, drain_value, value, base->devs[*i].threshold);
				viol.push_back(*i);
			}
		}
		if (not viol.empty()) {
			error("", "non-adiabatic transition " + base->nets[net].name + (value == 1 ? "+" : (value == 0 ? "-" : "~")), __FILE__, __LINE__);
			string msg = "{";
			for (auto i = viol.begin(); i != viol.end(); i++) {
				if (i != viol.begin()) {
					msg += ", ";
				}
				string source_name = base->nets[base->devs[*i].source].name;
				string gate_name = base->nets[net].name;
				string drain_name = base->nets[base->devs[*i].drain].name;
				msg += "@" + source_name + "&" + (value==0 ? "~" : "") + gate_name + "->" + drain_name + (base->devs[*i].driver==1 ? "+" : "-");
			}
			msg += "}";
			note("", msg, __FILE__, __LINE__);
		}
	}

	// Apply the value changes to the circuit state
	encoding.set(net, value);
	global.set(net, value);
	this->strength.set(net, 2-strength);
	
	// Handle remote nets (connected signals that mirror this net's value)
	for (auto i = base->nets[net].remote.begin(); i != base->nets[net].remote.end(); i++) {
		if (*i == net) {
			continue;
		}
		encoding.remote_set(*i, value, stable);
		global.set(*i, value);
		this->strength.set(*i, 2-strength);
	}

	// Set up queue for propagation
	deque<int> tmp;
	bool doEval = false;
	if (q == nullptr) {
		q = &tmp;
		doEval = true;
	}

	// Propagate changes through the circuit
	for (auto i = base->nets[net].remote.begin(); i != base->nets[net].remote.end(); i++) {
		propagate(*q, *i, vacuous);
	}
	if (doEval and not q->empty()) {
		evaluate(*q);
	}
}

// This overloaded version of set() applies a boolean cube of values to multiple nets
// simultaneously. This is useful for setting related signals together.
// 
// When using this method, remote assignments (signals connected through the remote groups
// mechanism) are also handled appropriately.
// 
// @param action Boolean cube representing values to set
// @param strength The driving strength
// @param stable Whether these are stable values
// @param q Optional queue to add affected nets to for later evaluation
void simulator::set(boolean::cube action, int strength, bool stable, deque<int> *q) {
	// Calculate the remote actions (effects on connected nets)
	// Remote groups are collections of nets that are electrically connected
	boolean::cube remote_action = action.remote(base->remote_groups());
	
	// Cancel any pending events on affected nets
	for (int net = 0; net < action.size()*16; net++) {
		int val = action.get(net);
		if (val != 2 and net >= 0 and net < (int)nets.size() and at(net) != nullptr) {
			enabled.pop(at(net));
			at(net) = nullptr;
		}
	}

	// Apply the boolean cube operations to update circuit state
	// These operations efficiently update multiple signals at once:
	// 1. local_assign: Apply direct assignments to specified nets
	// 2. remote_assign: Propagate to connected nets
	global = local_assign(global, remote_action, true);
	encoding = remote_assign(local_assign(encoding, action, true), global, true);
	this->strength &= remote_action.mask().flip();

	// Set up queue for propagation
	deque<int> tmp;
	bool doEval = false;
	if (q == nullptr) {
		q = &tmp;
		doEval = true;
	}

	// Propagate changes through the circuit
	for (int net = 0; net < remote_action.size()*16; net++) {
		if (remote_action.get(net) != 2) {
			propagate(*q, net, false);
		}
	}
	if (doEval and not q->empty()) {
		evaluate(*q);
	}
}

// The reset() method initializes or resets the simulation to its starting state.
// 
// This function:
// 1. Clears all events and the event queue
// 2. Resets all signal values to undefined
// 3. Sets power nets and other default values based on the circuit definition
// 4. Calls wait() to allow the circuit to stabilize
// 5. Sets Reset/~Reset signals to their appropriate reset values (Reset=1, _Reset=0)
// 
// This should be called before beginning a new simulation run to ensure
// a clean initial state. After reset(), the circuit will be in its reset state
// (if it has one defined).
void simulator::reset()
{
	enabled.clear();
	nets.clear();
	global.values.clear();
	encoding.values.clear();
	strength.values.clear();
	for (int i = 0; i < (int)base->nets.size(); i++) {
		global.set(i, -1);
		encoding.set(i, -1);
	}

	for (int i = 0; i < (int)base->nets.size(); i++) {
		if (base->nets[i].driver >= 0) {
			set(i, base->nets[i].driver);
		}
	}

	wait();

	for (int i = 0; i < (int)base->nets.size(); i++) {
		if (base->nets[i].name == "Reset") {
			set(i, 1);
		} else if (base->nets[i].name == "_Reset") {
			set(i, 0);
		}
	}

	//instability_errors.clear();
	//interference_errors.clear();
	//mutex_errors.clear();
	//last = term_index();
}

// The wait() method allows the circuit to stabilize by scheduling events for 
// any nets whose values differ from their expected stable values.
// 
// This is useful after making changes to ensure the circuit reaches equilibrium
// before continuing with further simulation steps.
// 
// The method uses a relatively long delay (10000 time units) to ensure
// any shorter events complete first.
void simulator::wait()
{
	// Get remote groups (electrically connected nets)
	vector<vector<int> > groups = base->remote_groups();
	
	// Schedule events to make encoding converge to global for any mismatches
	// Uses a long delay (10000) to ensure these happen after shorter events
	for (int net = 0; net < (int)global.values.size()*16; net++) {
		int value = global.get(net);
		if (encoding.get(net) != value) {
			schedule(10000, 1, 1, net, value, 2, true);
		}
	}
}

// The run() method deasserts reset signals to allow the circuit to begin normal operation.
// 
// This function:
// 1. Sets Reset signal to 0 (inactive)
// 2. Sets _Reset signal to 1 (inactive)
// 
// After calling run(), you should repeatedly call fire() until all events
// are processed to simulate the circuit's behavior.
void simulator::run()
{
	for (int i = 0; i < (int)base->nets.size(); i++) {
		if (base->nets[i].name == "Reset") {
			set(i, 0);
		} else if (base->nets[i].name == "_Reset") {
			set(i, 1);
		}
	}
}

}
