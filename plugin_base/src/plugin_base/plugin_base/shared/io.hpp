#pragma once

#include <plugin_base/dsp/value.hpp>
#include <plugin_base/desc/plugin.hpp>
#include <plugin_base/shared/jarray.hpp>
#include <plugin_base/shared/utility.hpp>

#include <vector>
#include <string>
#include <filesystem>

namespace plugin_base {

struct load_result
{
  std::string error = {};
  std::vector<std::string> warnings = {};
  bool ok() const { return error.size() == 0; };

  load_result() = default;
  load_result(std::string const& error): error(error) {}
};

class plugin_io final
{
  plugin_desc const* const _desc;
public:
  INF_PREVENT_ACCIDENTAL_COPY(plugin_io);
  plugin_io(plugin_desc const* desc): _desc(desc) {}

  std::vector<char> save(jarray<plain_value, 4> const& state) const;
  load_result load(std::vector<char> const& data, jarray<plain_value, 4>& state) const;
  bool save_file(std::filesystem::path const& path, jarray<plain_value, 4> const& state) const;
  load_result load_file(std::filesystem::path const& path, jarray<plain_value, 4>& state) const;
};

}