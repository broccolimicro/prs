#pragma once

#include <parse/parse.h>
#include <parse/default/block_comment.h>
#include <parse/default/line_comment.h>
#include <parse/default/new_line.h>

#include <phy/Tech.h>
#include <prs/production_rule.h>
#include <parse_prs/factory.h>
#include <interpret_prs/import.h>

namespace test {

// Helper function to parse a production rule set from a string
prs::production_rule_set parse_prs_string(const std::string &prs_str);

// Helper function to create a minimal Tech structure for testing PRS
phy::Tech create_test_tech();

}
