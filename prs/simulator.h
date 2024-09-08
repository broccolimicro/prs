#pragma once

#include "calendar_queue.h"
#include "production_rule.h"
#include <ucs/variable.h>
#include <common/standard.h>

namespace prs {

struct enabled_transition {
	enabled_transition();
	enabled_transition(int dev, int value, uint64_t fire_at);
	~enabled_transition();

	int dev;
	int value;
	uint64_t fire_at;
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
	vector<queue::event*> devs;

	void schedule(int dev, int value);
	void propagate(deque<int> &q, int net, bool vacuous=false);
	void evaluate(deque<int> net);
	void fire(int dev=-1);

	void cover(int net, int val);
	void set(int net, int val);
	void set(boolean::cube action);

	void reset();
	void wait();
	void run();
};

}

