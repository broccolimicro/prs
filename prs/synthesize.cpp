#include "synthesize.h"
#include <cmath>
#include <interpret_ucs/export.h>

namespace prs {

sch::Subckt build_netlist(const phy::Tech &tech, const production_rule_set &prs, const ucs::variable_set &v) {
	sch::Subckt result(tech);

	result.name = "top";
	for (int i = 0; i < (int)prs.nets.size(); i++) {
		// TODO(edward.bingham) we gotta figure out IO
		result.nets.push_back(sch::Net(export_variable_name(i, v).to_string(), prs.nets[i].driver >= 0));
	}

	for (int i = -1; i >= -(int)prs.nodes.size(); i--) {
		result.nets.push_back(sch::Net(export_variable_name(i, v).to_string(), false));
	}

	int minLength = tech.paint[tech.wires[0].draw].minWidth;

	for (auto dev = prs.devs.begin(); dev != prs.devs.end(); dev++) {
		int model = 0;
		for (model = 0; model < (int)tech.models.size(); model++) {
			auto m = tech.models.begin()+model;
			if (((m->type == phy::Model::NMOS and dev->threshold == 1)
					or (m->type == phy::Model::PMOS and dev->threshold == 0))
				and ((dev->attr.variant == "" and m->name.find("svt") != string::npos) or (
					m->name.find(dev->attr.variant) != string::npos))) {
				break;
			}
		}
		if (model >= (int)tech.models.size()) {
			error("", std::string(dev->threshold == 1 ? "nmos" : "pmos") + " transistor variant " + (dev->attr.variant == "" ? "svt" : dev->attr.variant) + " not found.", __FILE__, __LINE__);
			continue;
		}
		int type = tech.models[model].type;
		int minWidth = tech.paint[tech.models[model].paint[0].draw].minWidth*3;

		result.mos.push_back(sch::Mos(model, type));
		result.mos.back().gate = prs.uid(dev->gate);
		result.mos.back().ports.push_back(prs.uid(dev->source));
		result.mos.back().ports.push_back(prs.uid(dev->drain));
		result.mos.back().bulk = prs.pwr[0][1-dev->threshold];
		if (dev->attr.size < 1.0) {
			result.mos.back().size[0] = (int)ceil(((float)minLength)/dev->attr.size);
			result.mos.back().size[1] = minWidth;
		} else {
			result.mos.back().size[0] = minLength;
			result.mos.back().size[1] = (int)ceil(dev->attr.size*(float)minWidth);
		}
	}
	return result;
}

}

