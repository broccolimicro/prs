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
	struct enabled_rule
	{
		enabled_rule();
		~enabled_rule();

		int index;
		vector<term_index> history;
		boolean::cube guard_action;
		vector<int> mutex;
		bool vacuous;
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

	struct interference : pair<term_index, term_index>
	{
		interference();
		interference(const term_index &first, const term_index &second);
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

		vector<enabled_rule> loaded;
		vector<pair<int, int> > ready;
		term_index last;

		boolean::cube encoding;
		boolean::cube global;

		const production_rule_set *base;
		const ucs::variable_set *variables;

		int enabled();
		boolean::cube fire(int index);

		void reset();
		void wait();
		void run();
	};
}

#endif
