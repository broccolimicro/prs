#pragma once

#include <common/standard.h>
#include <boolean/cover.h>

#include <vector>
#include <array>

using namespace std;

namespace prs
{

struct attributes {
	attributes();
	attributes(bool weak, bool pass=false, boolean::cube assume=1, uint64_t delay_max=10000);
	~attributes();

	bool weak;
	bool force;
	bool pass;
	
	uint64_t delay_max;
	boolean::cover assume;

	// relative to minimum, values between 0 and 1 are made longer
	float size;
	string variant;

	void set_internal();

	static attributes instant();
};

bool operator==(const attributes &a0, const attributes &a1);
bool operator!=(const attributes &a0, const attributes &a1);

struct device {
	device();
	device(int source, int gate, int drain, int threshold, int driver, attributes attr=attributes());
	~device();

	// index into nets
	int source;
	int gate;
	int drain;
	
	int threshold;
	int driver;

	attributes attr;
};

struct net {
	net(bool keep=false);
	net(string name, int region=0, bool keep=false, bool isIO=false);
	~net();

	// These arrays should include remote devices!
	// Check if the device is remote by comparing the net id against the relevant
	// gate, source, or drain id. If they are different, then the device is
	// remote and the transition should be delayed.
	string name;
	int region;

	// indexed by device::threshold
	array<vector<int>, 2> gateOf;

	// indexed by device::driver
	array<vector<int>, 2> sourceOf;
	array<vector<int>, 2> rsourceOf;
	array<vector<int>, 2> drainOf;

	vector<int> remote;

	bool isIO;
	bool keep;
	int mirror;
	int driver;

	void add_remote(int uid);
	bool isNode() const;
};

struct production_rule_set
{
	production_rule_set();
	~production_rule_set();

	string name;

	vector<array<int, 2> > pwr;

	vector<device> devs;
	// nets in this array should be ordered by uid
	vector<net> nets;

	// settings that control behavior
	bool assume_nobackflow; // (default false) nmos no longer drives weak 1 and pmos no longer drives weak 0
	bool assume_static;     // (default false) hold value at all named nodes

	// settings that control validation
	// all default to false
	bool require_driven;         // floating nodes not allowed if true
	bool require_stable;         // glitches not allowed if true
	bool require_noninterfering; // Vdd to GND shorts not allowed if true
	bool require_adiabatic;      // non-adiabatic transitions not allowed if true

	void print();

	int create(net n=net());

	int netIndex(string name, int region=0) const;
	int netIndex(string name, int region=0, bool define=false);
	pair<string, int> netAt(int uid) const;

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

