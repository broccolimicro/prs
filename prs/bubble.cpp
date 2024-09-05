/*
 *  bubble.cpp
 *
 *  Created on: July 9, 2023
 *      Author: nbingham
 */

#include "bubble.h"
#include <common/message.h>
#include <common/text.h>

namespace prs
{

/*bubble::bubble()
{
}

bubble::~bubble()
{
}

void bubble::load_prs(const production_rule_set &prs, const ucs::variable_set &variables)
{
	vector<arc> bnet;
	vector<int> gvars, avars;

	for (auto rule = prs.rules.begin(); rule != prs.rules.end(); rule++) {
		for (auto term = rule->guard.cubes.begin(); term != rule->guard.cubes.end(); term++) {
			gvars = term->vars();
			for (auto action = rule->local_action.cubes.begin(); action != rule->local_action.cubes.end(); action++) {
				avars = action->vars();
				for (auto gvar = gvars.begin(); gvar != gvars.end(); gvar++) {
					for (auto avar = avars.begin(); avar != avars.end(); avar++) {
						bnet.push_back(arc((*gvar)*2 + term->get(*gvar), (*avar)*2 + action->get(*avar)));
					}
				}
			}
		}
	}

	bnet.resize(unique(bnet.begin(), bnet.end()) - bnet.begin());

	vector<arc>::iterator removed_arcs;
	for (size_t i = 0; i < variables.nodes.size(); i++)
		for (size_t j = 0; j < variables.nodes.size(); j++)
			if (i != j) {
				// does a rule have both senses of the same variable in it's guard?
				if ((removed_arcs = find(bnet.begin(), bnet.end(), arc(i*2, j*2))) != bnet.end() &&
				                    find(bnet.begin(), bnet.end(), arc(i*2+1, j*2)) != bnet.end()) {
					error("", "dividing signal found in production rules {" + variables.nodes[i].to_string() + " -> " + variables.nodes[j].to_string() + "-}", __FILE__, __LINE__);
					bnet.erase(removed_arcs);
				}

				if (                find(bnet.begin(), bnet.end(), arc(i*2, j*2+1)) != bnet.end() &&
				    (removed_arcs = find(bnet.begin(), bnet.end(), arc(i*2+1, j*2+1))) != bnet.end())
				{
					error("", "dividing signal found in production rules {" + variables.nodes[i].to_string() + " -> " + variables.nodes[j].to_string() + "+}", __FILE__, __LINE__);
					bnet.erase(removed_arcs);
				}

				// do both the pull up and pull down networks have the same sense of a variable in their guards?
				if ((removed_arcs = find(bnet.begin(), bnet.end(), arc(i*2, j*2))) != bnet.end() &&
				                    find(bnet.begin(), bnet.end(), arc(i*2, j*2+1)) != bnet.end())
				{
					error("", "gating signal found in production rules {" + variables.nodes[i].to_string() + "- -> " + variables.nodes[j].to_string() + "}", __FILE__, __LINE__);
					bnet.erase(removed_arcs);
				}

				if (                find(bnet.begin(), bnet.end(), arc(i*2+1, j*2)) != bnet.end() &&
				    (removed_arcs = find(bnet.begin(), bnet.end(), arc(i*2+1, j*2+1))) != bnet.end())
				{
					error("", "gating signal found in production rules {" + variables.nodes[i].to_string() + "+ -> " + variables.nodes[j].to_string() + "}", __FILE__, __LINE__);
					bnet.erase(removed_arcs);
				}
			}

	net.clear();
	for (size_t i = 0; i < bnet.size(); i++) {
		arc a(bnet[i].first/2, bnet[i].second/2);

		graph::iterator j = net.find(a);
		if (j == net.end()) {
			bubbles b(true, bnet[i].first%2 == bnet[i].second%2);
			net.insert(pair<arc, bubbles>(a, b));
		} else {
			j->second.first = false;
		}
	}

	inverted.resize(variables.nodes.size(), false);
}

// cycles added, inverted signals
pair<int, bool> bubble::step(graph::iterator idx, bool forward, vector<int> cycle)
{
	int cycles_added = 0;
	bool inverted_signals = false;
	vector<int> negative_cycle;
	vector<int>::iterator found;

	cycle.push_back(forward ? idx->first.first : idx->first.second);

	found = find(cycle.begin(), cycle.end(), forward ? idx->first.second : idx->first.first);
	if (found == cycle.end()) {
		if (idx->second.first && idx->second.second && forward) {
			inverted_signals = true;
			inverted[idx->first.second] = !inverted[idx->first.second];
			for (graph::iterator j = net.begin(); j != net.end(); j++) {
				if (j->first.first == idx->first.second || j->first.second == idx->first.second) {
					j->second.second = !j->second.second;
				}
			}
		}
		else if (idx->second.first && idx->second.second && !forward) {
			inverted_signals = true;
			inverted[idx->first.first] = !inverted[idx->first.first];
			for (graph::iterator j = net.begin(); j != net.end(); j++) {
				if (j->first.first == idx->first.first || j->first.second == idx->first.first) {
					j->second.second = !j->second.second;
				}
			}
		}

		for (graph::iterator i = net.begin(); cycles_added == 0 && i != net.end(); i++) {
			pair<int, bool> result(0, false); 
			bool src_dst = i->first.first == idx->first.second;
			bool same_src = i->first.first == idx->first.first;
			bool same_dst = i->first.second == idx->first.second;
			bool dst_src = i->first.second == idx->first.first;
			if (forward && (src_dst || same_dst) && i != idx) {
				result = step(i, src_dst, cycle);
			} else if (!forward && (same_src || dst_src) && i != idx) {
				result = step(i, same_src, cycle);
			}
			cycles_added += result.first;
			inverted_signals = inverted_signals || result.second;
		}
	} else {
		vector<int> temp(found, cycle.end());
		sort(temp.begin(), temp.end());
		temp.resize(unique(temp.begin(), temp.end()) - temp.begin());
		cycles.push_back(bubbled_cycle(temp, !idx->second.first || !idx->second.second));
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
			tempstr += variables.nodes[cycles[i].first[j]].to_string();
		}
		error("", "negative cycle found " + tempstr, __FILE__, __LINE__);
	}

	map<size_t, size_t> inv;

	size_t num = variables.nodes.size();
	for (size_t i = 0; i < num; i++) {
		if (inverted[i]) {
			variables.nodes[i].name.back().name = "_" + variables.nodes[i].name.back().name;

			for (auto rule = prs->rules.begin(); rule != prs->rules.end(); rule++) {
				rule->sv_not(i);
			}
		}
	}
	
	// Apply local inversions to production rules
	for (graph::iterator i = net.begin(); i != net.end(); i++) {
		if (!i->second.first && i->second.second) {
			auto j = inv.find(i->first.first);
			if (j == inv.end()) {
				j = inv.insert(pair<size_t, size_t>(i->first.first, variables.nodes.size())).first;
				variables.nodes.push_back(variables.nodes[i->first.first]);
				if (variables.nodes.back().name.back().name[0] == '_') {
					variables.nodes.back().name.back().name.erase(variables.nodes.back().name.back().name.begin());
				} else {
					variables.nodes.back().name.back().name = "_" + variables.nodes.back().name.back().name;
				}

				production_rule pun, pdn;
				pun.guard = boolean::cover(j->first, 0);
				pun.local_action = boolean::cover(j->second, 1);
				pdn.guard = boolean::cover(j->first, 1);
				pdn.local_action = boolean::cover(j->second, 0);
				prs->rules.push_back(pun);
				prs->rules.push_back(pdn);
			}

			for (auto rule = prs->rules.begin(); rule != prs->rules.end(); rule++) {
				bool found = false;
				for (auto term = rule->local_action.cubes.begin(); !found && term != rule->local_action.cubes.end(); term++) {
					found = found || term->get((int)i->first.second) != 2;
				}
				
				if (found) {
					for (auto term = rule->guard.cubes.begin(); term != rule->guard.cubes.end(); term++) {
						int value = term->get(j->first);
						if (value != 2) {
							term->set(j->second, 1-value);
							term->set(j->first, 2);
						}
					}
				}
			}
		}
	}
}*/

}
