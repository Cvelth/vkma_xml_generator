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
		struct struct_t {
			std::string name;
			std::vector<variable_t> variables;
		};
		struct data_t {
			std::vector<struct_t> structs;
		};
		
		std::optional<pugi::xml_document> load_xml(std::filesystem::path const &file);
		std::optional<variable_t> parse_variable(pugi::xml_node const &xml);
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