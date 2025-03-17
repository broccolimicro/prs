#include "bubble.h"
#include <common/message.h>
#include <common/text.h>
#include <interpret_boolean/export.h>

// The bubble reshuffling algorithm solves a key problem in asynchronous circuit implementation:
// making circuits with isochronic forks CMOS-implementable by moving signal inversions ("bubbles")
// to locations where they won't cause hazards or race conditions.

namespace prs
{

bubble::arc::arc() {
	from = 0;
	to = 0;
	tval = -1;
	isochronic = true;
	bubble = false;
}

// In CMOS technology, gates are inherently inverting. A bubble is created when the
// source and target value are the same logical value (both 0 or both 1), which would
// require an inversion to implement.
bubble::arc::arc(int from, int fval, int to, int tval) {
	this->from = from;
	this->to = to;
	this->tval = tval;
	this->isochronic = true;
	this->bubble = (fval == tval);
}

bubble::arc::~arc() {
}

// Arcs are ordered primarily by source node, then by destination node.
// This ordering is needed for the set<arc> container to function properly.
bool operator<(const bubble::arc &a0, const bubble::arc &a1) {
	return a0.from < a1.from
		or (a0.from == a1.from and (a0.to < a1.to));
}

// Two arcs are considered equal if they connect the same nodes, regardless
// of other properties. This is important for detecting overlapping arcs.
bool operator==(const bubble::arc &a0, const bubble::arc &a1) {
	return a0.from == a1.from and a0.to == a1.to;
}

bubble::bubble()
{
}

bubble::~bubble()
{
}

// Load production rules into the bubble reshuffling algorithm
// 
// This function constructs a graph representation of the circuit from the
// given production rule set. Each node represents a signal, and each arc
// represents a wire connecting from an input to the gate driving the signal.
void bubble::load_prs(const production_rule_set &prs)
{
	net.clear();
	inverted.clear();
	linked.clear();
	inverted.resize(prs.nets.size(), false);
	linked.resize(prs.nets.size(), false);

	// Find all nets that are inputs to another gate.
	vector<int> rules;
	for (int i = 0; i < (int)prs.nets.size(); i++) {
		if (not prs.nets[i].gateOf[0].empty() or not prs.nets[i].gateOf[1].empty()) {
			rules.push_back(i);
		}
	}

	for (int i = 0; i < (int)rules.size(); i++) {
		// Get the canonical representative for this net
		int drain = prs.nets[rules[i]].remote[0];

		// Depth-first traversal starting from this rule
		vector<int> stack(1, rules[i]);
		while (not stack.empty()) {
			int curr = stack.back();
			stack.pop_back();

			// Process pull-up (driver=1) and pull-down (driver=0) networks
			for (int driver = 0; driver < 2; driver++) {
				for (auto di = prs.nets[curr].drainOf[driver].begin(); di != prs.nets[curr].drainOf[driver].end(); di++) {
					auto dev = prs.devs.begin()+*di;
					int gate = prs.nets[dev->gate].remote[0];
					
					// Create an arc representing the connection from the input to either
					// pull-up or pull-down network.
					arc a(gate, dev->threshold, drain, driver);
					a.gates.push_back(*di);
					// Try to insert or return the overlapping arc if found
					auto j = net.insert(a);
					
					// An overlapping arc is already in the graph, so we need to merge them.
					// This happens when the same input connects to both pull-up and pull-down
					// networks of the same gate.
					if (not j.second) {
						if (j.first->bubble != a.bubble) {
							// Bubble reshuffling cannot handle dividing or gating signals by definition.
							// These immediately create isochronic cycles with a bubble, which is a fundamental
							// limitation of the bubble reshuffling algorithm.
							
							// Dividing signal: Same signal drives multiple outputs with conflicting polarities
							if (j.first->tval == -1 or j.first->tval == a.tval) {
								error("", "dividing signal found in production rules {" + prs.nets[a.from].name + " -> " + prs.nets[a.to].name + (a.tval == 1 ? "+" : "-") + "}", __FILE__, __LINE__);
							}
							// Gating signal: Same signal is used in contradictory ways in the same gate
							if (j.first->tval == -1 or j.first->tval != a.tval) {
								error("", "gating signal found in production rules {" + prs.nets[a.from].name + (((a.tval == 1 and not a.bubble) or (a.tval != 1 and a.bubble)) ? "+" : "-") + " -> " + prs.nets[a.to].name + "}", __FILE__, __LINE__);
							}
						} else if (j.first->tval != a.tval) {
							// If we found an overlapping arc with the opposite value,
							// then merging them together means the arc is not isochronic.
							// This happens when the same signal is used in both pull-up and pull-down
							// networks of the same gate, which is a non-isochronic fork.
							linked[j.first->from] = true;
							linked[j.first->to] = true;
							j.first->tval = -1;  // Mark as don't care (both pull-up and pull-down)
							j.first->isochronic = false;
							// Add this gate to the list of gates implementing this arc
							auto pos = lower_bound(j.first->gates.begin(), j.first->gates.end(), *di);
							if (pos == j.first->gates.end() or *pos != *di) {
								j.first->gates.insert(pos, *di);
							}
						}
					}
					stack.push_back(dev->source);
				}
			}
		}
	}
}

// Recursive step in the bubble reshuffling algorithm. This function pushing bubbles off isochronic forks
// by inverting signals, following successive bubbles on isochronic forks until this bubble has been resolved.
//
// @param idx Iterator pointing to the current arc being analyzed
// @param forward Direction of traversal (true for forward, false for backward)
// @param cycle Current path being analyzed for cycle detection
// @return A pair containing (cycles added, signals inverted)
pair<int, bool> bubble::step(graph::iterator idx, bool forward, vector<int> cycle)
{
	int cycles_added = 0;
	bool inverted_signals = false;
	vector<int> negative_cycle;
	vector<int>::iterator found;

	// Add current node to the path we're exploring
	cycle.push_back(forward ? idx->from : idx->to);

	// Check if we've completed a cycle by finding a repeated node
	found = find(cycle.begin(), cycle.end(), forward ? idx->to : idx->from);
	if (found == cycle.end()) {
		// No cycle detected yet, continue traversal
		int n = forward ? idx->to : idx->from;
		
		// CORE ALGORITHM: If this arc has a bubble and is isochronic, 
		// invert the signal to push the bubble off the isochronic fork.
		if (idx->isochronic and idx->bubble) {
			inverted_signals = true;
			inverted[n] = not inverted[n];
			
			// When we invert a node, we need to flip all bubbles connected to this node
			// because changing the polarity of a signal affects all its connections
			for (graph::iterator j = net.begin(); j != net.end(); j++) {
				if (j->from == n or j->to == n) {
					j->bubble = not j->bubble;
				}
			}
		}

		// Continue traversal by exploring connected arcs
		// We stop exploring this path if we find a cycle (cycles_added > 0)
		for (graph::iterator i = net.begin(); cycles_added == 0 and i != net.end(); i++) {
			pair<int, bool> result(0, false); 
			// Determine how this arc connects to the current one
			bool src_dst = i->from == idx->to;         // This arc starts from where the current arc ends
			bool same_src = i->from == idx->from;      // This arc starts from the same node as current
			bool same_dst = i->to == idx->to;          // This arc ends at the same node as current
			bool dst_src = i->to == idx->from;         // This arc ends where the current arc starts
			
			// Select next arc to traverse based on direction
			if (forward and (src_dst or same_dst) and i != idx) {
				result = step(i, src_dst, cycle);
			} else if (!forward and (same_src or dst_src) and i != idx) {
				result = step(i, same_src, cycle);
			}
			cycles_added += result.first;
			inverted_signals = inverted_signals or result.second;
		}
	} else {
		// Found a cycle, record it
		vector<int> temp(found, cycle.end());
		sort(temp.begin(), temp.end());
		temp.resize(unique(temp.begin(), temp.end()) - temp.begin());
		
		// Record the cycle with a flag indicating if it's a negative cycle
		// A negative cycle is one that has an odd number of bubbles or non-isochronic forks
		// These cycles are problematic because they can't be fully optimized
		cycles.push_back(bubbled_cycle(temp, not idx->isochronic or not idx->bubble));
		cycles_added += 1;
	}

	return pair<int, bool>(cycles_added, inverted_signals);
}

// Run a quick optimization pass to minimize the number of inverters in the final circuit.
// 
// This function examines each node to determine if inverting it would reduce the number of bubbles.
// It considers the fan-in and fan-out of each node to make this determination.
// 
// The key insight: If a node has more bubbled inputs than non-bubbled inputs plus output bubbles,
// then inverting the node will reduce the total number of bubbles in the circuit.
bool bubble::complete()
{
	bool inverted_signals = false;
	for (int n = 0; n < (int)inverted.size(); n++) {
		// Count various types of connections
		int inb = 0;   // Count of bubbled inputs (connections TO this node with bubbles)
		int in = 0;    // Count of non-bubbled inputs
		int out = 0;   // Flag indicating if there are non-bubbled outputs
		bool isochronic = false;  // Check if any connection is isochronic
		
		for (graph::iterator j = net.begin(); j != net.end() and not isochronic; j++) {
			if (j->from == n) {  // This is an output connection FROM this node
				out = j->bubble ? out : 1;  // Set to 1 if there's any non-bubbled output
				isochronic = j->isochronic;
			} else if (j->to == n) {  // This is an input connection TO this node
				inb += j->bubble;     // Count bubbled inputs
				in += not j->bubble;  // Count non-bubbled inputs
				isochronic = j->isochronic;
			}
		}

		// If this node has non-isochronic forks and inverting it would reduce bubbles,
		// then invert it. The condition inb > in+out means: 
		// "There are more bubbled inputs than non-bubbled inputs plus output flags"
		// This is a heuristic to minimize the total number of inversions in the circuit.
		if (not isochronic and inb > in+out) {
			inverted_signals = true;
			inverted[n] = not inverted[n];
			
			// Flip all bubbles connected to this node
			for (graph::iterator j = net.begin(); j != net.end(); j++) {
				if (j->from == n or j->to == n) {
					j->bubble = not j->bubble;
				}
			}
		}
	}

	return inverted_signals;
}

// Execute bubble reshuffling algorithm. Check each wire, if that wire
// is isochronic and there is a bubble, then push it around the circuit
// until there are no bubbles on isochronic arcs. If we find a cycle of
// isochronic arcs with a bubble, then we cannot resolve the graph.
// Record this in cycles and move on.
void bubble::reshuffle()
{
	for (graph::iterator i = net.begin(); i != net.end(); i++) {
		step(i);
	}
}

// Apply the bubble reshuffling results back to the production rule set.
// 
// This function handles:
// 1. Processing cycles to identify problematic configurations
// 2. Applying global inversions to signals
// 3. Adding local inversions (actual inverter gates) where needed
// 
// The function distinguishes between global inversions (changing a signal's polarity
// everywhere) and local inversions (adding an actual inverter gate).
void bubble::save_prs(production_rule_set *prs)
{
	// Remove duplicate cycles
	sort(cycles.begin(), cycles.end());
	cycles.resize(unique(cycles.begin(), cycles.end()) - cycles.begin());

	// Annihilate conflicting cycles (cycles that cancel each other out)
	// This happens when two identical cycles have opposite bubble flags
	for (size_t i = 1; i < cycles.size(); i++) {
		if (cycles[i].first == cycles[i-1].first) {
			cycles.erase(cycles.begin() + i);
			cycles.erase(cycles.begin() + i-1);
			i--;
		}
	}

	// Remove positive cycles (they don't affect functionality)
	// A positive cycle is one with an even number of bubbles, which can be safely ignored
	for (size_t i = 0; i < cycles.size(); i++) {
		if (cycles[i].second) {
			cycles.erase(cycles.begin() + i);
			i--;
		}
	}

	// Report negative cycles (these indicate potential issues in the circuit)
	// A negative cycle is one with an odd number of bubbles that cannot be resolved
	// This indicates a fundamental limitation in the circuit's implementability
	for (size_t i = 0; i < cycles.size(); i++) {
		string tempstr;
		for (size_t j = 0; j < cycles[i].first.size(); j++) {
			if (j != 0)
				tempstr += ", ";
			tempstr += prs->nets[cycles[i].first[j]].name;
		}
		error("", "negative cycle found " + tempstr, __FILE__, __LINE__);
	}

	// This map tracks where we've already inserted inverters
	// The key is the source node, and the value is a list of new inverted nodes
	map<int, vector<int> > inv;

	// Apply global inversions to nets
	// A global inversion means changing the polarity of a signal everywhere it's used
	for (int i = 0; i < (int)inverted.size(); i++) {
		if (inverted[i]) {
			// Mark all instances of this net with '_' prefix to indicate inversion
			for (auto j = prs->nets[i].remote.begin(); j != prs->nets[i].remote.end(); j++) {
				if (*j >= 0) {
					prs->nets[*j].name = "_" + prs->nets[*j].name;
				}
			}
			// Invert the production rules for this net
			prs->invert(i);
		}
	}

	// Apply local inversions to production rules
	// A local inversion means adding an actual inverter gate
	for (graph::iterator i = net.begin(); i != net.end(); i++) {
		// We need to add an inverter for non-isochronic arcs with bubbles
		// These are locations where we can't just flip signal polarity globally
		if (not i->isochronic and i->bubble) {
			auto ins = inv.insert(pair<int, vector<int> >(i->from, vector<int>()));
			auto j = ins.first;

			// If this is the first time we're adding an inverter for this source node
			if (ins.second) {
				// Create a new inverted version of the signal
				int canonical = (int)prs->nets.size();
				for (int k = 0; k < (int)prs->nets[i->from].remote.size(); k++) {
					int uid = prs->nets[i->from].remote[k];
					// Create a new net for each remote reference
					int idx = prs->create();
					
					// Link all the new nets together
					if (not j->second.empty()) {
						prs->nets[j->second[0]].add_remote(idx);
					}
					j->second.push_back(idx);
					
					// Create the proper name for this inverted signal
					if (not prs->nets[uid].name.empty()) {
						prs->nets[idx].name = prs->nets[uid].name;
						prs->nets[idx].region = prs->nets[uid].region;
						if (prs->nets[idx].name[0] == '_') {
							prs->nets[idx].name.erase(prs->nets[idx].name.begin());
						} else {
							prs->nets[idx].name = "_" + prs->nets[idx].name;
						}
					}

					if (uid == i->from) {
						canonical = idx;
					}
				}

				// Add actual inverter gates between the original signal and its inverted version
				// We need both pull-up and pull-down networks for the inverter
				prs->connect(prs->add_source(i->from, canonical, 0, 1), prs->pwr[0][1]);  // Pull-up
				prs->connect(prs->add_source(i->from, canonical, 1, 0), prs->pwr[0][0]);  // Pull-down
			}

			// Update all gates that need to use the inverted signal
			for (auto di = i->gates.begin(); di != i->gates.end(); di++) {
				int canonical = j->second[0];
				// Find which specific inverted net should be used for this gate
				for (int k = 0; k < (int)prs->nets[i->from].remote.size(); k++) {
					if (prs->nets[i->from].remote[k] == prs->devs[*di].gate) {
						canonical = j->second[k];
						break;
					}
				}

				// Modify the gate to use the inverted signal with the opposite threshold
				prs->move_gate(*di, canonical, 1-prs->devs[*di].threshold);
			}
		}
	}
}

// Print a text representation of the bubble graph to stdout
// Format: from -[isochronic]> [bubble]to
// where isochronic is blank for isochronic arcs, '-' for non-isochronic arcs
// and bubble is 'o ' if there's a bubble, blank otherwise
void bubble::print() const {
	for (auto a = net.begin(); a != net.end(); a++) {
		printf("%d -%s> %s%d\n", a->from, (a->isochronic ? " " : "-"), (a->bubble ? "o " : ""), a->to);
	}
}

}
