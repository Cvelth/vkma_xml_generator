// Copyright (c) 2021 Cvelth <cvelth.mail@gmail.com>
// SPDX-License-Identifier: MIT

#include "generator.hpp"

#include <chrono>
#include <iostream>
#include <set>
#include <vector>
#include <string_view>
using namespace std::literals;

std::optional<pugi::xml_document> vkma_xml::detail::load_xml(std::filesystem::path const &file) {
	auto output = std::make_optional<pugi::xml_document>();
	if (auto result = output->load_file(file.c_str()); result)
		return output;
	else
		std::cout << "Error: Fail to load '" << std::filesystem::absolute(file) << "': "
			<< result.description() << '\n';
	return std::nullopt;
}

inline vkma_xml::detail::type_registry::underlying_t::iterator
vkma_xml::detail::type_registry::get(identifier_t &&name) {
	auto [iterator, result] = underlying.try_emplace(
		std::move(name), type_t{ type::undefined{}, type_tag::helper }
	);
	return iterator;
}
inline vkma_xml::detail::type_registry::underlying_t::iterator
vkma_xml::detail::type_registry::add(identifier_t &&name, type_t &&type_data) {
	auto [iterator, result] = underlying.try_emplace(
		std::move(name),
		std::move(type_data)
	);

	if (!result)
		if (std::holds_alternative<type::undefined>(iterator->second.state))
			iterator->second = std::move(type_data);
		else if (std::holds_alternative<type::structure>(iterator->second.state)
				 && std::holds_alternative<type::handle>(type_data.state)) {
			if (iterator->second.tag == type_tag::core)
				type_data.tag = type_tag::core;
			iterator->second = std::move(type_data);
		} else {
			std::cout << "Warning: Attempt to define a typename '" << iterator->first
				<< "' more than once: second definition ignored.\n";
			return iterator;
		}

	struct on_add_visitor {
		vkma_xml::detail::type_registry &registry_ref;
		identifier_t const &name_ref;

		void operator()(type::undefined const &) {}
		void operator()(type::structure const &structure) {
			for (auto const &member : structure.members)
				registry_ref.get(member.type.name);
		}
		void operator()(type::handle const &) {
			if (auto iterator = registry_ref.find(name_ref + "_T");
					 iterator != registry_ref.end() && iterator->second.tag == type_tag::core)
				iterator->second.tag = type_tag::helper;
		}
		void operator()(type::macro const &) {}
		void operator()(type::enumeration const &enumeration) {
			if (enumeration.type)
				registry_ref.get(enumeration.type->name);
		}
		void operator()(type::function const &function) {
			registry_ref.get(function.return_type.name);
			for (auto const &parameter : function.parameters)
				registry_ref.get(parameter.type.name);
		}
		void operator()(type::function_pointer const &function_pointer) {
			registry_ref.get(function_pointer.return_type.name);
			for (auto const &parameter : function_pointer.parameters)
				registry_ref.get(parameter.type.name);
		}
		void operator()(type::alias const alias) {
			registry_ref.get(alias.real_type.name);
		}
		void operator()(type::base const &) {}
	};
	std::visit(on_add_visitor{ *this, iterator->first }, iterator->second.state);
	return iterator;
}

static std::string optimize(std::string &&input) {
	static std::locale locale("en_US.UTF8");

	std::string output;
	output.reserve(input.size());
	for (size_t i = 0; i < input.size(); ++i)
		if (input[i] == '\n') {
			input[i] = ' '; --i;
		} else if (i != 0 && std::isblank(input[i], locale) && std::isblank(input[i - 1], locale)) {
			// Skip consecutive blank characters.
		} else
			output += input[i];
		return std::move(output);
}
static std::string to_string(pugi::xml_node const &xml) {
	std::string output;
	for (auto &child : xml.children())
		if (child.type() == pugi::xml_node_type::node_pcdata)
			output += child.value();
		else if (child.name() == "ref"sv)
			output += child.child_value();
		else
			std::cout << "Warning: Ignore an unknown tag: '" << child.name() << "'.\n";
	return optimize(std::move(output));
}

static std::string trim(std::string &&input) {
	auto begin = input.find_first_not_of(' ');
	if (begin == std::string::npos)
		begin = 0;
	auto end = input.find_last_not_of(' ');
	if (end == std::string::npos)
		end = input.size();
	return input.substr(begin, end - begin + 1);
}
vkma_xml::detail::decorated_typename_t::decorated_typename_t(std::string input) : name(input) {
	bool changed = false;
	do {
		changed = false;
		for (auto const &token : accepted_prefixes)
			if (name.size() > token.size() && std::string_view(name).substr(0, token.size()) == token) {
				prefix += std::string_view(name).substr(0, token.size());
				name.erase(0, token.size());
				changed = true;
			}
	} while (changed);
	do {
		changed = false;
		for (auto const &token : accepted_postfixes)
			if (size_t position = name.size() - token.size(); name.size() > token.size() &&
					std::string_view(name).substr(position) == token) {
				postfix.insert(0, std::string_view(name).substr(position));
				name.erase(position, token.size());
				changed = true;
			}
	} while (changed);
	prefix = trim(std::move(prefix));
	postfix = trim(std::move(postfix));
}

