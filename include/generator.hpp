// Copyright (c) 2021 Cvelth <cvelth.mail@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once
#include "pugixml.hpp"

#include <array>
#include <concepts>
#include <filesystem>
#include <initializer_list>
#include <map>
#include <optional>
#include <set>
#include <type_traits>
#include <variant>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace vkma_xml {
	struct input {
		std::filesystem::path const &xml_directory;
		std::vector<std::filesystem::path> const &header_files;
	};
	namespace detail {
		template<typename T> concept parser_input = std::is_same<T, input>::value;
		using transparent_set = std::set<std::string, std::less<>>;

		using identifier_t = std::string;
		using value_t = std::string;
		struct decorated_typename_t {
			std::string prefix;
			identifier_t name;
			std::string postfix;

			decorated_typename_t() = default;
			decorated_typename_t(std::string input);
			operator std::string() const {
				return prefix + name + postfix;
			}
			operator bool() const { return !name.empty(); }
			bool operator!() const { return name.empty(); }

			auto to_string() const { return std::string(*this); }
		};
		inline std::ostream &operator<<(std::ostream &stream, decorated_typename_t const &type) {
			return stream << type.prefix << type.name << type.postfix;
		}

		struct variable_t {
			identifier_t name;
			decorated_typename_t type;
		};
		struct constant_t {
			identifier_t name;
			value_t value;
		};

		namespace type {
			struct undefined {};
			struct structure {
				std::vector<variable_t> members;
			};
			struct handle {
				bool dispatchable;
				std::optional<identifier_t> parent;
			};
			struct macro {
				value_t value;
			};
			struct enumeration {
				std::optional<decorated_typename_t> type;
				std::vector<constant_t> values;
			};
			struct function {
				decorated_typename_t return_type;
				std::vector<variable_t> parameters;
			};
			struct function_pointer {
				decorated_typename_t return_type;
				std::vector<variable_t> parameters;
			};
			struct alias {
				decorated_typename_t real_type;
			};
			struct base {};
		}

		enum class type_tag {
			core,
			helper
		};
		struct type_t {
			using state_t = std::variant<
				type::undefined,
				type::structure,
				type::handle,
				type::macro,
				type::enumeration,
				type::function,
				type::function_pointer,
				type::alias,
				type::base
			>;
			state_t state;
			type_tag tag;
		};

		struct enum_t {
			identifier_t name;
			type::enumeration state;
		};
		struct function_t {
			identifier_t name;
			type::function state;
		};

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
			using underlying_t = std::pmr::unordered_map<identifier_t, type_t,
														 underlying_hash_t,
														 underlying_comparator_t>;
		public:
			underlying_t::iterator get(identifier_t &&name);
			inline auto get(std::string_view name) {
				return get(identifier_t(name));
			}
			underlying_t::iterator add(identifier_t &&name, type_t &&type_data);
			inline auto add(std::string_view name, type_t &&type_data) {
				return add(identifier_t(name), std::move(type_data));
			}

			inline auto contains(std::string_view name) const { return underlying.contains(name); }
			inline auto find(std::string_view name) const { return underlying.find(name); }
			inline auto find(std::string_view name) { return underlying.find(name); }
			inline auto begin() const { return underlying.begin(); }
			inline auto end() const { return underlying.end(); }
			inline auto empty() const { return underlying.empty(); }
			inline auto size() const { return underlying.size(); }

		protected:
			underlying_t underlying;
		};

		struct api_t {
			static std::optional<variable_t> load_variable(pugi::xml_node const &xml);
			static std::optional<constant_t> load_define(pugi::xml_node const &xml);
			static std::optional<constant_t> load_enum_value(pugi::xml_node const &xml);
			static std::optional<enum_t> load_enum(pugi::xml_node const &xml);
			static std::optional<variable_t> load_typedef(pugi::xml_node const &xml);
			static std::optional<variable_t> load_function_parameter(pugi::xml_node const &xml);
			static std::optional<function_t> load_function(pugi::xml_node const &xml);
			static std::optional<type::function_pointer> load_function_pointer(std::string_view type_name);

			void load_struct(pugi::xml_node const &xml, type_tag tag);
			void load_file(pugi::xml_node const &xml, type_tag tag);
			void load_compound(std::string_view refid, std::filesystem::path const &directory, type_tag tag);
			void load_helper(input const &helper_api);

		public:
			type_registry registry;
		};

		struct generator_t {
			static void append_typename(pugi::xml_node &xml, decorated_typename_t const &type);

			void append_header();
			void append_types();
			void append_enumerations();

		public:
			generator_t(api_t const &api);
		public:
			api_t const &api;
			std::unordered_set<std::string_view> appended;
			std::optional<pugi::xml_document> output;
			std::optional<pugi::xml_node> registry;
		};

		std::optional<pugi::xml_document> load_xml(std::filesystem::path const &file);
		std::map<identifier_t, type::handle> load_handle_list(std::vector<std::filesystem::path> const &files);

		using namespace std::string_view_literals;
		constexpr std::array base_types = {
			"void"sv, "size_t"sv,
			"char"sv, "signed char"sv, "unsigned char"sv,
			"short"sv, "signed short"sv, "unsigned short"sv,
			"int"sv, "signed int"sv, "unsigned int"sv,
			"long"sv, "signed long"sv, "unsigned long"sv,
			"long long"sv, "signed long long"sv, "unsigned long long"sv,
			"float"sv, "double"sv, "long double"sv,

			"int8_t"sv, "uint8_t"sv,
			"int16_t"sv, "uint16_t"sv,
			"int32_t"sv, "uint32_t"sv,
			"int64_t"sv, "uint64_t"sv
		};
		constexpr std::array accepted_prefixes {
			"*"sv, "&"sv, " "sv, "const"sv,
			"enum"sv, "struct"sv, "union"sv
		};
		constexpr std::array accepted_postfixes {
			"*"sv, "&"sv, " "sv, "const"sv
		};
	}

	std::optional<detail::api_t> parse(input main_api, std::initializer_list<input> const &helper_apis);
	template <detail::parser_input ...helper_api_ts>
	std::optional<detail::api_t> parse(input main_api, helper_api_ts ...helper_apis) {
		return parse(main_api, std::initializer_list<input>{ helper_apis... });
	}

	std::optional<pugi::xml_document> generate(detail::api_t const &api);
	template <detail::parser_input ...helper_api_ts>
	std::optional<pugi::xml_document> generate(input main_api, helper_api_ts ...helper_apis) {
		if (auto api = parse(main_api, helper_apis...); api)
			return generate(*api);
		else
			return std::nullopt;
	}
}