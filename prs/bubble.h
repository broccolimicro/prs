/*
 *  bubble.h
 *
 *  Created on: July 9, 2023
 *      Author: nbingham
 */

#pragma once

#include <common/standard.h>
#include <ucs/variable.h>
#include "production_rule.h"

namespace prs
{

struct bubble
{
	bubble();
	~bubble();

	struct arc {
		arc();
		arc(int from, int fval, int to, int tval);
		~arc();

		int from;
		int to;
		mutable int tval;
		mutable array<vector<int>, 2> gates;
		mutable bool isochronic;
		mutable bool bubble;
	};

	typedef set<arc> graph;
	typedef vector<int> cycle;
	typedef pair<cycle, bool> bubbled_cycle;
	
	int nets;
	int nodes;

	graph net;
	vector<bubbled_cycle> cycles;
	vector<bool> inverted;
	vector<bool> linked;

	int uid(int net) const;

	void load_prs(const production_rule_set &prs, const ucs::variable_set &variables);

	pair<int, bool> step(graph::iterator idx, bool forward = true, vector<int> cycle = vector<int>());
	void reshuffle();

	void save_prs(production_rule_set *prs, ucs::variable_set &variables);
};

bool operator<(const bubble::arc &a0, const bubble::arc &a1);
bool operator==(const bubble::arc &a0, const bubble::arc &a1);

}
