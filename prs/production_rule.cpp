#include "production_rule.h"
#include <limits>
#include <common/message.h>
#include <interpret_boolean/export.h>

using namespace std;

namespace prs
{

const bool debug = false;

attributes::attributes() {
	weak = false;
	pass = false;
	assume = 1;
	delay_max = 10000; // 10ns
	size = 0.0;
	variant = "";
}

attributes::attributes(bool weak, bool pass, boolean::cube assume, uint64_t delay_max) {
	this->weak = weak;
	this->pass = pass;
	this->assume = assume;
	this->delay_max = delay_max;
	this->size = 0.0;
	this->variant = "";
}

attributes::~attributes() {
}

void attributes::set_internal() {
	assume = 1;
	delay_max = 0;
}

bool operator==(const attributes &a0, const attributes &a1) {
	return a0.weak == a1.weak and a0.pass == a1.pass and a0.delay_max == a1.delay_max and a0.assume == a1.assume;
}

bool operator!=(const attributes &a0, const attributes &a1) {
	return a0.weak != a1.weak or a0.pass != a1.pass or a0.delay_max != a1.delay_max or a0.assume != a1.assume;
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
	this->mirror = 0;
	this->driver = -1;
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

void production_rule_set::print(const ucs::variable_set &v) {
	cout << "nets " << nets.size() << endl;
	for (int i = 0; i < (int)nets.size(); i++) {
		cout << "net " << i << ": " << export_variable_name(i, v).to_string() << " gateOf=" << to_string(nets[i].gateOf[0]) << to_string(nets[i].gateOf[1]) << " sourceOf=" << to_string(nets[i].sourceOf[0]) << to_string(nets[i].sourceOf[1]) << " drainOf=" << to_string(nets[i].drainOf[0]) << to_string(nets[i].drainOf[1]) << " remote=" << to_string(nets[i].remote) << (nets[i].keep ? " keep" : "") << endl;
	}

	cout << "nodes " << nodes.size() << endl;
	for (int i = 0; i < (int)nodes.size(); i++) {
		cout << "node " << i << ": " << export_variable_name(flip(i), v).to_string() << " gateOf=" << to_string(nodes[i].gateOf[0]) << to_string(nodes[i].gateOf[1]) << " sourceOf=" << to_string(nodes[i].sourceOf[0]) << to_string(nodes[i].sourceOf[1]) << " drainOf=" << to_string(nodes[i].drainOf[0]) << to_string(nodes[i].drainOf[1]) << " remote=" << to_string(nodes[i].remote) << (nodes[i].keep ? " keep" : "") << endl;
	}

	cout << "devs " << devs.size() << endl;
	for (int i = 0; i < (int)devs.size(); i++) {
		cout << "dev " << i << ": source=" << export_variable_name(devs[i].source, v).to_string() << "(" << devs[i].source << ") gate=" << export_variable_name(devs[i].gate, v).to_string() << "(" << devs[i].gate << ") drain=" << export_variable_name(devs[i].drain, v).to_string() << "(" << devs[i].drain << ") threshold=" << devs[i].threshold << " driver=" << devs[i].driver << (not devs[i].attr.assume.is_tautology() ? " {" + export_expression(devs[i].attr.assume, v).to_string() + "}" : "") << (devs[i].attr.weak ? " weak" : "") << (devs[i].attr.pass ? " pass" : "") << endl;
	}
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

int production_rule_set::idx(int uid) const {
	if (uid >= (int)nets.size()) {
		return flip(uid-(int)nets.size());
	}
	return uid;
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

int production_rule_set::sources(int net, int value) const {
	int result = 0;
	for (auto i = at(net).sourceOf[value].begin(); i != at(net).sourceOf[value].end(); i++) {
		result += (devs[*i].source == net);
	}
	return result;
}

int production_rule_set::drains(int net, int value) const {
	int result = 0;
	for (auto i = at(net).drainOf[value].begin(); i != at(net).drainOf[value].end(); i++) {
		result += (devs[*i].drain == net);
	}
	return result;
}


int production_rule_set::sources(int net, int value, attributes attr) const {
	int result = 0;
	for (auto i = at(net).sourceOf[value].begin(); i != at(net).sourceOf[value].end(); i++) {
		result += (devs[*i].source == net and devs[*i].attr == attr);
	}
	return result;
}

int production_rule_set::drains(int net, int value, attributes attr) const {
	int result = 0;
	for (auto i = at(net).drainOf[value].begin(); i != at(net).drainOf[value].end(); i++) {
		result += (devs[*i].drain == net and devs[*i].attr == attr);
	}
	return result;
}

vector<attributes> production_rule_set::attribute_groups(int net, int value) const {
	vector<attributes> result;
	for (auto i = at(net).drainOf[value].begin(); i != at(net).drainOf[value].end(); i++) {
		auto dev = devs.begin()+*i;
		bool found = false;
		for (auto j = result.begin(); j != result.end() and not found; j++) {
			found = *j == dev->attr;
		}
		if (not found) {
			result.push_back(dev->attr);
		}
	}
	return result;
}

void production_rule_set::set_power(int vdd, int gnd) {
	at(vdd).keep = true;
	at(vdd).driver = 1;
	at(vdd).mirror = gnd;
	at(gnd).keep = true;
	at(gnd).driver = 0;
	at(gnd).mirror = vdd;
	pwr.push_back({gnd, vdd});
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

void production_rule_set::replace(map<int, int> &lst, int from, int to) {
	if (to == from) {
		return;
	}
	for (auto i = lst.begin(); i != lst.end(); i++) {
		if (i->second == from) {
			i->second = to;
		}
		if (from >= 0 and i->second > from) {
			i->second--;
		} else if (from < 0 and i->second < from) {
			i->second++;
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
			attr.set_internal();
		}
	}

	for (int i = 0; i < (int)nets.size() and not guard.is_tautology(); i++) {
		int threshold = guard.get(i);
		if (threshold != 2) {
			drain = add_source(i, drain, threshold, driver, attr);
			guard.hide(i);
			attr.set_internal();
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
		attr.set_internal();
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

void production_rule_set::add(int source, boolean::cover guard, int var, int val, attributes attr, vector<int> order) {
	connect(add_hfactor(guard, var, val, attr, order), source);
}

void production_rule_set::add(int source, boolean::cover guard, boolean::cover action, attributes attr, vector<int> order) {
	for (auto c = action.cubes.begin(); c != action.cubes.end(); c++) {
		for (int i = 0; i < (int)nets.size(); i++) {
			int driver = c->get(i);
			if (driver != 2) {
				add(source, guard, i, driver, attr, order);
			}
		}
	}
}

void production_rule_set::move_gate(int dev, int gate, int threshold) {
	int prev_threshold = devs[dev].threshold;
	int prev_gate = devs[dev].gate;
	if (threshold >= 0) {
		devs[dev].threshold = threshold;
	} else {
		threshold = prev_threshold;
	}
	if (gate != prev_gate or threshold != prev_threshold) {
		devs[dev].gate = gate;
		for (auto i = at(prev_gate).remote.begin(); i != at(prev_gate).remote.end(); i++) {
			at(*i).gateOf[prev_threshold].erase(find(at(*i).gateOf[prev_threshold].begin(), at(*i).gateOf[prev_threshold].end(), dev));
		}
		for (auto i = at(gate).remote.begin(); i != at(gate).remote.end(); i++) {
			at(*i).gateOf[threshold].insert(lower_bound(at(*i).gateOf[threshold].begin(), at(*i).gateOf[threshold].end(), dev), dev);
		}
	}
}

void production_rule_set::move_source_drain(int dev, int source, int drain, int driver) {
	int prev_driver = devs[dev].driver;
	int prev_source = devs[dev].source;
	int prev_drain = devs[dev].drain;

	if (driver >= 0) {
		devs[dev].driver = driver;
	} else {
		driver = prev_driver;
	}

	if (source != prev_source or driver != prev_driver) {
		devs[dev].source = source;
		for (auto i = at(prev_source).remote.begin(); i != at(prev_source).remote.end(); i++) {
			at(*i).sourceOf[prev_driver].erase(find(at(*i).sourceOf[prev_driver].begin(), at(*i).sourceOf[prev_driver].end(), dev));
		}
		for (auto i = at(source).remote.begin(); i != at(source).remote.end(); i++) {
			at(*i).sourceOf[driver].insert(lower_bound(at(*i).sourceOf[driver].begin(), at(*i).sourceOf[driver].end(), dev), dev);
		}
	}

	if (drain != prev_drain or driver != prev_driver) {
		devs[dev].drain = drain;
		for (auto i = at(prev_drain).remote.begin(); i != at(prev_drain).remote.end(); i++) {
			at(*i).drainOf[prev_driver].erase(find(at(*i).drainOf[prev_driver].begin(), at(*i).drainOf[prev_driver].end(), dev));
		}
		for (auto i = at(drain).remote.begin(); i != at(drain).remote.end(); i++) {
			at(*i).drainOf[driver].insert(lower_bound(at(*i).drainOf[driver].begin(), at(*i).drainOf[driver].end(), dev), dev);
		}
	}
}

void production_rule_set::invert(int net) {
	for (int threshold = 0; threshold < 2; threshold++) {
		for (auto i = at(net).gateOf[threshold].begin(); i != at(net).gateOf[threshold].end(); i++) {
			devs[*i].threshold = 1-devs[*i].threshold;
		}
	}
	for (auto i = at(net).remote.begin(); i != at(net).remote.end(); i++) {
		std::swap(at(*i).gateOf[0], at(*i).gateOf[1]);
	}
	
	vector<int> stack;
	stack.insert(stack.end(), at(net).drainOf[0].begin(), at(net).drainOf[0].end());
	stack.insert(stack.end(), at(net).drainOf[1].begin(), at(net).drainOf[1].end());
	sort(stack.begin(), stack.end());
	stack.erase(unique(stack.begin(), stack.end()), stack.end());
	while (not stack.empty()) {
		int di = stack.back();
		auto dev = devs.begin()+di;
		stack.pop_back();

		int new_source = dev->source;
		if (at(dev->source).driver >= 0) {
			new_source = at(dev->source).mirror;
		//} else if (not at(dev->source).gateOf.empty()) {
		//	TODO(edward.bingham) What happens for bubble reshuffling pass
		//	transistor logic?
		} else if (at(dev->source).gateOf[0].empty() and at(dev->source).gateOf[1].empty()) {
			stack.insert(stack.end(), at(dev->source).drainOf[0].begin(), at(dev->source).drainOf[0].end());
			stack.insert(stack.end(), at(dev->source).drainOf[1].begin(), at(dev->source).drainOf[1].end());
			sort(stack.begin(), stack.end());
			stack.erase(unique(stack.begin(), stack.end()), stack.end());
		}
 
		move_source_drain(di, new_source, dev->drain, 1-dev->driver);
	}
}

bool production_rule_set::cmos_implementable() {
	for (int i = 0; i < (int)devs.size(); i++) {
		if (devs[i].threshold == devs[i].driver) {
			return false;
		}
	}
	return true;
}

boolean::cover production_rule_set::guard_of(int net, int driver, bool weak) {
	struct walker {
		int net;
		boolean::cube guard;
	};

	boolean::cover result;
	vector<walker> stack;
	stack.push_back({net, 1});
	while (not stack.empty()) {
		walker curr = stack.back();
		stack.pop_back();

		if ((curr.net != net
			and (not at(curr.net).gateOf[0].empty()
				or not at(curr.net).gateOf[1].empty()))
			or drains(curr.net, driver, weak?1:0) == 0) {
			if (curr.net == net) {
				continue;
			}

			if (at(curr.net).driver < 0) {
				curr.guard.set(uid(curr.net), driver);
			}

			result |= curr.guard;
			continue;
		}

		for (auto i = at(curr.net).drainOf[driver].begin(); i != at(curr.net).drainOf[driver].end(); i++) {
			auto dev = devs.begin()+*i;
			if (dev->drain != curr.net or dev->driver != driver or dev->attr.weak != weak) {
				continue;
			}
			boolean::cube guard = curr.guard;
			guard.set(uid(dev->gate), dev->threshold);
			stack.push_back({dev->source, guard});
		}
	}

	return result;
}

bool production_rule_set::has_inverter(int net, int &_net) {
	array<vector<int>, 2> unary;
	for (int i = 0; i < 2; i++) {
		for (auto j = at(net).gateOf[i].begin(); j != at(net).gateOf[i].end(); j++) {
			auto dev = devs.begin()+*j;
			if (dev->gate != net) {
				continue;
			}
			if (at(dev->source).driver == 1-dev->threshold) {
				unary[i].push_back(*j);
			}
		}
	}

	for (auto i = unary[0].begin(); i != unary[0].end(); i++) {
		for (auto j = unary[1].begin(); j != unary[1].end(); j++) {
			if (devs[*i].drain != devs[*j].drain) {
				continue;
			}
			int n = devs[*i].drain;

			boolean::cover up = guard_of(n, 1);
			boolean::cover dn = guard_of(n, 0);

			if (up == boolean::cover(uid(net), 0) and dn == boolean::cover(uid(net), 1)) {
				_net = n;
				return true;
			}
		}
	}

	return false;
}

void production_rule_set::add_keepers(bool share, boolean::cover keep) {
	bool hasweakpwr = false;
	array<int, 2> sharedweakpwr={
		std::numeric_limits<int>::max(),
		std::numeric_limits<int>::max()
	};
	for (int net = -(int)nodes.size(); net < (int)nets.size(); net++) {
		if (not at(net).keep) {
			continue;
		}

		boolean::cover up = guard_of(net, 1, false);
		boolean::cover dn = guard_of(net, 0, false);

		boolean::cover keep_up = guard_of(net, 1, true);
		boolean::cover keep_dn = guard_of(net, 0, true);

		if (debug) {
			cout << "checking keepers for" << endl;
			cout << "up: " << up << endl;
			cout << "dn: " << dn << endl;
			cout << "keep_up: " << keep_up << endl;
			cout << "keep_dn: " << keep_dn << endl;
			cout << "keep: " << keep << endl;
			cout << "covered: " << (up|dn|keep_up|keep_dn) << endl;
		}

		at(net).keep = false;
		if (at(net).driver >= 0 or keep.is_subset_of(up|dn|keep_up|keep_dn)) {
			if (debug) cout << "not needed" << endl << endl;
			continue;
		}
		if (debug) cout << "making keeper" << endl << endl;

		// identify output inverter if it exists, create it if it doesn't
		// (using the half cycle timing assumption)
		int _net=std::numeric_limits<int>::max();
		if (not has_inverter(net, _net)) {
			_net = flip(nodes.size());
			create(_net);
			attributes attr;
			attr.delay_max = 0;
			connect(add_source(net, _net, 1, 0, attr), pwr[0][0]);
			connect(add_source(net, _net, 0, 1, attr), pwr[0][1]);
		}

		array<int, 2> weakpwr;
		weakpwr[0] = sharedweakpwr[0];
		weakpwr[1] = sharedweakpwr[1];
		if (not share or not hasweakpwr) {
			weakpwr[0] = add_drain(pwr[0][0], pwr[0][1], 1, 0, attributes(true, false));
			weakpwr[1] = add_drain(pwr[0][1], pwr[0][0], 0, 1, attributes(true, false));
			sharedweakpwr = weakpwr;
			hasweakpwr = true;
		}

		connect(add_source(_net, net, 1, 0), weakpwr[0]);
		connect(add_source(_net, net, 0, 1), weakpwr[1]);
	}
}

}
