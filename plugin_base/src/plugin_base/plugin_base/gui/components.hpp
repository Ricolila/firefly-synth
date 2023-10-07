#pragma once

#include <plugin_base/desc/desc.hpp>
#include <plugin_base/topo/plugin.hpp>
#include <plugin_base/gui/gui.hpp>
#include <plugin_base/gui/listeners.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

namespace plugin_base {

// base class for anything that should react to gui_bindings
// i.e. has it's enabled/visible bound to a set of plugin parameters
class binding_component:
public plugin_listener
{
public:
  virtual ~binding_component();
  void plugin_changed(int index, plain_value plain) override;

protected:
  plugin_gui* const _gui;
  int const _own_slot_index;
  module_desc const* const _module;
  gui_bindings const* const _bindings;

  virtual void init();
  binding_component(
    plugin_gui* gui, module_desc const* module, 
    gui_bindings const* bindings, int own_slot_index);

private:
  std::vector<int> _enabled_values = {};
  std::vector<int> _enabled_params = {};
  std::vector<int> _visibility_values = {};
  std::vector<int> _visibility_params = {};

  void setup_bindings(std::vector<int> const& topo_params, std::vector<int>& params);
  bool bind(gui_binding const& binding, std::vector<int> const& params, std::vector<int>& values);
};

}