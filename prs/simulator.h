#pragma once

#include "production_rule.h"
#include <ucs/variable.h>
#include <common/standard.h>

namespace prs {

struct enabled_transition {
	enabled_transition();
	enabled_transition(int net, int dev, uint64_t fire_at);
	~enabled_transition();

	int net;
	int dev;
	uint64_t fire_at;
};

bool operator<(enabled_transition t0, enabled_transition t1);
bool operator>(enabled_transition t0, enabled_transition t1);

struct simulator {
	simulator();
	simulator(const production_rule_set *base, const ucs::variable_set *variables);
	~simulator();

	const production_rule_set *base;
	const ucs::variable_set *variables;

	boolean::cube encoding;
	boolean::cube global;
	uint64_t now;

	vector<enabled_transition> enabled;

	void fire(int index=-1);

	void update(int uid, int val);
	void set(int uid, int val);
	void set(boolean::cube action);
	void reset();
	void wait();
	void run();
};

}

