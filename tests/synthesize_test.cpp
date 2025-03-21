#include <gtest/gtest.h>
#include <prs/synthesize.h>
#include "test_helpers.h"

#include <interpret_prs/export.h>

using namespace prs;
using namespace sch;
using namespace phy;
using namespace test_helpers;

TEST(SynthesizeTest, RoundTripTest) {
	string prs_str = R"(require driven, stable, noninterfering
@x&a&(b|c)->d-
@x&~a|~b&~c->d+
)";

	string target_prs_str = R"(@x&a&(b|c)->d-
@x&~a|~b&~c->d+
)";

	production_rule_set prs = parse_prs_string(prs_str);
	Tech tech = create_test_tech();

	string initial_str = export_production_rule_set(prs).to_string();
	EXPECT_EQ(initial_str, prs_str);

	// Build netlist
	Subckt ckt = build_netlist(tech, prs);

	// Extract rules back from the netlist
	production_rule_set extracted_prs = extract_rules(tech, ckt);
	string exported_str = export_production_rule_set(extracted_prs).to_string();

	EXPECT_EQ(exported_str, target_prs_str);
}

 
