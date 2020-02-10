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

enabled_rule::enabled_rule()
{
	index = -1;
	stable = false;
	vacuous = true;
}

enabled_rule::~enabled_rule()
{

}

string enabled_rule::to_string(const production_rule_set &base, const ucs::variable_set &v)
{
	string result;
	result = export_expression(guard_action, v).to_string();
	if (stable)
		result += "->";
	else
		result += "~>";
	result += export_composition(base.rules[index].local_action, v).to_string();
	return result;
}

bool operator<(enabled_rule i, enabled_rule j)
{
	return (i.index < j.index) ||
		   (i.index == j.index && i.history < j.history);
}

bool operator>(enabled_rule i, enabled_rule j)
{
	return (i.index > j.index) ||
		   (i.index == j.index && i.history > j.history);
}

bool operator<=(enabled_rule i, enabled_rule j)
{
	return (i.index < j.index) ||
		   (i.index == j.index && i.history <= j.history);
}

bool operator>=(enabled_rule i, enabled_rule j)
{
	return (i.index > j.index) ||
		   (i.index == j.index && i.history >= j.history);
}

bool operator==(enabled_rule i, enabled_rule j)
{
	return (i.index == j.index && i.history == j.history);
}

bool operator!=(enabled_rule i, enabled_rule j)
{
	return (i.index != j.index || i.history != j.history);
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

string instability::to_string(const production_rule_set &base, const ucs::variable_set &v)
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

interference::interference(const term_index &first, const term_index &second)
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

string interference::to_string(const production_rule_set &base, const ucs::variable_set &v)
{
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

string mutex::to_string(const production_rule_set &base, const ucs::variable_set &v)
{
	return "vacuous firings break mutual exclusion for rules " + first.to_string(base, v) + " and " + second.to_string(base, v);
}

simulator::simulator()
{
	base = NULL;
	variables = NULL;
}

simulator::simulator(const production_rule_set *base, const ucs::variable_set *variables)
{
	this->base = base;
	this->variables = variables;
	if (variables != NULL)
		for (int i = 0; i < (int)variables->nodes.size(); i++)
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

	vector<enabled_rule> preload;

	for (int i = 0; i < (int)base->rules.size(); i++)
	{
		enabled_rule t;
		t.index = i;
		bool previously_enabled = false;
		for (int j = 0; j < (int)loaded.size() && !previously_enabled; j++)
			if (loaded[j].index == t.index)
			{
				t.history = loaded[j].history;
				previously_enabled = true;
			}

		int ready = boolean::passes_guard(encoding, global, base->rules[t.index].guard, &t.guard_action);
		if (ready < 0 && previously_enabled)
			ready = 0;

		t.stable = (ready > 0);
		t.vacuous = boolean::vacuous_assign(global, base->rules[t.index].remote_action, t.stable);
		t.mutex = boolean::passes_constraint(local_assign(global, base->rules[t.index].remote_action, t.stable), base->mutex);

		if (ready >= 0 && !t.vacuous && t.mutex.size() > 0)
			preload.push_back(t);
	}

	loaded = preload;
	ready.clear();

	for (int i = 0; i < (int)loaded.size(); i++)
		if (!loaded[i].vacuous)
			for (int j = 0; j < (int)loaded[i].mutex.size(); j++)
				ready.push_back(pair<int, int>(i, loaded[i].mutex[j]));

	if (last.index >= 0)
		for (int i = 0; i < (int)loaded.size(); i++)
			loaded[i].history.push_back(last);

	/*for (int i = 0; i < (int)loaded.size(); i++)
	{
		cout << "\t\tLoaded " << i << ":" << loaded[i].index << " " << (loaded[i].stable ? "stable" : "unstable") << " " << (loaded[i].vacuous ? "vacuous" : "nonvacuous") << " " << export_expression(loaded[i].guard_action, *variables).to_string() << " -> " << export_composition(base->rules[loaded[i].index].local_action, *variables).to_string() << endl;
		for (int j = 0; j < (int)loaded[i].history.size(); j++)
			cout << "\t\t\t" << export_expression(base->rules[loaded[i].history[j].index].guard, *variables).to_string() << " -> " << export_composition(base->rules[loaded[i].history[j].index].local_action[loaded[i].history[j].term], *variables).to_string() << endl;
	}*/

	return ready.size();
}

boolean::cube simulator::fire(int index)
{
	if (base == NULL || index < 0 || index >= (int)ready.size())
		return boolean::cube();

	enabled_rule t = loaded[ready[index].first];
	int term = ready[index].second;

	loaded.erase(loaded.begin() + ready[index].first);

	ready.clear();

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

	// Check for interfering transitions. Interfering transitions are the active transitions that have fired since this
	// active transition was enabled.
	boolean::cube local_action = base->rules[t.index].local_action[term];
	boolean::cube remote_action = base->rules[t.index].remote_action[term];
	for (int j = 0; j < (int)t.history.size(); j++)
	{
		if (boolean::are_mutex(base->rules[t.index].remote_action[term], base->rules[t.history[j].index].local_action[t.history[j].term]))
		{
			interference err(term_index(t.index, term), t.history[j]);
			vector<interference>::iterator loc = lower_bound(interference_errors.begin(), interference_errors.end(), err);
			if (loc == interference_errors.end() || *loc != err)
			{
				interference_errors.insert(loc, err);
				error("", err.to_string(*base, *variables), __FILE__, __LINE__);
			}
		}

		local_action = boolean::interfere(local_action, base->rules[t.history[j].index].remote_action[t.history[j].term]);
		remote_action = boolean::interfere(remote_action, base->rules[t.history[j].index].remote_action[t.history[j].term]);
	}

	// Update the state
	if (t.stable)
	{
		global &= t.guard_action;
		encoding &= t.guard_action;
	}

	global = local_assign(global, remote_action, t.stable);
	encoding = remote_assign(local_assign(encoding, local_action, t.stable), global, true);

	last = term_index(t.index, term);

	return local_action;
}

void simulator::reset()
{
	for (int i = 0; i < (int)variables->nodes.size(); i++)
	{
		global.set(i, -1);
		encoding.set(i, -1);
	}
	loaded.clear();
	ready.clear();
	instability_errors.clear();
	interference_errors.clear();
	mutex_errors.clear();
	last = term_index();
}
}
