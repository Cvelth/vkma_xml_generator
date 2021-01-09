#pragma once
#include "pugixml.hpp"

#include <filesystem>
#include <optional>

namespace vma_xml {
	namespace detail {
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
	}

	std::optional<detail::data_t> parse(std::filesystem::path const &directory);
	std::optional<pugi::xml_document> generate(detail::data_t const &data);
	inline std::optional<pugi::xml_document> generate(std::filesystem::path const &directory) {
		if (auto data = parse(directory); data)
			return generate(*data);
		else
			return std::nullopt;
	}
}