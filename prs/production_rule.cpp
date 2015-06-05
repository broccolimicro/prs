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

production_rule_set::production_rule_set()
{

}

production_rule_set::~production_rule_set()
{

}

void production_rule_set::post_process(const boolean::variable_set &variables)
{
	for (int i = 0; i < (int)rules.size(); i++)
		rules[i].remote_action = variables.remote(rules[i].local_action);
}

term_index::term_index()
{
	index = -1;
	guard = -1;
	term = -1;
}

term_index::term_index(int index, int guard, int term)
{
	this->index = index;
	this->guard = guard;
	this->term = term;
}

term_index::~term_index()
{

}

string term_index::to_string(const production_rule_set &prs, const boolean::variable_set &v)
{
	return "T" + ::to_string(index) + "." + ::to_string(guard) + "->" + ::to_string(term) + ":" + export_guard(prs.rules[index].guard.cubes[guard], v).to_string() + "->" + export_assignment(prs.rules[index].local_action.cubes[term], v).to_string();
}

bool operator<(term_index i, term_index j)
{
	return (i.index < j.index) ||
		   (i.index == j.index && (i.guard < j.guard ||
				                  (i.guard == j.guard && i.term < j.term)));
}

bool operator>(term_index i, term_index j)
{
	return (i.index > j.index) ||
		   (i.index == j.index && (i.guard > j.guard ||
								  (i.guard == j.guard && i.term > j.term)));
}

bool operator<=(term_index i, term_index j)
{
	return (i.index < j.index) ||
		   (i.index == j.index && (i.guard < j.guard ||
								  (i.guard == j.guard && i.term <= j.term)));
}

bool operator>=(term_index i, term_index j)
{
	return (i.index > j.index) ||
		   (i.index == j.index && (i.guard > j.guard ||
								  (i.guard == j.guard && i.term >= j.term)));
}

bool operator==(term_index i, term_index j)
{
	return (i.index == j.index && i.guard == j.guard && i.term == j.term);
}

bool operator!=(term_index i, term_index j)
{
	return (i.index != j.index || i.guard != j.guard || i.term != j.term);
}

}
