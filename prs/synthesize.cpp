#include "synthesize.h"

namespace prs {

sch::Subckt build_netlist(const production_rule_set &prs) {
	// TODO Circuit and Netlist Synthesis
	//
	// The goal of this project is to generate a spice netlist from the
	// production rule set. With this spice netlist, we'll be able to divide the
	// netlist up into gates and then run those gates through Floret for cell
	// generation.
	//
	// The following steps are guidelines and not hard rules. If you think you
	// found a better way to approach the problem, then feel free to chase that
	// down. If you need supporting infrastructure anywhere else in the project,
	// feel free to add that in. If you need to modify this function definition,
	// go for it.
	//
	// 1. Export production rule set to a spice process
	// 2. Automatically determine transistor sizing using logical effort
	// 3. Expand upon this to implement optimal QDI transistor sizing
	// 4. Update production rules to fold transistors
	// 5. Determine optimal folding based on state space
	// 6. Automatically break a production rule set into cells for layout
	//
	// === Successful completion of project ===
	// 
	// Your time is yours, what do you want to tackle next?
	// Some ideas:
	// 1. Use state graph to determine optimal transistor length to minimize
	//    leakage current while maintaining performance.
	//
	// Final cleanup:
	// 1. Clean up any bugs
	// 2. Prepare demo
	// 3. Document as needed
}

}

