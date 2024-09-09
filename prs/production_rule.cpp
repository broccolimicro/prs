#include "production_rule.h"
#include <limits>

using namespace std;

namespace prs
{

attributes::attributes() {
	weak = false;
	pass = false;
	width = 0.0;
	length = 0.0;
	variant = "";
	delay_max = 10000; // 10ns
}

attributes::attributes(bool weak, bool pass, float width, float length, string variant, uint64_t delay_max) {
	this->weak = weak;
	this->pass = pass;
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

net::net(bool keep) {
	this->keep = keep;
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

int production_rule_set::uid(int index) const {
	if (index < 0) {
		return (int)nets.size()+flip(index);
	}
	return index;
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

net &production_rule_set::create(int index, bool keep) {
	if (index >= 0) {
		int sz = (int)nets.size();
		if (index >= sz) {
			nets.resize(index+1);
			for (int i = sz; i < (int)nets.size(); i++) {
				nets[i].add_remote(i);
			}
		}
		if (keep) {
			nets[index].keep = true;
		}
		return nets[index];
	}
	index = flip(index);
	int sz = (int)nodes.size();
	if (index >= sz) {
		nodes.resize(index+1);
		for (int i = sz; i < (int)nodes.size(); i++) {
			nodes[i].add_remote(flip(i));
		}
	}
	if (keep) {
		nodes[index].keep = true;
	}
	return nodes[index];
}

void production_rule_set::connect_remote(int n0, int n1) {
	net *i0 = &at(n0);
	net *i1 = &at(n1);

	i1->remote.insert(i1->remote.end(), i0->remote.begin(), i0->remote.end());
	sort(i1->remote.begin(), i1->remote.end());
	i1->remote.erase(unique(i1->remote.begin(), i1->remote.end()), i1->remote.end());
	i0->remote = i1->remote;
	for (int i = 0; i < 2; i++) {
		i1->gateOf[i].insert(i1->gateOf[i].end(), i0->gateOf[i].begin(), i0->gateOf[i].end());
		sort(i1->gateOf[i].begin(), i1->gateOf[i].end());
		i1->gateOf[i].erase(unique(i1->gateOf[i].begin(), i1->gateOf[i].end()), i1->gateOf[i].end());
		i0->gateOf[i] = i1->gateOf[i];
		
		i1->drainOf[i].insert(i1->drainOf[i].end(), i0->drainOf[i].begin(), i0->drainOf[i].end());
		sort(i1->drainOf[i].begin(), i1->drainOf[i].end());
		i1->drainOf[i].erase(unique(i1->drainOf[i].begin(), i1->drainOf[i].end()), i1->drainOf[i].end());
		i0->drainOf[i] = i1->drainOf[i];
		
		i1->sourceOf[i].insert(i1->sourceOf[i].end(), i0->sourceOf[i].begin(), i0->sourceOf[i].end());
		sort(i1->sourceOf[i].begin(), i1->sourceOf[i].end());
		i1->sourceOf[i].erase(unique(i1->sourceOf[i].begin(), i1->sourceOf[i].end()), i1->sourceOf[i].end());
		i0->sourceOf[i] = i1->sourceOf[i];

		i1->rsourceOf[i].insert(i1->rsourceOf[i].end(), i0->rsourceOf[i].begin(), i0->rsourceOf[i].end());
		sort(i1->rsourceOf[i].begin(), i1->rsourceOf[i].end());
		i1->rsourceOf[i].erase(unique(i1->rsourceOf[i].begin(), i1->rsourceOf[i].end()), i1->rsourceOf[i].end());
		i0->rsourceOf[i] = i1->rsourceOf[i];
	}
}

int production_rule_set::connect(int n0, int n1) {
	create(n1);
	if (n0 == n1
		or (n0 >= 0 and n0 >= (int)nets.size())
		or (n0 < 0 and flip(n0) >= (int)nodes.size())) {
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

	for (auto i = at(n0).remote.begin(); i != at(n0).remote.end(); i++) {
		if (*i == n0) {
			continue;
		}

		at(*i).remote.insert(at(*i).remote.end(), at(n1).remote.begin(), at(n1).remote.end());
		sort(at(*i).remote.begin(), at(*i).remote.end());
		at(*i).remote.erase(unique(at(*i).remote.begin(), at(*i).remote.end()), at(*i).remote.end());
		for (int j = 0; j < 2; j++) {
			at(*i).gateOf[j].insert(at(*i).gateOf[j].end(), at(n1).gateOf[j].begin(), at(n1).gateOf[j].end());
			sort(at(*i).gateOf[j].begin(), at(*i).gateOf[j].end());
			at(*i).gateOf[j].erase(unique(at(*i).gateOf[j].begin(), at(*i).gateOf[j].end()), at(*i).gateOf[j].end());
			at(*i).drainOf[j].insert(at(*i).drainOf[j].end(), at(n1).drainOf[j].begin(), at(n1).drainOf[j].end());
			sort(at(*i).drainOf[j].begin(), at(*i).drainOf[j].end());
			at(*i).drainOf[j].erase(unique(at(*i).drainOf[j].begin(), at(*i).drainOf[j].end()), at(*i).drainOf[j].end());
			at(*i).sourceOf[j].insert(at(*i).sourceOf[j].end(), at(n1).sourceOf[j].begin(), at(n1).sourceOf[j].end());
			sort(at(*i).sourceOf[j].begin(), at(*i).sourceOf[j].end());
			at(*i).sourceOf[j].erase(unique(at(*i).sourceOf[j].begin(), at(*i).sourceOf[j].end()), at(*i).sourceOf[j].end());
			at(*i).rsourceOf[j].insert(at(*i).rsourceOf[j].end(), at(n1).rsourceOf[j].begin(), at(n1).rsourceOf[j].end());
			sort(at(*i).rsourceOf[j].begin(), at(*i).rsourceOf[j].end());
			at(*i).rsourceOf[j].erase(unique(at(*i).rsourceOf[j].begin(), at(*i).rsourceOf[j].end()), at(*i).rsourceOf[j].end());
		}
	}

	for (auto i = at(n1).remote.begin(); i != at(n1).remote.end(); i++) {
		if (*i == n1) {
			continue;
		}

		at(*i).remote.insert(at(*i).remote.end(), at(n0).remote.begin(), at(n0).remote.end());
		sort(at(*i).remote.begin(), at(*i).remote.end());
		at(*i).remote.erase(unique(at(*i).remote.begin(), at(*i).remote.end()), at(*i).remote.end());
		for (int j = 0; j < 2; j++) {
			at(*i).gateOf[j].insert(at(*i).gateOf[j].end(), at(n0).gateOf[j].begin(), at(n0).gateOf[j].end());
			sort(at(*i).gateOf[j].begin(), at(*i).gateOf[j].end());
			at(*i).gateOf[j].erase(unique(at(*i).gateOf[j].begin(), at(*i).gateOf[j].end()), at(*i).gateOf[j].end());
			at(*i).drainOf[j].insert(at(*i).drainOf[j].end(), at(n0).drainOf[j].begin(), at(n0).drainOf[j].end());
			sort(at(*i).drainOf[j].begin(), at(*i).drainOf[j].end());
			at(*i).drainOf[j].erase(unique(at(*i).drainOf[j].begin(), at(*i).drainOf[j].end()), at(*i).drainOf[j].end());
			at(*i).sourceOf[j].insert(at(*i).sourceOf[j].end(), at(n0).sourceOf[j].begin(), at(n0).sourceOf[j].end());
			sort(at(*i).sourceOf[j].begin(), at(*i).sourceOf[j].end());
			at(*i).sourceOf[j].erase(unique(at(*i).sourceOf[j].begin(), at(*i).sourceOf[j].end()), at(*i).sourceOf[j].end());
			at(*i).rsourceOf[j].insert(at(*i).rsourceOf[j].end(), at(n0).rsourceOf[j].begin(), at(n0).rsourceOf[j].end());
			sort(at(*i).rsourceOf[j].begin(), at(*i).rsourceOf[j].end());
			at(*i).rsourceOf[j].erase(unique(at(*i).rsourceOf[j].begin(), at(*i).rsourceOf[j].end()), at(*i).rsourceOf[j].end());
		}
	}

	at(n1).remote.insert(at(n1).remote.end(), at(n0).remote.begin(), at(n0).remote.end());
	sort(at(n1).remote.begin(), at(n1).remote.end());
	at(n1).remote.erase(unique(at(n1).remote.begin(), at(n1).remote.end()), at(n1).remote.end());
	for (int j = 0; j < 2; j++) {
		at(n1).gateOf[j].insert(at(n1).gateOf[j].end(), at(n0).gateOf[j].begin(), at(n0).gateOf[j].end());
		sort(at(n1).gateOf[j].begin(), at(n1).gateOf[j].end());
		at(n1).gateOf[j].erase(unique(at(n1).gateOf[j].begin(), at(n1).gateOf[j].end()), at(n1).gateOf[j].end());
		at(n1).drainOf[j].insert(at(n1).drainOf[j].end(), at(n0).drainOf[j].begin(), at(n0).drainOf[j].end());
		sort(at(n1).drainOf[j].begin(), at(n1).drainOf[j].end());
		at(n1).drainOf[j].erase(unique(at(n1).drainOf[j].begin(), at(n1).drainOf[j].end()), at(n1).drainOf[j].end());
		at(n1).sourceOf[j].insert(at(n1).sourceOf[j].end(), at(n0).sourceOf[j].begin(), at(n0).sourceOf[j].end());
		sort(at(n1).sourceOf[j].begin(), at(n1).sourceOf[j].end());
		at(n1).sourceOf[j].erase(unique(at(n1).sourceOf[j].begin(), at(n1).sourceOf[j].end()), at(n1).sourceOf[j].end());
		at(n1).rsourceOf[j].insert(at(n1).rsourceOf[j].end(), at(n0).rsourceOf[j].begin(), at(n0).rsourceOf[j].end());
		sort(at(n1).rsourceOf[j].begin(), at(n1).rsourceOf[j].end());
		at(n1).rsourceOf[j].erase(unique(at(n1).rsourceOf[j].begin(), at(n1).rsourceOf[j].end()), at(n1).rsourceOf[j].end());
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
		for (auto n = nodes.begin(); n != nodes.end(); n++) {
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
		for (auto n = nets.begin(); n != nets.end(); n++) {
			for (int i = (int)n->remote.size()-1; i >= 0; i--) {
				if (n->remote[i] == n0) {
					n->remote.erase(n->remote.begin()+i);
				} else if (n->remote[i] < n0) {
					n->remote[i]++;
				}
			}
		}
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
	nodes.back().remote.push_back(source);
	nodes.back().sourceOf[driver].push_back(devs.size());

	for (auto i = at(gate).remote.begin(); i != at(gate).remote.end(); i++) {
		at(*i).gateOf[threshold].push_back(devs.size());
	}
	
	for (auto i = at(drain).remote.begin(); i != at(drain).remote.end(); i++) {
		at(*i).drainOf[driver].push_back(devs.size());
		if (attr.pass) {
			at(*i).rsourceOf[driver].push_back(devs.size());
		}
	}
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
	nodes.back().remote.push_back(drain);
	nodes.back().sourceOf[driver].push_back(devs.size());
	for (auto i = at(gate).remote.begin(); i != at(gate).remote.end(); i++) {
		at(*i).gateOf[threshold].push_back(devs.size());
	}
	for (auto i = at(drain).remote.begin(); i != at(drain).remote.end(); i++) {
		at(*i).drainOf[driver].push_back(devs.size());
		if (attr.pass) {
			at(*i).rsourceOf[driver].push_back(devs.size());
		}
	}
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
