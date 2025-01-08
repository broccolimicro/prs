#pragma once

#include "production_rule.h"
#include <sch/Subckt.h>

namespace prs {

sch::Subckt build_netlist(const phy::Tech &tech, const production_rule_set &prs, const ucs::variable_set &v, bool report_progress=false);
production_rule_set extract_rules(ucs::variable_set &v, const phy::Tech &tech, const sch::Subckt &ckt);

}

