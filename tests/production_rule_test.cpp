#include <gtest/gtest.h>
#include <prs/production_rule.h>
#include "test_helpers.h"
#include <sstream>
#include <numeric>
#include <algorithm>

#include <interpret_prs/export.h>

using namespace prs;
using namespace test_helpers;
using namespace std;

TEST(ProductionRuleTest, BasicParsingTest) {
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

@R01 & ~v01 -> R.i+ // nokeep
@R01 & v00 -> R.i- // nokeep
v01 -> R.i- // nokeep

)";

	string target_str = R"(require driven, stable, noninterfering
v01|@R01&v00->R.i-
@R01&~v01->R.i+
v0->L.f'1-
~v0->L.f'1+
v1->L.t'1-
~v1->L.t'1+
R.f|R.t->L.e-
~R.t&~R.f->L.e+
v2->R.f-
~v2->R.f+
v3->R.t-
~v3->R.t+
R.f'1|R.t'1->R.e'1-
~R.t'1&~R.f'1->R.e'1+
_Reset&v0&L.e'1->v1- {v0}
~_Reset|~v0|~L.e'1->v1+
_Reset&v1&L.e'1->v0- {v1}
~_Reset|~v1|~L.e'1->v0+
_Reset&L.f&R.e->v2- [keep]
~_Reset|~L.f&~R.e->v2+ [keep]
_Reset&L.t&R.e->v3- [keep]
~_Reset|~L.t&~R.e->v3+ [keep]
)";

	production_rule_set prs = parse_prs_string(prs_str);
	
	// Check that requirements are correctly set
	EXPECT_TRUE(prs.require_driven);
	EXPECT_TRUE(prs.require_stable);
	EXPECT_TRUE(prs.require_noninterfering);
	EXPECT_FALSE(prs.require_adiabatic);
	EXPECT_FALSE(prs.assume_nobackflow);
	EXPECT_FALSE(prs.assume_static);

	// Re-export the production rule set and compare to original
	parse_prs::production_rule_set exported_prs = export_production_rule_set(prs);
	string exported_str = exported_prs.to_string();
	
	EXPECT_EQ(target_str, exported_str);
	
	// Check if the structures match
	EXPECT_EQ(exported_prs.require.size(), 3u);
	EXPECT_EQ(exported_prs.assume.size(), 0u);
	EXPECT_TRUE(find(exported_prs.require.begin(), exported_prs.require.end(), "driven") != exported_prs.require.end());
	EXPECT_TRUE(find(exported_prs.require.begin(), exported_prs.require.end(), "stable") != exported_prs.require.end());
	EXPECT_TRUE(find(exported_prs.require.begin(), exported_prs.require.end(), "noninterfering") != exported_prs.require.end());
}

TEST(ProductionRuleTest, GuardExtractionTest) {
	string prs_str = R"(
a&(b|c)->v0-
~a|~b&~c->v0+
)";

	production_rule_set prs = parse_prs_string(prs_str);
	
	// Find v1 net
	int v1 = prs.netIndex("v0");
	int a = prs.netIndex("a");
	int b = prs.netIndex("b");
	int c = prs.netIndex("c");

	ASSERT_GE(v1, 0);
	ASSERT_GE(a, 0);
	ASSERT_GE(b, 0);
	ASSERT_GE(c, 0);
	
	boolean::cover pull_down = boolean::cover(a, 1) & (boolean::cover(b, 1) | boolean::cover(c, 1));
	boolean::cover pull_up = boolean::cover(a, 0) | (boolean::cover(b, 0) & boolean::cover(c, 0));
	
	EXPECT_EQ(prs.guard_of(v1, 0), pull_down);
	EXPECT_EQ(prs.guard_of(v1, 1), pull_up);
}

TEST(ProductionRuleTest, AddInverterTest) {
	production_rule_set prs;
	
	// Create nets
	int in_net = prs.create(net("in"));
	int out_net = prs.create(net("out"));
	int vdd = prs.create(net("vdd"));
	int gnd = prs.create(net("gnd"));
	
	// Set power nets
	prs.set_power(vdd, gnd);
	
	// Add inverter
	prs.add_inverter_between(in_net, out_net);
	
	// Verify that devices were created
	EXPECT_EQ(prs.devs.size(), 2u);
	
	EXPECT_EQ(prs.guard_of(out_net, 0), boolean::cover(in_net, 1));
	EXPECT_EQ(prs.guard_of(out_net, 1), boolean::cover(in_net, 0));
} 
