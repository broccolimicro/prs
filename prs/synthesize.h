#pragma once

#include "production_rule.h"
#include <sch/Subckt.h>

namespace prs {

sch::Subckt build_netlist(const production_rule_set &prs);

}