std::optional<vkma_xml::detail::variable_t>
vkma_xml::detail::api_t::load_variable(pugi::xml_node const &xml) {
	if (auto name = xml.child("name"), type = xml.child("type"); name && type) {
		if (auto argsstring = xml.child("argsstring"); argsstring)
			if (auto str = to_string(argsstring); !str.empty())
				if (str.size() > 2 && str.front() == '[' && str.back() == ']')
					return std::make_optional<variable_t>(to_string(name), to_string(type),
														  str.substr(1, str.size() - 2));
				else
					std::cout << "Warning: Unable to parse 'argsstring' of a variable("
						<< to_string(name) << "): " << str << ".\n";
		return std::make_optional<variable_t>(to_string(name), to_string(type), std::nullopt);
	}
	return std::nullopt;
}
std::optional<vkma_xml::detail::constant_t>
vkma_xml::detail::api_t::load_define(pugi::xml_node const &xml) {
	if (auto name = xml.child("name"), value = xml.child("initializer"); name && value)
		return std::make_optional<constant_t>(to_string(name), to_string(value));
	return std::nullopt;
}
std::optional<vkma_xml::detail::enum_t>
vkma_xml::detail::api_t::load_enum(pugi::xml_node const &xml) {
	enum_t output;
	for (auto &child : xml.children())
		if (child.name() == "type"sv)
			if (auto string = to_string(child); string != "")
				output.state.type = string;
			else
				output.state.type = std::nullopt;
		else if (child.name() == "name"sv)
			output.name = to_string(child);
		else if (child.name() == "enumvalue"sv)
			if (auto name = child.child("name"), value = child.child("initializer"); name && value) {
				auto value_str = to_string(value);
				if (std::string_view(value_str).substr(0, 2) == "= ")
					value_str = value_str.substr(2);
				if (auto name_str = to_string(name);
						std::string_view(name_str).substr(name_str.size() - 9) != "_MAX_ENUM")
					if (std::ranges::find_if(output.state.values, [&value_str](constant_t const &value) {
						return value_str == value.name;
					}) == output.state.values.end())
						output.state.values.emplace_back(std::move(name_str), std::move(value_str));
					else
						output.state.aliases.emplace_back(std::move(name_str), std::move(value_str));
			}
	if (output.name != "")
		return output;
	else
		return std::nullopt;
}
std::optional<vkma_xml::detail::variable_t>
vkma_xml::detail::api_t::load_typedef(pugi::xml_node const &xml) {
	if (auto name = xml.child("name"), type = xml.child("type"), args = xml.child("argsstring"); name && type && args)
		return std::make_optional<variable_t>(to_string(name), to_string(type) + to_string(args));
	return std::nullopt;
}
std::optional<vkma_xml::detail::variable_t>
vkma_xml::detail::api_t::load_function_parameter(pugi::xml_node const &xml) {
	if (auto name = xml.child("declname"), type = xml.child("type"); name && type)
		return std::make_optional<variable_t>(to_string(name), to_string(type));
	return std::nullopt;
}
std::optional<vkma_xml::detail::function_t>
vkma_xml::detail::api_t::load_function(pugi::xml_node const &xml) {
	function_t output;
	for (auto &child : xml.children())
		if (child.name() == "type"sv)
			output.state.return_type = to_string(child);
		else if (child.name() == "name"sv)
			output.name = to_string(child);
		else if (child.name() == "param"sv)
			if (auto parameter = load_function_parameter(child); parameter)
				output.state.parameters.emplace_back(*parameter);
	if (output.name != "" && output.state.return_type)
		return output;
	else
		return std::nullopt;
}
std::optional<vkma_xml::detail::type::function_pointer>
vkma_xml::detail::api_t::load_function_pointer(std::string_view type_name) {
	type::function_pointer output;

	size_t offset = type_name.find("(");
	output.return_type = std::string(type_name.substr(0, offset));

	if (type_name.substr(offset, 4) == "(*)(" && type_name.substr(type_name.size() - 1) == ")")
		offset += 4;
	else if (type_name.substr(offset, 14) == "(VKAPI_PTR *)(" && type_name.substr(type_name.size() - 1) == ")")
		offset += 14;
	else
		return std::nullopt;

	if (type_name.substr(offset, type_name.size() - offset - 1) == "void")
		return output;

	while (offset < type_name.size()) {
		auto space_pos = type_name.find(" ", offset);
		if (space_pos == std::string_view::npos)
			return std::nullopt;

		auto comma_pos = type_name.find(", ", space_pos);
		if (comma_pos == std::string_view::npos)
			comma_pos = type_name.size() - 1;

		output.parameters.emplace_back(
			std::string(type_name.begin() + space_pos + 1, type_name.begin() + comma_pos),
			std::string(type_name.begin() + offset, type_name.begin() + space_pos)
		);
		offset = comma_pos + 2;
	}
	return output;
}

void vkma_xml::detail::api_t::load_struct(pugi::xml_node const &xml, type_tag tag) {
	std::string_view name;
	type::structure structure;
	for (auto &child : xml.children())
		if (child.name() == "compoundname"sv)
			name = child.child_value();
		else if (child.name() == "sectiondef"sv)
			for (auto &member : child.children())
				if (member.name() == "memberdef"sv)
					if (auto kind = member.attribute("kind").value(); kind == "variable"sv) {
						if (auto variable = load_variable(member); variable)
							structure.members.emplace_back(*variable);
					} else
						std::cout << "Warning: Ignore a struct member: '" << kind
							<< "'. Only variables are supported.\n";
				else
					std::cout << "Warning: Ignore an unknown struct member: '" << member.name() << "'.\n";

	if (name != "")
		registry.add(name, type_t{ std::move(structure), tag });
	else
		std::cout << "Warning: Ignore a struct compound without a name.\n";
}

