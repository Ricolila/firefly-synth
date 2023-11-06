#pragma once

#include <plugin_base/topo/domain.hpp>
#include <plugin_base/topo/shared.hpp>
#include <plugin_base/shared/value.hpp>
#include <plugin_base/shared/utility.hpp>

#include <vector>
#include <string>
#include <algorithm>

namespace plugin_base {

struct param_topo;
struct module_topo;

enum class param_rate { accurate, block };
enum class param_direction { input, output };
enum class param_layout { single, horizontal, vertical };
enum class param_automate { none, automate, modulate, both };

typedef std::function<param_automate(int module_slot)>
automate_selector;
typedef std::function<bool(int that_val, int this_val)>
gui_item_binding_selector;

// binding to item enabled
struct gui_item_binding final {
  bool auto_bind = false;
  param_topo_mapping param = {};
  gui_item_binding_selector selector = {};
  static inline int const match_param_slot = -1;
  INF_PREVENT_ACCIDENTAL_COPY_DEFAULT_CTOR(gui_item_binding);

  void validate() const { assert(!is_param_bound() || !auto_bind); }
  bool is_param_bound() const { return selector != nullptr; }
  bool is_bound() const { return is_param_bound() || auto_bind; }
  void bind_param(param_topo_mapping param_, gui_item_binding_selector selector_);
};

// parameter dsp
struct param_dsp final {
  param_rate rate;
  param_direction direction;
  automate_selector automate_selector;

  void validate(int module_slot) const;
  bool can_modulate(int module_slot) const;
  bool can_automate(int module_slot) const;
  INF_PREVENT_ACCIDENTAL_COPY_DEFAULT_CTOR(param_dsp);
};

// parameter ui
struct param_topo_gui final {
  int section;
  gui_label label;
  param_layout layout;
  gui_position position;
  gui_bindings bindings;
  gui_edit_type edit_type;
  gui_item_binding item_enabled = {};
  std::shared_ptr<gui_submenu> submenu;

  INF_PREVENT_ACCIDENTAL_COPY_DEFAULT_CTOR(param_topo_gui);
  bool is_list() const;
  void validate(module_topo const& module, param_topo const& param) const;
};

// parameter in module
struct param_topo final {
  param_dsp dsp;
  topo_info info;
  param_topo_gui gui;
  param_domain domain;

  INF_PREVENT_ACCIDENTAL_COPY_DEFAULT_CTOR(param_topo);
  void validate(module_topo const& module, int index) const;
};

inline bool 
param_dsp::can_modulate(int module_slot) const
{
  auto mode = automate_selector(module_slot);
  return mode == param_automate::both || mode == param_automate::modulate;
}

inline bool
param_dsp::can_automate(int module_slot) const
{
  auto mode = automate_selector(module_slot);
  return mode == param_automate::both || mode == param_automate::automate;
}

inline bool
param_topo_gui::is_list() const
{ return edit_type == gui_edit_type::list || edit_type == gui_edit_type::autofit_list; }

}
