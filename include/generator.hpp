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

		bool parse_header(std::filesystem::path const &path, detail::data_t &data);
		*/

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
			struct handle {};
			struct external {};
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
		}
		using type_t = std::variant<
			type::undefined,
			type::structure,
			type::handle,
			type::external,
			type::macro,
			type::enumeration,
			type::function,
			type::alias
		>;

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
			size_t incomplete_type_counter = 0u;
		};

		struct api_t {
			static std::optional<variable_t> load_variable(pugi::xml_node const &xml);
			static std::optional<constant_t> load_define(pugi::xml_node const &xml);
			static std::optional<constant_t> load_enum_value(pugi::xml_node const &xml);
			static std::optional<enum_t> load_enum(pugi::xml_node const &xml);
			static std::optional<variable_t> load_typedef(pugi::xml_node const &xml);
			static std::optional<variable_t> load_function_parameter(pugi::xml_node const &xml);
			static std::optional<function_t> load_function(pugi::xml_node const &xml);

			bool load_struct(pugi::xml_node const &xml);
			bool load_file(pugi::xml_node const &xml);
			bool load_compound(std::string_view refid, std::filesystem::path const &directory);

		public:
			type_registry registry;
		};

		std::optional<pugi::xml_document> load_xml(std::filesystem::path const &file);

		struct type_t_printer {
			std::ostream &stream_ref;
			identifier_t const &name_ref;

			void operator()(vkma_xml::detail::type::undefined const &) {
				stream_ref << "- " << name_ref << " - an undefined type.\n";
			}
			void operator()(vkma_xml::detail::type::structure const &structure) {
				stream_ref << "- " << name_ref << " - a struct {\n";
				for (auto const &member : structure.members)
					stream_ref << "    " << member.type << ' ' << member.name << ";\n";
				stream_ref << "};\n";
			}
			void operator()(vkma_xml::detail::type::handle const &) {
				stream_ref << "- " << name_ref << " - an object handle.\n";
			}
			void operator()(vkma_xml::detail::type::external const &) {
				stream_ref << "- " << name_ref << " - an external type.\n";
			}
			void operator()(vkma_xml::detail::type::macro const &macro) {
				stream_ref << "- " << name_ref << " - a preprocessor macro.\n"
					<< "    " << macro.value << "\n";
			}
			void operator()(vkma_xml::detail::type::enumeration const &enumeration) {
				stream_ref << "- " << name_ref << " - an enumeration";
				if (enumeration.type)
					stream_ref << " : " << *enumeration.type;
				stream_ref << " {\n";
				for (auto const &enumerator : enumeration.values)
					stream_ref << "    " << enumerator.name << " = " << enumerator.value << ";\n";
				stream_ref << "};\n";
			}
			void operator()(vkma_xml::detail::type::function const &function) {
				stream_ref << "- " << name_ref << " - a function: "
					<< function.return_type << "(&)(\n";
				for (auto const &parameter : function.parameters)
					stream_ref << "    " << parameter.type << ' ' << parameter.name << ",\n";
				stream_ref << ");\n";
			}
			void operator()(vkma_xml::detail::type::alias const alias) {
				stream_ref << "- " << name_ref << " - an alias for " << alias.real_type << "\n";
			}
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