void vkma_xml::detail::api_t::load_file(pugi::xml_node const &xml, type_tag tag) {
	for (auto &child : xml.children())
		if (child.name() == "sectiondef"sv)
			for (auto &member : child.children())
				if (member.name() == "memberdef"sv) {
					if (auto kind = member.attribute("kind").value(); kind == "define"sv) {
						if (auto define = load_define(member); define)
							registry.add(std::move(define->name), type_t {
								type::macro{ std::move(define->value) }, tag
							});
					} else if (kind == "enum"sv) {
						if (auto enumeration = load_enum(member); enumeration)
							registry.add(std::move(enumeration->name), type_t {
								type::enumeration{ std::move(enumeration->state) }, tag
							});
					} else if (kind == "typedef"sv) {
						if (auto type_def = load_typedef(member); type_def)
							if (type_def->name != type_def->type.name)
								if (std::string_view(type_def->name).substr(0, 3) == "PFN")
									if (auto pointer = load_function_pointer(type_def->type.name); pointer)
										registry.add(std::move(type_def->name), type_t {
											*pointer, tag
										});
									else
										std::cout << "Warning: Ignore a function pointer: '"
											<< type_def->name << "'. Parsing has failed.\n";
								else
									registry.add(std::move(type_def->name), type_t {
										type::alias{ std::move(type_def->type) }, tag
									});
					} else if (kind == "function"sv) {
						if (auto function = load_function(member); function)
							registry.add(std::move(function->name), type_t {
								type::function{ std::move(function->state) }, tag
							});
					} else
						std::cout << "Ignore an unknown file entry '" << kind << "'.\n";
				}
}

void vkma_xml::detail::api_t::load_compound(std::string_view refid,
											std::filesystem::path const &directory, type_tag tag) {
	std::filesystem::path file_path = directory; (file_path /= refid.data()) += ".xml";
	if (auto compound_xml = detail::load_xml(file_path); compound_xml)
		if (auto doxygen = compound_xml->child("doxygen"); doxygen)
			if (auto compound = doxygen.child("compounddef"); compound)
				if (std::string_view kind = compound.attribute("kind").value(); kind == "struct"sv)
					return load_struct(compound, tag);
				else if (kind == "file"sv)
					return load_file(compound, tag);
				else
					std::cout << "Warning: Ignore a compound of an unknown kind: '" << kind << "'.\n";
}

void vkma_xml::detail::api_t::load_helper(input const &helper_api) {
	if (auto api_index = detail::load_xml(helper_api.xml_directory.string() + "/index.xml"); api_index)
		if (auto index = api_index->child("doxygenindex"); index)
			for (auto const &compound : index.children())
				if (compound.name() == "compound"sv)
					if (auto kind = compound.attribute("kind").value(); kind == "struct"sv || kind == "file"sv)
						load_compound(compound.attribute("refid").value(),
									  helper_api.xml_directory,
									  type_tag::helper);
					else if (kind == "page"sv || kind == "dir"sv) {
						// Silently ignore 'page' and 'dir' index entries.
					} else
						std::cout << "Warning: Ignore a compound of an unknown kind: '" << kind << "'.\n";
				else
					std::cout << "Warning: Ignore an unknown node: " << compound.name() << '\n';

	for (auto const &handle : detail::load_handle_list(helper_api.header_files))
		registry.add(handle.first, detail::type_t {
			detail::type::handle{ handle.second }, detail::type_tag::helper
		});
}

std::optional<vkma_xml::detail::api_t> vkma_xml::parse(input main_api, std::initializer_list<input> const &helper_apis) {
	std::cout << "Parse API located at " << main_api.xml_directory << ""
		<< (main_api.header_files.size() ? " with headers:" : "") << "\n";
	for (auto const &header : main_api.header_files)
		std::cout << "    " << header << "\n";
	if (helper_apis.size()) {
		std::cout << "\nHelpers:\n";
		for (auto const &helper : helper_apis) {
			std::cout << "API located at " << helper.xml_directory << ""
				<< (helper.header_files.size() ? " with headers:" : "") << "\n";
			for (auto const &header : helper.header_files)
				std::cout << "    " << header << "\n";
		}
	}
	std::cout << std::endl;
	
	auto start_time = std::chrono::high_resolution_clock::now();
	if (auto main_api_index = detail::load_xml(main_api.xml_directory.string() + "/index.xml"); main_api_index)
		if (auto index = main_api_index->child("doxygenindex"); index) {
			detail::api_t api;
			for (auto const &compound : index.children())
				if (compound.name() == "compound"sv)
					if (auto kind = compound.attribute("kind").value(); kind == "struct"sv || kind == "file"sv)
						api.load_compound(compound.attribute("refid").value(),
										  main_api.xml_directory,
										  detail::type_tag::core);
					else if (kind == "page"sv || kind == "dir"sv) {
						// Silently ignore 'page' and 'dir' index entries.
					} else
						std::cout << "Warning: Ignore a compound of an unknown kind: '" << kind << "'.\n";
				else
					std::cout << "Warning: Ignore an unknown node: " << compound.name() << '\n';
	
			for (auto const &handle : detail::load_handle_list(main_api.header_files))
				api.registry.add(handle.first, detail::type_t {
					detail::type::handle{ handle.second }, detail::type_tag::core
				});

			for (auto const &helper_api : helper_apis)
				api.load_helper(helper_api);

			for (auto const &base_type : detail::base_types)
				api.registry.add(base_type, detail::type_t {
					detail::type::base{}, detail::type_tag::helper
				});

			detail::transparent_set undefined;
			for (auto const &type : api.registry)
				if (std::holds_alternative<detail::type::undefined>(type.second.state))
					undefined.insert(type.first);
			
			std::cout << "Generator: finish parsing XMLs (It took " <<
				std::chrono::duration_cast<std::chrono::duration<float>>(
					std::chrono::high_resolution_clock::now() - start_time
				).count() << "s)\n";
			if (!undefined.empty()) {
				std::cout << "Warning: Undefined types left after parsing is over:\n";
				for (auto const &name : undefined)
					std::cout << "- " << name << "\n";
			}
			std::cout << std::endl;

			return api;
		}
	return std::nullopt;
}

