#include "production_rule.h"
#include <limits>

using namespace std;

namespace prs
{

attributes::attributes() {
	weak = false;
	keeper = false;
	width = 0.0;
	length = 0.0;
	variant = "";
	delay_max = 10000; // 10ns
}

attributes::attributes(bool weak, bool keeper, float width, float length, string variant, uint64_t delay_max) {
	this->weak = weak;
	this->keeper = keeper;
	this->width = width;
	this->length = length;
	this->variant = variant;
	this->delay_max = delay_max;
}

attributes::~attributes() {
}

device::device() {
	gate = -1;
	source = -1;
	drain = -1;
	threshold = 1;
	driver = 0;
}

device::device(int source, int gate, int drain, int threshold, int driver, attributes attr) {
	this->source = source;
	this->gate = gate;
	this->drain = drain;
	this->threshold = threshold;
	this->driver = driver;
	this->attr = attr;
}

device::~device() {
}

net::net() {
}

net::~net() {
}

void net::add_remote(int uid) {
	auto pos = lower_bound(remote.begin(), remote.end(), uid);
	if (pos == remote.end() or *pos != uid) {
		remote.insert(pos, uid);
	}
}

production_rule_set::production_rule_set() {
}

production_rule_set::production_rule_set(const ucs::variable_set &v) {
	init(v);
}

production_rule_set::~production_rule_set() {
}

void production_rule_set::init(const ucs::variable_set &v) {
	if (nets.size() < v.nodes.size()) {
		nets.resize(v.nodes.size());
	}
	vector<vector<int> > groups = v.get_groups();
	for (int i = 0; i < (int)groups.size(); i++) {
		for (int j = 0; j < (int)groups[i].size(); j++) {
			nets[groups[i][j]].remote = groups[i];
		}
	}
}

int production_rule_set::flip(int index) {
	return -index-1;
}

net &production_rule_set::at(int index) {
	if (index >= 0) {
		return nets[index];
	}
	return nodes[flip(index)];
}

const net &production_rule_set::at(int index) const {
	if (index >= 0) {
		return nets[index];
	}
	return nodes[flip(index)];
}

net &production_rule_set::create(int index) {
	if (index >= 0) {
		if (index >= (int)nets.size()) {
			nets.resize(index+1);
		}
		return nets[index];
	}
	index = flip(index);
	if (index >= (int)nodes.size()) {
		nodes.resize(index+1);
	}
	return nodes[index];
}

int production_rule_set::connect(int n0, int n1) {
	if (n1 >= (int)nets.size()) {
		nets.resize(n1+1);
	}

	if (n0 == n1) {
		return n1;
	}

	for (auto d = devs.begin(); d != devs.end(); d++) {
		if (d->gate == n0) {
			d->gate = n1;
		}
		if (n0 >= 0 and d->gate > n0) {
			d->gate--;
		} else if (n0 < 0 and d->gate < n0) {
			d->gate++;
		}

		if (d->source == n0) {
			d->source = n1;
		}
		if (n0 >= 0 and d->source > n0) {
			d->source--;
		} else if (n0 < 0 and d->source < n0) {
			d->source++;
		}

		if (d->drain == n0) {
			d->drain = n1;
		}
		if (n0 >= 0 and d->drain > n0) {
			d->drain--;
		} else if (n0 < 0 and d->drain < n0) {
			d->drain++;
		}
	}

	at(n1).gateOf.insert(at(n1).gateOf.end(), at(n0).gateOf.begin(), at(n0).gateOf.end());
	sort(at(n1).gateOf.begin(), at(n1).gateOf.end());
	at(n1).gateOf.erase(unique(at(n1).gateOf.begin(), at(n1).gateOf.end()), at(n1).gateOf.end());
	at(n1).remote.insert(at(n1).remote.end(), at(n0).remote.begin(), at(n0).remote.end());
	sort(at(n1).remote.begin(), at(n1).remote.end());
	at(n1).remote.erase(unique(at(n1).remote.begin(), at(n1).remote.end()), at(n1).remote.end());
	for (int i = 0; i < 2; i++) {
		at(n1).drainOf[i].insert(at(n1).drainOf[i].end(), at(n0).drainOf[i].begin(), at(n0).drainOf[i].end());
		sort(at(n1).drainOf[i].begin(), at(n1).drainOf[i].end());
		at(n1).drainOf[i].erase(unique(at(n1).drainOf[i].begin(), at(n1).drainOf[i].end()), at(n1).drainOf[i].end());
		at(n1).sourceOf[i].insert(at(n1).sourceOf[i].end(), at(n0).sourceOf[i].begin(), at(n0).sourceOf[i].end());
		sort(at(n1).sourceOf[i].begin(), at(n1).sourceOf[i].end());
		at(n1).sourceOf[i].erase(unique(at(n1).sourceOf[i].begin(), at(n1).sourceOf[i].end()), at(n1).sourceOf[i].end());
	}

	if (n0 >= 0) {
		nets.erase(nets.begin()+n0);
		for (auto n = nets.begin(); n != nets.end(); n++) {
			for (int i = (int)n->remote.size()-1; i >= 0; i--) {
				if (n->remote[i] == n0) {
					n->remote.erase(n->remote.begin()+i);
				} else if (n->remote[i] > n0) {
					n->remote[i]--;
				}
			}
		}
		if (n0 < n1) {
			n1--;
		}
	} else {
		nodes.erase(nodes.begin()+flip(n0));
		for (auto n = nodes.begin(); n != nodes.end(); n++) {
			for (int i = (int)n->remote.size()-1; i >= 0; i--) {
				if (n->remote[i] == n0) {
					n->remote.erase(n->remote.begin()+i);
				} else if (n->remote[i] < n0) {
					n->remote[i]++;
				}
			}
		}
		if (n0 > n1) {
			n1++;
		}
	}

	return n1;
}

void production_rule_set::replace(vector<int> &lst, int from, int to) {
	if (to == from) {
		return;
	}
	for (int i = (int)lst.size()-1; i >= 0; i--) {
		if (lst[i] == from) {
			lst[i] = to;
		}
		if (from >= 0 and lst[i] > from) {
			lst[i]--;
		} else if (from < 0 and lst[i] < from) {
			lst[i]++;
		}
	}
}

int production_rule_set::add_source(int gate, int drain, int threshold, int driver, attributes attr) {
	if (gate >= 0 and gate >= (int)nets.size()) {
		nets.resize(gate+1);
	} else if (gate < 0 and flip(gate) >= (int)nodes.size()) {
		nodes.resize(flip(gate)+1);
	}
	if (drain >= 0 and drain >= (int)nets.size()) {
		nets.resize(drain+1);
	} else if (drain < 0 and flip(drain) >= (int)nodes.size()) {
		nodes.resize(flip(drain)+1);
	}

	int source = flip(nodes.size());
	nodes.push_back(net());
	nodes.back().sourceOf[driver].push_back(devs.size());
	at(gate).gateOf.push_back(devs.size());
	at(drain).drainOf[driver].push_back(devs.size());
	devs.push_back(device(source, gate, drain, threshold, driver, attr));
	return source;
}

int production_rule_set::add_drain(int source, int gate, int threshold, int driver, attributes attr) {
	if (gate >= 0 and gate >= (int)nets.size()) {
		nets.resize(gate+1);
	} else if (gate < 0 and flip(gate) >= (int)nodes.size()) {
		nodes.resize(flip(gate)+1);
	}
	if (source >= 0 and source >= (int)nets.size()) {
		nets.resize(source+1);
	} else if (source < 0 and flip(source) >= (int)nodes.size()) {
		nodes.resize(flip(source)+1);
	}

	int drain = flip(nodes.size());
	nodes.push_back(net());
	nodes.back().sourceOf[driver].push_back(devs.size());
	at(gate).gateOf.push_back(devs.size());
	at(drain).drainOf[driver].push_back(devs.size());
	devs.push_back(device(source, gate, drain, threshold, driver, attr));
	return drain;
}

int production_rule_set::add(boolean::cube guard, int drain, int driver, attributes attr, vector<int> order) {
	for (int i = 0; i < (int)order.size() and not guard.is_tautology(); i++) {
		int threshold = guard.get(order[i]);
		if (threshold != 2) {
			drain = add_source(order[i], drain, threshold, driver, attr);
			guard.hide(order[i]);
		}
	}

	for (int i = 0; i < (int)nets.size() and not guard.is_tautology(); i++) {
		int threshold = guard.get(i);
		if (threshold != 2) {
			drain = add_source(i, drain, threshold, driver, attr);
			guard.hide(i);
		}
	}
	return drain;
}

int production_rule_set::add_hfactor(boolean::cover guard, int drain, int driver, attributes attr, vector<int> order) {
	if (guard.cubes.size() == 1) {
		return add(guard.cubes[0], drain, driver, attr, order);
	}

	boolean::cube common = guard.supercube();
	if (not common.is_tautology()) {
		guard.cofactor(common);
		drain = add(common, drain, driver, attr, order);
	}

	if (not guard.is_tautology()) {
		boolean::cover left, right;
		guard.partition(left, right);
		int drainLeft = add_hfactor(left, drain, driver, attr, order);
		drain = add_hfactor(right, drain, driver, attr, order);
		drain = connect(drainLeft, drain);
	}

	return drain;
}

void production_rule_set::add(int source, boolean::cover guard, boolean::cover action, attributes attr, vector<int> order) {
	for (auto c = action.cubes.begin(); c != action.cubes.end(); c++) {
		for (int i = 0; i < (int)nets.size(); i++) {
			int driver = c->get(i);
			if (driver != 2) {
				connect(add_hfactor(guard, i, driver, attr, order), source);
			}
		}
	}
}

}
