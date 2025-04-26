#include "synthesize.h"
#include <cmath>
#include <common/timer.h>
#include <common/message.h>
#include <common/text.h>

namespace prs {

sch::Subckt build_netlist(const phy::Tech &tech, const production_rule_set &prs, bool report_progress) {
	if (report_progress) {
		printf("  %s...", prs.name.c_str());
		fflush(stdout);
	}
	
	Timer tmr;
	sch::Subckt result;
	result.name = prs.name;
	
	// Create all the nets in the subcircuit
	// Each net in the PRS maps to a corresponding net in the circuit
	for (int i = 0; i < (int)prs.nets.size(); i++) {
		// TODO(edward.bingham) we gotta figure out IO
		result.push(sch::Net(prs.nets[i].name.to_string(), prs.nets[i].driver >= 0));
	}

	// Get minimum length from technology constraints
	// This is used for proper transistor sizing based on the fabrication process
	int minLength = 1;
	if (not tech.wires.empty()) {
		minLength = tech.getWidth(tech.wires[0].draw);
	}

	// Create transistors for each device in the production rule set
	for (auto dev = prs.devs.begin(); dev != prs.devs.end(); dev++) {
		// Determine transistor type based on threshold
		// NMOS (threshold=1) or PMOS (threshold=0)
		int type = dev->threshold == 1 ? phy::Model::NMOS : phy::Model::PMOS;
		string variant = dev->attr.variant == "" ? "svt" : dev->attr.variant;
		int model = tech.findModel(type, variant);
		if (model < 0) {
			error("", std::string(dev->threshold == 1 ? "nmos" : "pmos") + " transistor variant " + variant + " not found.", __FILE__, __LINE__);
			continue;
		}

		// TODO(edward.bingham) this is a cheap trick for the PN ratio and it will
		// fall apart the moment someone uses different variants for the nmos and
		// pmos transistors. Really, I should understand the strength of every
		// condition in the sizing problem and size accordingly.

		// Find the corresponding transistor of opposite type for PN ratio calculations
		// This helps with sizing to ensure balanced pull-up and pull-down networks
		// PMOS typically needs to be larger than NMOS due to carrier mobility differences
		int other = tech.findModel(1-type, variant);
		if (other < 0) {
			other = tech.findModel(1-type, "svt");
		}

		// Calculate resistivity ratio for PN adjustments
		// Compensates for different transistor characteristics to achieve balanced timing
		float thisResist = tech.at(tech.models[model].diff).resistivity;
		float otherResist = thisResist;
		if (other >= 0) {
			otherResist = tech.at(tech.models[other].diff).resistivity;
		}

		// Adjust strength based on resistivity differences
		// Higher resistivity requires larger transistors to achieve the same current
		float strength = dev->attr.size;
		if (thisResist > otherResist) {
			strength *= thisResist/otherResist;
		}

		// Calculate transistor dimensions based on strength
		// Two ways to adjust transistor strength:
		// 1. Increase width (faster but uses more area)
		// 2. Decrease length (faster and smaller but may affect reliability)
		int minWidth = tech.getWidth(tech.at(tech.models[model].diff).draw)*3;
		vec2i size(1.0,1.0);
		if (strength < 1.0) {
			// For weaker transistors, decrease length to increase strength
			size[0] = (int)ceil(((float)minLength)/strength);
			size[1] = minWidth;
		} else {
			// For stronger transistors, increase width to increase strength
			size[0] = minLength;
			size[1] = (int)ceil(strength*(float)minWidth);
		}

		// Create and add the transistor to the subcircuit
		// Connect gate, drain, source, and body/substrate terminals
		result.push(sch::Mos(tech, model, type, dev->drain, dev->gate, dev->source, prs.pwr[0][1-dev->threshold], size));
	}

	// Establish remote connections between nets
	// This preserves the electrical connectivity between related signals
	for (int i = 0; i < (int)prs.nets.size(); i++) {
		for (auto j = prs.nets[i].remote.begin(); j != prs.nets[i].remote.end(); j++) {
			if (i != *j) {
				result.connectRemote(i, *j);
			}
		}
	}

	// Report statistics if requested
	// Provides insight into circuit complexity and area usage
	if (report_progress) {
		int gateArea = 0;
		for (auto d = result.mos.begin(); d != result.mos.end(); d++) {
			gateArea += d->size[0]*d->size[1];
		}

		printf("[%s%lu NETS %lu TRANSISTORS %d DBUNIT2 GATE AREA%s]\t%gs\n", KGRN, result.nets.size(), result.mos.size(), gateArea, KNRM, tmr.since());
	}
	return result;
}

// @brief Determines if a net name represents an internal node.
// 
// This utility function checks if a name follows the pattern for internal nodes
// (starting with underscore followed by digits). Internal nodes typically
// represent intermediate points in a circuit that don't directly correspond
// to signals in the behavioral description.
// 
// @param name The net name to check
// @return True if the name represents an internal node, false otherwise
bool isNode(string name) {
	// Empty names are considered nodes (default case)
	if (name.empty()) {
		return true;
	}

	// Internal nodes start with underscore
	if (name[0] != '_') {
		return false;
	}

	// All remaining characters must be digits
	for (int i = 1; i < (int)name.size(); i++) {
		if (name[i] < '0' or name[i] > '9') {
			return false;
		}
	}

	return true;
}

production_rule_set extract_rules(const phy::Tech &tech, const sch::Subckt &ckt) {
	production_rule_set result;

	// Get minimum length from technology constraints
	// Used for accurately interpreting transistor sizing
	int minLength = 1;
	if (not tech.wires.empty()) {
		minLength = tech.getWidth(tech.wires[0].draw);
	}

	// Initialize power net indices
	// We'll identify VDD and GND nets based on naming conventions
	int vdd = std::numeric_limits<int>::max();
	int gnd = std::numeric_limits<int>::max();

	// Map circuit nets to PRS nets
	// Each net in the circuit gets a corresponding net in the PRS
	map<int, int> netmap;
	for (int i = 0; i < (int)ckt.nets.size(); i++) {
		int uid = result.create(prs::net(ckt.nets[i].name));
		if (ckt.nets[i].remoteIO) {
			result.nets[uid].isIO = true;
		}

		netmap.insert(pair<int, int>(i, uid));

		// Identify power nets based on naming conventions
		// VDD, GND, and VSS are common power net names
		string lname = lower(ckt.nets[i].name);
		if (lname.find("weak") == string::npos) {
			if (lname.find("vdd") != string::npos) {
				vdd = uid;
			} else if (lname.find("gnd") != string::npos
				or lname.find("vss") != string::npos) {
				gnd = uid;
			}
		}
	}
	
	// Create power nets if not found
	// Every circuit needs power and ground, so we create them if they weren't detected
	if (vdd == std::numeric_limits<int>::max()) {
		vdd = result.create(net("Vdd"));
	}
	if (gnd == std::numeric_limits<int>::max()) {
		gnd = result.create(net("GND"));
	}
	result.set_power(vdd, gnd);

	// Process each transistor in the circuit
	// Extract its connectivity and properties to build the PRS
	for (auto dev = ckt.mos.begin(); dev != ckt.mos.end(); dev++) {
		// Calculate minimum width for transistor sizing
		// This is used to determine the relative strength of transistors
		int minWidth = 1;
		if (dev->model >= 0 and dev->model < (int)tech.models.size()) {
			minWidth = tech.getWidth(tech.at(tech.models[dev->model].diff).draw)*3;
		}

		// Map transistor connections to PRS nets
		// Find or create appropriate net indices for gate, source, and drain
		int gate = result.nets.size();
		auto pos = netmap.find(dev->gate);
		if (pos != netmap.end()) {
			gate = pos->second;
		}

		int source = result.nets.size();
		pos = netmap.find(dev->source);
		if (pos != netmap.end()) {
			source = pos->second;
		}

		int drain = result.nets.size();
		pos = netmap.find(dev->drain);
		if (pos != netmap.end()) {
			drain = pos->second;
		}

		// Determine threshold and driver values based on transistor type
		// NMOS pulls down (driver=0) when gate is high (threshold=1)
		// PMOS pulls up (driver=1) when gate is low (threshold=0)
		int threshold = dev->type == phy::Model::NMOS ? 1 : 0;
		int driver = dev->type == phy::Model::NMOS ? 0 : 1;

		// Calculate attributes including size, variant, and strength indicators
		// Size is determined by width/length ratio relative to minimum dimensions
		attributes attr;
		attr.size = ((float)dev->size[1]/(float)minWidth)/((float)dev->size[0]/(float)minLength);
		attr.variant = "svt";
		if (dev->model >= 0 and dev->model < (int)tech.models.size()) {
			attr.variant = tech.models[dev->model].variant;
		}
		
		// Classify transistors as weak or forced based on their relative size
		// This helps preserve design intent when reconstructing the PRS
		attr.weak = attr.size < 1.0;
		attr.force = attr.size > 10.0;

		// Add the device to the PRS
		// This creates the logical representation of the transistor's behavior
		result.add_mos(source, gate, drain, threshold, driver, attr);
	}

	// Normalize source and drain connections for consistency
	// This ensures that the PRS follows standard conventions
	result.normalize_source_drain();

	for (auto dev = result.devs.begin(); dev != result.devs.end(); dev++) {
		if (result.nets[dev->drain].isNode()) {
			dev->attr.delay_max = 0;
		}
	}

	return result;
}


}

