/*
 * production_rule.cpp
 *
 *  Created on: Feb 2, 2015
 *      Author: nbingham
 */

#include "production_rule.h"
#include <interpret_boolean/export.h>

namespace prs
{
production_rule::production_rule()
{
}

production_rule::~production_rule()
{

}

void production_rule::sv_not(int uid)
{
	for (auto term = guard.cubes.begin(); term != guard.cubes.end(); term++) {
		term->sv_not(uid);
	}

	for (auto term = local_action.cubes.begin(); term != local_action.cubes.end(); term++) {
		term->sv_not(uid);
	}

	for (auto term = remote_action.cubes.begin(); term != remote_action.cubes.end(); term++) {
		term->sv_not(uid);
	}
}


production_rule_set::production_rule_set()
{
	mutex = 1;
}

production_rule_set::~production_rule_set()
{
}

void production_rule_set::post_process(const ucs::variable_set &variables)
{
	for (int i = 0; i < (int)rules.size(); i++)
		rules[i].remote_action = rules[i].local_action.remote(variables.get_groups());
}

term_index::term_index()
{
	index = -1;
	term = -1;
}

term_index::term_index(int index, int term)
{
	this->index = index;
	this->term = term;
}

term_index::~term_index()
{

}

string term_index::to_string(const production_rule_set &prs, const ucs::variable_set &v)
{
	return "T" + ::to_string(index) + "." + ::to_string(term) + ":" + export_composition(prs.rules[index].local_action.cubes[term], v).to_string();
}

bool operator<(term_index i, term_index j)
{
	return (i.index < j.index) ||
		   (i.index == j.index && i.term < j.term);
}

bool operator>(term_index i, term_index j)
{
	return (i.index > j.index) ||
		   (i.index == j.index && i.term > j.term);
}

bool operator<=(term_index i, term_index j)
{
	return (i.index < j.index) ||
		   (i.index == j.index && i.term <= j.term);
}

bool operator>=(term_index i, term_index j)
{
	return (i.index > j.index) ||
		   (i.index == j.index && i.term >= j.term);
}

bool operator==(term_index i, term_index j)
{
	return (i.index == j.index && i.term == j.term);
}

bool operator!=(term_index i, term_index j)
{
	return (i.index != j.index || i.term != j.term);
}

}