static std::string to_upper_case(std::string_view input) {
	static std::locale locale("en_US.UTF8");

	std::string output(input.substr(0, 1));
	output.reserve(input.size());
	for (size_t i = 1; i < input.size(); ++i) {
		if (std::isupper(input[i], locale) && std::islower(input[i - 1], locale))
			output += '_';
		output += std::toupper(input[i], locale);
	}

	return output;
}

static std::string to_objtypeenum(std::string_view input) {
	std::string output = to_upper_case(input);
	if (std::string_view(output).substr(0, 4) == "VKMA_")
		return output.insert(0, "VK_OBJECT_TYPE_");
	return output;
}

vkma_xml::detail::generator_t::generator_t(api_t const &api)
	: api(api), output(std::make_optional<pugi::xml_document>())
	, registry(output->append_child("registry")) {}

void vkma_xml::detail::generator_t::append_typename(pugi::xml_node &xml,
													decorated_typename_t const &type) {
	xml.append_child(pugi::node_pcdata).set_value(type.prefix.data());
	xml.append_child("type").append_child(pugi::node_pcdata).set_value(type.name.data());
	xml.append_child(pugi::node_pcdata).set_value(type.postfix.data());
}

void vkma_xml::detail::generator_t::append_header() {
	if (registry) {
		registry->append_child("comment").append_child(pugi::node_pcdata).set_value(
			"\nCopyright (c) 2021 Cvelth (cvelth.mail@gmail.com)"
			"\nSPDX-License-Identifier: Unlicense."
		);
		registry->append_child("comment").append_child(pugi::node_pcdata).set_value(
			"\nDO NOT MODIFY MANUALLY!"
			"\nThis file was generated using [generator](https://github.com/Cvelth/vkma_xml_generator)."
			"\nGenerated files are licensed under [The Unlicense](https://unlicense.org)."
			"\nThe generator itself is licensed under [MIT License](https://www.mit.edu/~amini/LICENSE.md)."
		);
		registry->append_child("comment").append_child(pugi::node_pcdata).set_value(
			"\nThis file was generated from xml 'doxygen' documentation for "
			"[vkma_bindings.hpp](https://github.com/Cvelth/vkma_bindings/blob/main/include/vkma_bindings.hpp) "
			"header."
			"\nHeaders used for name lookup: "
			"\n[vk_mem_alloc.h (VulkanMemoryAllocator)](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/blob/master/src/vk_mem_alloc.h) "
			"\n[vulkan_core.h (Vulkan-Headers)](https://github.com/KhronosGroup/Vulkan-Headers/blob/master/include/vulkan/vulkan_core.h) "
			"\n\nIt is intended to be used as [vulkan-hpp fork](https://github.com/Cvelth/vkma_vulkan_hpp_fork) generator input."
			"\nThe goal is to generate a [vulkan-hpp](https://github.com/KhronosGroup/Vulkan-Hpp/blob/master/vulkan/vulkan.hpp) "
			"compatible header - a better c++ interface for VulkanMemoryAllocator."
		);

		auto platforms = registry->append_child("platforms");
		platforms.append_attribute("comment").set_value("empty");
		auto platform = platforms.append_child("platform");
		platform.append_attribute("name").set_value("does_not_matter");
		platform.append_attribute("protect").set_value("VKMA_DOES_NOT_MATTER");
		platform.append_attribute("comment").set_value("Why am I even required to specify this?");

		auto tags = registry->append_child("tags");
		tags.append_attribute("comment").set_value("empty");
		auto tag = tags.append_child("tag");
		tag.append_attribute("name").set_value("WC");
		tag.append_attribute("author").set_value("Who cares?");
		tag.append_attribute("contact").set_value("@cvelth");
	}
}

