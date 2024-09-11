#pragma once

#include "calendar_queue.h"
#include "production_rule.h"
#include <ucs/variable.h>
#include <common/standard.h>

namespace prs {

struct enabled_transition {
	enabled_transition();
	enabled_transition(uint64_t fire_at, boolean::cube assume, boolean::cube guard, int net, int value, int strength, bool stable);
	~enabled_transition();

	uint64_t fire_at;

	boolean::cube assume;	
	boolean::cube guard;
	int net;
	int value;
	int strength;
	bool stable;

	string to_string(const ucs::variable_set &v);
};

struct enabled_priority {
	uint64_t operator()(const enabled_transition &value) {
		return value.fire_at;
	}
};

bool operator<(enabled_transition t0, enabled_transition t1);
bool operator>(enabled_transition t0, enabled_transition t1);

struct simulator {
	simulator();
	simulator(const production_rule_set *base, const ucs::variable_set *variables);
	~simulator();

	using queue=calendar_queue<enabled_transition, enabled_priority>;

	const production_rule_set *base;
	const ucs::variable_set *variables;

	// 2 = undriven or unknown
	// 1 = stable one
	// 0 = stable 0
	// -1 = unstable
	// named nets are indexed first, then unnamed nodes are listed after
	boolean::cube encoding;
	boolean::cube global;
	// indexed by net
	// -1 = power
	// 0 = normal
	// 1 = weak
	// 2 = undriven
	boolean::cube strength;

	queue enabled;

	// indexed by device, points to events in the enabled queue.
	vector<queue::event*> nets;
	vector<queue::event*> nodes;

	queue::event* &at(int net);

	void schedule(uint64_t delay_max, boolean::cube assume, boolean::cube guard, int net, int value, int strength, bool stable=true);
	void propagate(deque<int> &q, int net, bool vacuous=false);
	void model(int i, bool reverse, boolean::cube &assume, boolean::cube &guard, int &value, int &drive_strength, int &glitch_value, int &glitch_strength, uint64_t &delay_max);
	void evaluate(deque<int> net);
	enabled_transition fire(int net=std::numeric_limits<int>::max());

	void assume(boolean::cube assume);

	void set(int net, int value, int strength=3, bool stable=true, deque<int> *q=nullptr);
	void set(boolean::cube action, int strength=3, bool stable=true, deque<int> *q=nullptr);

	void reset();
	void wait();
	void run();
};

}

