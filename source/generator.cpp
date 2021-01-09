#include "generator.hpp"

#include <iostream>
#include <string_view>
using namespace std::literals;

std::optional<pugi::xml_document> vma_xml::detail::load_xml(std::filesystem::path const &file) {
	auto output = std::make_optional<pugi::xml_document>();
	if (auto result = output->load_file(file.c_str()); result)
		return output;
	else
		std::cout << "Error: Fail to load '" << std::filesystem::absolute(file) << "': "
			<< result.description() << '\n';
	return std::nullopt;
}
std::optional<vma_xml::detail::variable_t> vma_xml::detail::parse_variable(pugi::xml_node const &xml) {
	if (auto name = xml.child("name"), type = xml.child("type"); name && type)
		if (auto child = type.first_child(); child.type() == pugi::xml_node_type::node_pcdata)
			return std::make_optional<vma_xml::detail::variable_t>(name.child_value(), type.child_value());
		else if (child.name() == "ref"sv)
			return std::make_optional<vma_xml::detail::variable_t>(name.child_value(), child.child_value());
		else
			std::cout << "Warning: Ignore a variable with unknown type: '" << name.child_value() << "'.\n";
		return std::nullopt;
}

bool vma_xml::detail::parse_struct(pugi::xml_node const &xml, detail::data_t &data) {
	struct_t output;
	for (auto &child : xml.children()) {
		if (child.name() == "compoundname"sv)
			output.name = child.child_value();
		else if (child.name() == "sectiondef"sv)
			for (auto &member : child.children())
				if (member.name() == "memberdef"sv)
					if (auto kind = member.attribute("kind").value(); kind == "variable"sv) {
						if (auto variable = parse_variable(member); variable)
							output.variables.emplace_back(*variable);
					} else
						std::cout << "Warning: Ignore a struct member: '" << kind 
							<< "'. Only variables are supported.\n";
				else
					std::cout << "Warning: Ignore unknown struct member: '" << member.name() << "'.\n";
	}

	if (output.name != "") {
		data.structs.emplace_back(std::move(output));
		return true;
	} else
		std::cout << "Warning: Ignore a struct compound without a name.\n";
	return false;
}
bool vma_xml::detail::parse_file(pugi::xml_node const &, detail::data_t &) {
	return false;
}

bool vma_xml::detail::parse_compound(std::string_view refid, std::filesystem::path const &directory, 
									 detail::data_t &data) {
	std::filesystem::path file_path = directory; (file_path /= refid.data()) += ".xml";
	if (auto compound_xml = detail::load_xml(file_path); compound_xml)
		if (auto doxygen = compound_xml->child("doxygen"); doxygen)
			if (auto compound = doxygen.child("compounddef"); compound)
				if (std::string_view kind = compound.attribute("kind").value(); kind == "struct"sv)
					return parse_struct(compound, data);
				else if (kind == "file"sv)
					return parse_file(compound, data);
				else
					std::cout << "Warning: Ignore a compound of an unknown kind: '" << kind << "'.\n";
	return false;
}

std::optional<vma_xml::detail::data_t> vma_xml::parse(std::filesystem::path const &directory) {
	if (auto index_xml = detail::load_xml(directory.string() + "/index.xml"); index_xml)
		if (auto index = index_xml->child("doxygenindex"); index) {
			detail::data_t output;
			for (auto const &compound : index.children())
				if (compound.name() == "compound"sv)
					if (auto kind = compound.attribute("kind").value(); kind == "struct"sv || kind == "file"sv)
						detail::parse_compound(compound.attribute("refid").value(), directory, output);
					else if (kind == "page"sv || kind == "dir"sv) {
						// Ignore 'page' and 'dir' index entries.
					} else
						std::cout << "Warning: Ignore a compound of an unknown kind: '" << kind << "'.\n";
				else
					std::cout << "Warning: Ignore an unknown node: " << compound.name() << '\n';
			return output;
		}
	return std::nullopt;
}

std::optional<pugi::xml_document> vma_xml::generate([[maybe_unused]] detail::data_t const &data) {
	auto output = std::make_optional<pugi::xml_document>();
	auto node = output->append_child("languages");

	auto english = node.append_child("english");
	english.append_attribute("locale").set_value("us");
	english.append_child(pugi::node_pcdata).set_value("true");

	node.append_child("日本語").append_child(pugi::node_pcdata).set_value("true");
	node.append_child("Español").append_child(pugi::node_pcdata).set_value("true");

	return std::move(output);
}