void vkma_xml::detail::generator_t::append_types() {
	struct append_types_visitor {
		identifier_t const &name_ref;
		type_tag const &tag;
		pugi::xml_node &types_ref;
		generator_t &generator_ref;

		inline void operator()(vkma_xml::detail::type::undefined const &) {
			std::cout << "Warning: Fail to append an undefined type: '"
				<< name_ref << "'.\n";
		}
		inline void operator()(vkma_xml::detail::type::structure const &structure) {
			if (tag == type_tag::core) {
				if (!generator_ref.appended_types.contains(name_ref)) {
					for (auto const &member : structure.members) {
						if (auto iterator = generator_ref.api.registry.find(member.type.name);
								 iterator != generator_ref.api.registry.end())
							std::visit(append_types_visitor {
								member.type.name, iterator->second.tag, types_ref, generator_ref
							}, iterator->second.state);
						if (member.array)
							if (auto iterator = generator_ref.api.registry.find(*member.array);
									 iterator != generator_ref.api.registry.end())
								std::visit(append_types_visitor {
									*member.array, iterator->second.tag, types_ref, generator_ref
								}, iterator->second.state);
					}

					auto type = types_ref.append_child("type");
					type.append_attribute("category").set_value("struct");
					type.append_attribute("name").set_value(name_ref.data());
					for (auto &member : structure.members) {
						auto output = type.append_child("member");
						append_typename(output, member.type);
						output.append_child(pugi::node_pcdata).set_value(" ");
						output.append_child("name").append_child(pugi::node_pcdata)
							.set_value(member.name.data());
						if (member.array) {
							output.append_child(pugi::node_pcdata).set_value("[");
							output.append_child("enum").append_child(pugi::node_pcdata)
								.set_value(member.array->data());
							output.append_child(pugi::node_pcdata).set_value("]");
						}
					}
					generator_ref.appended_types.emplace(name_ref);
				}
			} else if (!generator_ref.appended_basetypes.contains(name_ref)) {
				auto type = types_ref.append_child("type");
				type.append_attribute("category").set_value("basetype");
				type.append_child(pugi::node_pcdata).set_value("struct ");
				type.append_child("name").append_child(pugi::node_pcdata).set_value(name_ref.data());
				type.append_child(pugi::node_pcdata).set_value(";");
				generator_ref.appended_basetypes.emplace(name_ref);
			}
		}
		inline void operator()(vkma_xml::detail::type::handle const &handle) {
			if (tag == type_tag::core) {
				if (!generator_ref.appended_types.contains(name_ref)) {
					if (handle.parent)
						if (auto iterator = generator_ref.api.registry.find(*handle.parent);
								 iterator != generator_ref.api.registry.end())
							std::visit(append_types_visitor{ 
								iterator->first, iterator->second.tag, types_ref, generator_ref 
							}, iterator->second.state);
						else
							std::cout << "Warning: An undefined aliased type: '" << *handle.parent << "'.\n";

					auto type = types_ref.append_child("type");
					type.append_attribute("category").set_value("handle");
					if (handle.parent)
						type.append_attribute("parent").set_value(handle.parent->data());
					type.append_attribute("objtypeenum").set_value(to_objtypeenum(name_ref).data());
					if (handle.dispatchable)
						type.append_child("type").append_child(pugi::node_pcdata)
							.set_value("VK_DEFINE_HANDLE");
					else
						type.append_child("type").append_child(pugi::node_pcdata)
							.set_value("VK_DEFINE_NON_DISPATCHABLE_HANDLE");
					type.append_child(pugi::node_pcdata).set_value("(");
					type.append_child("name").append_child(pugi::node_pcdata).set_value(name_ref.data());
					type.append_child(pugi::node_pcdata).set_value(")");
					generator_ref.appended_types.emplace(name_ref);
				}
			} else if (!generator_ref.appended_basetypes.contains(name_ref)) {
				auto type = types_ref.append_child("type");
				type.append_attribute("category").set_value("basetype");
				type.append_child("name").append_child(pugi::node_pcdata).set_value(name_ref.data());
				generator_ref.appended_basetypes.emplace(name_ref);
			}
		}
		inline void operator()(vkma_xml::detail::type::macro const &macro) {
			if (tag == type_tag::core) {
				if (!generator_ref.appended_types.contains(name_ref)) {
					auto type = types_ref.append_child("type");
					type.append_attribute("category").set_value("define");
					type.append_child(pugi::node_pcdata).set_value("#define ");
					type.append_child("name").append_child(pugi::node_pcdata).set_value(name_ref.data());
					type.append_child(pugi::node_pcdata).set_value((" " + macro.value).data());
					generator_ref.appended_types.emplace(name_ref);
				}
			} else
				generator_ref.appended_constants.emplace(name_ref);
		}
		inline void operator()(vkma_xml::detail::type::enumeration const &) {
			if (tag == type_tag::core) {
				if (!generator_ref.appended_types.contains(name_ref)) {
					auto type = types_ref.append_child("type");
					type.append_attribute("name").set_value(name_ref.data());
					type.append_attribute("category").set_value("enum");
					generator_ref.appended_types.emplace(name_ref);
				}
			} else if (!generator_ref.appended_basetypes.contains(name_ref)) {
				auto type = types_ref.append_child("type");
				type.append_attribute("category").set_value("basetype");
				type.append_child(pugi::node_pcdata).set_value("enum ");
				type.append_child("name").append_child(pugi::node_pcdata).set_value(name_ref.data());
				type.append_child(pugi::node_pcdata).set_value(";");
				generator_ref.appended_basetypes.emplace(name_ref);
			}
		}
		inline void operator()(vkma_xml::detail::type::function const &function) {
			if (tag == type_tag::core) {
				for (auto const &parameter : function.parameters)
					if (auto iterator = generator_ref.api.registry.find(parameter.type.name);
							 iterator != generator_ref.api.registry.end())
						std::visit(append_types_visitor {
							parameter.type.name, iterator->second.tag, types_ref, generator_ref
						}, iterator->second.state);
				if (auto iterator = generator_ref.api.registry.find(function.return_type.name);
						 iterator != generator_ref.api.registry.end())
					std::visit(append_types_visitor {
						function.return_type.name, iterator->second.tag, types_ref, generator_ref
					}, iterator->second.state);
			}
		}
		inline void operator()(vkma_xml::detail::type::function_pointer const &function_pointer) {
			if (tag == type_tag::core) {
				if (!generator_ref.appended_types.contains(name_ref)) {
					for (auto const &parameter : function_pointer.parameters)
						if (auto iterator = generator_ref.api.registry.find(parameter.type.name);
								 iterator != generator_ref.api.registry.end())
							std::visit(append_types_visitor {
								parameter.type.name, iterator->second.tag, types_ref, generator_ref
							}, iterator->second.state);
					if (auto iterator = generator_ref.api.registry.find(function_pointer.return_type.name);
							 iterator != generator_ref.api.registry.end())
						std::visit(append_types_visitor {
							function_pointer.return_type.name, iterator->second.tag, types_ref, generator_ref
						}, iterator->second.state);

					auto type = types_ref.append_child("type");
					type.append_attribute("category").set_value("funcpointer");
					type.append_child(pugi::node_pcdata).set_value(
						("typedef " + function_pointer.return_type.to_string() + "(*").data()
					);
					type.append_child("name").append_child(pugi::node_pcdata).set_value(name_ref.data());
					type.append_child(pugi::node_pcdata).set_value(")(");
					for (auto iterator = function_pointer.parameters.begin()
						 ; iterator != std::prev(function_pointer.parameters.end())
						 ; ++iterator) {

						append_typename(type, iterator->type);
						type.append_child(pugi::node_pcdata).set_value(
							(" " + iterator->name + ", ").data()
						);
					}
					append_typename(type, function_pointer.parameters.back().type);
					type.append_child(pugi::node_pcdata).set_value(
						(" " + function_pointer.parameters.back().name + ");").data()
					);
					generator_ref.appended_types.emplace(name_ref);
				}
			} else if (!generator_ref.appended_basetypes.contains(name_ref)) {
				auto type = types_ref.append_child("type");
				type.append_attribute("category").set_value("basetype");
				type.append_child("name").append_child(pugi::node_pcdata).set_value(name_ref.data());
				generator_ref.appended_basetypes.emplace(name_ref);
			}
		}
		inline void operator()(vkma_xml::detail::type::alias const &alias) {
			if (tag == type_tag::core) {
				if (!generator_ref.appended_types.contains(name_ref))
					if (std::string_view(name_ref).substr(name_ref.size() - 5) == "Flags" &&
							alias.real_type.name == "VkFlags") {
						auto type = types_ref.append_child("type");
						type.append_attribute("category").set_value("bitmask");
						if (auto iterator = generator_ref.api.registry.find(
							name_ref.substr(0, name_ref.size() - 1) + "Bits"
						); iterator != generator_ref.api.registry.end())
							type.append_attribute("requires").set_value(iterator->first.data());
						else
							type.append_attribute("requires").set_value("none");
						type.append_child(pugi::node_pcdata).set_value("typedef ");
						type.append_child("type").append_child(pugi::node_pcdata).set_value("VkFlags");
						type.append_child(pugi::node_pcdata).set_value(" ");
						type.append_child("name").append_child(pugi::node_pcdata).set_value(name_ref.data());
						type.append_child(pugi::node_pcdata).set_value(";");
						generator_ref.appended_types.insert(name_ref);
					} else if (auto iterator = generator_ref.api.registry.find(alias.real_type.name);
							   iterator != generator_ref.api.registry.end())
						std::visit(append_types_visitor{ name_ref, tag, types_ref, generator_ref },
								   iterator->second.state);
					else
						std::cout << "Warning: An undefined aliased type: '" << alias.real_type.name << "'.\n";
			} else if (!generator_ref.appended_basetypes.contains(name_ref))
				if (auto iterator = generator_ref.api.registry.find(alias.real_type.name);
						 iterator != generator_ref.api.registry.end()) {
					std::visit(append_types_visitor{
						alias.real_type.name, iterator->second.tag, types_ref, generator_ref
					}, iterator->second.state);
					auto type = types_ref.append_child("type");
					type.append_attribute("category").set_value("basetype");
					type.append_child(pugi::node_pcdata).set_value("typedef ");
					append_typename(type, alias.real_type);
					type.append_child(pugi::node_pcdata).set_value(" ");
					type.append_child("name").append_child(pugi::node_pcdata).set_value(name_ref.data());
					type.append_child(pugi::node_pcdata).set_value(";");
					generator_ref.appended_basetypes.emplace(name_ref);
				}
		}
		inline void operator()(vkma_xml::detail::type::base const &) {
			if (!generator_ref.appended_basetypes.contains(name_ref)) {
				auto type = types_ref.append_child("type");
				type.append_attribute("category").set_value("basetype");
				type.append_child("name").append_child(pugi::node_pcdata).set_value(name_ref.data());
				generator_ref.appended_basetypes.emplace(name_ref);
			}
		}
	};

	if (registry) {
		auto types = registry->append_child("types");
		types.append_attribute("comment").set_value("VKMA type definitions");

		auto required_comment = types.append_child("comment");
		required_comment.append_child(pugi::node_pcdata).set_value("Why is a comment here required?!");

		auto vma_include = types.append_child("type");
		vma_include.append_attribute("name").set_value("vma");
		vma_include.append_attribute("category").set_value("include");
		vma_include.append_child(pugi::node_pcdata).set_value("#include \"vk_mem_alloc.h\"");

		for (auto const &type : api.registry)
			if (type.second.tag == type_tag::core)
				std::visit(append_types_visitor{ type.first, type.second.tag, types, *this },
						   type.second.state);
	}
}

