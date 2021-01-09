#include "generator.hpp"

#include <iostream>
#include <string_view>
using namespace std::literals;

std::optional<pugi::xml_document> vma_xml::load_xml(std::filesystem::path const &file) {
	auto output = std::make_optional<pugi::xml_document>();
	if (auto result = output->load_file(file.c_str()); result)
		return output;
	else
		std::cout << "Error: Fail to load '" << std::filesystem::absolute(file) << "': "
			<< result.description() << '\n';
	return std::nullopt;
}
vma_xml::detail::entity_t vma_xml::parse_attributes(pugi::xml_node const &input) {
	vma_xml::detail::entity_t output;
	for (auto const &attribute : input.attributes())
		if (attribute.name() == "kind"sv)
			output.kind = attribute.value();
		else if (attribute.name() == "refid"sv)
			output.refid = attribute.value();
		else
			std::cout << "Warning: Ignore an unknown attribute: " << attribute.name()
				<< "=\"" << attribute.value() << "\".\n";
	return output;
}
std::string_view name_only(pugi::xml_node const &input) {
	std::string_view output;
	for (auto const &child : input.children())
		if (child.name() == "name"sv)
			output = child.child_value();
		else
			std::cout << "Warning: Ignore an unknown child: " << child.name() << ".\n";
	return output;
}

vma_xml::detail::variable_t vma_xml::parse_variable(pugi::xml_node const &input, std::string_view refid) {
	return vma_xml::detail::variable_t {
		.name = name_only(input),
		.refid = refid
	};
}
vma_xml::detail::struct_t vma_xml::parse_struct(pugi::xml_node const &input, std::string_view refid) {
	vma_xml::detail::struct_t output; output.refid = refid;
	for (auto const &child : input.children())
		if (child.name() == "name"sv)
			output.name = child.child_value();
		else if (child.name() == "member"sv)
			if (auto entity = parse_attributes(child); entity.kind == "variable"sv)
				output.variables.emplace_back(parse_variable(child, entity.refid));
			else
				std::cout << "Warning: Ignore a struct member: '" 
					<< entity.kind << "'. Only variables are supported.\n";
		else
			std::cout << "Warning: Ignore an unknown child: " << child.name() << ".\n";
	return output;
}

vma_xml::detail::define_t vma_xml::parse_define(pugi::xml_node const &input, std::string_view refid) {
	return vma_xml::detail::define_t {
		.name = name_only(input),
		.refid = refid
	};
}
vma_xml::detail::enum_t vma_xml::parse_enum(pugi::xml_node const &input, std::string_view refid) {
	return vma_xml::detail::enum_t{
		.name = name_only(input),
		.refid = refid
	};
}
vma_xml::detail::enum_value_t vma_xml::parse_enumvalue(pugi::xml_node const &input, std::string_view refid) {
	return vma_xml::detail::enum_value_t {
		.name = name_only(input),
		.refid = refid
	};
}
vma_xml::detail::typedef_t vma_xml::parse_typedef(pugi::xml_node const &input, std::string_view refid) {
	return vma_xml::detail::typedef_t {
		.name = name_only(input),
		.refid = refid
	};
}
vma_xml::detail::function_t vma_xml::parse_function(pugi::xml_node const &input, std::string_view refid) {
	return vma_xml::detail::function_t{
		.name = name_only(input),
		.refid = refid
	};
}
vma_xml::detail::file_t vma_xml::parse_file(pugi::xml_node const &input, std::string_view refid) {
	detail::file_t output; output.refid = refid;
	for (auto const &child : input.children())
		if (child.name() == "name"sv)
			output.name = child.child_value();
		else if (child.name() == "member"sv)
			if (auto entity = parse_attributes(child); entity.kind == "define"sv)
				output.defines.emplace_back(parse_define(child, entity.refid));
			else if (entity.kind == "enum"sv)
				output.enums.emplace_back(parse_enum(child, entity.refid));
			else if (entity.kind == "enumvalue"sv)
				if (!output.enums.empty())
					output.enums.back().values.emplace_back(
						parse_enumvalue(child, entity.refid)
					);
				else
					std::cout << "Warning: Ignore enumvalue: no enums were defined.";
			else if (entity.kind == "typedef"sv)
				output.typedefs.emplace_back(parse_typedef(child, entity.refid));
			else if (entity.kind == "function"sv)
				output.functions.emplace_back(parse_function(child, entity.refid));
			else
				std::cout << "Warning: Ignore an unknown file member: '"
					<< entity.kind << "'.\n";
		else
			std::cout << "Warning: Ignore an unknown child: " << child.name() << ".\n";
	return output;
}

std::optional<vma_xml::detail::data_t> vma_xml::parse_index(pugi::xml_document const &index_file) {
	if (auto index = index_file.child("doxygenindex"); index) {
		detail::data_t output;
		for (auto const &compound : index.children())
			if (compound.name() == "compound"sv)
				if (auto entity = parse_attributes(compound); entity.kind == "struct"sv)
					output.structs.emplace_back(parse_struct(compound, entity.refid));
				else if (entity.kind == "file"sv)
					output.files.emplace_back(parse_file(compound, entity.refid));
				else if (entity.kind == "page"sv || entity.kind == "dir"sv) {
					// Ignore 'page' and 'dir' index entries.
				} else
					std::cout << "Warning: Ignore a compound with an unknown kind: '" 
						<< entity.kind << "'.\n";
			else
				std::cout << "Warning: Ignore unknown node: " << compound.name() << '\n';
		return output;
	} else
		std::cout << "Error: Fail to locate 'doxygenindex' node in the provided input file.";
	return std::nullopt;
}

std::optional<pugi::xml_document> vma_xml::generate(std::filesystem::path const &directory) {
	if (auto index_xml = load_xml(directory.string() + "/index.xml"); index_xml) {
		auto index_data = parse_index(*index_xml);


		auto output = std::make_optional<pugi::xml_document>();
		auto node = output->append_child("languages");

		auto english = node.append_child("english");
		english.append_attribute("locale").set_value("us");
		english.append_child(pugi::node_pcdata).set_value("true");

		node.append_child("日本語").append_child(pugi::node_pcdata).set_value("true");
		node.append_child("Español").append_child(pugi::node_pcdata).set_value("true");

		return std::move(output);
	}
	return std::nullopt;
}

int main(int argc, char **argv) {
	std::string_view input_path = "../doxygen_xml/";
	std::string_view output_path = "../output/";
	if (argc >= 2)
		input_path = argv[1];
	if (argc >= 3)
		output_path = argv[2];
	if (argc > 3)
		std::cout << "Warning: All the argument after the first two are ignored.\n";
	if (argc == 1)
		std::cout << "Warning: No paths provided. Using default values:\n"
			<< "input index: '" << input_path << "'.\n"
			<< "output: '" << output_path << "'.\n";

	if (auto output = vma_xml::generate(input_path); output) {
		std::filesystem::create_directory(output_path);
		std::filesystem::path output_file = output_path; output_file /= "xma.xml";
		if (auto result = output->save_file(output_file.c_str()); result)
			std::cout << "\nSuccess: " << std::filesystem::absolute(output_file) << "\n";
		else
			std::cout << "Error: Unable to save '" << std::filesystem::absolute(output_file) << "'.";
	} else
		std::cout << "Error: Generation failed.";
	return 0;
}