#pragma once

#include "production_rule.h"
#include <sch/Subckt.h>

// @brief Provides functionality to convert abstract logical descriptions (PRS) to physical circuit
// implementations (spice netlist) and vice versa.
namespace prs {

// @brief Synthesizes a spice netlist from a production rule set.
// 
// 1. Creates nets for each signal in the PRS
// 2. For each device in the PRS, creates an appropriate transistor with:
//    - Proper type (NMOS/PMOS) based on threshold
//    - Sizing based on specified strength and technology characteristics
//    - Connections to appropriate nets (gate, drain, source, substrate)
// 3. Establishes remote connections between nets
// 
// @param tech Technology specifications used for transistor sizing and properties
// @param prs Production rule set describing the circuit's logical behavior
// @param report_progress Whether to output progress information during synthesis
// @return Subcircuit containing the complete transistor-level implementation
sch::Subckt build_netlist(const phy::Tech &tech, const production_rule_set &prs, bool report_progress=false);

// @brief Extracts a production rule set from a transistor-level netlist.
// 
// This function analyzes a physical circuit implementation to derive the production rules
// that describe its behavior. It implements the backward path in the synthesis process for
// circuit analysis, verification, and reverse engineering.
// 
// 1. Creates a new production rule set
// 2. Maps circuit nets to corresponding nets in the PRS
// 3. Identifies power (VDD) and ground (GND) nets
// 4. For each transistor, extracts connection information and properties
// 5. Adds appropriate devices to the PRS with attributes preserved
// 6. Normalizes for consistency
// 
// @param tech Technology specifications used for transistor property interpretation
// @param ckt The transistor-level netlist to analyze
// @return Production rule set describing the logical behavior of the circuit
production_rule_set extract_rules(const phy::Tech &tech, const sch::Subckt &ckt);

}

