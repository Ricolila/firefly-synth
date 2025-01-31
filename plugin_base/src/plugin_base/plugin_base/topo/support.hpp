#pragma once

#include <plugin_base/topo/plugin.hpp>

#include <string>
#include <vector>

namespace plugin_base { 

std::vector<list_item>
make_midi_note_list();
std::shared_ptr<gui_submenu>
make_midi_note_submenu();

std::shared_ptr<gui_submenu>
make_timesig_submenu(std::vector<timesig> const& sigs);
std::vector<timesig>
make_default_timesigs(bool with_zero, timesig low, timesig high);
std::vector<timesig>
make_timesigs(std::vector<int> const& steps, timesig low, timesig high);

gui_label 
make_label(gui_label_contents contents, gui_label_align align, gui_label_justify justify);
inline gui_label 
make_label_none()
{ return make_label(gui_label_contents::none, gui_label_align::left, gui_label_justify::center); }

topo_tag
make_topo_tag(
  std::string const& id, bool name_one_based,
  std::string const& full_name, std::string const& display_name, 
  std::string const& menu_display_name, std::string const& tabular_display_name);
inline topo_tag
make_topo_tag_basic(
  std::string const& id, std::string const& name)
{ return make_topo_tag(id, true, name, name, name, name); }

topo_info
make_topo_info(
  std::string const& id, bool name_one_based,
  std::string const& full_name, std::string const& display_name,
  std::string const& menu_display_name,
  int index, int slot_count);
topo_info
make_topo_info_tabular(
  std::string const& id, std::string const& name, 
  std::string const& tabular_name, int index, int slot_count);
inline topo_info
make_topo_info_basic(
  std::string const& id, std::string const& name, int index, int slot_count)
{ return make_topo_info(id, true, name, name, name, index, slot_count); }

param_section
make_param_section(int index, topo_tag const& tag, param_section_gui const& gui);
custom_section_gui 
make_custom_section_gui(int index, std::string const& full_name, gui_position const& position, custom_gui_factory factory);
param_section_gui
make_param_section_gui(gui_position const& position, gui_dimension const& dimension, gui_label_edit_cell_split split = gui_label_edit_cell_split::no_split);

module_section_gui
make_module_section_gui_none(std::string const& id, int index);
module_section_gui
make_module_section_gui(std::string const& id, int index, gui_position const& position, gui_dimension const& dimension);
module_section_gui
make_module_section_gui_tabbed(std::string const& id, int index, gui_position const& position, std::vector<int> const& tab_order);

midi_source
make_midi_source(topo_tag const& tag, int id, float default_);
module_dsp_output
make_module_dsp_output(bool is_modulation_source, int underlying_module_index, topo_info const& info);
module_topo
make_module(topo_info const& info, module_dsp const& dsp, module_topo_gui const& gui);
module_dsp
make_module_dsp(module_stage stage, module_output output, int scratch_count, std::vector<module_dsp_output> const& outputs);

module_topo_gui
make_module_gui_none(int section);
module_topo_gui
make_module_gui(int section, gui_position const& position, gui_dimension const& dimension);
module_topo_gui
make_module_gui_tabbed(int section, gui_position const& position, std::vector<int> const& tab_order);

param_dsp
make_param_dsp_midi(midi_topo_mapping const& source);
param_dsp
make_param_dsp(param_direction direction, param_rate rate, param_automate automate);
inline param_dsp
make_param_dsp_input(param_rate rate, param_automate automate)
{ return make_param_dsp(param_direction::input, rate, automate); }
inline param_dsp
make_param_dsp_block(param_automate automate)
{ return make_param_dsp(param_direction::input, param_rate::block, automate); }
inline param_dsp
make_param_dsp_voice(param_automate automate)
{ return make_param_dsp(param_direction::input, param_rate::voice, automate); }
inline param_dsp
make_param_dsp_accurate(param_automate automate)
{ return make_param_dsp(param_direction::input, param_rate::accurate, automate); }
inline param_dsp
make_param_dsp_output()
{ return make_param_dsp(param_direction::output, param_rate::block, param_automate::none); }
inline param_dsp
make_param_dsp_input(bool voice, param_automate automate)
{ return make_param_dsp(param_direction::input, voice? param_rate::voice: param_rate::block, automate); }
inline param_dsp
make_param_dsp_automate_if_voice(bool voice)
{ return make_param_dsp(param_direction::input, voice? param_rate::voice: param_rate::block, voice? param_automate::automate: param_automate::none); }

param_domain
make_domain_toggle(bool default_);
param_domain
make_domain_step(int min, int max, int default_, int display_offset);
param_domain
make_domain_item(std::vector<list_item> const& items, std::string const& default_);
param_domain
make_domain_name(std::vector<std::string> const& names, std::string const& default_);
param_domain
make_domain_timesig(std::vector<timesig> const& sigs, timesig const& default_);
param_domain
make_domain_timesig_default(bool with_zero, timesig const& max, timesig const& default_);

param_domain
make_domain_percentage_identity(double default_, int precision, bool unit);
param_domain
make_domain_percentage(double min, double max, double default_, int precision, bool unit);
param_domain
make_domain_linear(double min, double max, double default_, int precision, std::string const& unit);
param_domain
make_domain_log(double min, double max, double default_, double midpoint, int precision, std::string const& unit);

param_topo_gui
make_param_gui_none(int section);
param_topo_gui
make_param_gui(int section, gui_edit_type edit_type, param_layout layout, gui_position position, gui_label label);
param_topo
make_param(topo_info const& info, param_dsp const& dsp, param_domain const& domain, param_topo_gui const& gui);
inline param_topo_gui
make_param_gui_single(int section, gui_edit_type edit_type, gui_position position, gui_label label)
{ return make_param_gui(section, edit_type, param_layout::single, position, gui_label(label)); }

}
