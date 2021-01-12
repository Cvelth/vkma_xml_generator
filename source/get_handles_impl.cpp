// Copyright (c) 2021 Cvelth <cvelth.mail@gmail.com>
// SPDX-License-Identifier: MIT

#include "generator.hpp"

#pragma warning(push)
#pragma warning(disable: 4702)
#include "ctre.hpp"
#pragma warning(pop)

#include <fstream>
#include <iostream>

static constexpr auto handle_pattern = ctll::fixed_string{ R"(VK_DEFINE_HANDLE\(([A-Za-z_0-9]+)\))" };
static constexpr auto nd_handle_pattern = ctll::fixed_string{ 
	R"(VK_DEFINE_NON_DISPATCHABLE_HANDLE\(([A-Za-z_0-9]+)\))" 
};

std::map<vkma_xml::detail::identifier_t, bool>
vkma_xml::detail::load_handle_list(std::vector<std::filesystem::path> const &files) {
	std::map<identifier_t, bool> output;

	for (auto const &file : files)
		if (std::ifstream stream(file, std::fstream::ate); stream) {
			size_t source_size = stream.tellg();
			std::string source(source_size, '\0');
			stream.seekg(0);
			stream.read(source.data(), source_size);
			for (auto search_result = ctre::search<handle_pattern>(source); search_result;) {
				output.emplace(search_result.get<1>().to_string(), true);
				search_result = ctre::search<handle_pattern>(search_result.get<1>().end(), source.end());
			}
			for (auto search_result = ctre::search<nd_handle_pattern>(source); search_result;) {
				output.emplace(search_result.get<1>().to_string(), false);
				search_result = ctre::search<nd_handle_pattern>(search_result.get<1>().end(), source.end());
			}
		} else
			std::cout << "Error: Ignore '" << std::filesystem::absolute(file)
				<< "'. Unable to read it. Make sure it exists and is accessible.";

	return output;
}