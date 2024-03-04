#pragma once

#include "std_alias.h"
#include "hir.h"
#include <memory>
#include <fstream>
#include <optional>

namespace Lb::parser {
	using namespace std_alias;

	void parse_file(char *fileName, Opt<std::string> parse_tree_output);
}
