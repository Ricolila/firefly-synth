#pragma once
#include <infernal.base/utility.hpp>
#include <infernal.base/topo.hpp>
#include <vector>
#include <string>

namespace infernal::base {

struct param_mapping final {
  int param_index = {};
  int module_group = {};
  int module_index = {};
};

struct param_desc final {
  int id_hash = {};
  std::string id = {};
  std::string name = {};

  INF_DECLARE_MOVE_ONLY(param_desc);
  param_desc(module_group_topo const& module_group, int module_index, param_topo const& param);
};

struct module_desc final {
  std::string name = {};
  std::vector<param_desc> params = {};
  
  INF_DECLARE_MOVE_ONLY(module_desc);
  module_desc(module_group_topo const& module_group, int module_index);
};

struct plugin_desc final {
  std::vector<module_desc> modules = {};
  std::vector<param_mapping> param_mappings = {};

  INF_DECLARE_MOVE_ONLY(plugin_desc);
  plugin_desc(plugin_topo const& plugin);
};

struct plugin_dims final {
  std::vector<int> module_counts;
  std::vector<std::vector<int>> module_param_counts;
  std::vector<std::vector<int>> module_channel_counts;

  INF_DECLARE_MOVE_ONLY(plugin_dims);
  plugin_dims(plugin_topo const& plugin);
};

struct plugin_frame_dims final {
  std::vector<std::vector<int>> module_frame_counts;
  std::vector<std::vector<std::vector<int>>> module_param_frame_counts;
  std::vector<std::vector<std::vector<int>>> module_channel_frame_counts;

  INF_DECLARE_MOVE_ONLY(plugin_frame_dims);
  plugin_frame_dims(plugin_topo const& plugin, int frame_count);
};

}
