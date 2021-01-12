// Copyright (c) 2021 Cvelth <cvelth.mail@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once
#include "pugixml.hpp"

#include <concepts>
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <type_traits>
//#include <set>
//#include <vector>

namespace vma_xml {
	namespace detail {
		/*
		struct variable_t {
			std::string name;
			std::string type;
		};
		struct define_t {
			std::string name;
			std::string value;
		};
		struct enum_value_t {
			std::string name;
			std::string value;
		};
		struct enum_t {
			std::string name;
			std::optional<std::string> type;
			std::vector<enum_value_t> values;
		};
		struct typedef_t {
			std::string name;
			std::string type;
		};
		struct function_t {
			std::string name;
			std::string return_type;
			std::vector<variable_t> parameters;
		};
		
		struct struct_t {
			std::string name;
			std::vector<variable_t> variables;
		};
		struct data_t {
			std::vector<struct_t> structs;
			std::vector<define_t> defines;
			std::vector<typedef_t> typedefs;
			std::vector<function_t> functions;
			std::vector<enum_t> enums;
		
			std::set<std::string> handle_names;
			std::set<std::string> vulkan_type_names;
		};
		
		std::optional<pugi::xml_document> load_xml(std::filesystem::path const &file);
		
		std::optional<variable_t> parse_variable(pugi::xml_node const &xml);
		std::optional<define_t> parse_define(pugi::xml_node const &xml);
		std::optional<enum_value_t> parse_enum_value(pugi::xml_node const &xml);
		std::optional<enum_t> parse_enum(pugi::xml_node const &xml);
		std::optional<typedef_t> parse_typedef(pugi::xml_node const &xml);
		std::optional<variable_t> parse_function_parameter(pugi::xml_node const &xml);
		std::optional<function_t> parse_function(pugi::xml_node const &xml);
		
		bool parse_struct(pugi::xml_node const &xml, detail::data_t &data);
		bool parse_file(pugi::xml_node const &xml, detail::data_t &data);
		bool parse_compound(std::string_view refid, std::filesystem::path const &directory, 
							detail::data_t &data);
		bool parse_header(std::filesystem::path const &path, detail::data_t &data);
		*/

		struct api_t {

		};
	}

	struct input {
		std::filesystem::path const &xml_directory;
		std::vector<std::filesystem::path> const &header_files;
	};
	namespace detail {
		template<typename T> concept parser_input = std::is_same<T, input>::value;
	}

	std::optional<detail::api_t> parse(input main_api, std::initializer_list<input> const &helper_apis);
	template <detail::parser_input ...helper_api_ts>
	std::optional<detail::api_t> parse(input main_api, helper_api_ts ...helper_apis) {
		return parse(main_api, std::initializer_list<input>{ helper_apis... });
	}

	std::optional<pugi::xml_document> generate(detail::api_t const &data);
	template <detail::parser_input ...helper_api_ts>
	std::optional<pugi::xml_document> generate(input main_api, helper_api_ts ...helper_apis) {
		if (auto data = parse(main_api, helper_apis...); data)
			return generate(*data);
		else
			return std::nullopt;
	}
}