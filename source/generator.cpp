#include "tinyxml2.h"
namespace xml = tinyxml2;

int main([[maybe_unused]] int argc, [[maybe_unused]] char **argv) {
	xml::XMLDocument document;
	document.LoadFile("../doxygen_xml/index.xml");

	return 0;
}