#pragma once

#include "production_rule.h"
#include <sch/Subckt.h>

namespace prs {

sch::Subckt build_netlist(const phy::Tech &tech, const production_rule_set &prs, const ucs::variable_set &v);

}

