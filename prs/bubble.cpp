/*
 *  bubble.cpp
 *
 *  Created on: July 9, 2023
 *      Author: nbingham
 */

#include "bubble.h"
#include <common/message.h>
#include <common/text.h>
#include <interpret_boolean/export.h>

namespace prs
{

bubble::arc::arc() {
	from = 0;
	to = 0;
	tval = -1;
	isochronic = true;
	bubble = false;
}

bubble::arc::arc(int from, int fval, int to, int tval) {
	this->from = from;
	this->to = to;
	this->tval = tval;
	this->isochronic = true;
	this->bubble = (fval == tval);
}

bubble::arc::~arc() {
}

bool operator<(const bubble::arc &a0, const bubble::arc &a1) {
	return a0.from < a1.from
		or (a0.from == a1.from and (a0.to < a1.to));
}

bool operator==(const bubble::arc &a0, const bubble::arc &a1) {
	return a0.from == a1.from and a0.to == a1.to;
}

bubble::bubble()
{
}

bubble::~bubble()
{
}

int bubble::uid(int net) const {
	if (net < 0) {
		return nets-net-1;
	}
	return net;
}

void bubble::load_prs(const production_rule_set &prs, const ucs::variable_set &variables)
{
	nets = (int)prs.nets.size();
	nodes = (int)prs.nodes.size();

	net.clear();
	inverted.clear();
	linked.clear();
	inverted.resize(nets+nodes, false);
	linked.resize(nets+nodes, false);

	vector<int> rules;
	for (int i = 0; i < nets; i++) {
		if (not prs.nets[i].gateOf[0].empty() or not prs.nets[i].gateOf[1].empty()) {
			rules.push_back(i);
		}
	}

	for (int i = 0; i < nodes; i++) {
		if (not prs.nodes[i].gateOf[0].empty() or not prs.nodes[i].gateOf[1].empty()) {
			rules.push_back(prs.flip(i));
		}
	}

	for (int i = 0; i < (int)rules.size(); i++) {
		int drain = prs.at(rules[i]).remote[0];

		vector<int> stack(1, rules[i]);
		while (not stack.empty()) {
			int curr = stack.back();
			stack.pop_back();

			for (int driver = 0; driver < 2; driver++) {
				for (auto di = prs.at(curr).drainOf[driver].begin(); di != prs.at(curr).drainOf[driver].end(); di++) {
					auto dev = prs.devs.begin()+*di;
					int gate = prs.at(dev->gate).remote[0];
					
					arc a(gate, dev->threshold, drain, driver);
					a.gates[dev->threshold].push_back(*di);
					auto j = net.insert(a);
					if (not j.second) {
						if (j.first->bubble != a.bubble) {
							if (j.first->tval == -1 or j.first->tval == a.tval) {
								error("", "dividing signal found in production rules {" + export_variable_name(a.from, variables).to_string() + " -> " + export_variable_name(a.to, variables).to_string() + (a.tval == 1 ? "+" : "-") + "}", __FILE__, __LINE__);
							}
							if (j.first->tval == -1 or j.first->tval != a.tval) {
								error("", "gating signal found in production rules {" + export_variable_name(a.from, variables).to_string() + (((a.tval == 1 and not a.bubble) or (a.tval != 1 and a.bubble)) ? "+" : "-") + " -> " + export_variable_name(a.to, variables).to_string() + "}", __FILE__, __LINE__);
							}
						} else if (j.first->tval != a.tval) {
							linked[j.first->from] = true;
							linked[j.first->to] = true;
							j.first->tval = -1;
							j.first->isochronic = false;
							auto pos = lower_bound(j.first->gates[dev->threshold].begin(), j.first->gates[dev->threshold].end(), *di);
							if (pos == j.first->gates[dev->threshold].end() or *pos != *di) {
								j.first->gates[dev->threshold].insert(pos, *di);
							}
						}
					}
					stack.push_back(dev->source);
				}
			}
		}
	}
}

// cycles added, inverted signals
pair<int, bool> bubble::step(graph::iterator idx, bool forward, vector<int> cycle)
{
	int cycles_added = 0;
	bool inverted_signals = false;
	vector<int> negative_cycle;
	vector<int>::iterator found;

	cycle.push_back(forward ? idx->from : idx->to);

	found = find(cycle.begin(), cycle.end(), forward ? idx->to : idx->from);
	if (found == cycle.end()) {
		int n = forward ? idx->to : idx->from;
		int inb = 0, in = 0;
		int out = 0;
		bool isochronic = idx->isochronic;
		for (graph::iterator j = net.begin(); j != net.end() and not isochronic; j++) {
			if (j->isochronic) {
				isochronic = true;
			}
			if (j->from == n and not j->bubble) {
				out = 1;
			} else if (j->to == n) {
				inb += j->bubble;
				in += not j->bubble;
			}
		}

		if ((idx->isochronic and idx->bubble) or (not isochronic and inb > in+out)) {
			inverted_signals = true;
			inverted[uid(n)] = not inverted[uid(n)];
			for (graph::iterator j = net.begin(); j != net.end(); j++) {
				if (j->from == n or j->to == n) {
					j->bubble = not j->bubble;
				}
			}
		}

		for (graph::iterator i = net.begin(); cycles_added == 0 and i != net.end(); i++) {
			pair<int, bool> result(0, false); 
			bool src_dst = i->from == idx->to;
			bool same_src = i->from == idx->from;
			bool same_dst = i->to == idx->to;
			bool dst_src = i->to == idx->from;
			if (forward and (src_dst or same_dst) and i != idx) {
				result = step(i, src_dst, cycle);
			} else if (!forward and (same_src or dst_src) and i != idx) {
				result = step(i, same_src, cycle);
			}
			cycles_added += result.first;
			inverted_signals = inverted_signals or result.second;
		}
	} else {
		vector<int> temp(found, cycle.end());
		sort(temp.begin(), temp.end());
		temp.resize(unique(temp.begin(), temp.end()) - temp.begin());
		cycles.push_back(bubbled_cycle(temp, not idx->isochronic or not idx->bubble));
		cycles_added += 1;
	}

	return pair<int, bool>(cycles_added, inverted_signals);
}

void bubble::reshuffle()
{
	// Execute bubble reshuffling algorithm
	for (graph::iterator i = net.begin(); i != net.end(); i++) {
		step(i);
	}
}

void bubble::save_prs(production_rule_set *prs, ucs::variable_set &variables)
{
	// remove duplicate cycles
	sort(cycles.begin(), cycles.end());
	cycles.resize(unique(cycles.begin(), cycles.end()) - cycles.begin());

	// annihilate conflicting cycles
	for (size_t i = 1; i < cycles.size(); i++) {
		if (cycles[i].first == cycles[i-1].first) {
			cycles.erase(cycles.begin() + i);
			cycles.erase(cycles.begin() + i-1);
			i--;
		}
	}

	// remove positive cycles
	for (size_t i = 0; i < cycles.size(); i++) {
		if (cycles[i].second) {
			cycles.erase(cycles.begin() + i);
			i--;
		}
	}

	// Remove Negative Cycles (currently negative cycles just throw an error message)
	for (size_t i = 0; i < cycles.size(); i++) {
		string tempstr;
		for (size_t j = 0; j < cycles[i].first.size(); j++) {
			if (j != 0)
				tempstr += ", ";
			tempstr += export_variable_name(cycles[i].first[j], variables).to_string();
		}
		error("", "negative cycle found " + tempstr, __FILE__, __LINE__);
	}

	map<int, vector<int> > inv;

	for (int i = 0; i < (int)inverted.size(); i++) {
		if (inverted[i]) {
			int idx = prs->idx(i);
			cout << "inverting " << export_variable_name(idx, variables).to_string() << "(" << idx << ")" << endl;
			for (auto j = prs->at(idx).remote.begin(); j != prs->at(idx).remote.end(); j++) {
				if (*j >= 0) {
					variables.nodes[*j].name.back().name = "_" + variables.nodes[*j].name.back().name;
				}
			}
			prs->invert(idx);
			cout << "done" << endl;
		}
	}

	// Apply local inversions to production rules
	for (graph::iterator i = net.begin(); i != net.end(); i++) {
		if (not i->isochronic and i->bubble) {
			auto ins = inv.insert(pair<int, vector<int> >(i->from, vector<int>()));
			auto j = ins.first;
			if (ins.second) {
				int canonical = i->from >= 0 ? (int)prs->nets.size() : prs->flip(prs->nodes.size());
				for (auto k = prs->at(i->from).remote.begin(); k != prs->at(i->from).remote.end(); k++) {
					int idx = *k >= 0 ? (int)prs->nets.size() : prs->flip(prs->nodes.size());
					prs->create(idx);
					if (not j->second.empty()) {
						prs->at(j->second[0]).add_remote(idx);
					}
					j->second.push_back(idx);
					if (*k >= 0) {
						variables.nodes.push_back(variables.nodes[*k]);
						if (variables.nodes.back().name.back().name[0] == '_') {
							variables.nodes.back().name.back().name.erase(variables.nodes.back().name.back().name.begin());
						} else {
							variables.nodes.back().name.back().name = "_" + variables.nodes.back().name.back().name;
						}
					}

					if (*k == i->from) {
						canonical = idx;
					}
				}

				prs->connect(prs->add_source(i->from, canonical, 0, 1), prs->pwr[0][1]);
				prs->connect(prs->add_source(i->from, canonical, 1, 0), prs->pwr[0][0]);
			}

			for (int threshold = 0; threshold < 2; threshold++) {
				for (auto di = i->gates[threshold].begin(); di != i->gates[threshold].end(); di++) {
					int canonical = j->second[0];
					for (int k = 0; k < (int)prs->at(i->from).remote.size(); k++) {
						if (prs->at(i->from).remote[k] == prs->devs[*di].gate) {
							canonical = j->second[k];
							break;
						}
					}

					prs->move_gate(*di, canonical, 1-threshold);
				}
			}
		}
	}
}

}
