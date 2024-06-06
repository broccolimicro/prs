#pragma once

#include "production_rule.h"
#include "netlist.h"

namespace prs {

netlist build_netlist(const production_rule_set &prs);

}

