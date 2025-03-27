#pragma once

#include <common/standard.h>
#include <boolean/cover.h>

#include <vector>
#include <array>

using namespace std;

namespace prs
{

// This library implements production rule sets (PRS) for asynchronous circuit modeling.
// It provides data structures and algorithms for representing, analyzing, and 
// manipulating CMOS circuits at a level between boolean logic and physical transistors.
//
// The model represents:
// - Nets (signals/wires) and their properties
// - Devices (transistors) and their connections
// - Power supplies and special nets
// - Remote connections for modeling timing regions and isochronic forks
//
// The implementation supports quasi-delay-insensitive (QDI) circuit analysis,
// boolean expression manipulation, transistor sizing, and circuit verification.

// Defines behavioral and physical attributes for devices
// Used to specify properties like weak/strong drivers, pass transistors,
// timing constraints, and physical sizing information
struct attributes {
	attributes();
	attributes(bool weak, bool pass=false, boolean::cube assume=1, uint64_t delay_max=10000);
	~attributes();

	bool weak;     // Whether this is a weak driver (e.g., for staticizers/keepers)
	bool force;    // Whether this is a very strong driver
	bool pass;     // Whether this is a pass transistor (can conduct in both directions)
	
	uint64_t delay_max;  // Maximum delay in picoseconds
	boolean::cover assume;  // Assumptions about circuit state before active

	// relative to minimum, values between 0 and 1 are made longer
	float size;    // Relative transistor size; values < 1 increase length
	string variant;  // Technology variant for this device

	void set_internal();

	static attributes instant();  // Creates attributes for zero-delay transitions
};

bool operator==(const attributes &a0, const attributes &a1);
bool operator!=(const attributes &a0, const attributes &a1);

// Represents a transistor in the circuit model
// Each device connects three nets (source, gate, drain) and has properties
// that define how it functions (threshold, driver value, attributes)
struct device {
	device();
	device(int source, int gate, int drain, int threshold, int driver, attributes attr=attributes());
	~device();

	// index into nets
	int source;
	int gate;
	int drain;
	
	int threshold; // Gate value that turns transistor on (1 for NMOS, 0 for PMOS)
	int driver;    // Value driven when on (0 for NMOS, 1 for PMOS)

	attributes attr;  // Additional behavioral and physical attributes
};

// Represents an electrical node/wire in the circuit
// Maintains references to all connected devices and remote connections
// Can represent inputs, outputs, power rails, or internal nodes
struct net {
	net(bool keep=false);
	net(string name, int region=0, bool keep=false, bool isIO=false);
	~net();

	// These arrays should include remote devices!
	// Check if the device is remote by comparing the net id against the relevant
	// gate, source, or drain id. If they are different, then the device is
	// remote and the transition should be delayed.
	string name;    // Net name (empty for internal nodes)
	int region;     // Isochronic region identifier for timing analysis

	// indexed by device::threshold
	array<vector<int>, 2> gateOf;    // Devices where this net connects to gate (by threshold)

	// indexed by device::driver
	array<vector<int>, 2> sourceOf;  // Devices where this net connects to source (by driver)
	array<vector<int>, 2> rsourceOf; // Special case for pass transistors
	array<vector<int>, 2> drainOf;   // Devices where this net connects to drain (by driver)

	vector<int> remote;  // Other nets electrically connected across region boundaries

	bool isIO;     // Whether this is an input/output net
	bool keep;     // Whether state should be preserved with keepers/staticizers
	int mirror;    // Complementary net (e.g., GND for VDD, vice versa)
	int driver;    // Constant driver value (-1 for non-power, 0 for GND, 1 for VDD)

	// Creates a remote connection to another net
	// Remote connections preserve separate net identities while sharing properties
	void add_remote(int uid);
	
	// Checks if this is an unnamed internal node
	bool isNode() const;
};

// Main container class for a production rule set circuit model
// Manages collections of nets and devices, implements circuit analysis
// and manipulation algorithms, and enforces circuit invariants
struct production_rule_set
{
	production_rule_set();
	~production_rule_set();

	string name;  // Name of the production rule set (usually circuit name)

	vector<array<int, 2> > pwr;  // Power supply pairs [GND, VDD]

	vector<device> devs;  // All transistors in the circuit
	// nets in this array should be ordered by uid
	vector<net> nets;     // All nets/nodes in the circuit

	// settings that control behavior
	bool assume_nobackflow; // (default false) nmos no longer drives weak 1 and pmos no longer drives weak 0
	bool assume_static;     // (default false) hold value at all named nodes

	// settings that control validation
	// all default to false
	bool require_driven;         // floating nodes not allowed if true
	bool require_stable;         // glitches not allowed if true
	bool require_noninterfering; // Vdd to GND shorts not allowed if true
	bool require_adiabatic;      // non-adiabatic transitions not allowed if true

	void print() const;

	int create(net n=net());

	int netIndex(string name, int region=0) const;
	int netIndex(string name, int region=0, bool define=false);
	pair<string, int> netAt(int uid) const;
	int netCount() const;

	vector<vector<int> > remote_groups() const;

	int sources(int net, int value) const;
	int drains(int net, int value) const;
	int sources(int net, int value, attributes attr) const;
	int drains(int net, int value, attributes attr) const;
	int sources(int net, int value, bool weak) const;
	int drains(int net, int value, bool weak) const;
	vector<attributes> attribute_groups(int net, int value) const;

	void set_power(int vdd, int gnd);
	void connect_remote(int n0, int n1);
	int connect(int n0, int n1);
	void replace(vector<int> &lst, int from, int to);
	void replace(map<int, int> &lst, int from, int to);
	int add_source(int gate, int drain, int threshold, int driver, attributes attr=attributes());
	int add_drain(int source, int gate, int threshold, int driver, attributes attr=attributes());
	void add_mos(int source, int gate, int drain, int threshold, int driver, attributes attr);
	int add(boolean::cube guard, int drain, int driver, attributes attr=attributes(), vector<int> order=vector<int>());
	int add_hfactor(boolean::cover guard, int drain, int driver, attributes attr=attributes(), vector<int> order=vector<int>());

	void add(int source, boolean::cover guard, int var, int val, attributes attr=attributes(), vector<int> order=vector<int>());
	void add(int source, boolean::cover guard, boolean::cover action, attributes attr=attributes(), vector<int> order=vector<int>());

	void move_gate(int dev, int net, int threshold=-1);
	void move_source_drain(int dev, int source, int drain, int driver=-1);
	void invert(int net);
	bool cmos_implementable();

	boolean::cover guard_of(int net, int driver, bool weak=false);

	bool has_inverter_after(int net, int &_net);
	void add_inverter_between(int net, int _net, attributes attr=attributes(), int vdd=std::numeric_limits<int>::max(), int gnd=std::numeric_limits<int>::max());
	int add_inverter_after(int net, attributes attr=attributes(), int vdd=std::numeric_limits<int>::max(), int gnd=std::numeric_limits<int>::max());
	array<int, 2> add_buffer_before(int net, attributes attr=attributes(), int vdd=std::numeric_limits<int>::max(), int gnd=std::numeric_limits<int>::max());
	void add_keepers(bool share=true, bool hcta=false, boolean::cover keep=1, bool report_progress=false);
	vector<bool> identify_weak_drivers();

	vector<vector<int> > size_with_stack_length();
	void size_devices(float ratio=0.1, bool report_progress=false);

	void swap_source_drain(int dev);
	void normalize_source_drain();
};

}

