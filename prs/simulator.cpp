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

string enabled_transition::to_string(const ucs::variable_set &v) {
	string result = export_expression(guard, v).to_string() + "->" + export_variable_name(net, v).to_string();
	switch (value) {
	case -1: result += "~"; break;
	case 0: result += "-"; break;
	case 1: result += "+"; break;
	}
	
	switch (strength) {
	case 0: result += " floating"; break;
	case 1: result += " weak"; break;
	case 3: result += " power"; break;
	}

	if (not stable) {
		result += " unstable";
	}

	if (not assume.is_tautology()) {
		result += " {" + export_expression(assume, v).to_string() + "}";
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
	variables = NULL;
	debug = false;
}

simulator::simulator(const production_rule_set *base, const ucs::variable_set *variables, bool debug)
{
	this->base = base;
	this->variables = variables;
	this->debug = debug;
	if (variables != NULL) {
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

		for (int i = 0; i < (int)base->nodes.size(); i++) {
			global.set(base->nets.size()+i, -1);
			encoding.set(base->nets.size()+i, -1);
		}
	}
}

simulator::~simulator()
{

}

simulator::queue::event* &simulator::at(int net) {
	if (net >= 0) {
		return nets[net];
	}
	return nodes[base->flip(net)];
}

void simulator::schedule(uint64_t delay_max, boolean::cube assume, boolean::cube guard, int net, int value, int strength, bool stable) {
	if (net >= (int)nets.size()) {
		nets.resize(net+1, nullptr);
	} else if (base->flip(net) >= (int)nodes.size()) {
		nodes.resize(base->flip(net)+1, nullptr);
	}
	
	int prev_value = encoding.get(base->uid(net));

	uint64_t fire_at = enabled.now + pareto(delay_max, 5.0);
	if (at(net) == nullptr) {
		at(net) = enabled.push(enabled_transition(fire_at, assume, guard, net, value, strength, stable));
	} else if (at(net)->value.strength == 0 or at(net)->value.value == prev_value or are_mutex(global.xoutnulls(), at(net)->value.assume)) {
		// it was a vacuous transition, it doesn't go unstable
		at(net)->value.assume = assume;
		at(net)->value.guard = guard;
		at(net)->value.value = value;
		at(net)->value.strength = strength;
		at(net)->value.stable = stable;
	} else {
		// TODO(edward.bingham) maybe schedule a new time it fire_at is sooner than the previous event?
		//enabled.set(devs[dev], enabled_transition(dev, ((devs[dev]->value+1)&(value+1))-1, fire_at));
		at(net)->value.guard &= guard;
		at(net)->value.assume &= assume;

		if (value != at(net)->value.value) {
			at(net)->value.value = -1;
			at(net)->value.stable = false;
		}

		if (at(net)->value.strength < strength) {
			at(net)->value.strength = strength;
		}
	}
}

void simulator::propagate(deque<int> &q, int net, bool vacuous) {
	for (int driver = 0; driver < 2; driver++) {
		for (auto i = base->at(net).sourceOf[driver].begin(); i != base->at(net).sourceOf[driver].end(); i++) {
			auto dev = base->devs.begin()+*i;
			int local_value = encoding.get(base->uid(dev->gate));
			if (local_value == 2 or local_value == dev->threshold) {
				q.push_back(dev->drain);
			}
		}
	}

	/*for (int driver = 0; driver < 2; driver++) {
		for (auto i = base->at(net).rsourceOf[driver].begin(); i != base->at(net).rsourceOf[driver].end(); i++) {
			auto dev = base->devs.begin()+*i;
			int local_value = encoding.get(base->uid(dev->gate));
			if (local_value == 2 or local_value == dev->threshold) {
				q.push_back(dev->drain);
			}
		}
	}*/

	for (int threshold = 0; threshold < 2 and not vacuous; threshold++) {
		for (auto i = base->at(net).gateOf[threshold].begin(); i != base->at(net).gateOf[threshold].end(); i++) {
			q.push_back(base->devs[*i].drain);
		}
	}
	sort(q.begin(), q.end());
	q.erase(unique(q.begin(), q.end()), q.end());
}

void simulator::model(int i, bool reverse, boolean::cube &assume, boolean::cube &guard, int &value, int &drive_strength, int &glitch_value, int &glitch_strength, uint64_t &delay_max) {
	auto dev = base->devs.begin()+i;
	bool fail_assumption = are_mutex(global.xoutnulls(), dev->attr.assume);
	if (debug and fail_assumption) {
		cout << "\tfailed assumption " << export_composition(global, *variables).to_string() << " & " << export_expression(dev->attr.assume, *variables).to_string() << endl;
	}
	boolean::cube observed = encoding;
	boolean::cube assume_action;
	if (not fail_assumption) {
		for (auto c = dev->attr.assume.cubes.begin(); c != dev->attr.assume.cubes.end(); c++) {
			if (not are_mutex(encoding, *c)) {
				assume_action &= *c;
			}
		}
		assume_action = assume_action.xoutnulls();
		observed &= assume_action;
	}

	int source = reverse ? dev->drain : dev->source;
	int drain = reverse ? dev->source : dev->drain;
	int gate_uid = base->uid(dev->gate);
	int source_uid = base->uid(source);
	int drain_uid = base->uid(drain);

	int prev_value = observed.get(drain_uid)+1;
	int prev_strength = 2-strength.get(drain_uid);

	//bool isremote = net != drain;

	int local_value = observed.get(gate_uid);
	int global_value = global.get(gate_uid);

	int source_value = observed.get(source_uid)+1;
	int source_strength = 2-strength.get(source_uid);
	if (fail_assumption) {
		source_strength = prev_strength;
		source_value = prev_value;
	} else if (source_value-1 == 1-dev->driver) {
		if (base->assume_nobackflow) {
			source_strength = 0;
			source_value = 3;
		} else if (source_strength > 1) {
			source_strength = 1;
		}
	} else if (dev->attr.force and source_strength > 2) {
		source_strength = 3;
	} else if (dev->attr.weak and source_strength > 1) {
		source_strength = 1;
	} else if (source_strength > 2) {
		source_strength = 2;
	}

	if (debug) cout << "\t@" << export_variable_name(source, *variables).to_string() << ":" << source_value-1 << "*" << source_strength << "&" << (dev->threshold==0 ? "~" : "") << export_variable_name(dev->gate, *variables).to_string() << ":" << local_value << "->" << export_variable_name(drain, *variables).to_string() << (dev->driver==0 ? "-" : "+") << "*" << drive_strength;

	if (local_value == dev->threshold or (local_value == 2 and global_value == dev->threshold)) {
		if (source_value == 3 or drive_strength > source_strength) {
			// undriven
			// TODO(edward.bingham) consider for backprop
			if (debug) cout << "\tundriven" << endl;
			return;
		} else if (drive_strength < source_strength) {
			// device is driving net stronger than other devices
			value = source_value;
			drive_strength = source_strength;
			if (dev->attr.delay_max < delay_max) {
				delay_max = dev->attr.delay_max;
			} else {
				// TODO(edward.bingham) weak instability?
			}
			if (debug) cout << "\tstronger " << value << "*" << drive_strength << endl;
		} else if (not fail_assumption) /*if (drive_strength == source_strength)*/ {
			value &= source_value;
			if (dev->attr.delay_max < delay_max) {
				delay_max = dev->attr.delay_max;
			}
			// TODO(edward.bingham) this might also cause instability
			if (debug) cout << "\tdriven " << value << "*" << drive_strength << endl;
		}
		if (not fail_assumption and global_value != 2 and global_value != -1) {
			guard.set(gate_uid, global_value);
			assume &= assume_action;
		}
	} /*else if (local_value == 1-dev->threshold) {
		// this device is disabled, value remains unaffected
		// TODO(edward.bingham) check instability?
	}*/ else if (not fail_assumption and (source_value&prev_value) != prev_value and (local_value == -1 or (local_value == 2 and global_value != dev->threshold))) {
		// instability
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

void simulator::evaluate(deque<int> nets) {
	deque<int> q = nets;
	boolean::cube ack;
	while (not q.empty()) {
		int net = q.front();
		int uid = base->uid(net);
		q.pop_front();

		int glitch_value = 3;
		int glitch_strength = 0;
		int drive_strength = 0;
		int value = 3;
		if (base->at(net).keep) {
			drive_strength = 1;
			value = encoding.get(uid)+1;
		}
		boolean::cube guard;
		boolean::cube assumed;
		uint64_t delay_max = std::numeric_limits<uint64_t>::max();

		if (debug) cout << "evaluating " << net << "/(" << base->flip(base->nodes.size()) << "," << base->nets.size() << ") " << export_variable_name(net, *variables).to_string() << ":" << encoding.get(uid) << (base->at(net).keep ? " keep" : "") << endl;
		for (int driver = 0; driver < 2; driver++) {
			for (auto i = base->at(net).drainOf[driver].begin(); i != base->at(net).drainOf[driver].end(); i++) {
				model(*i, false, assumed, guard, value, drive_strength, glitch_value, glitch_strength, delay_max);
			}

			/*for (auto i = base->at(net).rsourceOf[driver].begin(); i != base->at(net).rsourceOf[driver].end(); i++) {
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
				value = encoding.get(uid);
			} else {
				value = -1;
			}
		}

		if (debug) cout << value << " strength = " << drive_strength << endl;

		// TODO(edward.bingham) we should only propagate instantly here if delay_max is 0, we need to handle the other condition in the import/export of production rules, not in the simulator
		if ((delay_max == 0 or (base->at(net).gateOf[0].empty() and base->at(net).gateOf[1].empty()))) {
			if (value >= 0) {
				ack &= guard & assumed;
				assume(assumed);
			}

			int avalue = assumed.get(base->uid(net));
			if (avalue == 2 or avalue != 1-value) {
				set(net, value, drive_strength, stable, &q);
			}
		} else {
			int avalue = assumed.get(base->uid(net));
			if (avalue == 2 or avalue != 1-value) {
				schedule(delay_max, assumed, guard, net, value, drive_strength, stable);
			}
		}
	}

	encoding = encoding & ack;
}

enabled_transition simulator::fire(int net) {
	enabled_transition t;
	if (net >= (int)nets.size()) {
		t = enabled.pop();
	} else if (at(net) != nullptr) {
		t = enabled.pop(at(net));
		at(net) = nullptr;
	} else {
		return t;
	}

	at(t.net) = nullptr;
	
	if (debug) {
		printf("firing %s->%s%c:%d%s {%s}\n", export_expression(t.guard, *variables).to_string().c_str(), export_variable_name(t.net, *variables).to_string().c_str(), t.value == 0 ? '-' : (t.value == 1 ? '+' : '~'), t.strength, t.stable ? "" : " unstable", export_expression(t.assume, *variables).to_string().c_str());
	}

	if (t.value >= 0) {
		encoding &= t.guard & t.assume;
		assume(t.assume);
	}

	deque<int> q;
	set(t.net, t.value, t.strength, t.stable);
	return t;
}

void simulator::assume(boolean::cube assume) {
	for (int uid = 0; uid < (int)assume.values.size()*16; uid++) {
		int value = assume.get(uid);
		int net = base->idx(uid);
		if (value != 2) {
			auto e = at(net);
			if (e != nullptr and (e->value.value != value or not e->value.stable)) {
				if (debug) printf("popping event %d\n", net);
				enabled.pop(e);
				at(net) = nullptr;
			}
		}
	}
}

void simulator::set(int net, int value, int strength, bool stable, deque<int> *q) {
	if (base->require_stable and not stable and strength > 0) {
		error("", "unstable rule " + export_variable_name(net, *variables).to_string() + (value == 1 ? "+" : (value == 0 ? "-" : "~")), __FILE__, __LINE__);
	}
	if (base->require_noninterfering and stable and value == -1 and strength > 0) {
		error("", "interference " + export_variable_name(net, *variables).to_string(), __FILE__, __LINE__);
	}
	if (base->require_driven and strength == 0 and net >= 0) {
		error("", "floating node " + export_variable_name(net, *variables).to_string(), __FILE__, __LINE__);
	}

	if (net > base->flip((int)nodes.size()) and net < (int)nets.size() and at(net) != nullptr) {
		enabled.pop(at(net));
		at(net) = nullptr;
	}

	int uid = base->uid(net);
	int prev_value = encoding.get(uid);
	int prev_strength = 2-this->strength.get(uid);

	//cout << "\tprev value = " << prev_value << " strength = " << prev_strength << endl;
	bool vacuous = false;
	if (value == prev_value) {
		if (strength == prev_strength) {
			// vacuous transition
			return;
		}
		vacuous = true;
	}

	if (base->require_adiabatic and not vacuous and (value == 0 or value == 1)) {
		vector<int> viol;
		for (auto i = base->at(net).gateOf[value].begin(); i != base->at(net).gateOf[value].end(); i++) {
			int drain_value = encoding.get(base->uid(base->devs[*i].drain));
			int source_value = encoding.get(base->uid(base->devs[*i].source));
			if ((not base->assume_nobackflow or source_value == base->devs[*i].driver)
				and drain_value != source_value) {
				//printf("assume_nobackflow %d source_value %d driver %d drain_value %d value %d threshold %d\n", (int)base->assume_nobackflow, source_value, base->devs[*i].driver, drain_value, value, base->devs[*i].threshold);
				viol.push_back(*i);
			}
		}
		if (not viol.empty()) {
			error("", "non-adiabatic transition " + export_variable_name(net, *variables).to_string() + (value == 1 ? "+" : (value == 0 ? "-" : "~")), __FILE__, __LINE__);
			string msg = "{";
			for (auto i = viol.begin(); i != viol.end(); i++) {
				if (i != viol.begin()) {
					msg += ", ";
				}
				string source_name = export_variable_name(base->devs[*i].source, *variables).to_string();
				string gate_name = export_variable_name(net, *variables).to_string();
				string drain_name = export_variable_name(base->devs[*i].drain, *variables).to_string();
				msg += "@" + source_name + "&" + (value==0 ? "~" : "") + gate_name + "->" + drain_name + (base->devs[*i].driver==1 ? "+" : "-");
			}
			msg += "}";
			note("", msg, __FILE__, __LINE__);
		}
	}

	encoding.set(uid, value);
	global.set(uid, value);
	this->strength.set(uid, 2-strength);
	for (auto i = base->at(net).remote.begin(); i != base->at(net).remote.end(); i++) {
		if (*i == net) {
			continue;
		}
		encoding.remote_set(*i, value, stable);
		global.set(*i, value);
		this->strength.set(*i, 2-strength);
	}

	deque<int> tmp;
	bool doEval = false;
	if (q == nullptr) {
		q = &tmp;
		doEval = true;
	}

	for (auto i = base->at(net).remote.begin(); i != base->at(net).remote.end(); i++) {
		propagate(*q, *i, vacuous);
	}
	if (doEval and not q->empty()) {
		evaluate(*q);
	}
}

void simulator::set(boolean::cube action, int strength, bool stable, deque<int> *q) {
	boolean::cube remote_action = action.remote(variables->get_groups());
	for (int uid = 0; uid < action.size()*16; uid++) {
		int net = base->idx(uid);
		int val = action.get(uid);
		if (val != 2 and net > base->flip((int)nodes.size()) and net < (int)nets.size() and at(net) != nullptr) {
			enabled.pop(at(net));
			at(net) = nullptr;
		}
	}

	global = local_assign(global, remote_action, true);
	encoding = remote_assign(local_assign(encoding, action, true), global, true);
	this->strength &= remote_action.mask().flip();

	deque<int> tmp;
	bool doEval = false;
	if (q == nullptr) {
		q = &tmp;
		doEval = true;
	}

	for (int uid = 0; uid < remote_action.size()*16; uid++) {
		if (remote_action.get(uid) != 2) {
			propagate(*q, base->idx(uid), false);
		}
	}
	if (doEval and not q->empty()) {
		evaluate(*q);
	}
}

void simulator::reset()
{
	enabled.clear();
	nets.clear();
	nodes.clear();
	global.values.clear();
	encoding.values.clear();
	strength.values.clear();
	for (int i = -(int)base->nodes.size(); i < (int)base->nets.size(); i++) {
		int uid = base->uid(i);
		global.set(uid, -1);
		encoding.set(uid, -1);
	}

	for (int i = -(int)base->nodes.size(); i < (int)base->nets.size(); i++) {
		if (base->at(i).driver >= 0) {
			set(base->uid(i), base->at(i).driver);
		}
	}

	wait();

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
	for (int uid = 0; uid < (int)global.values.size()*16; uid++) {
		int net = base->idx(uid);
		int value = global.get(uid);
		if (encoding.get(uid) != value) {
			schedule(10000, 1, 1, net, value, 2, true);
		}
	}
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
