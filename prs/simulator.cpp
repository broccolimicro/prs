/*
 * simulator.cpp
 *
 *  Created on: Jun 2, 2015
 *      Author: nbingham
 */

#include "simulator.h"
#include <common/message.h>

namespace prs
{

instability::instability()
{

}

instability::instability(term_index effect, vector<term_index> cause)
{
	this->effect = effect;
	this->cause = cause;
}

instability::~instability()
{

}

string instability::to_string(const production_rule_set &base, const boolean::variable_set &v)
{
	string result = "unstable rule " + effect.to_string(base, v);

	result += " cause: {";

	for (int j = 0; j < (int)cause.size(); j++)
	{
		if (j != 0)
			result += "; ";

		result += cause[j].to_string(base, v);
	}
	result += "}";
	return result;
}

bool operator<(instability i, instability j)
{
	return (i.effect < j.effect) ||
		   (i.effect == j.effect && i.cause < j.cause);
}

bool operator>(instability i, instability j)
{
	return (i.effect > j.effect) ||
		   (i.effect == j.effect && i.cause > j.cause);
}

bool operator<=(instability i, instability j)
{
	return (i.effect < j.effect) ||
		   (i.effect == j.effect && i.cause <= j.cause);
}

bool operator>=(instability i, instability j)
{
	return (i.effect > j.effect) ||
		   (i.effect == j.effect && i.cause >= j.cause);
}

bool operator==(instability i, instability j)
{
	return (i.effect == j.effect && i.cause == j.cause);
}

bool operator!=(instability i, instability j)
{
	return (i.effect != j.effect || i.cause != j.cause);
}

interference::interference()
{

}

interference::interference(term_index first, term_index second) : pair<term_index, term_index>(first, second)
{
}

interference::~interference()
{

}

string interference::to_string(const production_rule_set &base, const boolean::variable_set &v)
{
	return "interfering assignments " + first.to_string(base, v) + " and " + second.to_string(base, v);
}

simulator::simulator()
{
	base = NULL;
	variables = NULL;
}

simulator::simulator(const production_rule_set *base, const boolean::variable_set *variables)
{
	this->base = base;
	this->variables = variables;
}

simulator::~simulator()
{

}

int simulator::enabled()
{
	if (base == NULL)
		return 0;

	vector<term_index> potential;
	for (int i = 0; i < (int)base->rules.size(); i++)
		for (int j = 0; j < (int)base->rules[i].guard.cubes.size(); j++)
			if (!are_mutex(base->rules[i].guard.cubes[j], encoding))
				for (int k = 0; k < (int)base->rules[i].local_action.cubes.size(); k++)
					potential.push_back(term_index(i, j, k));

	// Check for interference
	for (int i = 0; i < (int)potential.size(); i++)
		for (int j = i+1; j < (int)potential.size(); j++)
			if (boolean::are_mutex(base->rules[potential[i].index].local_action[potential[i].term], base->rules[potential[j].index].local_action[potential[j].term]))
			{
				interference err(potential[i], potential[j]);
				vector<interference>::iterator loc = lower_bound(interfering.begin(), interfering.end(), err);
				if (loc == interfering.end() || *loc != err)
				{
					interfering.insert(loc, err);
					error("", err.to_string(*base, *variables), __FILE__, __LINE__);
				}
			}

	// Check for instability
	int i = 0, j = 0;
	while (i < (int)ready.size())
	{
		if (ready[i] < potential[j])
		{
			i++;
			unstable.push_back(instability(ready[i], vector<term_index>()));
			error("", unstable.back().to_string(*base, *variables), __FILE__, __LINE__);
 		}
		else if (potential[j] < ready[i])
			j++;
		else
		{
			i++;
			j++;
		}
	}

	ready = potential;

	return ready.size();
}

boolean::cube simulator::fire(int index)
{
	if (base == NULL || index >= (int)ready.size())
		return boolean::cube();

	term_index t = ready[index];

	encoding = boolean::local_transition(encoding & base->rules[t.index].guard.cubes[t.guard], base->rules[t.index].local_action.cubes[t.term]);
	if (t.term < (int)base->rules[t.index].remote_action.cubes.size())
		encoding = boolean::remote_transition(encoding, base->rules[t.index].remote_action.cubes[t.term]);

	for (int i = (int)ready.size()-1; i >= 0; i--)
		if (ready[i].index == t.index)
			ready.erase(ready.begin() + i);

	return base->rules[t.index].local_action[t.term];
}

}
