#include "production_rule.h"
#include <limits>
#include <common/message.h>
#include <common/timer.h>
#include <interpret_boolean/export.h>

using namespace std;

namespace prs
{

const bool debug = false;

attributes::attributes() {
	weak = false;
	force = false;
	pass = false;
	assume = 1;
	delay_max = 10000; // 10ns
	size = 0.0;
	variant = "";
}

attributes::attributes(bool weak, bool pass, boolean::cube assume, uint64_t delay_max) {
	this->weak = weak;
	this->force = false;
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

attributes attributes::instant() {
	return attributes(false, false, 1, 0);
}

bool operator==(const attributes &a0, const attributes &a1) {
	return a0.weak == a1.weak and a0.force == a1.force and a0.pass == a1.pass and a0.delay_max == a1.delay_max and a0.assume == a1.assume;
}

bool operator!=(const attributes &a0, const attributes &a1) {
	return a0.weak != a1.weak or a0.force != a1.force or a0.pass != a1.pass or a0.delay_max != a1.delay_max or a0.assume != a1.assume;
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
	this->region = 0;
	this->keep = keep;
	this->isIO = false;
	this->mirror = 0;
	this->driver = -1;
}

net::net(string name, int region, bool keep, bool isIO) {
	this->name = name;
	this->region = region;
	this->keep = keep;
	this->isIO = isIO;
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

bool net::isNode() const {
	return name.empty();
}

production_rule_set::production_rule_set() {
	assume_nobackflow = false;
	assume_static = false;
	require_driven = false;
	require_stable = false;
	require_noninterfering = false;
	require_adiabatic = false;
}

production_rule_set::~production_rule_set() {
}

void production_rule_set::print() {
	cout << "nets " << nets.size() << endl;
	for (int i = 0; i < (int)nets.size(); i++) {
		cout << "net " << i << ": " << nets[i].name << "'" << nets[i].region << " gateOf=" << to_string(nets[i].gateOf[0]) << to_string(nets[i].gateOf[1]) << " sourceOf=" << to_string(nets[i].sourceOf[0]) << to_string(nets[i].sourceOf[1]) << " drainOf=" << to_string(nets[i].drainOf[0]) << to_string(nets[i].drainOf[1]) << " remote=" << to_string(nets[i].remote) << (nets[i].keep ? " keep" : "") << (nets[i].isIO ? " io" : "") << " mirror=" << nets[i].mirror << " driver=" << nets[i].driver << endl;
	}

	cout << "devs " << devs.size() << endl;
	for (int i = 0; i < (int)devs.size(); i++) {
		cout << "dev " << i << ": source=" << nets[devs[i].source].name << "(" << devs[i].source << ") gate=" << nets[devs[i].gate].name << "(" << devs[i].gate << ") drain=" << nets[devs[i].drain].name << "(" << devs[i].drain << ") threshold=" << devs[i].threshold << " driver=" << devs[i].driver << (not devs[i].attr.assume.is_tautology() ? " {" + export_expression(devs[i].attr.assume, *this).to_string() + "}" : "") << (devs[i].attr.weak ? " weak" : "") << (devs[i].attr.force ? " force" : "") << (devs[i].attr.pass ? " pass" : "") << " after=" << devs[i].attr.delay_max << " size=" << devs[i].attr.size << " variant=" << devs[i].attr.variant << endl;
	}

	cout << "power " << pwr.size() << endl;
	for (int i = 0; i < (int)pwr.size(); i++) {
		cout << "pwr " << i << ": " << nets[pwr[i][0]].name << "(" << pwr[i][0] << ") " << nets[pwr[i][1]].name << "(" << pwr[i][1] << ")" << endl;
	}
}

int production_rule_set::create(net n) {
	int uid = (int)nets.size();
	nets.push_back(n);
	nets.back().remote.push_back(uid);
	return uid;
}

int production_rule_set::netIndex(string name, int region) const {
	for (int i = 0; i < (int)nets.size(); i++) {
		if (nets[i].name == name and nets[i].region == region) {
			return i;
		}
	}
	
	return -1;
}

int production_rule_set::netIndex(string name, int region, bool define) {
	vector<int> remote;
	for (int i = 0; i < (int)nets.size(); i++) {
		if (nets[i].name == name) {
			remote.push_back(i);
			if (nets[i].region == region) {
				return i;
			}
		}
	}

	if (define or not remote.empty()) {
		int uid = create(net(name, region));
		for (int i = 0; i < (int)remote.size(); i++) {
			connect_remote(uid, remote[i]);
		}
		return uid;
	}
	return -1;
}

pair<string, int> production_rule_set::netAt(int uid) const {
	if (uid < 0) {
		return pair<string, int>("_" + ::to_string(uid), 0);
	}
	return pair<string, int>(nets[uid].name, nets[uid].region);
}

vector<vector<int> > production_rule_set::remote_groups() const {
	vector<vector<int> > groups;

	for (int i = 0; i < (int)nets.size(); i++) {
		bool found = false;
		for (int j = 0; j < (int)groups.size() and not found; j++) {
			found = (find(groups[j].begin(), groups[j].end(), i) != groups[j].end());
		}
		if (not found) {
			groups.push_back(nets[i].remote);
		}
	}

	return groups;
}

int production_rule_set::sources(int net, int value) const {
	int result = 0;
	for (auto i = nets[net].sourceOf[value].begin(); i != nets[net].sourceOf[value].end(); i++) {
		result += (devs[*i].source == net);
	}
	return result;
}

int production_rule_set::drains(int net, int value) const {
	int result = 0;
	for (auto i = nets[net].drainOf[value].begin(); i != nets[net].drainOf[value].end(); i++) {
		result += (devs[*i].drain == net);
	}
	return result;
}

int production_rule_set::sources(int net, int value, attributes attr) const {
	int result = 0;
	for (auto i = nets[net].sourceOf[value].begin(); i != nets[net].sourceOf[value].end(); i++) {
		result += (devs[*i].source == net and devs[*i].attr == attr);
	}
	return result;
}

int production_rule_set::drains(int net, int value, attributes attr) const {
	int result = 0;
	for (auto i = nets[net].drainOf[value].begin(); i != nets[net].drainOf[value].end(); i++) {
		result += (devs[*i].drain == net and devs[*i].attr == attr);
	}
	return result;
}

int production_rule_set::sources(int net, int value, bool weak) const {
	int result = 0;
	for (auto i = nets[net].sourceOf[value].begin(); i != nets[net].sourceOf[value].end(); i++) {
		result += (devs[*i].source == net and devs[*i].attr.weak == weak);
	}
	return result;
}

int production_rule_set::drains(int net, int value, bool weak) const {
	int result = 0;
	for (auto i = nets[net].drainOf[value].begin(); i != nets[net].drainOf[value].end(); i++) {
		result += (devs[*i].drain == net and devs[*i].attr.weak == weak);
	}
	return result;
}

vector<attributes> production_rule_set::attribute_groups(int net, int value) const {
	vector<attributes> result;
	for (auto i = nets[net].drainOf[value].begin(); i != nets[net].drainOf[value].end(); i++) {
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
	nets[vdd].keep = true;
	nets[vdd].driver = 1;
	nets[vdd].mirror = gnd;
	nets[vdd].isIO = true;
	nets[gnd].keep = true;
	nets[gnd].driver = 0;
	nets[gnd].mirror = vdd;
	nets[gnd].isIO = true;
	pwr.push_back({gnd, vdd});
}

void production_rule_set::connect_remote(int n0, int n1) {
	net *i0 = &nets[n0];
	net *i1 = &nets[n1];

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
	//create(n1);
	if (n0 == n1 or n0 >= (int)nets.size()) {
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

	for (auto i = nets[n0].remote.begin(); i != nets[n0].remote.end(); i++) {
		if (*i == n0) {
			continue;
		}

		nets[*i].remote.insert(nets[*i].remote.end(), nets[n1].remote.begin(), nets[n1].remote.end());
		sort(nets[*i].remote.begin(), nets[*i].remote.end());
		nets[*i].remote.erase(unique(nets[*i].remote.begin(), nets[*i].remote.end()), nets[*i].remote.end());
		for (int j = 0; j < 2; j++) {
			nets[*i].gateOf[j].insert(nets[*i].gateOf[j].end(), nets[n1].gateOf[j].begin(), nets[n1].gateOf[j].end());
			sort(nets[*i].gateOf[j].begin(), nets[*i].gateOf[j].end());
			nets[*i].gateOf[j].erase(unique(nets[*i].gateOf[j].begin(), nets[*i].gateOf[j].end()), nets[*i].gateOf[j].end());
			nets[*i].drainOf[j].insert(nets[*i].drainOf[j].end(), nets[n1].drainOf[j].begin(), nets[n1].drainOf[j].end());
			sort(nets[*i].drainOf[j].begin(), nets[*i].drainOf[j].end());
			nets[*i].drainOf[j].erase(unique(nets[*i].drainOf[j].begin(), nets[*i].drainOf[j].end()), nets[*i].drainOf[j].end());
			nets[*i].sourceOf[j].insert(nets[*i].sourceOf[j].end(), nets[n1].sourceOf[j].begin(), nets[n1].sourceOf[j].end());
			sort(nets[*i].sourceOf[j].begin(), nets[*i].sourceOf[j].end());
			nets[*i].sourceOf[j].erase(unique(nets[*i].sourceOf[j].begin(), nets[*i].sourceOf[j].end()), nets[*i].sourceOf[j].end());
			nets[*i].rsourceOf[j].insert(nets[*i].rsourceOf[j].end(), nets[n1].rsourceOf[j].begin(), nets[n1].rsourceOf[j].end());
			sort(nets[*i].rsourceOf[j].begin(), nets[*i].rsourceOf[j].end());
			nets[*i].rsourceOf[j].erase(unique(nets[*i].rsourceOf[j].begin(), nets[*i].rsourceOf[j].end()), nets[*i].rsourceOf[j].end());
		}
	}

	for (auto i = nets[n1].remote.begin(); i != nets[n1].remote.end(); i++) {
		if (*i == n1) {
			continue;
		}

		nets[*i].remote.insert(nets[*i].remote.end(), nets[n0].remote.begin(), nets[n0].remote.end());
		sort(nets[*i].remote.begin(), nets[*i].remote.end());
		nets[*i].remote.erase(unique(nets[*i].remote.begin(), nets[*i].remote.end()), nets[*i].remote.end());
		for (int j = 0; j < 2; j++) {
			nets[*i].gateOf[j].insert(nets[*i].gateOf[j].end(), nets[n0].gateOf[j].begin(), nets[n0].gateOf[j].end());
			sort(nets[*i].gateOf[j].begin(), nets[*i].gateOf[j].end());
			nets[*i].gateOf[j].erase(unique(nets[*i].gateOf[j].begin(), nets[*i].gateOf[j].end()), nets[*i].gateOf[j].end());
			nets[*i].drainOf[j].insert(nets[*i].drainOf[j].end(), nets[n0].drainOf[j].begin(), nets[n0].drainOf[j].end());
			sort(nets[*i].drainOf[j].begin(), nets[*i].drainOf[j].end());
			nets[*i].drainOf[j].erase(unique(nets[*i].drainOf[j].begin(), nets[*i].drainOf[j].end()), nets[*i].drainOf[j].end());
			nets[*i].sourceOf[j].insert(nets[*i].sourceOf[j].end(), nets[n0].sourceOf[j].begin(), nets[n0].sourceOf[j].end());
			sort(nets[*i].sourceOf[j].begin(), nets[*i].sourceOf[j].end());
			nets[*i].sourceOf[j].erase(unique(nets[*i].sourceOf[j].begin(), nets[*i].sourceOf[j].end()), nets[*i].sourceOf[j].end());
			nets[*i].rsourceOf[j].insert(nets[*i].rsourceOf[j].end(), nets[n0].rsourceOf[j].begin(), nets[n0].rsourceOf[j].end());
			sort(nets[*i].rsourceOf[j].begin(), nets[*i].rsourceOf[j].end());
			nets[*i].rsourceOf[j].erase(unique(nets[*i].rsourceOf[j].begin(), nets[*i].rsourceOf[j].end()), nets[*i].rsourceOf[j].end());
		}
	}

	nets[n1].remote.insert(nets[n1].remote.end(), nets[n0].remote.begin(), nets[n0].remote.end());
	sort(nets[n1].remote.begin(), nets[n1].remote.end());
	nets[n1].remote.erase(unique(nets[n1].remote.begin(), nets[n1].remote.end()), nets[n1].remote.end());
	for (int j = 0; j < 2; j++) {
		nets[n1].gateOf[j].insert(nets[n1].gateOf[j].end(), nets[n0].gateOf[j].begin(), nets[n0].gateOf[j].end());
		sort(nets[n1].gateOf[j].begin(), nets[n1].gateOf[j].end());
		nets[n1].gateOf[j].erase(unique(nets[n1].gateOf[j].begin(), nets[n1].gateOf[j].end()), nets[n1].gateOf[j].end());
		nets[n1].drainOf[j].insert(nets[n1].drainOf[j].end(), nets[n0].drainOf[j].begin(), nets[n0].drainOf[j].end());
		sort(nets[n1].drainOf[j].begin(), nets[n1].drainOf[j].end());
		nets[n1].drainOf[j].erase(unique(nets[n1].drainOf[j].begin(), nets[n1].drainOf[j].end()), nets[n1].drainOf[j].end());
		nets[n1].sourceOf[j].insert(nets[n1].sourceOf[j].end(), nets[n0].sourceOf[j].begin(), nets[n0].sourceOf[j].end());
		sort(nets[n1].sourceOf[j].begin(), nets[n1].sourceOf[j].end());
		nets[n1].sourceOf[j].erase(unique(nets[n1].sourceOf[j].begin(), nets[n1].sourceOf[j].end()), nets[n1].sourceOf[j].end());
		nets[n1].rsourceOf[j].insert(nets[n1].rsourceOf[j].end(), nets[n0].rsourceOf[j].begin(), nets[n0].rsourceOf[j].end());
		sort(nets[n1].rsourceOf[j].begin(), nets[n1].rsourceOf[j].end());
		nets[n1].rsourceOf[j].erase(unique(nets[n1].rsourceOf[j].begin(), nets[n1].rsourceOf[j].end()), nets[n1].rsourceOf[j].end());
	}

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
	//create(gate);
	//create(drain);
	//int source = nets.size();
	//create(source);
	int source = create();

	nets[source].sourceOf[driver].push_back(devs.size());
	for (auto i = nets[gate].remote.begin(); i != nets[gate].remote.end(); i++) {
		nets[*i].gateOf[threshold].push_back(devs.size());
	}
	for (auto i = nets[drain].remote.begin(); i != nets[drain].remote.end(); i++) {
		nets[*i].drainOf[driver].push_back(devs.size());
		if (attr.pass) {
			nets[*i].rsourceOf[driver].push_back(devs.size());
		}
	}
	devs.push_back(device(source, gate, drain, threshold, driver, attr));
	return source;
}

int production_rule_set::add_drain(int source, int gate, int threshold, int driver, attributes attr) {
	//create(gate);
	//create(source);
	//int drain = nets.size();
	//create(drain);
	int drain = create();
	nets[drain].drainOf[driver].push_back(devs.size());
	for (auto i = nets[gate].remote.begin(); i != nets[gate].remote.end(); i++) {
		nets[*i].gateOf[threshold].push_back(devs.size());
	}
	for (auto i = nets[source].remote.begin(); i != nets[source].remote.end(); i++) {
		nets[*i].sourceOf[driver].push_back(devs.size());
	}
	devs.push_back(device(source, gate, drain, threshold, driver, attr));
	return drain;
}

void production_rule_set::add_mos(int source, int gate, int drain, int threshold, int driver, attributes attr) {
	//create(gate);
	//create(source);
	//create(drain);

	for (auto i = nets[gate].remote.begin(); i != nets[gate].remote.end(); i++) {
		nets[*i].gateOf[threshold].push_back(devs.size());
	}
	for (auto i = nets[drain].remote.begin(); i != nets[drain].remote.end(); i++) {
		nets[*i].drainOf[driver].push_back(devs.size());
		if (attr.pass) {
			nets[*i].rsourceOf[driver].push_back(devs.size());
		}
	}
	for (auto i = nets[source].remote.begin(); i != nets[source].remote.end(); i++) {
		nets[*i].sourceOf[driver].push_back(devs.size());
	}

	devs.push_back(device(source, gate, drain, threshold, driver, attr));
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
	if (guard.is_null()) {
		return std::numeric_limits<int>::max();
	} if (guard.cubes.size() == 1) {
		return add(guard.cubes[0], drain, driver, attr, order);
	}

	boolean::cube common = guard.supercube();
	if (not common.is_tautology() and not common.is_null()) {
		guard.cofactor(common);
		drain = add(common, drain, driver, attr, order);
		attr.set_internal();
	}

	if (not guard.is_tautology()) {
		boolean::cover left, right;
		guard.partition(left, right);
		int drainLeft = add_hfactor(left, drain, driver, attr, order);
		drain = add_hfactor(right, drain, driver, attr, order);
		if (drain == std::numeric_limits<int>::max()) {
			drain = drainLeft;
		} else if (drainLeft != std::numeric_limits<int>::max()) {
			drain = connect(drainLeft, drain);
		}
	}

	return drain;
}

void production_rule_set::add(int source, boolean::cover guard, int var, int val, attributes attr, vector<int> order) {
	int drain = add_hfactor(guard, var, val, attr, order);
	if (drain != std::numeric_limits<int>::max()) {
		connect(drain, source);
	}
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
		for (auto i = nets[prev_gate].remote.begin(); i != nets[prev_gate].remote.end(); i++) {
			nets[*i].gateOf[prev_threshold].erase(find(nets[*i].gateOf[prev_threshold].begin(), nets[*i].gateOf[prev_threshold].end(), dev));
		}
		for (auto i = nets[gate].remote.begin(); i != nets[gate].remote.end(); i++) {
			nets[*i].gateOf[threshold].insert(lower_bound(nets[*i].gateOf[threshold].begin(), nets[*i].gateOf[threshold].end(), dev), dev);
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
		for (auto i = nets[prev_source].remote.begin(); i != nets[prev_source].remote.end(); i++) {
			nets[*i].sourceOf[prev_driver].erase(find(nets[*i].sourceOf[prev_driver].begin(), nets[*i].sourceOf[prev_driver].end(), dev));
		}
		for (auto i = nets[source].remote.begin(); i != nets[source].remote.end(); i++) {
			nets[*i].sourceOf[driver].insert(lower_bound(nets[*i].sourceOf[driver].begin(), nets[*i].sourceOf[driver].end(), dev), dev);
		}
	}

	if (drain != prev_drain or driver != prev_driver) {
		devs[dev].drain = drain;
		for (auto i = nets[prev_drain].remote.begin(); i != nets[prev_drain].remote.end(); i++) {
			nets[*i].drainOf[prev_driver].erase(find(nets[*i].drainOf[prev_driver].begin(), nets[*i].drainOf[prev_driver].end(), dev));
		}
		for (auto i = nets[drain].remote.begin(); i != nets[drain].remote.end(); i++) {
			nets[*i].drainOf[driver].insert(lower_bound(nets[*i].drainOf[driver].begin(), nets[*i].drainOf[driver].end(), dev), dev);
		}
	}
}

void production_rule_set::invert(int net) {
	for (int threshold = 0; threshold < 2; threshold++) {
		for (auto i = nets[net].gateOf[threshold].begin(); i != nets[net].gateOf[threshold].end(); i++) {
			devs[*i].threshold = 1-devs[*i].threshold;
		}
	}
	for (auto i = nets[net].remote.begin(); i != nets[net].remote.end(); i++) {
		std::swap(nets[*i].gateOf[0], nets[*i].gateOf[1]);
	}
	
	vector<int> stack;
	stack.insert(stack.end(), nets[net].drainOf[0].begin(), nets[net].drainOf[0].end());
	stack.insert(stack.end(), nets[net].drainOf[1].begin(), nets[net].drainOf[1].end());
	sort(stack.begin(), stack.end());
	stack.erase(unique(stack.begin(), stack.end()), stack.end());
	while (not stack.empty()) {
		int di = stack.back();
		auto dev = devs.begin()+di;
		stack.pop_back();

		int new_source = dev->source;
		if (nets[dev->source].driver >= 0) {
			new_source = nets[dev->source].mirror;
		//} else if (not nets[dev->source].gateOf.empty()) {
		//	TODO(edward.bingham) What happens for bubble reshuffling pass
		//	transistor logic?
		} else if (nets[dev->source].gateOf[0].empty() and nets[dev->source].gateOf[1].empty()) {
			stack.insert(stack.end(), nets[dev->source].drainOf[0].begin(), nets[dev->source].drainOf[0].end());
			stack.insert(stack.end(), nets[dev->source].drainOf[1].begin(), nets[dev->source].drainOf[1].end());
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
			and (not nets[curr.net].gateOf[0].empty()
				or not nets[curr.net].gateOf[1].empty()))
			or nets[curr.net].driver >= 0) {
			if (curr.net == net) {
				continue;
			}

			if (nets[curr.net].driver < 0) {
				curr.guard.set(curr.net, driver);
			}

			result |= curr.guard;
			continue;
		}

		for (auto i = nets[curr.net].drainOf[driver].begin(); i != nets[curr.net].drainOf[driver].end(); i++) {
			auto dev = devs.begin()+*i;
			if (dev->drain != curr.net or dev->driver != driver or dev->attr.weak != weak) {
				continue;
			}
			boolean::cube guard = curr.guard;
			guard.set(dev->gate, dev->threshold);
			stack.push_back({dev->source, guard});
		}
	}

	return result;
}

bool production_rule_set::has_inverter_after(int net, int &_net) {
	array<vector<int>, 2> unary;
	for (int i = 0; i < 2; i++) {
		for (auto j = nets[net].gateOf[i].begin(); j != nets[net].gateOf[i].end(); j++) {
			auto dev = devs.begin()+*j;
			if (dev->gate != net) {
				continue;
			}
			if (nets[dev->source].driver == 1-dev->threshold) {
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

			if (up == boolean::cover(net, 0) and dn == boolean::cover(net, 1)) {
				_net = n;
				return true;
			}
		}
	}

	return false;
}

void production_rule_set::add_inverter_between(int from, int to, attributes attr, int vdd, int gnd) {
	if (vdd >= (int)nets.size()) {
		vdd = pwr[0][1];
	}
	if (gnd >= (int)nets.size()) {
		gnd = pwr[0][0];
	}
	connect(add_source(from, to, 1, 0, attr), gnd);
	connect(add_source(from, to, 0, 1, attr), vdd);
}

int production_rule_set::add_inverter_after(int net, attributes attr, int vdd, int gnd) {
	int _net = create();
	//int _net = nets.size();
	//create(_net);
	add_inverter_between(net, _net, attr, vdd, gnd);
	return _net;
}

array<int, 2> production_rule_set::add_buffer_before(int net, attributes attr, int vdd, int gnd) {
	//int __net = nets.size();
	//create(__net);
	int __net = create();
	for (int i = 0; i < 2; i++) {
		nets[__net].drainOf[i] = nets[net].drainOf[i];
		nets[net].drainOf[i].clear();
	}
	for (int i = 0; i < 2; i++) {
		for (auto j = nets[__net].drainOf[i].begin(); j != nets[__net].drainOf[i].end(); j++) {
			if (devs[*j].drain == net) {
				devs[*j].drain = __net;
			}
		}
	}

	//int _net = nets.size();
	//create(_net);
	int _net = create();
	add_inverter_between(__net, _net, attr, vdd, gnd);
	add_inverter_between(_net, net, attr, vdd, gnd);
	return {__net, _net};
}

void production_rule_set::add_keepers(bool share, bool hcta, boolean::cover keep, bool report_progress) {
	if (report_progress) {
		printf("  %s...", name.c_str());
		fflush(stdout);
	}

	int inverterCount = 0;

	Timer tmr;
	bool hasweakpwr = false;
	array<int, 2> sharedweakpwr={
		std::numeric_limits<int>::max(),
		std::numeric_limits<int>::max()
	};
	for (int net = 0; net < (int)nets.size(); net++) {
		if (not nets[net].keep) {
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

		nets[net].keep = false;
		if (nets[net].driver >= 0 or keep.is_subset_of(up|dn|keep_up|keep_dn)) {
			if (debug) cout << "not needed" << endl << endl;
			continue;
		}
		if (debug) cout << "making keeper" << endl << endl;

		// identify output inverter if it exists, create it if it doesn't
		// (using the half cycle timing assumption)
		int _net=std::numeric_limits<int>::max();
		int __net=net;
		if (not has_inverter_after(__net, _net)) {
			if (hcta) {
				inverterCount++;
				_net = add_inverter_after(__net, attributes::instant());
			} else {
				inverterCount += 2;
				auto n = add_buffer_before(__net);
				__net = n[0];
				_net = n[1];
			}
		}

		array<int, 2> weakpwr;
		weakpwr[0] = sharedweakpwr[0];
		weakpwr[1] = sharedweakpwr[1];
		if (not share or not hasweakpwr) {
			if (share) {
				// If we're sharing the weak power signals across multiple cells, then
				// we need to make them named nets so that we can put them in the IO
				// ports.
				weakpwr[0] = create("weak_" + nets[pwr[0][0]].name);
				weakpwr[1] = create("weak_" + nets[pwr[0][1]].name);
			} else {
				weakpwr[0] = create();
				weakpwr[1] = create();
			}

			add_mos(pwr[0][0], pwr[0][1], weakpwr[0], 1, 0, attributes(true, false));
			add_mos(pwr[0][1], pwr[0][0], weakpwr[1], 0, 1, attributes(true, false));
			sharedweakpwr = weakpwr;
			hasweakpwr = true;
		}

		add_inverter_between(_net, __net, attributes(), weakpwr[1], weakpwr[0]);
	}

	if (report_progress) {
		printf("[%s%d INVERTERS ADDED%s]\t%gs\n", KGRN, inverterCount, KNRM, tmr.since());
	}
}

vector<bool> production_rule_set::identify_weak_drivers() {
	// Depth first search from weak devices from source to drains, this allows us
	// to propagate weak driver information down the stack so we don't oversize
	// our weak drivers.

	// There are two separate questions that I need to answer.
	// Is the drain of this device always driven by a weak driver?
	// 	- In this case, this devices and all of its sources should be sized as a weak driver.
	// Is this net ever driven by a weak driver?
	//  - In this case, what devices are driving it during that time? what
	//    devices overpower it? what is their drive strength? and therefore the
	//    weak driver should be sized to at most 1/10th of that.
	// boolean::cover strong = guard_of(curr.net, curr.val, false) | guard_of(curr.net, 1-curr.val, false);
	// curr.guard = curr.guard & ~strong;


	// This function implements part 1.

	struct frame {
		int net;
		int val;
	};

	vector<bool> weak;
	weak.resize(devs.size(), false);

	vector<frame> frames;
	for (int i = 0; i < (int)devs.size(); i++) {
		if (devs[i].attr.weak) {
			frames.push_back({devs[i].drain, devs[i].driver});
			weak[i] = true;
		}
	}

	while (not frames.empty()) {
		auto curr = frames.back();
		frames.pop_back();

		// Propagate only if all sources of this net for this value are weak
		bool found = false;
		for (auto i = nets[curr.net].drainOf[curr.val].begin(); i != nets[curr.net].drainOf[curr.val].end() and not found; i++) {
			found = not weak[*i];
		}
		if (found) {
			continue;
		}

		for (auto i = nets[curr.net].sourceOf[curr.val].begin(); i != nets[curr.net].sourceOf[curr.val].end(); i++) {
			if (not weak[*i]) {
				weak[*i] = true;
				frames.push_back({devs[*i].drain, curr.val});
			}
		}
	}

	return weak;
}

vector<vector<int> > production_rule_set::size_with_stack_length() {
	struct frame {
		int net;
		int val;
		vector<int> devs;
	};
	vector<frame> frames;
	for (int i = 0; i < (int)nets.size(); i++) {
		for (int val = 0; val < 2; val++) {
			if (not nets[i].drainOf[1-val].empty() and not nets[i].drainOf[val].empty()) {
				frames.push_back({i, val, vector<int>()});
			}
		}
	}

	// I need to know for each node, all of the boolean cubes that could drive
	// that node up or down, the strength to which it is driven up or down, and
	// the devices that participate (for weak drivers).

	vector<vector<int> > device_tree;	
	while (not frames.empty()) {
		frame curr = frames.back();
		frames.pop_back();

		if (nets[curr.net].drainOf[curr.val].empty()) {
			// TODO(edward.bingham) throw error if this node is not a driver (outside
			// node feeding into pass transistor logic is not acceptable). Assume
			// driving stack is four transistors long plus length of current stack,
			// size accordingly.
			// TODO(edward.bingham) look for sequential devices whose size is less
			// than both it's neighbors. Size up to minimum. This avoids notches in
			// cell layout.
			if (not curr.devs.empty()) {
				device_tree.push_back(curr.devs);
			}
			int stack_size = (float)curr.devs.size();
			for (auto i = curr.devs.begin(); i != curr.devs.end(); i++) {
				//int device_to_source = (curr.devs.end()-i);
				// TODO(edward.bingham) nmos and pmos have different drive strengths
				devs[*i].attr.size = max(devs[*i].attr.size, (float)stack_size);
			}
			continue;
		}

		for (auto i = nets[curr.net].drainOf[curr.val].begin(); i != nets[curr.net].drainOf[curr.val].end(); i++) {
			bool found = false;
			for (int j = (int)curr.devs.size()-1; j >= 0 and not found; j--) {
				found = curr.devs[j] == *i;
			}
			if (not found) {
				frame next = curr;
				next.devs.push_back(*i);
				next.net = devs[*i].source;
				frames.push_back(next);
			}
		}
	}

	return device_tree;
}

void production_rule_set::size_devices(float ratio, bool report_progress) {
	// TODO(edward.bingham) The general case sizing problem is really quite
	// challenging. We can take the transistor network, break it up into mutually
	// exclusive conditions, then for each of those conditions, we can treat the
	// driving network as a resistor network with multiple unknowns. We want the
	// final drive strength of that network to be equal to one. This gives us a
	// constraint to solve. However, since we have multiple of these conditions
	// and networks, we want them all to have at least a drive strength of 1
	// while minimizing area. This creates a many-variable multi-constraint
	// search space. At least the search space is continuous... This is a great
	// problem to use some machine learning techniques on.
	
	// Thankfully, Quasi-delay insensitive circuits come to the rescue. All data
	// must be represented by a delay insensitive encoding. Most of the time,
	// that delay insensitive encoding is a 1-hot encoding, which means that most
	// of the time, the driving conditions for a given gate are going to be
	// mutually exclusive. This means that we can get pretty close to an optimum
	// solution by simply examining each condition independently and taking the
	// max required sizing across all conditions. That might be a good starting
	// point for the subsequent optimization engine.

	// For now, just to the simple thing.
 
	// Depth first search from the drains to the source to determine stack
	// length. This allows us to compute device-level sizing

	if (report_progress) {
		printf("  %s...", name.c_str());
		fflush(stdout);
	}

	Timer tmr;

	vector<vector<int> > device_tree = size_with_stack_length();

	// normalize weak drivers so that they are all min width and length (setting size to 1.0).
	vector<bool> weak = identify_weak_drivers();
	for (int i = 0; i < (int)weak.size(); i++) {
		if (weak[i]) {
			devs[i].attr.size = 1.0;
		}
	}

	// TODO(edward.bingham) From here, sizing gets weird. In the general case, we
	// can take the transistor network, break it up into mutually exclusive
	// conditions, then for each of those conditions, we can treat the driving
	// network as a resistor network with multiple unknowns. We know that the
	// drive strength of that network must be 10x less than the drive strength of
	// the opposing strong driver. This means that we'll have a polynomial
	// inequality. On one side we'll have the polynomial representing the
	// resistance computation and on the other, we'll have the constraint imposed
	// by the weak driver requirements. Really, we want a solution on the
	// boundary of that inequality, but we don't just want any solution. We want
	// a solution that minimizes circuit area and energy expenditure. That means
	// that we have to do gradient descent along the boundary of that
	// multidimensional surface to identify the optimum sizing for the superweak
	// transistors.

	// Since all of these superweak transistors are min width, then it's really
	// just a question of length, which roughly speaking is equal to 1/size for
	// each device since 0 < size < 1. So we would sum that across all unknowns
	// involved.

	// Energy is dependent on two things. First, the delay between the transition
	// on the conflicting net and the disabling of a given weak stack. Second,
	// the total resistance of the weak drive stack and the conflicting strong
	// driver. Total energy is then just the sum of the individual energies.

	// Since this is a 2-variable optimization, there is likely to be a nonlinear
	// pareto boundary of "optimum" solutions that favor one or the other metric.
	// This means that the user will have to tell us how to select a given sizing
	// solution from that pareto boundary.

	// Now for the vast majority of applications, this will be total overkill.
	// Most weak drivers are a single transistor and most nodes are only driven
	// by a single weak driver. However, in the case of pass transistor logic and
	// other oddities (perhaps adiabatic logic), this starts to become a larger
	// issue. This means that for computation time sake, we'd probably want to
	// let the user select between different algorithms with optimization levels.
	// For example a low optimization level would use the simple algorithm and a
	// high optimization level would use the more general case algorithm.

	// For now, just do the simple thing.

	vector<bool> superweak;
	superweak.resize(devs.size(), false);
	for (auto i = device_tree.begin(); i != device_tree.end(); i++) {
		for (auto j = i->rbegin(); j != i->rend(); j++) {
			if (weak[*j]) {
				superweak[*j] = true;
				break;
			}
		}
	}
	
	for (auto i = device_tree.begin(); i != device_tree.end(); i++) {
		int idx = 0;
		for (; idx < (int)i->size() and not superweak[(*i)[idx]]; idx++);
		if (idx >= (int)i->size()) {
			continue;
		}

		/*boolean::cube weak_driver;
		for (auto j = i->begin(); j != i->end(); j++) {
			weak_driver.set(devs[*j].gate, devs[*j].threshold);
		}

		float drive_strength = get_condition_strength(weak_driver, devs[i->back()].drain, 1-devs[i->back()].driver, false);
		drive_strength *= ratio;

		float weak_strength = 0.0;*/
		
		// TODO(edward.bingham) this assumes that the conflicting transitions are
		// sized to at least 1.0 strength. A more complete approach would instead
		// try to compute the conflicting transition's strength compared to the
		// weak driver's strength and assign a size based on that.
		auto dev = devs.begin()+(*i)[idx];
		dev->attr.size = ratio;//min(dev->size, drive_strength
	}

	if (report_progress) {
		float totalStrength = 0;
		for (auto d = devs.begin(); d != devs.end(); d++) {
			totalStrength += d->attr.size;
		}
		printf("[%s%g TOTAL STRENGTH%s]\t%gs\n", KGRN, totalStrength, KNRM, tmr.since());
	}
}

void production_rule_set::swap_source_drain(int dev) {
	int prev_source = devs[dev].source;
	int prev_drain = devs[dev].drain;
	int driver = devs[dev].driver;

	devs[dev].source = prev_drain;
	devs[dev].drain = prev_source;

	for (auto i = nets[prev_source].remote.begin(); i != nets[prev_source].remote.end(); i++) {
		nets[*i].sourceOf[driver].erase(find(nets[*i].sourceOf[driver].begin(), nets[*i].sourceOf[driver].end(), dev));
	}
	for (auto i = nets[devs[dev].source].remote.begin(); i != nets[devs[dev].source].remote.end(); i++) {
		nets[*i].sourceOf[driver].insert(lower_bound(nets[*i].sourceOf[driver].begin(), nets[*i].sourceOf[driver].end(), dev), dev);
	}

	for (auto i = nets[prev_drain].remote.begin(); i != nets[prev_drain].remote.end(); i++) {
		nets[*i].drainOf[driver].erase(find(nets[*i].drainOf[driver].begin(), nets[*i].drainOf[driver].end(), dev));
	}
	for (auto i = nets[devs[dev].drain].remote.begin(); i != nets[devs[dev].drain].remote.end(); i++) {
		nets[*i].drainOf[driver].insert(lower_bound(nets[*i].drainOf[driver].begin(), nets[*i].drainOf[driver].end(), dev), dev);
	}
}

void production_rule_set::normalize_source_drain() {
	struct frame {
		int net;
		int val;
	};

	list<frame> frames;
	for (int i = 0; i < (int)nets.size(); i++) {
		if (nets[i].driver == 1 or (nets[i].isIO and nets[i].drainOf[0].empty() and nets[i].sourceOf[0].empty())) {
			frames.push_back({i, 1});
		}
		if (nets[i].driver == 0 or (nets[i].isIO and nets[i].drainOf[1].empty() and nets[i].sourceOf[1].empty())) {
			frames.push_back({i, 0});
		}
	}

	set<int> seen;
	set<pair<int, int> > visited;

	// propagate from source to drain
	while (not frames.empty()) {
		auto curr = frames.front();
		frames.pop_front();
		visited.insert({curr.net, curr.val});

		vector<int> drains = nets[curr.net].drainOf[curr.val];
		for (auto i = drains.begin(); i != drains.end(); i++) {
			if (seen.find(*i) == seen.end() and find(nets[curr.net].remote.begin(), nets[curr.net].remote.end(), devs[*i].drain) != nets[curr.net].remote.end()) {
				swap_source_drain(*i);
			}
		}

		for (auto i = nets[curr.net].sourceOf[curr.val].begin(); i != nets[curr.net].sourceOf[curr.val].end(); i++) {
			if (seen.find(*i) == seen.end()) {
				if (not nets[devs[*i].drain].isIO
					and nets[devs[*i].drain].gateOf[0].empty()
					and nets[devs[*i].drain].gateOf[1].empty()
					and nets[devs[*i].drain].drainOf[1-curr.val].empty()
					and nets[devs[*i].drain].sourceOf[1-curr.val].empty()) {
					devs[*i].attr.set_internal();
					frames.push_back({devs[*i].drain, curr.val});
				}
				seen.insert(*i);
			}
		}
	}

	// find devices we missed
	/*for (int i = 0; i < (int)devs.size(); i++) {
		if (seen.find(i) == seen.end()) {
			if (visited.find({devs[i].drain, devs[i].driver}) != visited.end()) {
				frames.push_back({devs[i].drain, devs[i].driver});
			} else if (visited.find({devs[i].source, devs[i].driver}) != visited.end()) {
				frames.push_back({devs[i].source, devs[i].driver});
			}
		}
	}

	printf("frames: %d\n", (int)frames.size());

	// propagate from drain to source
	while (not frames.empty()) {
		auto curr = frames.front();
		frames.pop_front();
		visited.insert({curr.net, curr.val});

		vector<int> sources = nets[curr.net].sourceOf[curr.val];
		for (auto i = sources.begin(); i != sources.end(); i++) {
			if (seen.find(*i) == seen.end() and find(nets[curr.net].remote.begin(), nets[curr.net].remote.end(), devs[*i].drain) != nets[curr.net].remote.end()) {
				swap_source_drain(*i);
			}
		}

		for (auto i = nets[curr.net].drainOf[curr.val].begin(); i != nets[curr.net].drainOf[curr.val].end(); i++) {
			if (seen.find(*i) == seen.end()) {
				if (not nets[devs[*i].drain].isIO
					and nets[devs[*i].drain].gateOf[0].empty()
					and nets[devs[*i].drain].gateOf[1].empty()
					and nets[devs[*i].drain].drainOf[1-curr.val].empty()
					and nets[devs[*i].drain].sourceOf[1-curr.val].empty()) {
					devs[*i].attr.set_internal();
				}
				frames.push_back({devs[*i].source, curr.val});
				seen.insert(*i);
			}
		}
	}*/
}

}
