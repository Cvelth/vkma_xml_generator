// Copyright (c) 2021 Cvelth <cvelth.mail@gmail.com>
// SPDX-License-Identifier: MIT

#pragma warning(push)
#pragma warning(disable : 4702)
#include "ctre.hpp"
#pragma warning(pop)

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <string_view>
#include <vector>
using namespace std::string_view_literals;

namespace vkma_xml::detail {
  using identifier_t = std::string;
  namespace type {
    struct handle {
      bool dispatchable;
      std::optional<identifier_t> parent;
    };
  } // namespace type
  std::map<identifier_t, type::handle>
  load_handle_list(std::vector<std::filesystem::path> const &files);
} // namespace vkma_xml::detail

static constexpr auto handle_pattern = ctll::fixed_string{
  R"(VK_DEFINE_HANDLE\(([A-Za-z_0-9]+)\))"
};
static constexpr auto nd_handle_pattern = ctll::fixed_string{
  R"(VK_DEFINE_NON_DISPATCHABLE_HANDLE\(([A-Za-z_0-9]+)\))"
};

template <auto &pattern>
void append_handles(
  std::string_view source, bool is_dispatchable,
  std::map<vkma_xml::detail::identifier_t, vkma_xml::detail::type::handle> &output) {
  for (auto search_result = ctre::search<pattern>(source); search_result;) {
    std::string_view remaining_text{ search_result.get<0>().end(), source.end() };
    std::string_view remaining_line{ remaining_text.begin(),
                                     remaining_text.begin() + remaining_text.find('\n') };
    std::optional<vkma_xml::detail::identifier_t> parent = std::nullopt;
    if (remaining_line.size() > 12 && remaining_line.substr(0, 12) == " // parent: ")
      if (remaining_line.substr(12) != "none")
        parent = vkma_xml::detail::identifier_t(remaining_line.substr(12));
    output.emplace(search_result.get<1>().to_string(),
                   vkma_xml::detail::type::handle{ is_dispatchable, parent });
    search_result = ctre::search<pattern>(search_result.get<1>().end(), source.end());
  }
}

std::map<vkma_xml::detail::identifier_t, vkma_xml::detail::type::handle>
vkma_xml::detail::load_handle_list(std::vector<std::filesystem::path> const &files) {
  std::map<identifier_t, type::handle> output;
  for (auto const &file : files)
    if (std::ifstream stream(file, std::fstream::ate); stream) {
      size_t source_size = stream.tellg();
      std::string source(source_size, '\0');
      stream.seekg(0);
      stream.read(source.data(), source_size);

      append_handles<handle_pattern>(source, true, output);
      append_handles<nd_handle_pattern>(source, false, output);
    } else
      std::cout << "Error: Ignore '" << std::filesystem::absolute(file)
                << "'. Unable to read it. Make sure it exists and is accessible.";

  return output;
}
