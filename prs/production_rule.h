#pragma once

#include <common/standard.h>
#include <boolean/cover.h>
#include <ucs/variable.h>

namespace prs
{

struct production_rule
{
	production_rule();
	~production_rule();

	boolean::cover guard;
	boolean::cover local_action;
	boolean::cover remote_action;

	void sv_not(int uid);
};

struct production_rule_set
{
	production_rule_set();
	~production_rule_set();

	vector<production_rule> rules;

	// This specifies the set of legal states
	// So if you want to make a and b mutually exclusive high,
	// then mutex would be ~a | ~b
	boolean::cover mutex;

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

