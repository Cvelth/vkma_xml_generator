// Copyright (c) 2021 Cvelth <cvelth.mail@gmail.com>
// SPDX-License-Identifier: MIT

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

std::string to_string(pugi::xml_node const &xml) {
	std::string output;
	for (auto &child : xml.children())
		if (child.type() == pugi::xml_node_type::node_pcdata)
			output += child.value();
		else if (child.name() == "ref"sv)
			output += child.child_value();
		else
			std::cout << "Warning: Ignore an unknown tag: '" << child.name() << "'.\n";
	return output;
}

std::optional<vma_xml::detail::variable_t> vma_xml::detail::parse_variable(pugi::xml_node const &xml) {
	if (auto name = xml.child("name"), type = xml.child("type"); name && type)
		return std::make_optional<variable_t>(to_string(name), to_string(type));
	return std::nullopt;
}
std::optional<vma_xml::detail::define_t> vma_xml::detail::parse_define(pugi::xml_node const &xml) {
	if (auto name = xml.child("name"), value = xml.child("initializer"); name && value)
		return std::make_optional<define_t>(to_string(name), to_string(value));
	return std::nullopt;
}
std::optional<vma_xml::detail::enum_value_t> vma_xml::detail::parse_enum_value(pugi::xml_node const &xml) {
	if (auto name = xml.child("name"), value = xml.child("initializer"); name && value)
		if (auto value_str = to_string(value); std::string_view(value_str).substr(0, 2) == "= ")
			return std::make_optional<enum_value_t>(to_string(name), value_str.substr(2));
		else
			return std::make_optional<enum_value_t>(to_string(name), value_str);
	return std::nullopt;
}
std::optional<vma_xml::detail::enum_t> vma_xml::detail::parse_enum(pugi::xml_node const &xml) {
	enum_t output;
	for (auto &child : xml.children())
		if (child.name() == "type"sv)
			if (auto string = to_string(child); string != "")
				output.type = string;
			else
				output.type = std::nullopt;
		else if (child.name() == "name"sv)
			output.name = to_string(child);
		else if (child.name() == "enumvalue"sv)
			if (auto value = parse_enum_value(child); value)
				output.values.emplace_back(*value);
	if (output.name != "")
		return output;
	else
		return std::nullopt;
}
std::optional<vma_xml::detail::typedef_t> vma_xml::detail::parse_typedef(pugi::xml_node const &xml) {
	if (auto name = xml.child("name"), type = xml.child("type"), args = xml.child("argsstring"); name && type && args)
		return std::make_optional<typedef_t>(to_string(name), to_string(type) + to_string(args));
	return std::nullopt;
}
std::optional<vma_xml::detail::variable_t> vma_xml::detail::parse_function_parameter(pugi::xml_node const &xml) {
	if (auto name = xml.child("declname"), type = xml.child("type"); name && type)
		return std::make_optional<variable_t>(to_string(name), to_string(type));
	return std::nullopt;
}
std::optional<vma_xml::detail::function_t> vma_xml::detail::parse_function(pugi::xml_node const &xml) {
	function_t output;
	for (auto &child : xml.children())
		if (child.name() == "type"sv)
			output.return_type = to_string(child);
		else if (child.name() == "name"sv)
			output.name = to_string(child);
		else if (child.name() == "param"sv)
			if (auto parameter = parse_function_parameter(child); parameter)
				output.parameters.emplace_back(*parameter);
	if (output.name != "" && output.return_type != "")
		return output;
	else
		return std::nullopt;
}

bool vma_xml::detail::parse_struct(pugi::xml_node const &xml, detail::data_t &data) {
	struct_t output;
	for (auto &child : xml.children())
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

	if (output.name != "") {
		data.structs.emplace_back(std::move(output));
		return true;
	} else
		std::cout << "Warning: Ignore a struct compound without a name.\n";
	return false;
}
bool vma_xml::detail::parse_file(pugi::xml_node const &xml, detail::data_t &data) {
	for (auto &child : xml.children())
		if (child.name() == "sectiondef"sv)
			for (auto &member : child.children())
				if (member.name() == "memberdef"sv)
					if (auto kind = member.attribute("kind").value(); kind == "define"sv) {
						if (auto define = parse_define(member); define)
							data.defines.emplace_back(*define);
					} else if (kind == "enum"sv) {
						if (auto enumeration = parse_enum(member); enumeration)
							data.enums.emplace_back(*enumeration);
					} else if (kind == "typedef"sv) {
						if (auto type_def = parse_typedef(member); type_def)
							data.typedefs.emplace_back(*type_def);
					} else if (kind == "function"sv) {
						if (auto function = parse_function(member); function)
							data.functions.emplace_back(*function);
					}

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
	auto registry = output->append_child("registry");

	registry.append_child("comment").append_child(pugi::node_pcdata).set_value(
		"DO NOT MODIFY MANUALLY!"
	);
	registry.append_child("comment").append_child(pugi::node_pcdata).set_value(
		"This file was generated using [generator](https://github.com/Cvelth/vma_xml_generator)."
	);
	registry.append_child("comment").append_child(pugi::node_pcdata).set_value(
		"Generated files are licensed under [The Unlicense](https://unlicense.org)."
	);
	registry.append_child("comment").append_child(pugi::node_pcdata).set_value(
		"The generator itself is licensed under [MIT License](https://www.mit.edu/~amini/LICENSE.md)."
	);
	registry.append_child("comment").append_child(pugi::node_pcdata).set_value(
		"SPDX-License-Identifier: Unlicense."
	);
	registry.append_child("comment").append_child(pugi::node_pcdata).set_value(
		""
	);
	registry.append_child("comment").append_child(pugi::node_pcdata).set_value(
		"This file was generated from xml 'doxygen' documentation for "
		"[vk_mem_alloc.h (VulkanMemoryAllocator)](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/blob/master/src/vk_mem_alloc.h) "
		"header."
	);
	registry.append_child("comment").append_child(pugi::node_pcdata).set_value(
		"It is intended to be used as [vulkan-hpp](https://github.com/KhronosGroup/Vulkan-Hpp) generator input."
	);
	registry.append_child("comment").append_child(pugi::node_pcdata).set_value(
		"The goal is to generate a [vulkan.hpp](https://github.com/KhronosGroup/Vulkan-Hpp/blob/master/vulkan/vulkan.hpp) "
		"compatible header - an improved c++ interface."
	);

	auto types = registry.append_child("types");
	for (auto &define : data.defines) {
		auto type = types.append_child("type");
		type.append_attribute("category").set_value("define");
		type.append_child(pugi::node_pcdata).set_value("#define ");
		type.append_child("name").append_child(pugi::node_pcdata).set_value(define.name.data());
		type.append_child(pugi::node_pcdata).set_value((" " + define.value).data());
	}

	types.append_child("comment").append_child(pugi::node_pcdata).set_value(
		"Bitmask types"
	);
	for (auto &type_def : data.typedefs)
		if (type_def.type == "VkFlags") {
			auto type = types.append_child("type");
			type.append_attribute("category").set_value("bitmask");
			type.append_child(pugi::node_pcdata).set_value("typedef ");
			type.append_child("type").append_child(pugi::node_pcdata).set_value("VkFlags");
			type.append_child(pugi::node_pcdata).set_value(" ");
			type.append_child("name").append_child(pugi::node_pcdata).set_value(type_def.name.data());
			type.append_child(pugi::node_pcdata).set_value(";");
		}


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