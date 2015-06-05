/*
 * simulator.h
 *
 *  Created on: Jun 2, 2015
 *      Author: nbingham
 */

#include "production_rule.h"
#include <boolean/variable.h>
#include <common/standard.h>

#ifndef prs_simulator_h
#define prs_simulator_h

namespace prs
{
	struct instability
	{
		instability();
		instability(term_index effect, vector<term_index> cause);
		~instability();

		term_index effect;
		vector<term_index> cause;

		string to_string(const production_rule_set &base, const boolean::variable_set &v);
	};

	bool operator<(instability i, instability j);
	bool operator>(instability i, instability j);
	bool operator<=(instability i, instability j);
	bool operator>=(instability i, instability j);
	bool operator==(instability i, instability j);
	bool operator!=(instability i, instability j);

	struct interference : pair<term_index, term_index>
	{
		interference();
		interference(term_index first, term_index second);
		~interference();

		string to_string(const production_rule_set &base, const boolean::variable_set &v);
	};

	struct simulator
	{
		simulator();
		simulator(const production_rule_set *base, const boolean::variable_set *variables);
		~simulator();

		vector<instability> unstable;
		vector<interference> interfering;

		vector<term_index> ready;

		boolean::cube encoding;

		const production_rule_set *base;
		const boolean::variable_set *variables;

		int enabled();
		boolean::cube fire(int index);
	};
}

#endif
