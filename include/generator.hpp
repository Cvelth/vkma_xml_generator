#pragma once
#include "pugixml.hpp"

#include <filesystem>
#include <optional>

namespace vma_xml {
	namespace detail {
		struct entity_t {
			std::string_view kind;
			std::string_view refid;
		};
		struct variable_t {
			std::string_view name;
			std::string_view refid;
		};
		struct struct_t {
			std::string_view name;
			std::string_view refid;
			std::vector<variable_t> variables;
		};
		struct define_t {
			std::string_view name;
			std::string_view refid;
		};
		struct enum_value_t {
			std::string_view name;
			std::string_view refid;
		};
		struct enum_t {
			std::string_view name;
			std::string_view refid;
			std::vector<enum_value_t> values;
		};
		struct typedef_t {
			std::string_view name;
			std::string_view refid;
		};
		struct function_t {
			std::string_view name;
			std::string_view refid;
		};
		struct file_t {
			std::string_view name;
			std::string_view refid;
			std::vector<define_t> defines;
			std::vector<enum_t> enums;
			std::vector<typedef_t> typedefs;
			std::vector<function_t> functions;
		};
		struct data_t {
			std::vector<struct_t> structs;
			std::vector<file_t> files;
		};
	}
	detail::entity_t parse_attributes(pugi::xml_node const &input);

	detail::variable_t parse_variable(pugi::xml_node const &input, std::string_view refid);
	detail::struct_t parse_struct(pugi::xml_node const &input, std::string_view refid);

	detail::define_t parse_define(pugi::xml_node const &input, std::string_view refid);
	detail::enum_value_t parse_enumvalue(pugi::xml_node const &input, std::string_view refid);
	detail::enum_t parse_enum(pugi::xml_node const &input, std::string_view refid);
	detail::typedef_t parse_typedef(pugi::xml_node const &input, std::string_view refid);
	detail::function_t parse_function(pugi::xml_node const &input, std::string_view refid);
	detail::file_t parse_file(pugi::xml_node const &input, std::string_view refid);

	std::optional<detail::data_t> parse_index(pugi::xml_document const &index_file);

	std::optional<pugi::xml_document> load_xml(std::filesystem::path const &file);

	std::optional<pugi::xml_document> generate(std::filesystem::path const &directory);
}