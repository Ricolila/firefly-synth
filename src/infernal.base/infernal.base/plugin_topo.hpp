#ifndef INFERNAL_BASE_PLUGIN_TOPO_HPP
#define INFERNAL_BASE_PLUGIN_TOPO_HPP

#include <infernal.base/plugin_block.hpp>
#include <infernal.base/plugin_support.hpp>

#include <vector>
#include <string>

namespace infernal::base {

enum class plugin_kind { synth, fx };
enum class module_scope { voice, global };
enum class module_output { none, cv, audio };

enum class param_slope { linear, log };
enum class param_format { real, step };
enum class param_storage { num, text };
enum class param_rate { accurate, block };
enum class param_direction { input, output };
enum class param_display { toggle, list, knob, slider };

struct param_topo {
  int stepped_min;
  int stepped_max;
  std::string id;
  std::string name;
  std::string unit;
  std::string default_;
  param_rate rate;
  param_slope slope;
  param_format format;
  param_storage storage;
  param_display display;
  param_direction direction;
  INF_DECLARE_MOVE_ONLY(param_topo);

  param_value default_value() const;
  std::string to_text(param_value value) const;
  bool from_text(std::string const& text, param_value& value) const;
};

struct submodule_topo {
  int type;
  std::string name;
  std::vector<param_topo> params;
  INF_DECLARE_MOVE_ONLY(submodule_topo);
};

struct module_dependency {
  int module_type;
  int module_index;
  INF_DECLARE_MOVE_ONLY(module_dependency);
};

struct module_topo {
  int type;
  int count;
  std::string id;
  std::string name;
  module_scope scope;
  module_output output;
  module_process process;
  std::vector<submodule_topo> submodules;
  std::vector<module_dependency> dependencies;
  INF_DECLARE_MOVE_ONLY(module_topo);
};

struct plugin_topo {
  bool is_fx;
  plugin_kind kind;
  int polyphony;
  int note_limit;
  int channel_count;
  int block_automation_limit;
  int accurate_automation_limit;
  std::vector<module_topo> modules;
  INF_DECLARE_MOVE_ONLY(plugin_topo);
};

struct runtime_param_topo
{
  int id_hash;
  std::string id;
  std::string name;
  int module_type;
  int module_index;
  int module_param_index;
  param_topo const* static_topo;
  INF_DECLARE_MOVE_ONLY(runtime_param_topo);
};

struct runtime_module_topo
{
  std::string name;
  module_topo const* static_topo;
  std::vector<runtime_param_topo> params;
  INF_DECLARE_MOVE_ONLY(runtime_module_topo);
};

struct flat_module_topo
{
  module_topo const* static_topo;
  std::vector<param_topo const*> params;
  INF_DECLARE_MOVE_ONLY(flat_module_topo);
};

struct runtime_plugin_topo
{
  plugin_topo static_topo;
  std::vector<flat_module_topo> flat_modules = {};
  std::vector<runtime_param_topo> runtime_params = {};
  std::vector<runtime_module_topo> runtime_modules = {};
  explicit runtime_plugin_topo(plugin_topo&& static_topo);
  INF_DECLARE_MOVE_ONLY(runtime_plugin_topo);
};

}
#endif