void vkma_xml::detail::generator_t::append_enumerations() {
	struct append_enumerations_visitor {
		identifier_t const &name_ref;
		type_tag const &tag;
		pugi::xml_node &registry_ref;
		generator_t &generator_ref;

		inline void operator()(vkma_xml::detail::type::undefined const &) {}
		inline void operator()(vkma_xml::detail::type::structure const &) {}
		inline void operator()(vkma_xml::detail::type::handle const &) {}
		inline void operator()(vkma_xml::detail::type::macro const &) {}
		inline void operator()(vkma_xml::detail::type::enumeration const &enumeration) {
			if (tag == type_tag::core) {
				auto enums = registry_ref.append_child("enums");
				enums.append_attribute("name").set_value(name_ref.data());
				if (std::string_view(name_ref).substr(name_ref.size() - 8) == "FlagBits")
					enums.append_attribute("type").set_value("bitmask");
				else
					enums.append_attribute("type").set_value("enum");
				for (auto &enumerator : enumeration.values) {
					auto enum_ = enums.append_child("enum");
					enum_.append_attribute("value").set_value(enumerator.value.data());
					enum_.append_attribute("name").set_value(enumerator.name.data());
				}
				for (auto &alias : enumeration.aliases) {
					auto enum_ = enums.append_child("enum");
					enum_.append_attribute("name").set_value(alias.name.data());
					enum_.append_attribute("alias").set_value(alias.value.data());
				}
			}
		}
		inline void operator()(vkma_xml::detail::type::function const &) {}
		inline void operator()(vkma_xml::detail::type::function_pointer const &) {}
		inline void operator()(vkma_xml::detail::type::alias const &alias) {
			if (tag == type_tag::core)
				if (auto iterator = generator_ref.api.registry.find(alias.real_type.name);
						 iterator != generator_ref.api.registry.end())
					if (std::holds_alternative<type::enumeration>(iterator->second.state))
						std::visit(append_enumerations_visitor{ name_ref, tag, registry_ref, generator_ref },
								   iterator->second.state);
		}
		inline void operator()(vkma_xml::detail::type::base const &) {}
	};

	if (registry) {
		auto enums = registry->append_child("enums");
		enums.append_attribute("name").set_value("API Constants");
		enums.append_attribute("comment").set_value("Hardcoded constants - not an enumerated type, part of the header boilerplate");
		for (auto const &constant_name : appended_constants)
			if (auto iterator = api.registry.find(constant_name);
					 iterator != api.registry.end())
				if (std::holds_alternative<type::macro>(iterator->second.state)) {
					auto enum_ = enums.append_child("enum");
					enum_.append_attribute("value").set_value(std::get<type::macro>(iterator->second.state).value.data());
					enum_.append_attribute("name").set_value(constant_name.data());
				} else
					std::cout << "Ignore a constant(" << constant_name << "): its type is not supported.\n";
			else
				std::cout << "Warning: Ignore an unknown constant: " << constant_name << ".\n";
		for (auto const &type : api.registry)
			std::visit(append_enumerations_visitor{ type.first, type.second.tag, *registry, *this },
					   type.second.state);
	}
}

