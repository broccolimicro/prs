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

int bubble::uid(int net) {
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
	vector<arc> bnet;

	vector<int> rules;
	for (int i = 0; i < (int)prs.nets.size(); i++) {
		if (not prs.nets[i].gateOf.empty()) {
			rules.push_back(i);
		}
	}

	for (int i = 0; i < (int)prs.nodes.size(); i++) {
		if (not prs.nodes[i].gateOf.empty()) {
			rules.push_back(prs.flip(i));
		}
	}

	for (int i = 0; i < (int)rules.size(); i++) {
		vector<int> stack(1, rules[i]);
		while (not stack.empty()) {
			int curr = stack.back();
			stack.pop_back();

			for (int driver = 0; driver < 2; driver++) {
				for (auto di = prs.at(curr).drainOf[driver].begin(); di != prs.at(curr).drainOf[driver].end(); di++) {
					auto dev = prs.devs.begin()+*di;
					arc a(dev->gate, dev->threshold, rules[i], driver);
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

	inverted.resize(prs.nets.size() + prs.nodes.size(), false);
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
		if (idx->isochronic and idx->bubble and forward) {
			inverted_signals = true;
			inverted[uid(idx->to)] = not inverted[uid(idx->to)];
			for (graph::iterator j = net.begin(); j != net.end(); j++) {
				if (j->from == idx->to or j->to == idx->to) {
					j->bubble = not j->bubble;
				}
			}
		}
		else if (idx->isochronic and idx->bubble and !forward) {
			inverted_signals = true;
			inverted[uid(idx->from)] = not inverted[uid(idx->from)];
			for (graph::iterator j = net.begin(); j != net.end(); j++) {
				if (j->from == idx->from or j->to == idx->from) {
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

	map<size_t, size_t> inv;

	for (size_t i = 0; i < inverted.size(); i++) {
		if (not inverted[i]) {
			int net = prs->idx(i);
			if (net >= 0) {
				variables.nodes[net].name.back().name = "_" + variables.nodes[net].name.back().name;
			}

			prs->invert(net);
		}
	}

	// Apply local inversions to production rules
	for (graph::iterator i = net.begin(); i != net.end(); i++) {
		if (not i->isochronic and i->bubble) {
			auto ins = inv.insert(pair<size_t, size_t>(i->from, variables.nodes.size()));
			auto j = ins.first;
			if (ins.second) {
				variables.nodes.push_back(variables.nodes[i->from]);
				if (variables.nodes.back().name.back().name[0] == '_') {
					variables.nodes.back().name.back().name.erase(variables.nodes.back().name.back().name.begin());
				} else {
					variables.nodes.back().name.back().name = "_" + variables.nodes.back().name.back().name;
				}

				prs->connect(prs->add_source(j->first, j->second, 0, 1), prs->pwr[0][1]);
				prs->connect(prs->add_source(j->first, j->second, 1, 0), prs->pwr[0][0]);
			}

			for (int threshold = 0; threshold < 2; threshold++) {
				for (auto di = i->gates[threshold].begin(); di != i->gates[threshold].end(); di++) {
					auto dev = prs->devs.begin()+*di;
					dev->gate = j->second;
					prs->at(j->first).gateOf[threshold].erase(find(prs->at(j->first).gateOf[threshold].begin(), prs->at(j->first).gateOf[threshold].end(), *di));
					prs->at(j->second).gateOf[threshold].insert(lower_bound(prs->at(j->second).gateOf[threshold].begin(), prs->at(j->second).gateOf[threshold].end(), *di), *di);
				}
			}
		}
	}
}

}
