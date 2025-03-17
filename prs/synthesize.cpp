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
	for (int i = 0; i < (int)prs.nets.size(); i++) {
		// TODO(edward.bingham) we gotta figure out IO
		result.push(sch::Net(prs.nets[i].name, prs.nets[i].driver >= 0));
	}

	int minLength = 1;
	if (not tech.wires.empty()) {
		minLength = tech.getWidth(tech.wires[0].draw);
	}

	for (auto dev = prs.devs.begin(); dev != prs.devs.end(); dev++) {
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
		int other = tech.findModel(1-type, variant);
		if (other < 0) {
			other = tech.findModel(1-type, "svt");
		}

		float thisResist = tech.at(tech.models[model].diff).resistivity;
		float otherResist = thisResist;
		if (other >= 0) {
			otherResist = tech.at(tech.models[other].diff).resistivity;
		}

		float strength = dev->attr.size;
		if (thisResist > otherResist) {
			strength *= thisResist/otherResist;
		}

		int minWidth = tech.getWidth(tech.at(tech.models[model].diff).draw)*3;
		vec2i size(1.0,1.0);
		if (strength < 1.0) {
			size[0] = (int)ceil(((float)minLength)/strength);
			size[1] = minWidth;
		} else {
			size[0] = minLength;
			size[1] = (int)ceil(strength*(float)minWidth);
		}

		result.push(sch::Mos(tech, model, type, dev->drain, dev->gate, dev->source, prs.pwr[0][1-dev->threshold], size));
	}

	for (int i = 0; i < (int)prs.nets.size(); i++) {
		for (auto j = prs.nets[i].remote.begin(); j != prs.nets[i].remote.end(); j++) {
			if (i != *j) {
				result.connectRemote(i, *j);
			}
		}
	}

	if (report_progress) {
		int gateArea = 0;
		for (auto d = result.mos.begin(); d != result.mos.end(); d++) {
			gateArea += d->size[0]*d->size[1];
		}

		printf("[%s%lu NETS %lu TRANSISTORS %d DBUNIT2 GATE AREA%s]\t%gs\n", KGRN, result.nets.size(), result.mos.size(), gateArea, KNRM, tmr.since());
	}
	return result;
}

bool isNode(string name) {
	if (name.empty()) {
		return true;
	}

	if (name[0] != '_') {
		return false;
	}

	for (int i = 1; i < (int)name.size(); i++) {
		if (name[i] < '0' or name[i] > '9') {
			return false;
		}
	}

	return true;
}

production_rule_set extract_rules(const phy::Tech &tech, const sch::Subckt &ckt) {
	production_rule_set result;

	int minLength = 1;
	if (not tech.wires.empty()) {
		minLength = tech.getWidth(tech.wires[0].draw);
	}

	int vdd = std::numeric_limits<int>::max();
	int gnd = std::numeric_limits<int>::max();

	map<int, int> netmap;
	for (int i = 0; i < (int)ckt.nets.size(); i++) {
		int uid = result.nets.size();
		result.create(uid);
		if (ckt.nets[i].remoteIO) {
			result.nets[uid].isIO = true;
		}

		netmap.insert(pair<int, int>(i, uid));

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
	if (vdd == std::numeric_limits<int>::max()) {
		vdd = result.create(net("Vdd"));
	}
	if (gnd == std::numeric_limits<int>::max()) {
		gnd = result.create(net("GND"));
	}
	result.set_power(vdd, gnd);

	for (auto dev = ckt.mos.begin(); dev != ckt.mos.end(); dev++) {
		int minWidth = 1;
		if (dev->model >= 0 and dev->model < (int)tech.models.size()) {
			minWidth = tech.getWidth(tech.at(tech.models[dev->model].diff).draw)*3;
		}

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

		int threshold = dev->type == phy::Model::NMOS ? 1 : 0;
		int driver = dev->type == phy::Model::NMOS ? 0 : 1;

		attributes attr;
		attr.size = ((float)dev->size[1]/(float)minWidth)/((float)dev->size[0]/(float)minLength);
		attr.variant = "svt";
		if (dev->model >= 0 and dev->model < (int)tech.models.size()) {
			attr.variant = tech.models[dev->model].variant;
		}
		attr.weak = attr.size < 1.0;
		attr.force = attr.size > 10.0;

		result.add_mos(source, gate, drain, threshold, driver, attr);
	}

	result.normalize_source_drain();

	return result;
}


}

