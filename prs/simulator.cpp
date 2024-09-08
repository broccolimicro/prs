#include "simulator.h"
#include <common/message.h>
#include <common/math.h>
#include <interpret_boolean/export.h>

namespace prs {

enabled_transition::enabled_transition() {
	this->dev = -1;
	this->value = 2;
	this->fire_at = 0;
}

enabled_transition::enabled_transition(int dev, int value, uint64_t fire_at) {
	this->dev = dev;
	this->value = value;
	this->fire_at = fire_at;
}

enabled_transition::~enabled_transition() {
}

enabled_transition::operator uint64_t() const {
	return fire_at;
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

		for (int i = 0; i < (int)base->nodes.size(); i++) {
			global.set(base->nets.size()+i, -1);
			encoding.set(base->nets.size()+i, -1);
		}
	}
}

simulator::~simulator()
{

}

void simulator::schedule(int dev, int value) {
	if (dev >= (int)devs.size()) {
		devs.resize(dev+1, nullptr);
	}
	auto di = base->devs.begin()+dev;
	uint64_t fire_at = enabled.now + pareto(di->attr.delay_max, 5.0);
	if (devs[dev] == nullptr) {
		devs[dev] = enabled.push(enabled_transition(dev, value, fire_at));
	} else {
		// TODO(edward.bingham) flag instability
		enabled.set(devs[dev], enabled_transition(dev, ((devs[dev]->value+1)&(value+1))-1, fire_at));
	}
}

void simulator::propagate(deque<int> &q, int net, bool vacuous) {
	for (int driver = 0; driver < 2; driver++) {
		for (auto i = base->at(net).sourceOf[driver].begin(); i != base->at(net).sourceOf[driver].end(); i++) {
			auto dev = base->devs.begin()+*i;
			bool isremote = net != dev->source;
			int gate_uid = base->uid(dev->gate);
			int local_value = encoding.get(gate_uid);
			if (local_value == 2 or local_value == dev->threshold) {
				if (isremote) {
					schedule(*i, local_value);
				} else {
					q.push_back(dev->drain);
				}
			}
		}
	}

	// TODO(edward.bingham) loop through all remote nets as well
	for (int threshold = 0; threshold < 2 and not vacuous; threshold++) {
		for (auto i = base->at(net).gateOf[threshold].begin(); i != base->at(net).gateOf[threshold].end(); i++) {
			auto dev = base->devs.begin()+*i;
			int gate_uid = base->uid(dev->gate);
			int local_value = encoding.get(gate_uid);
			schedule(*i, local_value);
		}
	}
}


void simulator::evaluate(deque<int> nets) {
	deque<int> q = nets;
	boolean::cube guard;
	while (not q.empty()) {
		int net = q.front();
		q.pop_front();

		int glitch_value = 3;
		int glitch_strength = 0;
		int drive_strength = 0;
		int value = 3;
		int uid = base->uid(net);
		boolean::cube guard_action;
		cout << "evaluating " << net << "/(" << base->flip(base->nodes.size()) << "," << base->nets.size() << ") " << export_variable_name(net, *variables).to_string() << endl;
		for (int driver = 0; driver < 2; driver++) {
			for (auto i = base->at(net).drainOf[driver].begin(); i != base->at(net).drainOf[driver].end(); i++) {
				auto dev = base->devs.begin()+*i;

				bool isremote = net != dev->drain;
				int gate_uid = base->uid(dev->gate);
				int source_uid = base->uid(dev->source);

				int local_value = encoding.get(gate_uid);
				int global_value = global.get(gate_uid);

				int source_value = encoding.get(source_uid)+1;
				int source_strength = 2-strength.get(source_uid);
				if (dev->attr.weak and source_strength > 1) {
					source_strength = 1;
				} else if (source_strength > 2) {
					source_strength = 2;
				}

				cout << "\t@" << export_variable_name(dev->source, *variables).to_string() << ":" << source_value-1 << "&" << (dev->threshold==0 ? "~" : "") << export_variable_name(dev->gate, *variables).to_string() << ":" << local_value << "->" << export_variable_name(dev->drain, *variables).to_string() << (dev->driver==0 ? "-" : "+");


				if (local_value == dev->threshold or (local_value == 2 and global_value == dev->threshold)) {
					if (source_value == 3 or drive_strength > source_strength) {
						// undriven
						// TODO(edward.bingham) consider for backprop
						cout << "\tundriven" << endl;
						continue;
					} else if (drive_strength < source_strength) {
						// device is driving net stronger than other devices
						value = source_value;
						drive_strength = source_strength;
						cout << "\tstronger" << endl;
					} else /*if (drive_strength == source_strength)*/ {
						value &= source_value;
						cout << "\tdriven" << endl;
					}
					guard_action.set(gate_uid, global_value);
				} /*else if (local_value == 1-dev->threshold) {
					// this device is disabled, value remains unaffected
					// TODO(edward.bingham) check instability?
				}*/ else if (local_value == 2 and global_value == 1-dev->threshold) {
					// instability
					if (source_strength > glitch_strength) {
						glitch_value = source_value;
						glitch_strength = source_strength;
						cout << "\tstronger glitch" << endl;
					} else if (source_strength == glitch_strength) {
						glitch_value &= source_value;
						cout << "\tglitch" << endl;
					} else {
						cout << "\tweaker" << endl;
					}
					guard_action.set(gate_uid, global_value);
				} else {
					cout << "\tdisabled" << endl;
				}
			}
		}

		cout << "\tfinal value = ";
		bool stable = true;
		if (glitch_strength >= drive_strength and ((glitch_value != 3 and glitch_value != value) or glitch_value == 0)) {
			// This net is unstable
			value &= glitch_value;
			drive_strength = glitch_strength;
			stable = false;
			// TODO(edward.bingham) flag an error
			cout << "unstable ";
		}

		value -= 1;
		cout << value << " strength = " << drive_strength << endl;

		if (value >= 0) {
			guard &= guard_action;
		}
		int prev_value = encoding.get(uid);
		int prev_strength = 2-strength.get(uid);
		cout << "\tprev value = " << prev_value << " strength = " << prev_strength << endl;
		bool vacuous = false;
		if (value == prev_value) {
			if (drive_strength == prev_strength) {
				// vacuous transition
				continue;
			}
			vacuous = true;
		}

		encoding.set(uid, value);
		global.set(uid, value);
		strength.set(uid, 2-drive_strength);
		for (auto rem = base->at(net).remote.begin(); rem != base->at(net).remote.end(); rem++) {
			global.set(*rem, value);
			encoding.remote_set(*rem, value, stable);
			strength.set(*rem, 2-drive_strength);
		}

		propagate(q, net, vacuous);
	}

	encoding &= guard;
}

