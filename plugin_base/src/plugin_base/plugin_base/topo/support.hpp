#pragma once

#include <plugin_base/topo/plugin.hpp>
#include <string>

namespace plugin_base {

int
index_of_item_tag(std::vector<list_item> const& items, int tag);

topo_tag
make_topo_tag(std::string const& id, std::string const& name);
topo_info
make_topo_info(std::string const& id, std::string const& name, int index, int slot_count);
gui_label 
make_label(gui_label_contents contents, gui_label_align align, gui_label_justify justify);

section_topo
make_section(int index, topo_tag const& tag, section_topo_gui const& gui);
section_topo_gui
make_section_gui(gui_position const& position, gui_dimension const& dimension);

module_dsp 
make_module_dsp(module_stage stage, module_output output, int output_count);
module_topo
make_module(topo_info const& info, module_dsp const& dsp, module_topo_gui const& gui);
module_topo_gui
make_module_gui(gui_layout layout, gui_position const& position, gui_dimension const& dimension);

param_topo_gui
make_param_gui(int section, gui_layout layout, gui_position position, gui_edit_type edit_type, gui_label label);

// block-rate
param_topo
param_toggle(
  std::string const& id, std::string const& name, int index, int slot_count, int section,
  bool default_, 
  param_direction direction,
  gui_label_contents label_contents, gui_label_align label_align, gui_label_justify label_justify,
  gui_layout layout, gui_position const& position);
param_topo
param_steps(
  std::string const& id, std::string const& name, int index, int slot_count, int section,
  int min, int max, int default_,
  param_direction direction, gui_edit_type edit_type,
  gui_label_contents label_contents, gui_label_align label_align, gui_label_justify label_justify,
  gui_layout layout, gui_position const& position);
param_topo
param_items(
  std::string const& id, std::string const& name, int index, int slot_count, int section,
  std::vector<list_item>&& items, std::string const& default_,
  param_direction direction, gui_edit_type edit_type,
  gui_label_contents label_contents, gui_label_align label_align, gui_label_justify label_justify,
  gui_layout layout, gui_position const& position);
param_topo
param_names(
  std::string const& id, std::string const& name, int index, int slot_count, int section,
  std::vector<std::string> const& names, std::string const& default_,
  param_direction direction, gui_edit_type edit_type,
  gui_label_contents label_contents, gui_label_align label_align, gui_label_justify label_justify,
  gui_layout layout, gui_position const& position);

// either block or accurate rate
param_topo
param_pct(
  std::string const& id, std::string const& name, int index, int slot_count, int section,
  double min, double max, double default_, int precision,
  param_direction direction, param_rate rate, param_format format, bool unit, gui_edit_type edit_type,
  gui_label_contents label_contents, gui_label_align label_align, gui_label_justify label_justify,
  gui_layout layout, gui_position const& position);
param_topo
param_linear(
  std::string const& id, std::string const& name, int index, int slot_count, int section,
  double min, double max, double default_, int precision, std::string const& unit,
  param_direction direction, param_rate rate, param_format format, gui_edit_type edit_type,
  gui_label_contents label_contents, gui_label_align label_align, gui_label_justify label_justify,
  gui_layout layout, gui_position const& position);
param_topo
param_log(
  std::string const& id, std::string const& name, int index, int slot_count, int section,
  double min, double max, double default_, double midpoint, int precision, std::string const& unit,
  param_direction direction, param_rate rate, param_format format, gui_edit_type edit_type,
  gui_label_contents label_contents, gui_label_align label_align, gui_label_justify label_justify,
  gui_layout layout, gui_position const& position);
}