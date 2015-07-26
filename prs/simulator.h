/*
 * simulator.h
 *
 *  Created on: Jun 2, 2015
 *      Author: nbingham
 */

#include "production_rule.h"
#include <ucs/variable.h>
#include <common/standard.h>

#ifndef prs_simulator_h
#define prs_simulator_h

namespace prs
{
	struct enabled_rule : term_index
	{
		vector<term_index> history;
		boolean::cube local_action;
		boolean::cube remote_action;
		boolean::cube guard;
		bool stable;

		string to_string(const production_rule_set &base, const ucs::variable_set &v);
	};

	struct instability : enabled_rule
	{
		instability();
		instability(const enabled_rule &cause);
		~instability();

		string to_string(const production_rule_set &base, const ucs::variable_set &v);
	};

	struct interference : pair<enabled_rule, enabled_rule>
	{
		interference();
		interference(const enabled_rule &first, const enabled_rule &second);
		~interference();

		string to_string(const production_rule_set &base, const ucs::variable_set &v);
	};

	struct mutex : pair<enabled_rule, enabled_rule>
	{
		mutex();
		mutex(const enabled_rule &first, const enabled_rule &second);
		~mutex();

		string to_string(const production_rule_set &base, const ucs::variable_set &v);
	};

	struct simulator
	{
		simulator();
		simulator(const production_rule_set *base, const ucs::variable_set *variables);
		~simulator();

		vector<instability> instability_errors;
		vector<interference> interference_errors;
		vector<mutex> mutex_errors;

		vector<enabled_rule> ready;
		vector<enabled_rule> firing;
		term_index last;

		boolean::cube encoding;
		boolean::cube global;

		const production_rule_set *base;
		const ucs::variable_set *variables;

		int enabled();
		boolean::cube fire(int index);

		void reset();
	};
}

#endif