void simulator::fire(int dev) {
	enabled_transition t;
	if (dev < 0) {
		t = enabled.pop();
	} else if (devs[dev] != nullptr) {
		t = enabled.pop(devs[dev]);
		devs[dev] = nullptr;
	} else {
		return;
	}

	devs[t.dev] = nullptr;
	evaluate(deque<int>(1, base->devs[t.dev].drain));
}

void simulator::cover(int net, int val) {
	for (auto i = base->at(net).drainOf[val].begin(); i != base->at(net).drainOf[val].end(); i++) {
		if (*i < (int)devs.size() and devs[*i] != nullptr) {
			enabled.pop(devs[*i]);
			devs[*i] = nullptr;
		}
	}
}

void simulator::set(int net, int val) {
	cover(net, val);

	global.set(net, val);
	encoding.set(net, val);
	for (auto i = base->at(net).remote.begin(); i != base->at(net).remote.end(); i++) {
		global.set(*i, val);
		encoding.remote_set(*i, val);
		strength.set(*i, -1);
	}

	deque<int> q;
	propagate(q, net, false);
	if (not q.empty()) {
		evaluate(q);
	}
}

void simulator::set(boolean::cube action) {
	boolean::cube remote_action = action.remote(variables->get_groups());
	for (int uid = 0; uid < action.size()*16; uid++) {
		int net = uid;
		if (net >= (int)base->nets.size()) {
			net = base->flip(net-base->nets.size());
		}
		int val = action.get(uid);
		if (val != 2) {
			cover(net, val);
		}
	}

	global = local_assign(global, remote_action, true);
	encoding = remote_assign(local_assign(encoding, action, true), global, true);
	strength &= remote_action.mask().flip();

	deque<int> q;
	for (int uid = 0; uid < action.size()*16; uid++) {
		int net = uid;
		if (net >= (int)base->nets.size()) {
			net = base->flip(net-base->nets.size());
		}

		propagate(q, net, false);
	}
	if (not q.empty()) {
		evaluate(q);
	}
}

void simulator::reset()
{
	enabled.clear();
	devs.clear();
	global.values.clear();
	encoding.values.clear();
	strength.values.clear();
	for (int i = 0; i < (int)variables->nodes.size(); i++) {
		if (variables->nodes[i].to_string() == "Vdd") {
			global.set(i, 1);
			encoding.set(i, 1);
			strength.set(i, -1);
		} else if (variables->nodes[i].to_string() == "GND") {
			global.set(i, 0);
			encoding.set(i, 0);
			strength.set(i, -1);
		} else {
			global.set(i, -1);
			encoding.set(i, -1);
		}
	}

	for (int i = 0; i < (int)base->nodes.size(); i++) {
		global.set(base->nets.size()+i, -1);
		encoding.set(base->nets.size()+i, -1);
	}

	for (int i = 0; i < (int)variables->nodes.size(); i++) {
		if (variables->nodes[i].to_string() == "Reset") {
			set(i, 1);
		} else if (variables->nodes[i].to_string() == "_Reset") {
			set(i, 0);
		}
	}

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
