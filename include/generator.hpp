// Copyright (c) 2021 Cvelth <cvelth.mail@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once
#include "pugixml.hpp"

#include <concepts>
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <type_traits>
#include <variant>
#include <vector>
#include <unordered_map>
//#include <set>

namespace vkma_xml {
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

		std::optional<define_t> parse_define(pugi::xml_node const &xml);
		std::optional<enum_value_t> parse_enum_value(pugi::xml_node const &xml);
		std::optional<enum_t> parse_enum(pugi::xml_node const &xml);
		std::optional<typedef_t> parse_typedef(pugi::xml_node const &xml);
		std::optional<variable_t> parse_function_parameter(pugi::xml_node const &xml);
		std::optional<function_t> parse_function(pugi::xml_node const &xml);

		bool parse_header(std::filesystem::path const &path, detail::data_t &data);
		*/

		struct variable_t {
			std::string name;
			std::string type;
		};

		namespace type {
			struct undefined {};
			struct structure {
				std::vector<variable_t> members;
			};
			struct handle {};
			struct external {};
			struct alias {
				std::string real_name;
			};
		}
		using type_t = std::variant<
			type::undefined,
			type::structure,
			type::handle,
			type::external,
			type::alias
		>;

		class type_registry {
		protected:
			struct underlying_comparator_t : std::equal_to<> {
				using is_transparent = void;
			};
			struct underlying_hash_t {
				using is_transparent = underlying_comparator_t::is_transparent;
				using transparent_key_equal = underlying_comparator_t;
				size_t operator()(std::string_view txt) const { return std::hash<std::string_view>{}(txt); }
				size_t operator()(std::string const &txt) const { return std::hash<std::string_view>{}(txt); }
				size_t operator()(char const *txt) const { return std::hash<std::string_view>{}(txt); }
			};
			using underlying_t = std::pmr::unordered_map<std::string, type_t,
														 underlying_hash_t,
														 underlying_comparator_t>;
		public:
			inline underlying_t::iterator get(std::string_view name);
			inline underlying_t::iterator add(std::string_view name, type_t &&type_data);

		protected:
			underlying_t underlying;
			size_t incomplete_type_counter = 0u;
		};

		struct api_t {
			static std::optional<variable_t> load_variable(pugi::xml_node const &xml);

			bool load_struct(pugi::xml_node const &xml);
			bool load_file(pugi::xml_node const &xml);
			bool load_compound(std::string_view refid, std::filesystem::path const &directory);

		public:
			type_registry types;
		};

		std::optional<pugi::xml_document> load_xml(std::filesystem::path const &file);
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