/*
 * production_rule.h
 *
 *  Created on: Feb 2, 2015
 *      Author: nbingham
 */

#include <common/standard.h>
#include <boolean/cover.h>

#ifndef prs_production_rule_h
#define prs_production_rule_h

namespace prs
{
struct production_rule
{
	production_rule();
	~production_rule();

	boolean::cover guard;
	boolean::cover action;
};

struct production_rule_set
{
	production_rule_set();
	~production_rule_set();

	vector<production_rule> rules;
};

}

#endif
