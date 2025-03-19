#include <gtest/gtest.h>
#include <prs/production_rule.h>
#include <prs/simulator.h>
#include "test_helpers.h"

using namespace prs;
using namespace test_helpers;

TEST(SimulatorTest, SimpleInverterSimulation) {
	string prs_str = R"(
in->out-
~in->out+
)";

	production_rule_set prs = parse_prs_string(prs_str);
	
	simulator sim(&prs);
	
	// Find the indices of 'in' and 'out' nets
	int in_idx = prs.netIndex("in");
	int out_idx = prs.netIndex("out");
	
	ASSERT_GE(in_idx, 0);
	ASSERT_GE(out_idx, 0);
	
	// Reset simulation state
	sim.reset();
	
	// Verify initial state (undefined/unknown)
	// 2 means undriven or unknown
	EXPECT_EQ(sim.encoding.get(out_idx), -1);
	
	// Set input to 1 - automatic evaluation happens
	sim.set(in_idx, 1, 1);

	while (not sim.enabled.empty()) {
		sim.fire();
	}
	
	// Verify output is 0
	EXPECT_EQ(sim.encoding.get(out_idx), 0);
	
	// Set input to 0 - automatic evaluation happens
	sim.set(in_idx, 0, 1);

	while (not sim.enabled.empty()) {
		sim.fire();
	}
	
	// Verify output is 1
	EXPECT_EQ(sim.encoding.get(out_idx), 1);
}

TEST(SimulatorTest, ResetSimulation) {
	string prs_str = R"(
require driven, stable, noninterfering
_Reset&L.t&R.e->v3- [keep]
~_Reset|~L.t&~R.e->v3+ [keep]
_Reset&L.f&R.e->v2- [keep]
~_Reset|~L.f&~R.e->v2+ [keep]
_Reset&v0&L.e'1->v1- {v0}
~_Reset|~v0|~L.e'1->v1+
_Reset&v1&L.e'1->v0- {v1}
~_Reset|~v1|~L.e'1->v0+
R.f'1|R.t'1->R.e'1-
~R.t'1&~R.f'1->R.e'1+
v3->R.t-
~v3->R.t+
v2->R.f-
~v2->R.f+
R.f|R.t->L.e-
~R.t&~R.f->L.e+
v1->L.t'1-
~v1->L.t'1+
v0->L.f'1-
~v0->L.f'1+
)";

	production_rule_set prs = parse_prs_string(prs_str);
	
	simulator sim(&prs);
	
	// Find the index of '_Reset' net
	int reset_idx = prs.netIndex("_Reset");
	int v0_idx = prs.netIndex("v0");
		int v1_idx = prs.netIndex("v1");
	
	ASSERT_GE(reset_idx, 0);
	ASSERT_GE(v0_idx, 0);
	ASSERT_GE(v1_idx, 0);
	
	// Reset the simulation
	sim.reset();
   
	EXPECT_EQ(sim.encoding.get(reset_idx), 0);
 
	// Run the simulation to reach a stable state
	while (not sim.enabled.empty()) {
		//printf("step\n");
		//for (int i = 0; i < (int)sim.nets.size(); i++) {
		//	if (sim.nets[i] != nullptr) {
		//		printf("(%d) %s\n", i, sim.nets[i]->value.to_string(&prs).c_str());
		//	}
		//}

		sim.fire();
		//auto e = sim.fire(); 
		//printf("firing %lu\t%s\n\n", e.fire_at, e.to_string(&prs).c_str());
	}
	
	// Reset should propagate through the circuit
	// Check a few key nets to verify behavior
	
	// After reset, both values should be 1
	EXPECT_EQ(sim.encoding.get(v0_idx), 1);
	EXPECT_EQ(sim.encoding.get(v1_idx), 1);
}