#ifndef VMA_XML_NO_MAIN
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
		std::filesystem::path output_file = output_path; output_file /= "vma.xml";
		if (auto result = output->save_file(output_file.c_str()); result)
			std::cout << "\nSuccess: " << std::filesystem::absolute(output_file) << "\n";
		else
			std::cout << "Error: Unable to save '" << std::filesystem::absolute(output_file) << "'.";
	} else
		std::cout << "Error: Generation failed.";
	return 0;
}
#endif

//vma_xml::detail::variable_t vma_xml::parse_variable(pugi::xml_node const &input, std::string_view refid) {
//	return vma_xml::detail::variable_t {
//		.name = name_only(input),
//		.refid = refid
//	};
//}
//std::optional<vma_xml::detail::struct_t> vma_xml::detail::parse_struct(pugi::xml_node const &input, 
//																	   std::string_view refid) {
//	if (auto index_xml = load_xml(directory.string() + "/index.xml"); index_xml)
//		if (auto index = index_xml->child("doxygenindex"); index) {
//	
//	vma_xml::detail::struct_t output; output.refid = refid;
//	for (auto const &child : input.children())
//		if (child.name() == "name"sv)
//			output.name = child.child_value();
//		else if (child.name() == "member"sv)
//			if (auto entity = parse_attributes(child); entity.kind == "variable"sv)
//				output.variables.emplace_back(parse_variable(child, entity.refid));
//			else
//				std::cout << "Warning: Ignore a struct member: '" 
//					<< entity.kind << "'. Only variables are supported.\n";
//		else
//			std::cout << "Warning: Ignore an unknown child: " << child.name() << ".\n";
//	return output;
//}

//vma_xml::detail::define_t vma_xml::parse_define(pugi::xml_node const &input, std::string_view refid) {
//	return vma_xml::detail::define_t {
//		.name = name_only(input),
//		.refid = refid
//	};
//}
//vma_xml::detail::enum_t vma_xml::parse_enum(pugi::xml_node const &input, std::string_view refid) {
//	return vma_xml::detail::enum_t{
//		.name = name_only(input),
//		.refid = refid
//	};
//}
//vma_xml::detail::enum_value_t vma_xml::parse_enumvalue(pugi::xml_node const &input, std::string_view refid) {
//	return vma_xml::detail::enum_value_t {
//		.name = name_only(input),
//		.refid = refid
//	};
//}
//vma_xml::detail::typedef_t vma_xml::parse_typedef(pugi::xml_node const &input, std::string_view refid) {
//	return vma_xml::detail::typedef_t {
//		.name = name_only(input),
//		.refid = refid
//	};
//}
//vma_xml::detail::function_t vma_xml::parse_function(pugi::xml_node const &input, std::string_view refid) {
//	return vma_xml::detail::function_t{
//		.name = name_only(input),
//		.refid = refid
//	};
//}
//vma_xml::detail::file_t vma_xml::parse_file(pugi::xml_node const &input, std::string_view refid) {
//	detail::file_t output; output.refid = refid;
//	for (auto const &child : input.children())
//		if (child.name() == "name"sv)
//			output.name = child.child_value();
//		else if (child.name() == "member"sv)
//			if (auto entity = parse_attributes(child); entity.kind == "define"sv)
//				output.defines.emplace_back(parse_define(child, entity.refid));
//			else if (entity.kind == "enum"sv)
//				output.enums.emplace_back(parse_enum(child, entity.refid));
//			else if (entity.kind == "enumvalue"sv)
//				if (!output.enums.empty())
//					output.enums.back().values.emplace_back(
//						parse_enumvalue(child, entity.refid)
//					);
//				else
//					std::cout << "Warning: Ignore enumvalue: no enums were defined.";
//			else if (entity.kind == "typedef"sv)
//				output.typedefs.emplace_back(parse_typedef(child, entity.refid));
//			else if (entity.kind == "function"sv)
//				output.functions.emplace_back(parse_function(child, entity.refid));
//			else
//				std::cout << "Warning: Ignore an unknown file member: '"
//					<< entity.kind << "'.\n";
//		else
//			std::cout << "Warning: Ignore an unknown child: " << child.name() << ".\n";
//	return output;
//}