std::string concatenate_success_codes(vkma_xml::detail::type_registry const &registry) {
	if (auto iterator = registry.find("VkResult"); iterator != registry.end())
		if (std::holds_alternative<vkma_xml::detail::type::enumeration>(iterator->second.state)) {
			auto const &enumeration = std::get<vkma_xml::detail::type::enumeration>(iterator->second.state);
			if (!enumeration.values.empty())
				return enumeration.values.front().name;
			else
				std::cout << "Warning: Unable to select success codes: VkResult enumeration has no enumerators.";
		} else
			std::cout << "Warning: Unable to select success codes: VkResult is not an enumeration.";
	else
		std::cout << "Warning: Unable to select success codes: VkResult is not defined.";
	return "";
}
std::string concatenate_error_codes(vkma_xml::detail::type_registry const &registry) {
	if (auto iterator = registry.find("VkResult"); iterator != registry.end())
		if (std::holds_alternative<vkma_xml::detail::type::enumeration>(iterator->second.state)) {
			auto const &enumeration = std::get<vkma_xml::detail::type::enumeration>(iterator->second.state);
			std::string output = "";
			if (!enumeration.values.empty()) {
				for (auto enumerator = ++enumeration.values.begin();
						  enumerator != std::prev(enumeration.values.end());
						++enumerator)
					output += enumerator->name + ", ";
				return output += enumeration.values.back().name;
			} else
				std::cout << "Warning: Unable to select error codes: VkResult enumeration has no enumerators.";
		} else
			std::cout << "Warning: Unable to select error codes: VkResult is not an enumeration.";
	else
		std::cout << "Warning: Unable to select error codes: VkResult is not defined.";
	return "";
}