TEST(SimulatorTest, EventSchedulingTest) {
	string prs_str = R"(
a->b- [after=100]
~a->b+ [after=200]
)";

	production_rule_set prs = parse_prs_string(prs_str);
	
	// Add power nets
	int vdd = prs.create(net("vdd"));
	int gnd = prs.create(net("gnd"));
	prs.set_power(vdd, gnd);
	
	simulator sim(&prs);
	
	// Find the indices of 'a' and 'b' nets
	int a_idx = prs.netIndex("a");
	int b_idx = prs.netIndex("b");
	
	ASSERT_GE(a_idx, 0);
	ASSERT_GE(b_idx, 0);
	
	// Reset simulation state
	sim.reset();

	// Propagate reset	
	uint64_t now = sim.enabled.now;
	while (not sim.enabled.empty()) {
		sim.fire();
	}

	sim.run();
	
	// Get initial state of b
	int v0 = sim.encoding.get(b_idx);
	
	// Set 'a' to 1
	sim.set(a_idx, 1);
	
	// Verify that setting a to 1 does not change b
	int v1 = sim.encoding.get(b_idx);
	EXPECT_EQ(v0, v1);

	// Propagate the event	
	now = sim.enabled.now;
	while (not sim.enabled.empty()) {
		sim.fire();
	}
	EXPECT_GE(sim.enabled.now, now);
	EXPECT_LE(sim.enabled.now, now+100);

	// Verify that the transition has propagated to b
	v0 = sim.encoding.get(b_idx);
	EXPECT_EQ(v0, 0);
	
	// Set input to 0
	sim.set(a_idx, 0);
	
	// Verify that setting a to 0 does not immediately change b
	v1 = sim.encoding.get(b_idx);
	EXPECT_EQ(v0, v1);

	// Propagate the event	
	now = sim.enabled.now;
	while (not sim.enabled.empty()) {
		sim.fire();
	}
	EXPECT_GE(sim.enabled.now, now);
	EXPECT_LE(sim.enabled.now, now+200);

	// Verify that the transition has propagated to b
	v0 = sim.encoding.get(b_idx);
	EXPECT_EQ(v0, 1);
}

TEST(SimulatorTest, SignalStrengthsTest) {
	// Test that verifies the model() function with various signal strength combinations
	string prs_str = R"(
	// Strong pullup/pulldown
	a->c-
	
	// Weak pullup/pulldown 
	b->c- [weak]
	~b->c+ [weak]
	
	// Another output
	a->d-
	~a->d+
	)";

	production_rule_set prs = parse_prs_string(prs_str);
	
	// Add power nets
	int vdd = prs.create(net("vdd"));
	int gnd = prs.create(net("gnd"));
	prs.set_power(vdd, gnd);
	
	simulator sim(&prs);
	
	// Find net indices
	int a_idx = prs.netIndex("a");
	int b_idx = prs.netIndex("b");
	int c_idx = prs.netIndex("c");
	int d_idx = prs.netIndex("d");
	
	ASSERT_GE(a_idx, 0);
	ASSERT_GE(b_idx, 0);
	ASSERT_GE(c_idx, 0);
	ASSERT_GE(d_idx, 0);
	
	// Reset simulation
	sim.reset();
	
	// Test case 1: Strong driver wins over weak driver
	// Set 'a' to 1 (strong) - this activates a->c- rule
	sim.set(a_idx, 1, 3);
	
	// Set 'b' to 0 (weak) - this activates ~b->c+ rule
	sim.set(b_idx, 0, 1);

	while (not sim.enabled.empty()) {
		sim.fire();
	}
	
	// Strong 'a' should drive 'c' to 0 despite weak 'b' trying to pull it to 1
	EXPECT_EQ(sim.encoding.get(c_idx), 0);
	
	// Test case 2: Weak driver when no strong driver
	// Set 'a' to a fully defined value first to establish a known state
	sim.set(a_idx, 0, 3);

	while (not sim.enabled.empty()) {
		sim.fire();
	}
	
	// Verify that d follows a (a=0 should make d=1 via ~a->d+)
	EXPECT_EQ(sim.encoding.get(d_idx), 1);
	
	// Now set 'a' low (0)
	sim.set(a_idx, 0, 3);

	while (not sim.enabled.empty()) {
		sim.fire();
	}

	// When 'a' is low, weak 'b' should drive 'c'
	// Since b=0, ~b->c+ [weak] rule activates, pulling 'c' to 1
	EXPECT_EQ(sim.encoding.get(c_idx), 1);
	
	// For 'd', the behavior is less predictable when 'a' is unknown/undriven.
	// The simulator might evaluate ~a->d+ differently when a=2 (unknown)
	// We just verify it's in a valid state (-1, 0, or 1)
	// -1 is a valid result as unknowns often lead to interference/undefined values
	EXPECT_EQ(sim.encoding.get(d_idx), 1);
	
	// Test case 3: Verify strength is stored correctly using the strength boolean cube
	// Note: In strength boolean cube, values are stored as (2-strength)
	// So power (3) = 2-3 = -1, normal (2) = 2-2 = 0, weak (1) = 2-1 = 1, undriven (0) = 2-0 = 2
	EXPECT_EQ(sim.strength.get(c_idx), 1);
}

