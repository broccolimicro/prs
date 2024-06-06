#pragma once

#include <common/standard.h>
#include <boolean/cover.h>
#include <ucs/variable.h>

namespace prs
{

struct net {
	// index into ucs::variables_set::nodes
	int id;

	// index into netlist::devs
	vector<int> ports;
	vector<int> precharge; 
};

struct device {
	// index into ucs::variables_set::nodes
	int gate;

	bool invert;
	float size; // in nanometers

	// index into netlist::nets
	int source;
	int drain;
};

// This describes a graph of transistors between different nets
struct netlist {
	vector<net> nets;
	vector<device> devs;

	void build_transistor_sizing();
	void build_gate_sizing();
};

}
