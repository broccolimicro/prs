#include "expression.h"
#include <parse_prs/expression.h>
#include <interpret_boolean/export.h>

namespace prs {

string emit_composition(boolean::cover expr, ucs::ConstNetlist nets) {
	return export_composition<parse_prs::composition>(expr, nets).to_string();
}

string emit_composition(boolean::cube expr, ucs::ConstNetlist nets) {
	return export_composition<parse_prs::composition>(expr, nets).to_string();
}

string emit_expression(boolean::cover expr, ucs::ConstNetlist nets) {
	return export_expression<parse_prs::expression>(expr, nets).to_string();
}

string emit_expression(boolean::cube expr, ucs::ConstNetlist nets) {
	return export_expression<parse_prs::expression>(expr, nets).to_string();
}

}