TEST(SimulatorTest, RaceConditionTest) {
	// Test that verifies the fire() method with events that nearly coincide
	string prs_str = R"(
	// Two paths to drive out with different delays
	a->x- [after=10]
	~a->x+ [after=15]
	
	a->y- [after=12]
	~a->y+ [after=13]
	
	x&y->out- [after=5]
	~x|~y->out+ [after=5]
	)";

	production_rule_set prs = parse_prs_string(prs_str);
	
	// Add power nets
	int vdd = prs.create(net("vdd"));
	int gnd = prs.create(net("gnd"));
	prs.set_power(vdd, gnd);
	
	simulator sim(&prs);
	
	// Find net indices
	int a_idx = prs.netIndex("a");
	int x_idx = prs.netIndex("x");
	int y_idx = prs.netIndex("y");
	int out_idx = prs.netIndex("out");
	
	ASSERT_GE(a_idx, 0);
	ASSERT_GE(x_idx, 0);
	ASSERT_GE(y_idx, 0);
	ASSERT_GE(out_idx, 0);
	
	// Reset simulation
	sim.reset();
	
	// Set initial values to ensure a known state
	// Initially set x and y to 1 (so out will be driven by x&y->out-)
	sim.set(x_idx, 1, 3);
	sim.set(y_idx, 1, 3);

	while (not sim.enabled.empty()) {
		sim.fire();
	}
	
	// Verify initial state
	EXPECT_EQ(sim.encoding.get(x_idx), 1);
	EXPECT_EQ(sim.encoding.get(y_idx), 1);
	EXPECT_EQ(sim.encoding.get(out_idx), 0);
	
	// Set input 'a' to 1 (which should eventually make x=0, y=0, out=1)
	sim.set(a_idx, 1, 3);
	
	// Run the simulation - all events should fire in the correct order
	while (not sim.enabled.empty()) {
		sim.fire();
	}
	
	// x and y should both be 0 after a=1 propagates
	EXPECT_EQ(sim.encoding.get(x_idx), 0);
	EXPECT_EQ(sim.encoding.get(y_idx), 0);
	
	// out should be 1 since ~x|~y=1|1=1, activating ~x|~y->out+
	EXPECT_EQ(sim.encoding.get(out_idx), 1);
	
	// Now switch 'a' to 0 (creating a new series of events)
	sim.set(a_idx, 0, 3);
	
	// Run simulation event by event, checking state
	while (not sim.enabled.empty()) {
		sim.fire();
	}
	
	// Final state: a=0 should make x=1, y=1, and out=0 (since x&y->out-)
	EXPECT_EQ(sim.encoding.get(x_idx), 1);
	EXPECT_EQ(sim.encoding.get(y_idx), 1);
	EXPECT_EQ(sim.encoding.get(out_idx), 0);
}

