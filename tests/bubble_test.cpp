#include <gtest/gtest.h>
#include <prs/production_rule.h>
#include <prs/bubble.h>
#include "test_helpers.h"
#include <interpret_prs/export.h>

using namespace prs;
using namespace test_helpers;

// Helper function to count the number of bubbles in the graph
size_t count_bubbles(const bubble& b) {
	size_t bubble_count = 0;
	for (auto& arc : b.net) {
		if (arc.bubble) {
			bubble_count++;
		}
	}
	return bubble_count;
}

// Helper function to count the number of isochronic arcs with bubbles
size_t count_isochronic_bubbles(const bubble& b) {
	size_t count = 0;
	for (auto& arc : b.net) {
		if (arc.isochronic && arc.bubble) {
			count++;
		}
	}
	return count;
}

bool is_inverted(const production_rule_set& prs, const std::string& signal_name) {
	for (auto net = prs.nets.begin(); net != prs.nets.end(); net++) {
		if (net->name == "_" + signal_name and (
			not net->gateOf[0].empty() or
			not net->gateOf[1].empty())) {
			return true;
		}
	}

	return false;
}

// Test a simple ring oscillator with no inversion needed
TEST(BubbleTest, NoInvertTest) {
	string prs_str = R"(
a->b-
~a->b+
b->c-
~b->c+
c->a+ // bubble from c -> a, non-isochronic
~c->a-
)";

	production_rule_set prs = parse_prs_string(prs_str);
	
	// Create a bubble object
	bubble b;
	
	// Load the PRS
	b.load_prs(prs);
	EXPECT_EQ(b.net.size(), 3U);
	
	// Apply the algorithm to detect cycles
	b.reshuffle();
	
	// No bubbles on isochronic forks should be present
	EXPECT_EQ(count_isochronic_bubbles(b), 0U);
	
	// Complete should not make any changes
	EXPECT_FALSE(b.complete());
	
	// Save the result and make sure no inversions were added
	production_rule_set result = prs;
	b.save_prs(&result);
	
	// No signals should be inverted
	EXPECT_FALSE(is_inverted(result, "a"));
	EXPECT_FALSE(is_inverted(result, "b"));
	EXPECT_TRUE(is_inverted(result, "c"));
}

// Test a complex circuit with isochronic forks and bubbles that need reshuffling
TEST(BubbleTest, EndToEndTest) {
	// This is a more complex production rule set representing a circuit with
	// isochronic forks and bubbles on those forks
	string prs_str = R"(
~reset & L.e & R.e & L.r & ~v1 -> R.r+
 reset | (v1 | ~L.e) & ~R.e -> R.r-
~reset & ~v2 & R.r -> v1+
 reset | v2 -> v1-
~reset & v1 & ~R.r -> v2+
 reset | ~L.e & ~R.r -> v2-
~reset & v2 & R.r -> L.e-
 reset | ~v2 & ~L.r -> L.e+
R.r -> R.e-
~R.r -> R.e+
L.e -> L.r+
~L.e -> L.r-
)";

	production_rule_set prs = parse_prs_string(prs_str);
	
	bubble b;
	b.load_prs(prs);

	EXPECT_EQ(b.net.size(), 18u);
	EXPECT_EQ(count_bubbles(b), 9u);
	EXPECT_EQ(count_isochronic_bubbles(b), 5u);
	
	// Apply reshuffling
	b.reshuffle();
	
	// After reshuffling, there should be no bubbles on isochronic forks
	EXPECT_EQ(count_isochronic_bubbles(b), 2u);
	
	// Apply optimization
	bool optimized = b.complete();
	EXPECT_FALSE(optimized);

	// Check that there are negative cycles (second element false)
	int negative_cycles = 0;
	for (const auto& cycle : b.cycles) {
		if (!cycle.second) {
			negative_cycles++;
		}
	}
	EXPECT_GT(negative_cycles, 0);

	// Save the result to get inversions
	production_rule_set result = prs;
	b.save_prs(&result);
	
	// Check which signals were globally inverted
	EXPECT_TRUE(is_inverted(result, "v1"));
	EXPECT_TRUE(is_inverted(result, "v2"));
	EXPECT_FALSE(is_inverted(result, "R.r"));
	EXPECT_TRUE(is_inverted(result, "R.e"));
	EXPECT_FALSE(is_inverted(result, "L.r"));
	EXPECT_TRUE(is_inverted(result, "L.e"));
	EXPECT_TRUE(is_inverted(result, "reset"));
}

