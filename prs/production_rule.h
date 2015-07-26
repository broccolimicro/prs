/*
 * production_rule.h
 *
 *  Created on: Feb 2, 2015
 *      Author: nbingham
 */

#include <common/standard.h>
#include <boolean/cover.h>
#include <ucs/variable.h>

#ifndef prs_production_rule_h
#define prs_production_rule_h

namespace prs
{

struct production_rule
{
	production_rule();
	~production_rule();

	boolean::cover guard;
	boolean::cover local_action;
	boolean::cover remote_action;
};

struct production_rule_set
{
	production_rule_set();
	~production_rule_set();

	vector<production_rule> rules;

	void post_process(const ucs::variable_set &variables);
};

/* This points to the cube 'term' in the action of transition 'index' in a graph.
 * i.e. g.transitions[index].action.cubes[term]
 */
struct term_index
{
	term_index();
	term_index(int index, int term);
	~term_index();

	int index;
	int term;

	string to_string(const production_rule_set &g, const ucs::variable_set &v);
};

bool operator<(term_index i, term_index j);
bool operator>(term_index i, term_index j);
bool operator<=(term_index i, term_index j);
bool operator>=(term_index i, term_index j);
bool operator==(term_index i, term_index j);
bool operator!=(term_index i, term_index j);

}

#endif