TEST(SimulatorTest, TimingPropagationTest) {
	// Test that verifies the evaluate() method with complex timing chains
	string prs_str = R"(
	a->b- [after=10]
	~a->b+ [after=20]
	
	b->c- [after=15]
	~b->c+ [after=25]
	
	c->d- [after=5]
	~c->d+ [after=10]
	
	d->e- [after=10]
	~d->e+ [after=5]
	)";

	production_rule_set prs = parse_prs_string(prs_str);
	
	// Add power nets
	int vdd = prs.create(net("vdd"));
	int gnd = prs.create(net("gnd"));
	prs.set_power(vdd, gnd);
	
	simulator sim(&prs);
	
	// Find net indices
	int a_idx = prs.netIndex("a");
	int b_idx = prs.netIndex("b");
	int c_idx = prs.netIndex("c");
	int d_idx = prs.netIndex("d");
	int e_idx = prs.netIndex("e");
	
	ASSERT_GE(a_idx, 0);
	ASSERT_GE(b_idx, 0);
	ASSERT_GE(c_idx, 0);
	ASSERT_GE(d_idx, 0);
	ASSERT_GE(e_idx, 0);
	
	// Reset simulation and initialize signals to known values
	sim.reset();
	
	// Initialize signals to ensure predictable starting point
	sim.set(a_idx, 0, 3);
	sim.set(b_idx, 1, 3);
	sim.set(c_idx, 0, 3);
	sim.set(d_idx, 1, 3);
	sim.set(e_idx, 0, 3);

	while (not sim.enabled.empty()) {
		sim.fire();
	}
	
	// Verify initial state
	EXPECT_EQ(sim.encoding.get(a_idx), 0);
	EXPECT_EQ(sim.encoding.get(b_idx), 1);
	EXPECT_EQ(sim.encoding.get(c_idx), 0);
	EXPECT_EQ(sim.encoding.get(d_idx), 1);
	EXPECT_EQ(sim.encoding.get(e_idx), 0);
	
	// Set 'a' to 1
	sim.set(a_idx, 1, 3);
	
	// Record event times to verify timing
	vector<int> order;
	
	// Track events as they fire
	uint64_t start = sim.enabled.now;
	while (!sim.enabled.empty()) {
		enabled_transition t = sim.fire();
		order.push_back(t.net);
	}
	EXPECT_LE(sim.enabled.now-start, 45u);
	EXPECT_EQ(order, vector<int>({b_idx, c_idx, d_idx, e_idx}));

	EXPECT_EQ(sim.encoding.get(a_idx), 1);
	EXPECT_EQ(sim.encoding.get(b_idx), 0);  // a->b-
	EXPECT_EQ(sim.encoding.get(c_idx), 1);  // ~b->c+
	EXPECT_EQ(sim.encoding.get(d_idx), 0);  // c->d-
	EXPECT_EQ(sim.encoding.get(e_idx), 1);  // ~d->e+
	
	// Now, for the a=0 transition path, we should explicitly set a=0 and ensure other signals have specific starting values
	sim.set(a_idx, 0, 3);
	
	// Clear and reload transition times
	order.clear();
	
	// Track events as they fire
	start = sim.enabled.now;
	while (!sim.enabled.empty()) {
		enabled_transition t = sim.fire();
		order.push_back(t.net);
	}

	EXPECT_LE(sim.enabled.now-start, 55u);
	EXPECT_EQ(order, vector<int>({b_idx, c_idx, d_idx, e_idx}));

	// Verify final state - all signals should have propagated correctly based on a=0
	EXPECT_EQ(sim.encoding.get(a_idx), 0);
	EXPECT_EQ(sim.encoding.get(b_idx), 1);  // ~a->b+
	EXPECT_EQ(sim.encoding.get(c_idx), 0);  // b->c-
	EXPECT_EQ(sim.encoding.get(d_idx), 1);  // ~c->d+
	EXPECT_EQ(sim.encoding.get(e_idx), 0);  // d->e-
}

TEST(SimulatorTest, MultiDriverResolutionTest) {
	// Test that verifies how the simulator resolves multiple drivers on the same net
	string prs_str = R"(
	// Multiple drivers for out
	a->out-
	~a->out+
	
	b->out-
	~b->out+
	)";

	production_rule_set prs = parse_prs_string(prs_str);
	
	// Add power nets
	int vdd = prs.create(net("vdd"));
	int gnd = prs.create(net("gnd"));
	prs.set_power(vdd, gnd);
	
	simulator sim(&prs);
	
	// Find net indices
	int a_idx = prs.netIndex("a");
	int b_idx = prs.netIndex("b");
	int out_idx = prs.netIndex("out");
	
	ASSERT_GE(a_idx, 0);
	ASSERT_GE(b_idx, 0);
	ASSERT_GE(out_idx, 0);
	
	// Reset simulation
	sim.reset();
	
	// Test case 1: Non-interfering drivers (both want the same value)
	sim.set(a_idx, 1, 3);
	sim.set(b_idx, 1, 3);

	while (!sim.enabled.empty()) {
		sim.fire();
	}
	
	// Both drivers want out=0, so it should be 0
	EXPECT_EQ(sim.encoding.get(out_idx), 0);
	
	// Test case 2: Interfering drivers (conflict)
	sim.set(a_idx, 1, 3);
	sim.set(b_idx, 0, 3);

	while (!sim.enabled.empty()) {
		sim.fire();
	}

	// Conflicting drivers: a wants out=0, b wants out=1
	// The result should be -1 (unstable/interference)
	EXPECT_EQ(sim.encoding.get(out_idx), -1);
	
	// Test case 3: should recover from interference
	sim.set(a_idx, 1, 3);
	sim.set(b_idx, 1, 3);

	while (!sim.enabled.empty()) {
		sim.fire();
	}
	
	// Both drivers want out=0, so it should be 0
	EXPECT_EQ(sim.encoding.get(out_idx), 0);
}
