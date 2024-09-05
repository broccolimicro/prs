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

/*struct bubble
{
	bubble();
	~bubble();

	// from, to -> is_isochronic, has_bubble
	typedef pair<int, int> arc;
	typedef pair<bool, bool> bubbles;
	typedef map<arc, bubbles> graph;
	typedef vector<int> cycle;
	typedef pair<cycle, bool> bubbled_cycle;
	
	graph net;
	vector<bubbled_cycle> cycles;
	vector<bool> inverted;

	void load_prs(const production_rule_set &prs, const ucs::variable_set &variables);

	pair<int, bool> step(graph::iterator idx, bool forward = true, vector<int> cycle = vector<int>());
	void reshuffle();
	
	void save_prs(production_rule_set *prs, ucs::variable_set &variables);
};*/

}
