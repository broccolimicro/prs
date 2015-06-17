/*
 * simulator.cpp
 *
 *  Created on: Jun 2, 2015
 *      Author: nbingham
 */

#include "simulator.h"
#include <common/message.h>
#include <interpret_boolean/export.h>

namespace prs
{

string enabled_rule::to_string(const production_rule_set &base, const boolean::variable_set &v)
{
	string result;
	result = export_guard(guard, v).to_string();
	if (stable)
		result += "->";
	else
		result += "~>";
	result += export_assignment(base.rules[index].local_action.cubes[term], v).to_string();
	return result;
}

bool operator<(enabled_rule i, enabled_rule j)
{
	return ((term_index)i < (term_index)j) ||
		   ((term_index)i == (term_index)j && i.history < j.history);
}

bool operator>(enabled_rule i, enabled_rule j)
{
	return ((term_index)i > (term_index)j) ||
		   ((term_index)i == (term_index)j && i.history > j.history);
}

bool operator<=(enabled_rule i, enabled_rule j)
{
	return ((term_index)i < (term_index)j) ||
		   ((term_index)i == (term_index)j && i.history <= j.history);
}

bool operator>=(enabled_rule i, enabled_rule j)
{
	return ((term_index)i > (term_index)j) ||
		   ((term_index)i == (term_index)j && i.history >= j.history);
}

bool operator==(enabled_rule i, enabled_rule j)
{
	return ((term_index)i == (term_index)j && i.history == j.history);
}

bool operator!=(enabled_rule i, enabled_rule j)
{
	return ((term_index)i != (term_index)j || i.history != j.history);
}

instability::instability()
{
}

instability::instability(const enabled_rule &cause) : enabled_rule(cause)
{
}

instability::~instability()
{

}

string instability::to_string(const production_rule_set &base, const boolean::variable_set &v)
{
	string result = "unstable rule " + enabled_rule::to_string(base, v);

	if (history.size() > 0)
	{
		result += " cause: {";

		for (int j = 0; j < (int)history.size(); j++)
		{
			if (j != 0)
				result += "; ";

			result += history[j].to_string(base, v);
		}
		result += "}";
	}
	return result;
}

interference::interference()
{
}

interference::interference(const enabled_rule &first, const enabled_rule &second)
{
	if (first < second)
	{
		this->first = first;
		this->second = second;
	}
	else
	{
		this->first = second;
		this->second = first;
	}
}

interference::~interference()
{

}

string interference::to_string(const production_rule_set &base, const boolean::variable_set &v)
{
	if (!first.stable || !second.stable)
		return "weakly interfering rules " + first.to_string(base, v) + " and " + second.to_string(base, v);
	else
		return "interfering rules " + first.to_string(base, v) + " and " + second.to_string(base, v);
}

mutex::mutex()
{
}

mutex::mutex(const enabled_rule &first, const enabled_rule &second)
{
	if (first < second)
	{
		this->first = first;
		this->second = second;
	}
	else
	{
		this->first = second;
		this->second = first;
	}
}

mutex::~mutex()
{

}

string mutex::to_string(const production_rule_set &base, const boolean::variable_set &v)
{
	return "vacuous firings break mutual exclusion for rules " + first.to_string(base, v) + " and " + second.to_string(base, v);
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
	if (variables != NULL)
		for (int i = 0; i < (int)variables->variables.size(); i++)
		{
			global.set(i, -1);
			encoding.set(i, -1);
		}
}

simulator::~simulator()
{

}

int simulator::enabled()
{
	if (base == NULL)
		return 0;

	vector<enabled_rule> potential;
	enabled_rule t;
	for (t.index = 0; t.index < (int)base->rules.size(); t.index++)
	{
		t.term = 0;
		int pass = boolean::passes_guard(encoding, global, base->rules[t.index].guard, &t.guard);

		vector<enabled_rule>::iterator loc = lower_bound(firing.begin(), firing.end(), t);
		if (pass == -1 && loc != firing.end() && loc->index == t.index)
			firing.erase(loc);
		else if (pass >= 0)
		{
			t.stable = (bool)pass;

			if (loc == firing.end() || loc->index != t.index)
				for (t.term = 0; t.term < (int)base->rules[t.index].local_action.cubes.size(); t.term++)
				{
					t.local_action = base->rules[t.index].local_action.cubes[t.term];
					t.remote_action = base->rules[t.index].remote_action.cubes[t.term];
					potential.push_back(t);
				}
			else
			{
				loc->guard = t.guard;
				loc->local_action = base->rules[loc->index].local_action.cubes[loc->term];
				loc->remote_action = base->rules[loc->index].remote_action.cubes[loc->term];

				if (loc->stable != t.stable)
				{
					loc->stable = t.stable;
					potential.push_back(*loc);
					firing.erase(loc);
				}
			}
		}
	}

	// Check for instability
	int i = 0, j = 0;
	while (i < (int)ready.size())
	{
		if (j >= (int)potential.size() || (term_index)ready[i] < (term_index)potential[j])
		{
			ready[i].stable = false;
			potential.insert(potential.begin() + j, ready[i]);
			i++;
			j++;
		}
		else if ((term_index)potential[j] < (term_index)ready[i])
			j++;
		else
		{
			potential[j].history = ready[i].history;
			i++;
			j++;
		}
	}

	if (last.index >= 0)
		for (int i = 0; i < (int)potential.size(); i++)
			potential[i].history.push_back(last);

	ready = potential;

	// Check for interference and mutex
	bool change = true;
	while (change)
	{
		change = false;
		for (int i = 0; i < (int)ready.size(); i++)
			for (int j = 0; j < (int)firing.size(); j++)
				if (!are_mutex(ready[i].guard, firing[j].guard))
				{
					ready[i].local_action = boolean::interfere(ready[i].local_action, firing[j].remote_action);
					ready[i].remote_action = boolean::interfere(ready[i].remote_action, firing[j].remote_action);
				}

		vector<enabled_rule> vacuous;
		for (int i = 0; i < (int)ready.size(); i++)
			if (vacuous_assign(global, ready[i].remote_action, ready[i].stable))
				vacuous.push_back(ready[i]);

		for (int i = 0; i < (int)vacuous.size(); i++)
			for (int j = i+1; j < (int)vacuous.size(); j++)
				if (vacuous[i].index == vacuous[j].index)
				{
					mutex err(vacuous[i], vacuous[j]);
					vector<mutex>::iterator loc = lower_bound(mutex_errors.begin(), mutex_errors.end(), err);
					if (loc == mutex_errors.end() || *loc != err)
					{
						mutex_errors.insert(loc, err);
						warning("", err.to_string(*base, *variables), __FILE__, __LINE__);
					}
				}

		for (int i = 0; i < (int)vacuous.size(); i++)
		{
			firing.insert(lower_bound(firing.begin(), firing.end(), vacuous[i]), vacuous[i]);
			for (int j = (int)ready.size()-1; j >= 0; j--)
			{
				if (ready[j].index != vacuous[i].index)
					ready[j].history.push_back(vacuous[i]);
				else
					ready.erase(ready.begin() + j);
			}
			change = true;
		}
	}


	return ready.size();
}

boolean::cube simulator::fire(int index)
{
	if (base == NULL || index >= (int)ready.size())
		return boolean::cube();

	enabled_rule t = ready[index];

	if (!t.stable)
	{
		instability err(t);
		vector<instability>::iterator loc = lower_bound(instability_errors.begin(), instability_errors.end(), err);
		if (loc == instability_errors.end() || *loc != err)
		{
			instability_errors.insert(loc, err);
			warning("", err.to_string(*base, *variables), __FILE__, __LINE__);
		}
	}

	encoding = encoding & t.guard;

	for (int j = 0; j < (int)firing.size(); j++)
		if ((t.stable || firing[j].stable) && !are_mutex(t.guard, firing[j].guard) && are_mutex(base->rules[t.index].remote_action.cubes[t.term], base->rules[firing[j].index].remote_action.cubes[firing[j].term]))
		{
			interference err(t, firing[j]);
			vector<interference>::iterator loc = lower_bound(interference_errors.begin(), interference_errors.end(), err);
			if (loc == interference_errors.end() || *loc != err)
			{
				interference_errors.insert(loc, err);
				warning("", err.to_string(*base, *variables), __FILE__, __LINE__);
			}
		}

	global = boolean::local_assign(global, t.remote_action, t.stable);
	encoding = boolean::local_assign(encoding & t.guard, t.local_action, t.stable);
	encoding = boolean::remote_assign(encoding, global, true);

	firing.insert(lower_bound(firing.begin(), firing.end(), t), t);

	for (int i = (int)ready.size()-1; i >= 0; i--)
		if (ready[i].index == t.index)
			ready.erase(ready.begin() + i);

	last = t;

	return t.local_action;
}

void simulator::reset()
{
	for (int i = 0; i < (int)variables->variables.size(); i++)
	{
		global.set(i, -1);
		encoding.set(i, -1);
	}
	ready.clear();
	firing.clear();
	instability_errors.clear();
	interference_errors.clear();
	mutex_errors.clear();
}
}
