#include "simulator.h"
#include <common/message.h>
#include <common/math.h>
#include <interpret_boolean/export.h>

namespace prs {

enabled_transition::enabled_transition() {
	this->net = -1;
	this->dev = -1;
	this->fire_at = 0;
}

enabled_transition::enabled_transition(int net, int dev, uint64_t fire_at) {
	this->net = net;
	this->dev = dev;
	this->fire_at = fire_at;
}

enabled_transition::~enabled_transition() {
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
	variables = NULL;
}

simulator::simulator(const production_rule_set *base, const ucs::variable_set *variables)
{
	this->base = base;
	this->variables = variables;
	if (variables != NULL) {
		for (int i = 0; i < (int)variables->nodes.size(); i++)
		{
			global.set(i, -1);
			encoding.set(i, -1);
		}
	}
}

simulator::~simulator()
{

}

void simulator::fire(int index) {
	if (index < 0) {
		std::pop_heap(enabled.begin(), enabled.end(), std::greater<>{});
		index = (int)enabled.size()-1;
	}

	enabled_transition t = enabled[index];
	enabled.erase(enabled.begin() + index);
	if (index != (int)enabled.size()) {
		std::make_heap(enabled.begin(), enabled.end(), std::greater<>{});
	}

	int value = 3;
	boolean::cube guard_action;
	for (int j = 0; j < 2; j++) {
		for (auto i = base->at(t.net).drainOf[j].begin(); i != base->at(t.net).drainOf[j].end(); i++) {
			int gate = base->devs[*i].gate;
			int local_value = encoding.get(gate);
			int global_value = global.get(gate);
			if (local_value == base->devs[*i].threshold or (local_value == 2 and global_value == base->devs[*i].threshold)) {
				value &= (encoding.get(base->devs[*i].source)+1);
				guard_action.set(gate, global_value);
			}
		}
	}

	bool stable = true;
	if (value == 0) {
		// interference
	} else if (value != 2-base->devs[t.dev].threshold) {
		// unstable
		stable = false;
	}
	value -= 1;
	// TODO(edward.bingham) need to propagate this down the stack
	
	if (value >= 0 and value < 2) {
		encoding &= guard_action;
	}

	boolean::cube local_action;
	boolean::cube remote_action;
	local_action.set(t.net, value);
	for (int i = 0; i < (int)base->nets[t.net].remote.size(); i++) {
		remote_action.set(base->nets[t.net].remote[i], value);
	}

	global = local_assign(global, remote_action, stable);
	encoding = remote_assign(local_assign(encoding, local_action, stable), global, true);
	for (int i = 0; i < (int)base->nets[t.net].remote.size(); i++) {
		update(base->nets[t.net].remote[i], value);
	}
}

void simulator::update(int uid, int val) {
	// check gateOf to update values for devices 
	for (auto i = base->at(uid).gateOf.begin(); i != base->at(uid).gateOf.end(); i++) {
		if (val == base->devs[*i].threshold and encoding.get(base->devs[*i].drain) != encoding.get(base->devs[*i].source)) {
			bool found = false;
			for (int k = 0; k < (int)enabled.size() and not found; k++) {
				if (enabled[k].net == base->devs[*i].drain) {
					found = true;
				}
			}
			if (found) {
				continue;
			}

			uint64_t fire_at = now + pareto(base->devs[*i].attr.delay_max, 5.0);

			enabled.push_back(enabled_transition(base->devs[*i].drain, *i, fire_at));
			std::push_heap(enabled.begin(), enabled.end(), std::greater<>{});
		}
	}

	// check sourceOf to update values for devices with enabled gates
	for (int j = 0; j < 2; j++) {
		for (auto i = base->at(uid).sourceOf[j].begin(); i != base->at(uid).sourceOf[j].end(); i++) {
			if (encoding.get(base->devs[*i].gate) == base->devs[*i].threshold and encoding.get(base->devs[*i].drain) != val) {
				bool found = false;
				for (int k = 0; k < (int)enabled.size() and not found; k++) {
					if (enabled[k].net == base->devs[*i].drain) {
						found = true;
					}
				}
				if (found) {
					continue;
				}

				uint64_t fire_at = now + pareto(base->devs[*i].attr.delay_max, 5.0);

				enabled.push_back(enabled_transition(base->devs[*i].drain, *i, fire_at));
				std::push_heap(enabled.begin(), enabled.end(), std::greater<>{});
			}
		}
	}
}

void simulator::set(int uid, int val) {
	boolean::cube local_action;
	boolean::cube remote_action;
	local_action.set(uid, val);
	for (int i = 0; i < (int)base->nets[uid].remote.size(); i++) {
		remote_action.set(base->nets[uid].remote[i], val);
	}

	// clear covered events
	bool reheap = false;
	for (int i = (int)enabled.size()-1; i >= 0; i--) {
		if (enabled[i].net == uid and base->devs[enabled[i].dev].driver == val) {
			enabled.erase(enabled.begin()+i);
			reheap = true;
		}
	}
	if (reheap) {
		std::make_heap(enabled.begin(), enabled.end(), std::greater<>{});
	}

	global = local_assign(global, remote_action, true);
	encoding = remote_assign(local_assign(encoding, local_action, true), global, true);
	for (int i = 0; i < (int)base->nets[uid].remote.size(); i++) {
		update(base->nets[uid].remote[i], val);
	}
}

void simulator::set(boolean::cube action) {
	boolean::cube remote_action = action.remote(variables->get_groups());

	vector<int> change;
	bool reheap = false;
	for (int i = 0; i < (int)variables->nodes.size(); i++) {
		int val = remote_action.get(i);
		if (val != 2) {
			change.push_back(i);
			for (int j = (int)enabled.size()-1; j >= 0; j--) {
				if (enabled[j].net == i and base->devs[enabled[j].dev].driver == val) {
					enabled.erase(enabled.begin()+j);
					reheap = true;
				}
			}
		}
	}
	if (reheap) {
		std::make_heap(enabled.begin(), enabled.end(), std::greater<>{});
	}

	global = local_assign(global, remote_action, true);
	encoding = remote_assign(local_assign(encoding, action, true), global, true);

	for (int i = 0; i < (int)change.size(); i++) {
		update(change[i], remote_action.get(change[i]));
	}
}

void simulator::reset()
{
	global.values.clear();
	encoding.values.clear();
	for (int i = 0; i < (int)variables->nodes.size(); i++) {
		if (variables->nodes[i].to_string() == "Vdd") {
			global.set(i, 1);
			encoding.set(i, 1);
		} else if (variables->nodes[i].to_string() == "GND") {
			global.set(i, 0);
			encoding.set(i, 0);
		} else {
			global.set(i, -1);
			encoding.set(i, -1);
		}
	}

	for (int i = 0; i < (int)variables->nodes.size(); i++) {
		if (variables->nodes[i].to_string() == "Reset") {
			set(i, 1);
		} else if (variables->nodes[i].to_string() == "_Reset") {
			set(i, 0);
		}
	}
	//loaded.clear();
	//instability_errors.clear();
	//interference_errors.clear();
	//mutex_errors.clear();
	//last = term_index();
}

void simulator::wait()
{
	vector<vector<int> > groups = variables->get_groups();
	encoding = encoding.remote(groups);
	encoding = global.remote(groups);
}

void simulator::run()
{
	for (int i = 0; i < (int)variables->nodes.size(); i++) {
		if (variables->nodes[i].to_string() == "Reset") {
			set(i, 0);
		} else if (variables->nodes[i].to_string() == "_Reset") {
			set(i, 1);
		}
	}
}

}
