#pragma once

#include "production_rule.h"
#include <sch/Subckt.h>

namespace prs {

sch::Subckt build_netlist(const phy::Tech &tech, const production_rule_set &prs, bool report_progress=false);
production_rule_set extract_rules(const phy::Tech &tech, const sch::Subckt &ckt);

}

