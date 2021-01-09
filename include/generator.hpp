#pragma once
#include "pugixml.hpp"

#include <optional>

namespace vma_xml {
	std::optional<pugi::xml_document> generate(pugi::xml_document const &input);
}