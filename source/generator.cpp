// Copyright (c) 2021 Cvelth <cvelth.mail@gmail.com>
// SPDX-License-Identifier: MIT

#include "generator.hpp"

#include <iostream>
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

/*
#include <array>

std::string to_upper_case(std::string_view input) {
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

std::string to_objtypeenum(std::string_view input) {
	std::string output = to_upper_case(input);
	if (std::string_view(output).substr(0, 4) == "VMA_")
		return output.insert(0, "VK_OBJECT_TYPE_");
	return output;
}

std::optional<vkma_xml::detail::function_t> parse_function_pointer(vkma_xml::detail::typedef_t input) {
	auto name = std::string_view(input.name);
	auto type = std::string_view(input.type);
	if (name.substr(0, 4) == "PFN_") {
		vkma_xml::detail::function_t output;
		output.name = name;

		size_t open_br_pos = type.find("(");
		output.return_type = std::string(type.substr(0, open_br_pos));

		if (type.substr(open_br_pos, 14) == "(VKAPI_PTR *)(" && type.substr(type.size() - 1) == ")") {
			size_t offset = open_br_pos + 14;
			while (offset < type.size()) {
				auto space_pos = type.find(" ", offset);
				auto comma_pos = type.find(", ", space_pos);
				if (comma_pos == std::string_view::npos)
					comma_pos = type.size() - 1;
				output.parameters.emplace_back(
					std::string(type.begin() + space_pos + 1, type.begin() + comma_pos),
					std::string(type.begin() + offset, type.begin() + space_pos)
				);
				offset = comma_pos + 2;
			}
			return output;
		}
	}
	return std::nullopt;
}

void append_typename(pugi::xml_node &xml, std::string_view name, std::string prefix = "", std::string suffix = "") {
	if (name.size() > 6 && name.substr(0, 6) == "const ")
		append_typename(xml, name.substr(6), prefix += name.substr(0, 6), suffix);
	else if (name.size() > 6 && name.substr(name.size() - 6) == " const")
		append_typename(xml, name.substr(0, name.size() - 6), prefix, suffix += name.substr(name.size() - 6));
	else if (name.size() > 3 && name.substr(name.size() - 3) == " **")
		append_typename(xml, name.substr(0, name.size() - 3), prefix, suffix += name.substr(name.size() - 2));
	else if (name.size() > 2 && name.substr(name.size() - 2) == " *")
		append_typename(xml, name.substr(0, name.size() - 2), prefix, suffix += name.substr(name.size() - 1));
	else {
		xml.append_child(pugi::node_pcdata).set_value(prefix.data());
		xml.append_child("type").append_child(pugi::node_pcdata).set_value(std::string(name).data());
		xml.append_child(pugi::node_pcdata).set_value(suffix.data());
	}
}

void append_header(pugi::xml_node &registry) {
	registry.append_child("comment").append_child(pugi::node_pcdata).set_value(
		"\nCopyright (c) 2021 Cvelth (cvelth.mail@gmail.com)"
		"\nSPDX-License-Identifier: Unlicense."
		"\n\nDO NOT MODIFY MANUALLY!"
		"\nThis file was generated using [generator](https://github.com/Cvelth/vma_xml_generator)."
		"\nGenerated files are licensed under [The Unlicense](https://unlicense.org)."
		"\nThe generator itself is licensed under [MIT License](https://www.mit.edu/~amini/LICENSE.md)."
	);
	registry.append_child("comment").append_child(pugi::node_pcdata).set_value(
		"\nThis file was generated from xml 'doxygen' documentation for "
		"[vk_mem_alloc.h (VulkanMemoryAllocator)](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/blob/master/src/vk_mem_alloc.h) "
		"header."
		"\nIt is intended to be used as [vulkan-hpp](https://github.com/KhronosGroup/Vulkan-Hpp) generator input."
		"\nThe goal is to generate a [vulkan.hpp](https://github.com/KhronosGroup/Vulkan-Hpp/blob/master/vulkan/vulkan.hpp) "
		"compatible header - an improved c++ interface."
	);

	auto platforms = registry.append_child("platforms");
	platforms.append_attribute("comment").set_value("empty");
	auto platform = platforms.append_child("platform");
	platform.append_attribute("name").set_value("does_not_matter");
	platform.append_attribute("protect").set_value("VMA_DOES_NOT_MATTER");
	platform.append_attribute("comment").set_value("Why am I even required to specify this?");

	auto tags = registry.append_child("tags");
	tags.append_attribute("comment").set_value("empty");
	auto tag = tags.append_child("tag");
	tag.append_attribute("name").set_value("WC");
	tag.append_attribute("author").set_value("Who cares?");
	tag.append_attribute("contact").set_value("@cvelth");
}

void append_includes(pugi::xml_node &types) {
	auto vulkan_include = types.append_child("type");
	vulkan_include.append_attribute("name").set_value("vulkan");
	vulkan_include.append_attribute("category").set_value("include");
	vulkan_include.append_child(pugi::node_pcdata).set_value("#include \"vulkan/vulkan.h\"");
}

void append_defines(pugi::xml_node &types, std::vector<vkma_xml::detail::define_t> const &defines) {
	for (auto &define : defines) {
		auto type = types.append_child("type");
		type.append_attribute("category").set_value("define");
		type.append_child(pugi::node_pcdata).set_value("#define ");
		type.append_child("name").append_child(pugi::node_pcdata).set_value(define.name.data());
		type.append_child(pugi::node_pcdata).set_value((" " + define.value).data());
	}

	auto flags = types.append_child("type");
	flags.append_attribute("category").set_value("basetype");
	flags.append_child(pugi::node_pcdata).set_value("typedef ");
	flags.append_child("type").append_child(pugi::node_pcdata).set_value("uint32_t");
	flags.append_child(pugi::node_pcdata).set_value(" ");
	flags.append_child("name").append_child(pugi::node_pcdata).set_value("VkFlags");
	flags.append_child(pugi::node_pcdata).set_value(";");
}

void append_basic(pugi::xml_node &types) {
	types.append_child("comment").append_child(pugi::node_pcdata).set_value("____");
	types.append_child("comment").append_child(pugi::node_pcdata).set_value("Basic C types");
	auto type_void = types.append_child("type");
	type_void.append_attribute("requires").set_value("vulkan");
	type_void.append_attribute("name").set_value("void");
	auto type_char = types.append_child("type");
	type_char.append_attribute("requires").set_value("vulkan");
	type_char.append_attribute("name").set_value("char");
	auto type_float = types.append_child("type");
	type_float.append_attribute("requires").set_value("vulkan");
	type_float.append_attribute("name").set_value("float");
	auto type_double = types.append_child("type");
	type_double.append_attribute("requires").set_value("vulkan");
	type_double.append_attribute("name").set_value("double");
	auto type_uint8_t = types.append_child("type");
	type_uint8_t.append_attribute("requires").set_value("vulkan");
	type_uint8_t.append_attribute("name").set_value("uint8_t");
	auto type_uint16_t = types.append_child("type");
	type_uint16_t.append_attribute("requires").set_value("vulkan");
	type_uint16_t.append_attribute("name").set_value("uint16_t");
	auto type_uint32_t = types.append_child("type");
	type_uint32_t.append_attribute("requires").set_value("vulkan");
	type_uint32_t.append_attribute("name").set_value("uint32_t");
	auto type_uint64_t = types.append_child("type");
	type_uint64_t.append_attribute("requires").set_value("vulkan");
	type_uint64_t.append_attribute("name").set_value("uint64_t");
	auto type_int32_t = types.append_child("type");
	type_int32_t.append_attribute("requires").set_value("vulkan");
	type_int32_t.append_attribute("name").set_value("int32_t");
	auto type_int64_t = types.append_child("type");
	type_int64_t.append_attribute("requires").set_value("vulkan");
	type_int64_t.append_attribute("name").set_value("int64_t");
	auto type_size_t = types.append_child("type");
	type_size_t.append_attribute("requires").set_value("vulkan");
	type_size_t.append_attribute("name").set_value("size_t");
	auto type_int = types.append_child("type");
	type_int.append_attribute("name").set_value("int");
}

void append_vulkan(pugi::xml_node &types, std::set<std::string> const &vulkan_type_names) {
	types.append_child("comment").append_child(pugi::node_pcdata).set_value("____");
	types.append_child("comment").append_child(pugi::node_pcdata).set_value("Vulkan types");
	for (auto &type_name : vulkan_type_names) {
		auto type = types.append_child("type");
		type.append_attribute("requires").set_value("vulkan");
		type.append_attribute("name").set_value(type_name.data());
	}
}

void append_bitmask(pugi::xml_node &types, std::vector<vkma_xml::detail::typedef_t> const &typedefs,
					std::vector<vkma_xml::detail::enum_t> const &enums) {
	types.append_child("comment").append_child(pugi::node_pcdata).set_value("____");
	types.append_child("comment").append_child(pugi::node_pcdata).set_value("Bitmask types");
	for (auto &type_def : typedefs)
		if (type_def.type == "VkFlags") {
			auto type = types.append_child("type");
			type.append_attribute("category").set_value("bitmask");
			if (auto iterator = std::ranges::find_if(enums, [&type_def](auto const &enumeration) {
				return std::string_view(type_def.name).substr(type_def.name.size() - 5) == "Flags"
					&& enumeration.name == (type_def.name.substr(0, type_def.name.size() - 1) + "Bits");
			}); iterator != enums.end())
				type.append_attribute("requires").set_value(iterator->name.data());
			else
				type.append_attribute("requires").set_value("none");
			type.append_child(pugi::node_pcdata).set_value("typedef ");
			type.append_child("type").append_child(pugi::node_pcdata).set_value("VkFlags");
			type.append_child(pugi::node_pcdata).set_value(" ");
			type.append_child("name").append_child(pugi::node_pcdata).set_value(type_def.name.data());
			type.append_child(pugi::node_pcdata).set_value(";");
		}
}

void append_handle(pugi::xml_node &types, std::set<std::string> handle_names) {
	types.append_child("comment").append_child(pugi::node_pcdata).set_value("____");
	types.append_child("comment").append_child(pugi::node_pcdata).set_value("Handle types");
	for (auto &handle : handle_names) {
		auto type = types.append_child("type");
		type.append_attribute("category").set_value("handle");
		type.append_attribute("objtypeenum").set_value(to_objtypeenum(handle).data());
		type.append_child("type").append_child(pugi::node_pcdata).set_value("VK_DEFINE_HANDLE");
		type.append_child(pugi::node_pcdata).set_value("(");
		type.append_child("name").append_child(pugi::node_pcdata).set_value(handle.data());
		type.append_child(pugi::node_pcdata).set_value(")");
	}
}

void append_enum(pugi::xml_node &types, std::vector<vkma_xml::detail::enum_t> const &enums) {
	types.append_child("comment").append_child(pugi::node_pcdata).set_value("____");
	types.append_child("comment").append_child(pugi::node_pcdata).set_value("Enumeration types");
	for (auto &enumeration : enums) {
		auto type = types.append_child("type");
		type.append_attribute("name").set_value(enumeration.name.data());
		type.append_attribute("category").set_value("enum");
	}

	types.append_child("comment").append_child(pugi::node_pcdata).set_value("____");
	types.append_child("comment").append_child(pugi::node_pcdata).set_value(
		"enums required by Vulkan-HPP"
	);

	auto result = types.append_child("type");
	result.append_attribute("name").set_value("VkResult");
	result.append_attribute("category").set_value("enum");

	auto structure_type = types.append_child("type");
	structure_type.append_attribute("name").set_value("VkStructureType");
	structure_type.append_attribute("category").set_value("enum");

	auto object_type = types.append_child("type");
	object_type.append_attribute("name").set_value("VkObjectType");
	object_type.append_attribute("category").set_value("enum");

	auto index_type = types.append_child("type");
	index_type.append_attribute("name").set_value("VkIndexType");
	index_type.append_attribute("category").set_value("enum");

	auto debug_object_type = types.append_child("type");
	debug_object_type.append_attribute("name").set_value("VkDebugReportObjectTypeEXT");
	debug_object_type.append_attribute("category").set_value("enum");
}

void append_function_pointer(pugi::xml_node &types, std::vector<vkma_xml::detail::typedef_t> const &typedefs) {
	types.append_child("comment").append_child(pugi::node_pcdata).set_value("____");
	types.append_child("comment").append_child(pugi::node_pcdata).set_value(
		"Function pointer typedefs"
	);
	for (auto &type_def : typedefs)
		if (auto function_pointer = parse_function_pointer(type_def); function_pointer) {
			auto type = types.append_child("type");
			type.append_attribute("category").set_value("funcpointer");
			type.append_child(pugi::node_pcdata).set_value(
				("typedef " + function_pointer->return_type + "(VKAPI_PTR *").data()
			);
			type.append_child("name").append_child(pugi::node_pcdata).set_value(function_pointer->name.data());
			type.append_child(pugi::node_pcdata).set_value(")(");
			for (auto iterator = function_pointer->parameters.begin()
				 ; iterator != std::prev(function_pointer->parameters.end())
				 ; ++iterator) {

				append_typename(type, iterator->type);
				//type.append_child("type").append_child(pugi::node_pcdata).set_value(
				//	iterator->type.data()
				//);
				type.append_child(pugi::node_pcdata).set_value(
					(" " + iterator->name + ", ").data()
				);
			}
			type.append_child("type").append_child(pugi::node_pcdata).set_value(
				function_pointer->parameters.back().type.data()
			);
			type.append_child(pugi::node_pcdata).set_value(
				(" " + function_pointer->parameters.back().name + ");").data()
			);
		}
}
void append_struct(pugi::xml_node &types, std::vector<vkma_xml::detail::struct_t> const &structs,
				   std::set<std::string> handle_names) {
	types.append_child("comment").append_child(pugi::node_pcdata).set_value("____");
	types.append_child("comment").append_child(pugi::node_pcdata).set_value("Struct types");
	for (auto &structure : structs)
		if (!handle_names.contains(structure.name)) {
			auto type = types.append_child("type");
			type.append_attribute("category").set_value("struct");
			type.append_attribute("name").set_value(structure.name.data());
			for (auto &variable : structure.variables) {
				auto member = type.append_child("member");
				append_typename(member, variable.type);
				//member.append_child("type").append_child(pugi::node_pcdata).set_value(variable.type.data());
				member.append_child(pugi::node_pcdata).set_value(" ");
				member.append_child("name").append_child(pugi::node_pcdata).set_value(variable.name.data());
			}
		}
}

struct constexpr_enum_value_t {
	std::string_view name, value;
};
constexpr std::array result_codes {
	constexpr_enum_value_t{ "VK_SUCCESS", "0" },
	constexpr_enum_value_t{ "VK_NOT_READY", "1" },
	constexpr_enum_value_t{ "VK_TIMEOUT", "2" },
	constexpr_enum_value_t{ "VK_EVENT_SET", "3" },
	constexpr_enum_value_t{ "VK_EVENT_RESET", "4" },
	constexpr_enum_value_t{ "VK_INCOMPLETE", "5" },
	constexpr_enum_value_t{ "VK_ERROR_OUT_OF_HOST_MEMORY", "-1" },
	constexpr_enum_value_t{ "VK_ERROR_OUT_OF_DEVICE_MEMORY", "-2" },
	constexpr_enum_value_t{ "VK_ERROR_INITIALIZATION_FAILED", "-3" },
	constexpr_enum_value_t{ "VK_ERROR_DEVICE_LOST", "-4" },
	constexpr_enum_value_t{ "VK_ERROR_MEMORY_MAP_FAILED", "-5" },
	constexpr_enum_value_t{ "VK_ERROR_LAYER_NOT_PRESENT", "-6" },
	constexpr_enum_value_t{ "VK_ERROR_EXTENSION_NOT_PRESENT", "-7" },
	constexpr_enum_value_t{ "VK_ERROR_FEATURE_NOT_PRESENT", "-8" },
	constexpr_enum_value_t{ "VK_ERROR_INCOMPATIBLE_DRIVER", "-9" },
	constexpr_enum_value_t{ "VK_ERROR_TOO_MANY_OBJECTS", "-10" },
	constexpr_enum_value_t{ "VK_ERROR_FORMAT_NOT_SUPPORTED", "-11" },
	constexpr_enum_value_t{ "VK_ERROR_FRAGMENTED_POOL", "-12" },
	constexpr_enum_value_t{ "VK_ERROR_SURFACE_LOST_KHR", "-1000000000" },
	constexpr_enum_value_t{ "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR", "-1000000001" },
	constexpr_enum_value_t{ "VK_SUBOPTIMAL_KHR", "1000001003" },
	constexpr_enum_value_t{ "VK_ERROR_OUT_OF_DATE_KHR", "-1000001004" },
	constexpr_enum_value_t{ "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR", "-1000003001" },
	constexpr_enum_value_t{ "VK_ERROR_VALIDATION_FAILED_EXT", "-1000011001" },
	constexpr_enum_value_t{ "VK_ERROR_INVALID_SHADER_NV", "-1000012000" }
};
constexpr std::string_view result_code_list = "VK_SUCCESS, VK_NOT_READY, VK_TIMEOUT, VK_EVENT_SET, "
	"VK_EVENT_RESET, VK_INCOMPLETE, VK_ERROR_OUT_OF_HOST_MEMORY, VK_ERROR_OUT_OF_DEVICE_MEMORY, "
	"VK_ERROR_INITIALIZATION_FAILED, VK_ERROR_DEVICE_LOST, VK_ERROR_MEMORY_MAP_FAILED, "
	"VK_ERROR_LAYER_NOT_PRESENT, VK_ERROR_EXTENSION_NOT_PRESENT, VK_ERROR_FEATURE_NOT_PRESENT, "
	"VK_ERROR_INCOMPATIBLE_DRIVER, VK_ERROR_TOO_MANY_OBJECTS, VK_ERROR_FORMAT_NOT_SUPPORTED, "
	"VK_ERROR_FRAGMENTED_POOL, VK_ERROR_SURFACE_LOST_KHR, VK_ERROR_NATIVE_WINDOW_IN_USE_KHR, "
	"VK_SUBOPTIMAL_KHR, VK_ERROR_OUT_OF_DATE_KHR, VK_ERROR_INCOMPATIBLE_DISPLAY_KHR, "
	"VK_ERROR_VALIDATION_FAILED_EXT, VK_ERROR_INVALID_SHADER_NV";

constexpr std::array index_types {
	constexpr_enum_value_t{ "VK_INDEX_TYPE_UINT16", "0" },
	constexpr_enum_value_t{ "VK_INDEX_TYPE_UINT32", "1" }
};

void append_vk_enums(pugi::xml_node &registry, std::set<std::string> const &handle_names) {
	registry.append_child("comment").append_child(pugi::node_pcdata).set_value("____");
	registry.append_child("comment").append_child(pugi::node_pcdata).set_value(
		"enums required by Vulkan-HPP"
	);

	auto result = registry.append_child("enums");
	result.append_attribute("name").set_value("VkResult");
	result.append_attribute("type").set_value("enum");
	for (auto &code : result_codes) {
		auto temp = result.append_child("enum");
		temp.append_attribute("value").set_value(code.value.data());
		temp.append_attribute("name").set_value(code.name.data());
	}

	auto structure_type = registry.append_child("enums");
	structure_type.append_attribute("name").set_value("VkStructureType");
	structure_type.append_attribute("type").set_value("enum");

	auto object_type = registry.append_child("enums");
	object_type.append_attribute("name").set_value("VkObjectType");
	object_type.append_attribute("type").set_value("enum");
	for (size_t counter = 1'000; auto &handle : handle_names) {
		auto temp = object_type.append_child("enum");
		temp.append_attribute("value").set_value(std::to_string(counter++).data());
		temp.append_attribute("name").set_value(to_objtypeenum(handle).data());
	}

	auto index_type = registry.append_child("enums");
	index_type.append_attribute("name").set_value("VkIndexType");
	index_type.append_attribute("type").set_value("enum");
	for (auto &type : index_types) {
		auto temp = index_type.append_child("enum");
		temp.append_attribute("value").set_value(type.value.data());
		temp.append_attribute("name").set_value(type.name.data());
	}

	auto debug_object_type = registry.append_child("enums");
	debug_object_type.append_attribute("name").set_value("VkDebugReportObjectTypeEXT");
	debug_object_type.append_attribute("type").set_value("enum");
}

void append_enumerations(pugi::xml_node &registry, std::vector<vkma_xml::detail::enum_t> const &enumerations) {
	registry.append_child("comment").append_child(pugi::node_pcdata).set_value("____");
	registry.append_child("comment").append_child(pugi::node_pcdata).set_value(
		"Enumeration definitions"
	);
	for (auto &enumeration : enumerations)
		if (std::string_view(enumeration.name).substr(enumeration.name.size() - 8) != "FlagBits") {
			auto enums = registry.append_child("enums");
			enums.append_attribute("name").set_value(enumeration.name.data());
			enums.append_attribute("type").set_value("enum");
			for (auto &enumerator : enumeration.values) {
				auto enum_ = enums.append_child("enum");
				enum_.append_attribute("value").set_value(enumerator.value.data());
				enum_.append_attribute("name").set_value(enumerator.name.data());
			}
		}

	registry.append_child("comment").append_child(pugi::node_pcdata).set_value("____");
	registry.append_child("comment").append_child(pugi::node_pcdata).set_value(
		"Flags"
	);
	for (auto &enumeration : enumerations)
		if (std::string_view(enumeration.name).substr(enumeration.name.size() - 8) == "FlagBits") {
			auto enums = registry.append_child("enums");
			enums.append_attribute("name").set_value(enumeration.name.data());
			enums.append_attribute("type").set_value("bitmask");
			for (auto &enumerator : enumeration.values) {
				auto enum_ = enums.append_child("enum");
				enum_.append_attribute("value").set_value(enumerator.value.data());
				enum_.append_attribute("name").set_value(enumerator.name.data());
			}
		}
}

void append_commands(pugi::xml_node &registry, std::vector<vkma_xml::detail::function_t> const &functions) {
	registry.append_child("comment").append_child(pugi::node_pcdata).set_value("____");
	registry.append_child("comment").append_child(pugi::node_pcdata).set_value(
		"Command definitions"
	);
	auto commands = registry.append_child("commands");
	commands.append_attribute("comment").set_value("VMA command definitions");
	for (auto &function : functions) {
		auto command = commands.append_child("command");
		if (function.return_type == "VkResult") {
			command.append_attribute("successcodes").set_value("VK_SUCCESS");
			command.append_attribute("errorcodes").set_value(result_code_list.data());
		}
		auto proto = command.append_child("proto");
		proto.append_child("type").append_child(pugi::node_pcdata).set_value(function.return_type.data());
		proto.append_child(pugi::node_pcdata).set_value(" ");
		proto.append_child("name").append_child(pugi::node_pcdata).set_value(function.name.data());

		for (auto &parameter : function.parameters) {
			auto param = command.append_child("param");
			append_typename(param, parameter.type);
			//param.append_child("type").append_child(pugi::node_pcdata).set_value(parameter.type.data());
			param.append_child(pugi::node_pcdata).set_value(" ");
			param.append_child("name").append_child(pugi::node_pcdata).set_value(parameter.name.data());
		}
	}
}

void append_api(pugi::xml_node &registry, vkma_xml::detail::data_t const &data) {
	auto feature = registry.append_child("feature");
	feature.append_attribute("api").set_value("vma");
	feature.append_attribute("name").set_value("VMA_VERSION_2_3");
	feature.append_attribute("number").set_value("2.3");
	feature.append_attribute("comment").set_value("VMA API interface definitions");

	auto headers = feature.append_child("require");
	headers.append_attribute("comment").set_value("headers");
	headers.append_child("type").append_attribute("name").set_value("vulkan");

	auto structs = feature.append_child("require");
	structs.append_attribute("comment").set_value("structs");
	for (auto &structure : data.structs)
		structs.append_child("type").append_attribute("name").set_value(structure.name.data());

	auto defines = feature.append_child("require");
	defines.append_attribute("comment").set_value("defines");
	for (auto &define : data.defines)
		defines.append_child("type").append_attribute("name").set_value(define.name.data());

	auto typedefs = feature.append_child("require");
	typedefs.append_attribute("comment").set_value("typedefs");
	for (auto &type_def : data.typedefs)
		typedefs.append_child("type").append_attribute("name").set_value(type_def.name.data());

	auto functions = feature.append_child("require");
	functions.append_attribute("comment").set_value("functions");
	for (auto &function : data.functions)
		functions.append_child("command").append_attribute("name").set_value(function.name.data());
}

void append_misc(pugi::xml_node &registry) {
	auto extensions = registry.append_child("extensions");
	extensions.append_attribute("comment").set_value("empty");
	auto extension = extensions.append_child("extension");
	extension.append_attribute("name").set_value("VK_WC_why_y_y_y_y");
	extension.append_attribute("number").set_value("1");
	extension.append_attribute("type").set_value("instance");
	extension.append_attribute("author").set_value("WC");
	extension.append_attribute("contact").set_value("@cvelth");
	extension.append_attribute("supported").set_value("disabled");

	// Skip 'spirvextensions' if it can be avoided
	auto spirvextensions = registry.append_child("spirvextensions");
	spirvextensions.append_attribute("comment").set_value("empty");

	// Skip 'spirvcapabilities' if it can be avoided
	auto spirvcapabilities = registry.append_child("spirvcapabilities");
	spirvcapabilities.append_attribute("comment").set_value("empty");
}

std::optional<pugi::xml_document> vkma_xml::generate(detail::data_t const &data) {
	auto output = std::make_optional<pugi::xml_document>();
	auto registry = output->append_child("registry");

	append_header(registry);
	{
		auto types = registry.append_child("types");
		types.append_attribute("comment").set_value("VMA type definitions");

		append_includes(types);
		append_defines(types, data.defines);
		append_basic(types);
		append_vulkan(types, data.vulkan_type_names);
		append_bitmask(types, data.typedefs, data.enums);
		append_handle(types, data.handle_names);
		append_enum(types, data.enums);
		append_function_pointer(types, data.typedefs);
		append_struct(types, data.structs, data.handle_names);
	}
	append_enumerations(registry, data.enums);
	append_vk_enums(registry, data.handle_names);
	append_commands(registry, data.functions);
	append_api(registry, data);
	append_misc(registry);

	return std::move(output);
}
*/

