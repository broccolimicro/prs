#include "helpers.h"

namespace test {

prs::production_rule_set parse_prs_string(const std::string &prs_str) {
	prs::production_rule_set prs;
	
	tokenizer tokens;
	tokens.register_token<parse::block_comment>(false);
	tokens.register_token<parse::line_comment>(false);
	parse_prs::register_syntax(tokens);
	
	// Insert the string into the tokenizer
	tokens.insert("string_input", prs_str, nullptr);
	
	tokens.increment(false);
	parse_prs::expect(tokens);
	if (tokens.decrement(__FILE__, __LINE__)) {
		parse_prs::production_rule_set syntax(tokens);
		import_production_rule_set(syntax, prs, -1, -1, prs::attributes(), 0, &tokens, true);
	}
	
	return prs;
}

phy::Tech create_test_tech() {
    // Create a minimal Tech structure sufficient for PRS testing
    phy::Tech tech("test_tech", "test");
    
    // Basic settings
    tech.dbunit = 1.0;
    tech.scale = 1.0;
    
    // Add some basic paint layers
    int boundary_layer = (int)tech.paint.size();
    tech.paint.push_back(phy::Paint("boundary", 0, 0));
    tech.boundary = boundary_layer;
    
    // Add active diffusion layers for NMOS and PMOS
    int nactive_idx = (int)tech.paint.size();
    tech.paint.push_back(phy::Paint("nactive", 1, 0));
    
    int pactive_idx = (int)tech.paint.size();
    tech.paint.push_back(phy::Paint("pactive", 2, 0));
    
    // Add poly layer
    int poly_idx = (int)tech.paint.size();
    tech.paint.push_back(phy::Paint("poly", 3, 0));
    
    // Add metal layers
    int m1_idx = (int)tech.paint.size();
    tech.paint.push_back(phy::Paint("m1", 4, 0));
    
    int m2_idx = (int)tech.paint.size();
    tech.paint.push_back(phy::Paint("m2", 5, 0));
    
    // Add substrate (diffusion) layers
    int nsubst_idx = (int)tech.subst.size();
    tech.subst.push_back(phy::Substrate(nactive_idx));
    
    int psubst_idx = (int)tech.subst.size();
    tech.subst.push_back(phy::Substrate(pactive_idx));
    
    // Add transistor models
    phy::Level nmos_diff(phy::Level::SUBST, nsubst_idx);
    tech.models.push_back(phy::Model(phy::Model::NMOS, "svt", "nmos", nmos_diff));
    
    phy::Level pmos_diff(phy::Level::SUBST, psubst_idx);
    tech.models.push_back(phy::Model(phy::Model::PMOS, "svt", "pmos", pmos_diff));
    
    // Add routing layers
    tech.wires.push_back(phy::Routing(poly_idx, poly_idx, poly_idx));
    tech.wires.push_back(phy::Routing(m1_idx, m1_idx, m1_idx));
    tech.wires.push_back(phy::Routing(m2_idx, m2_idx, m2_idx));
    
    // Create levels for routing layers
    phy::Level poly_level(phy::Level::ROUTE, 0);
    phy::Level m1_level(phy::Level::ROUTE, 1);
    phy::Level m2_level(phy::Level::ROUTE, 2);
    
    // Add vias between routing layers
    tech.vias.push_back(phy::Via(poly_level, m1_level, 6)); // Create a via layer for poly to m1
    tech.vias.push_back(phy::Via(m1_level, m2_level, 7));   // Create a via layer for m1 to m2
    
    // Add minimal DRC rules
    tech.setWidth(poly_idx, 200);  // Minimum poly width
    tech.setWidth(m1_idx, 300);    // Minimum metal1 width
    tech.setWidth(m2_idx, 300);    // Minimum metal2 width
    
    // Add spacing rules
    tech.setSpacing(poly_idx, poly_idx, 200);  // Poly to poly spacing
    tech.setSpacing(m1_idx, m1_idx, 300);      // Metal1 to metal1 spacing
    tech.setSpacing(m2_idx, m2_idx, 300);      // Metal2 to metal2 spacing
    
    return tech;
}

}
