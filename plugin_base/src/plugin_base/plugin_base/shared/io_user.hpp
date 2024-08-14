#pragma once

#include <plugin_base/topo/plugin.hpp>
#include <string>

namespace plugin_base {

// per-user state (e.g. plugin window size)

enum class user_io { base, plugin };

void user_io_save_num(std::string const& vendor, std::string const& full_name, user_io where, std::string const& key, double val);
void user_io_save_list(std::string const& vendor, std::string const& full_name, user_io where, std::string const& key, std::string const& val);
double user_io_load_num(std::string const& vendor, std::string const& full_name, user_io where, std::string const& key, double default_, double min, double max);
std::string user_io_load_list(std::string const& vendor, std::string const& full_name, user_io where, std::string const& key, std::string const& default_, std::vector<std::string> const& values);

}