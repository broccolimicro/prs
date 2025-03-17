#pragma once

#include <common/standard.h>
#include "production_rule.h"

namespace prs
{

// @brief The bubble class implements the bubble reshuffling algorithm for asynchronous circuits.
// 
// Bubble reshuffling is a synthesis technique for asynchronous circuits that identifies and 
// removes signal inversions (bubbles) from isochronic forks. This makes the circuit CMOS implementable.
// 
// The algorithm works by constructing a graph representation of the circuit, identifying cycles,
// and optimizing signal polarities to push inversions off of isochronic arcs.
struct bubble
{
	bubble();
	~bubble();

	// @brief Represents an arc (edge) in the signal dependency graph.
	// 
	// An arc connects two nodes in the graph, representing a wire between two gates.
	// The "bubble" attribute indicates if the relation is CMOS implementable.
	struct arc {
		arc();
		
		// @brief Construct an arc in the bubble graph.
		// 
		// @param from Source node index
		// @param fval Source node value (0 or 1)
		// @param to Destination node index
		// @param tval Destination node value (0 or 1)
		arc(int from, int fval, int to, int tval);
		~arc();

		int from;
		int to;
		mutable int tval;
		mutable vector<int> gates;
		mutable bool isochronic;
		mutable bool bubble;
	};

	typedef set<arc> graph;
	typedef vector<int> cycle;
	typedef pair<cycle, bool> bubbled_cycle;
	
	graph net;
	vector<bubbled_cycle> cycles;
	vector<bool> inverted;
	vector<bool> linked;

	// Constructs the bubble graph representation from production rules.
	void load_prs(const production_rule_set &prs);

	pair<int, bool> step(graph::iterator idx, bool forward = true, vector<int> cycle = vector<int>());
	bool complete();
	
	// @brief Execute the bubble reshuffling algorithm, pushing bubbles off of isochronic forks.
	void reshuffle();

	// @brief Modifies the production rule set to invert signals and insert inverters at the bubble locations.
	void save_prs(production_rule_set *prs);

	void print() const;
};

bool operator<(const bubble::arc &a0, const bubble::arc &a1);
bool operator==(const bubble::arc &a0, const bubble::arc &a1);

}
