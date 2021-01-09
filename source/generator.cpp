#include "generator.hpp"

#include <filesystem>
#include <iostream>
#include <string_view>

std::optional<pugi::xml_document> vma_xml::generate([[maybe_unused]] pugi::xml_document const &input) {
	auto output = std::make_optional<pugi::xml_document>();
	auto node = output->append_child("languages");

	auto english = node.append_child("english");
	english.append_attribute("locale").set_value("us");
	english.append_child(pugi::node_pcdata).set_value("true");

	node.append_child("日本語").append_child(pugi::node_pcdata).set_value("true");
	node.append_child("Español").append_child(pugi::node_pcdata).set_value("true");

	return std::move(output);
}

int main(int argc, char **argv) {
	std::string_view index_filename = "../doxygen_xml/index.xml";
	std::string_view output_filename = "../output/vma.xml";
	if (argc >= 2)
		index_filename = argv[1];
	if (argc >= 3)
		index_filename = argv[2];
	if (argc > 3)
		std::cout << "Warning: All the argument after the first two are ignored.\n";
	if (argc == 1)
		std::cout << "Warning: No paths provided. Using default values:\n"
			<< "input index: '" << index_filename << "'.\n"
			<< "output: '" << output_filename << "'.\n";

	pugi::xml_document input;
	if (auto loading_result = input.load_file(index_filename.data()); !loading_result)
		std::cout << "Error: Fail to load '" << std::filesystem::absolute(index_filename) << "': " 
			<< loading_result.description() << '\n';
	else if (auto output = vma_xml::generate(input); output) {
		std::filesystem::create_directory(std::filesystem::path(output_filename).parent_path());
		if (auto saving_result = output->save_file(output_filename.data()); !saving_result)
			std::cout << "Error: Unable to save to '"
				<< std::filesystem::absolute(output_filename) << "'.";
		else
			std::cout << "\nSuccess: " << std::filesystem::absolute(output_filename) << "\n";
	} else
		std::cout << "Error: Generation failed.";
	return 0;
}