inline vkma_xml::detail::type_registry::underlying_t::iterator
vkma_xml::detail::type_registry::get(std::string &&name) {
	auto [iterator, result] = underlying.try_emplace(
		std::move(name),
		type::undefined{}
	);
	if (result) ++incomplete_type_counter;
	return iterator;
}
inline vkma_xml::detail::type_registry::underlying_t::iterator
vkma_xml::detail::type_registry::add(std::string &&name, type_t &&type_data) {
	auto [iterator, result] = underlying.try_emplace(
		std::move(name),
		std::move(type_data)
	);

	if (!result) 
		if (std::holds_alternative<type::undefined>(iterator->second)) {
			iterator->second = std::move(type_data);
			if (!std::holds_alternative<type::undefined>(iterator->second))
				--incomplete_type_counter;
		} else {
			std::cout << "Warning: Attempt to define a typename '" << iterator->first
				<< "' more than once: second definition ignored.";
			return iterator;
		}

	struct on_add_visitor {
		vkma_xml::detail::type_registry &registry_ref;

		void operator()(type::undefined const &) {}
		void operator()(type::structure const &structure) {
			for (auto const &member : structure.members)
				registry_ref.get(member.type.name);
		}
		void operator()(type::handle const &) {}
		void operator()(type::external const &) {}
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
		void operator()(type::alias const alias) {
			registry_ref.get(alias.real_type.name);
		}
	};
	std::visit(on_add_visitor{ *this }, iterator->second);
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

vkma_xml::detail::decorated_typename_t::decorated_typename_t(std::string input) : name(input) {
	while (true) {
		if (name.size() > 1 && (name[0] == '*' || name[0] == ' ' || name[0] == '&')) {
			prefix += name[0];
			name.erase(0, 1);
		} else if (name.size() > 5 && std::string_view(name).substr(0, 5) == "const") {
			prefix += std::string_view(name).substr(0, 5);
			name.erase(0, 5);
		} else
			break;
	}
	while (true) {
		if (size_t last = name.size() - 1; name.size() > 1 && (
			name[last] == '*' || name[last] == ' ' || name[last] == '&'
		)) {
			postfix.insert(0, &name[last], 1);
			name.erase(last, 1);
		} else if (size_t post = name.size() - 5; name.size() > 5 && 
				   std::string_view(name).substr(post) == "const") {
			postfix.insert(0, std::string_view(name).substr(post));
			name.erase(post, 5);
		} else
			break;
	}
}

std::optional<vkma_xml::detail::variable_t> 
vkma_xml::detail::api_t::load_variable(pugi::xml_node const &xml) {
	if (auto name = xml.child("name"), type = xml.child("type"); name && type)
		return std::make_optional<variable_t>(to_string(name), to_string(type));
	return std::nullopt;
}
std::optional<vkma_xml::detail::constant_t> 
vkma_xml::detail::api_t::load_define(pugi::xml_node const &xml) {
	if (auto name = xml.child("name"), value = xml.child("initializer"); name && value)
		return std::make_optional<constant_t>(to_string(name), to_string(value));
	return std::nullopt;
}
std::optional<vkma_xml::detail::constant_t> 
vkma_xml::detail::api_t::load_enum_value(pugi::xml_node const &xml) {
	if (auto name = xml.child("name"), value = xml.child("initializer"); name && value)
		if (auto value_str = to_string(value); std::string_view(value_str).substr(0, 2) == "= ")
			return std::make_optional<constant_t>(to_string(name), value_str.substr(2));
		else
			return std::make_optional<constant_t>(to_string(name), value_str);
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
			if (auto value = load_enum_value(child); value)
				output.state.values.emplace_back(*value);
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

bool vkma_xml::detail::api_t::load_struct(pugi::xml_node const &xml) {
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

	if (name != "") {
		registry.add(name, std::move(structure));
		return true;
	} else
		std::cout << "Warning: Ignore a struct compound without a name.\n";
	return false;
}

bool vkma_xml::detail::api_t::load_file(pugi::xml_node const &xml) {
	for (auto &child : xml.children())
		if (child.name() == "sectiondef"sv)
			for (auto &member : child.children())
				if (member.name() == "memberdef"sv) {
					if (auto kind = member.attribute("kind").value(); kind == "define"sv) {
						if (auto define = load_define(member); define)
							registry.add(std::move(define->name), type::macro{ std::move(define->value) });
					} else if (kind == "enum"sv) {
						if (auto enumeration = load_enum(member); enumeration)
							registry.add(std::move(enumeration->name),
									  type::enumeration{ std::move(enumeration->state) });
					} else if (kind == "typedef"sv) {
						if (auto type_def = load_typedef(member); type_def)
							registry.add(std::move(type_def->name), type::alias{ std::move(type_def->type) });
					} else if (kind == "function"sv) {
						if (auto function = load_function(member); function)
							registry.add(std::move(function->name),
									  type::function{ std::move(function->state) });
					} else
						std::cout << "Ignore an unknown file entry '" << kind << "'.\n";
				}
	return false;
}

bool vkma_xml::detail::api_t::load_compound(std::string_view refid, std::filesystem::path const &directory) {
	std::filesystem::path file_path = directory; (file_path /= refid.data()) += ".xml";
	if (auto compound_xml = detail::load_xml(file_path); compound_xml)
		if (auto doxygen = compound_xml->child("doxygen"); doxygen)
			if (auto compound = doxygen.child("compounddef"); compound)
				if (std::string_view kind = compound.attribute("kind").value(); kind == "struct"sv)
					return load_struct(compound);
				else if (kind == "file"sv)
					return load_file(compound);
				else
					std::cout << "Warning: Ignore a compound of an unknown kind: '" << kind << "'.\n";
	return false;
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
	
	if (auto main_api_index = detail::load_xml(main_api.xml_directory.string() + "/index.xml"); main_api_index)
		if (auto index = main_api_index->child("doxygenindex"); index) {
			detail::api_t api;
			for (auto const &compound : index.children())
				if (compound.name() == "compound"sv)
					if (auto kind = compound.attribute("kind").value(); kind == "struct"sv || kind == "file"sv)
						api.load_compound(compound.attribute("refid").value(), main_api.xml_directory);
					else if (kind == "page"sv || kind == "dir"sv) {
						// Silently ignore 'page' and 'dir' index entries.
					} else
						std::cout << "Warning: Ignore a compound of an unknown kind: '" << kind << "'.\n";
				else
					std::cout << "Warning: Ignore an unknown node: " << compound.name() << '\n';
	
			auto handles = detail::load_handle_list(main_api.header_files);
			for (auto const &handle : handles)
				api.registry.add(std::move(handle.first), detail::type::handle{ handle.second });

			// TODO: Use helpers to verify and extend the api
	
			return api;
		}
	return std::nullopt;
}

std::optional<pugi::xml_document> vkma_xml::generate(detail::api_t const &data) {
	auto output = std::make_optional<pugi::xml_document>();
	auto registry = output->append_child("registry");

	for (auto const &type : data.registry)
		std::visit(vkma_xml::detail::type_t_printer{ std::cout, type.first }, type.second);

	// TODO: Implement the generator

	return std::move(output);
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

	std::filesystem::path const output_path = "../ouput/vkma.xml";

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