void vkma_xml::detail::generator_t::append_commands() {
	struct append_commands_visitor {
		identifier_t const &name_ref;
		type_tag const &tag;
		pugi::xml_node &commands_ref;
		generator_t &generator_ref;

		inline void operator()(vkma_xml::detail::type::undefined const &) {}
		inline void operator()(vkma_xml::detail::type::structure const &) {}
		inline void operator()(vkma_xml::detail::type::handle const &) {}
		inline void operator()(vkma_xml::detail::type::macro const &) {}
		inline void operator()(vkma_xml::detail::type::enumeration const &) {}
		inline void operator()(vkma_xml::detail::type::function const &function) {
			static auto success_code_list = concatenate_success_codes(generator_ref.api.registry);
			static auto error_code_list = concatenate_error_codes(generator_ref.api.registry);

			auto command = commands_ref.append_child("command");
			if (function.return_type.name == "VkResult" || function.return_type.name == "VkmaResult") {
				if (!success_code_list.empty())
					command.append_attribute("successcodes").set_value(success_code_list.data());
				if (!error_code_list.empty())
					command.append_attribute("errorcodes").set_value(error_code_list.data());
			}
			auto proto = command.append_child("proto");
			append_typename(proto, function.return_type);
			proto.append_child(pugi::node_pcdata).set_value(" ");
			proto.append_child("name").append_child(pugi::node_pcdata).set_value(name_ref.data());

			for (auto &parameter : function.parameters) {
				auto param = command.append_child("param");
				append_typename(param, parameter.type);
				param.append_child(pugi::node_pcdata).set_value(" ");
				param.append_child("name").append_child(pugi::node_pcdata).set_value(parameter.name.data());
			}
			generator_ref.appended_commands.emplace(name_ref);
		}
		inline void operator()(vkma_xml::detail::type::function_pointer const &) {}
		inline void operator()(vkma_xml::detail::type::alias const &) {}
		inline void operator()(vkma_xml::detail::type::base const &) {}
	};

	if (registry) {
		auto commands = registry->append_child("commands");
		commands.append_attribute("comment").set_value("VKMA command definitions");
		for (auto const &type : api.registry)
			if (type.second.tag == type_tag::core)
				std::visit(append_commands_visitor{ type.first, type.second.tag, commands, *this },
						   type.second.state);
	}
}

void vkma_xml::detail::generator_t::append_feature() {
	if (registry) {
		auto feature = registry->append_child("feature");
		feature.append_attribute("api").set_value("vkma");
		feature.append_attribute("name").set_value("VKMA_VERSION_2_3");
		feature.append_attribute("number").set_value("2.3");
		feature.append_attribute("comment").set_value("VKMA API interface definitions");

		if (registry) {
			auto required = feature.append_child("require");
			required.append_attribute("comment").set_value("a mess, isn't it?");
			required.append_child("type").append_attribute("name").set_value("vma");

			for (auto const &type_name : appended_types) {
				auto type = required.append_child("type");
				type.append_attribute("name").set_value(type_name.data());
			}
			for (auto const &command_name : appended_commands) {
				auto command = required.append_child("command");
				command.append_attribute("name").set_value(command_name.data());
			}
		}
	}
}

void vkma_xml::detail::generator_t::append_footer() {
	if (registry) {
		auto extensions = registry->append_child("extensions");
		extensions.append_attribute("comment").set_value("empty");
		auto extension = extensions.append_child("extension");
		extension.append_attribute("name").set_value("VK_WC_why_y_y_y_y");
		extension.append_attribute("number").set_value("1");
		extension.append_attribute("type").set_value("instance");
		extension.append_attribute("author").set_value("WC");
		extension.append_attribute("contact").set_value("@cvelth");
		extension.append_attribute("supported").set_value("disabled");
		auto spirvextensions = registry->append_child("spirvextensions");
		spirvextensions.append_attribute("comment").set_value("empty");
		auto spirvcapabilities = registry->append_child("spirvcapabilities");
		spirvcapabilities.append_attribute("comment").set_value("empty");
	}
}

std::optional<pugi::xml_document> vkma_xml::generate(detail::api_t const &api) {
	detail::generator_t generator = api;

	generator.append_header();
	generator.append_types();
	generator.append_enumerations();
	generator.append_commands();
	generator.append_feature();
	generator.append_footer();

	return std::move(generator.output);
}

#ifndef VMA_XML_NO_MAIN
int main() {

	std::filesystem::path const vkma_bindings_directory = "../xml/vkma_bindings";
	std::vector<std::filesystem::path> const vkma_bindings_header_files = {
		"../input/vkma_bindings/include/vkma_bindings.hpp"
	};

	std::filesystem::path const vma_directory = "../xml/VulkanMemoryAllocator";
	std::vector<std::filesystem::path> const vma_header_files = {
		"../input/VulkanMemoryAllocator/src/vk_mem_alloc.h"
	};

	std::filesystem::path const vulkan_directory = "../xml/Vulkan-Headers";
	std::vector<std::filesystem::path> const vulkan_header_files = {
		"../input/Vulkan-Headers/include/vulkan/vulkan_core.h",
		"../input/Vulkan-Headers/include/vulkan/vk_platform.h"
	};

	std::filesystem::path const output_path = "../output/vkma.xml";

	auto output = vkma_xml::generate(
		vkma_xml::input {
			.xml_directory = vkma_bindings_directory,
			.header_files = vkma_bindings_header_files
		},
		vkma_xml::input {
			.xml_directory = vma_directory,
			.header_files = vma_header_files
		},
		vkma_xml::input {
			.xml_directory = vulkan_directory,
			.header_files = vulkan_header_files
		}
	);

	if (output) {
		std::filesystem::create_directory(output_path.parent_path());
		if (auto result = output->save_file(output_path.c_str()); result)
			std::cout << "\nSuccess: " << std::filesystem::absolute(output_path) << "\n";
		else
			std::cout << "Error: Unable to save " << std::filesystem::absolute(output_path) << ".";
	} else
		std::cout << "Error: Generation failed.";
	return 0;
}
#endif