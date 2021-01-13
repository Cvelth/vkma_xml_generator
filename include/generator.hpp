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
			operator std::string() {
				return prefix + name + postfix;
			}
			operator bool() const { return !name.empty(); }
			bool operator!() const { return name.empty(); }
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
			underlying_t::iterator add(identifier_t &&name, type_t &&type_data);

			underlying_t::iterator get(std::string_view name) {
				identifier_t temp(name);
				return get(std::move(temp));
			}
			underlying_t::iterator add(std::string_view name, type_t &&type_data) {
				identifier_t temp(name);
				return add(std::move(temp), std::move(type_data));
			}

			auto begin() const { return underlying.begin(); }
			auto end() const { return underlying.end(); }
			auto empty() const { return underlying.empty(); }
			auto size() const { return underlying.size(); }

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

			void load_struct(pugi::xml_node const &xml, type_tag tag);
			void load_file(pugi::xml_node const &xml, type_tag tag);
			void load_compound(std::string_view refid, std::filesystem::path const &directory, type_tag tag);
			void load_helper(input const &helper_api);

		public:
			type_registry registry;
		};

		std::optional<pugi::xml_document> load_xml(std::filesystem::path const &file);
		std::map<identifier_t, bool> load_handle_list(std::vector<std::filesystem::path> const &files);

		struct type_t_printer {
			std::ostream &stream_ref;
			identifier_t const &name_ref;
			type_tag tag;

			void operator()(vkma_xml::detail::type::undefined const &) {
				if (tag == type_tag::core)
					stream_ref << "- " << name_ref << " - an undefined type.\n";
			}
			void operator()(vkma_xml::detail::type::structure const &structure) {
				if (tag == type_tag::core) {
					stream_ref << "- " << name_ref << " - a struct {\n";
					for (auto const &member : structure.members)
						stream_ref << "    " << member.type << ' ' << member.name << ";\n";
					stream_ref << "};\n";
				}
			}
			void operator()(vkma_xml::detail::type::handle const &handle) {
				if (tag == type_tag::core)
					stream_ref << "- " << name_ref << " - an "
						<< (handle.dispatchable ? "" : "non-dispatchable ")
						<< "object handle.\n";
			}
			void operator()(vkma_xml::detail::type::macro const &macro) {
				if (tag == type_tag::core)
					stream_ref << "- " << name_ref << " - a preprocessor macro.\n"
						<< "    " << macro.value << "\n";
			}
			void operator()(vkma_xml::detail::type::enumeration const &enumeration) {
				if (tag == type_tag::core) {
					stream_ref << "- " << name_ref << " - an enumeration";
					if (enumeration.type)
						stream_ref << " : " << *enumeration.type;
					stream_ref << " {\n";
					for (auto const &enumerator : enumeration.values)
						stream_ref << "    " << enumerator.name << " = " << enumerator.value << ";\n";
					stream_ref << "};\n";
				}
			}
			void operator()(vkma_xml::detail::type::function const &function) {
				if (tag == type_tag::core) {
					stream_ref << "- " << name_ref << " - a function: "
						<< function.return_type << "(&)(\n";
					for (auto const &parameter : function.parameters)
						stream_ref << "    " << parameter.type << ' ' << parameter.name << ",\n";
					stream_ref << ");\n";
				}
			}
			void operator()(vkma_xml::detail::type::alias const alias) {
				if (tag == type_tag::core)
					stream_ref << "- " << name_ref << " - an alias for " << alias.real_type << "\n";
			}
			void operator()(vkma_xml::detail::type::base const &) {
				if (tag == type_tag::core)
					stream_ref << "- " << name_ref << " - a base type.\n";
			}
		};

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