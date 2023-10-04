#include <plugin_base/support.hpp>

#include <cmath>

namespace plugin_base {

std::vector<std::string>
note_names() 
{ return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; }

static param_topo
param_base(
  std::string const& id, std::string const& name, int index, int slot_count, int section, std::string const& default_,
  param_dir dir, param_rate rate, param_format format, gui_edit_type edit_type,
  gui_label_contents label_contents, gui_label_align label_align, gui_label_justify label_justify,
  gui_layout layout, gui_position const& position)
{
  param_topo result = {};
  result.id = id;
  result.dir = dir;
  result.name = name;
  result.rate = rate;
  result.index = index;
  result.format = format;
  result.section = section;
  result.slot_count = slot_count;
  result.domain.default_ = default_;
  result.gui.layout = layout;
  result.gui.position = position;
  result.gui.edit_type = edit_type;
  result.gui.label_align = label_align;
  result.gui.label_justify = label_justify;
  result.gui.label_contents = label_contents;
  return result;
}

int
index_of_item_tag(std::vector<item_topo> const& items, int tag)
{
  for(int i = 0; i < items.size(); i++)
    if(items[i].tag == tag)
      return i;
  assert(false);
  return -1;
}

section_topo
make_section(
  std::string const& name, int index,
  gui_position const& position, gui_dimension const& dimension)
{
  section_topo result = {};
  result.name = name;
  result.index = index;
  result.position = position;
  result.dimension = dimension;
  return result;
}

module_topo
make_module(
  std::string const& id, std::string const& name, int index, int slot_count, 
  module_stage stage, module_output output, int output_count,
  gui_layout layout, gui_position const& position, gui_dimension const& dimension)
{
  module_topo result = {};
  result.id = id;
  result.name = name;
  result.index = index;
  result.stage = stage;
  result.output = output;
  result.layout = layout;
  result.position = position;
  result.dimension = dimension;
  result.slot_count = slot_count;
  result.output_count = output_count;
  return result;
}

param_topo
param_toggle(
  std::string const& id, std::string const& name, int index, int slot_count, int section,
  bool default_,
  param_dir dir,
  gui_label_contents label_contents, gui_label_align label_align, gui_label_justify label_justify,
  gui_layout layout, gui_position const& position)
{
  param_topo result(param_base(
    id, name, index, slot_count, section, default_? "On": "Off", 
    dir, param_rate::block, param_format::plain, gui_edit_type::toggle, 
    label_contents, label_align, label_justify, layout, position));
  result.domain.min = 0;
  result.domain.max = 1;
  result.domain.type = domain_type::toggle;
  return result;
}

param_topo
param_steps(
  std::string const& id, std::string const& name, int index, int slot_count, int section,
  int min, int max, int default_,
  param_dir dir, gui_edit_type edit_type,
  gui_label_contents label_contents, gui_label_align label_align, gui_label_justify label_justify,
  gui_layout layout, gui_position const& position)
{
  param_topo result(param_base(
    id, name, index, slot_count, section, std::to_string(default_), 
    dir, param_rate::block, param_format::plain, edit_type,
    label_contents, label_align, label_justify, layout, position));
  result.domain.min = min;
  result.domain.max = max;
  result.domain.type = domain_type::step;
  return result;
}

param_topo
param_items(
  std::string const& id, std::string const& name, int index, int slot_count, int section,
  std::vector<item_topo>&& items, std::string const& default_,
  param_dir dir, gui_edit_type edit_type,
  gui_label_contents label_contents, gui_label_align label_align, gui_label_justify label_justify,
  gui_layout layout, gui_position const& position)
{
  param_topo result(param_base(
    id, name, index, slot_count, section, default_.size()? default_: items[0].name,
    dir, param_rate::block, param_format::plain, edit_type,
    label_contents, label_align, label_justify, layout, position));
  result.domain.items = std::move(items);
  result.domain.min = 0;
  result.domain.max = result.domain.items.size() - 1;
  result.domain.type = domain_type::item;
  return result;
}

param_topo
param_names(
  std::string const& id, std::string const& name, int index, int slot_count, int section,
  std::vector<std::string> const& names, std::string const& default_,
  param_dir dir, gui_edit_type edit_type,
  gui_label_contents label_contents, gui_label_align label_align, gui_label_justify label_justify,
  gui_layout layout, gui_position const& position)
{
  param_topo result(param_base(
    id, name, index, slot_count, section, default_.size()? default_: names[0],
    dir, param_rate::block, param_format::plain, edit_type,
    label_contents, label_align, label_justify, layout, position));
  result.domain.min = 0;
  result.domain.max = names.size() - 1;
  result.domain.names = names;
  result.domain.type = domain_type::name;
  return result;
}

param_topo
param_pct(
  std::string const& id, std::string const& name, int index, int slot_count, int section,
  double min, double max, double default_, int precision,
  param_dir dir, param_rate rate, param_format format, bool unit, gui_edit_type edit_type,
  gui_label_contents label_contents, gui_label_align label_align, gui_label_justify label_justify,
  gui_layout layout, gui_position const& position)
{
  param_topo result(param_base(
    id, name, index, slot_count, section, std::to_string(default_ * 100), 
    dir, rate, format, edit_type,
    label_contents, label_align, label_justify, layout, position));
  result.domain.min = min;
  result.domain.max = max;
  result.domain.unit = unit? "%": "";
  result.domain.precision = precision;
  result.domain.type = domain_type::linear;
  result.domain.display = domain_display::percentage;
  return result;
}

param_topo
param_linear(
  std::string const& id, std::string const& name, int index, int slot_count, int section,
  double min, double max, double default_, int precision, std::string const& unit,
  param_dir dir, param_rate rate, param_format format, gui_edit_type edit_type,
  gui_label_contents label_contents, gui_label_align label_align, gui_label_justify label_justify,
  gui_layout layout, gui_position const& position)
{
  param_topo result(param_base(
    id, name, index, slot_count, section, std::to_string(default_), 
    dir, rate, format, edit_type,
    label_contents, label_align, label_justify, layout, position));
  result.domain.min = min;
  result.domain.max = max;
  result.domain.unit = unit;
  result.domain.precision = precision;
  result.domain.type = domain_type::linear;
  return result;
}

param_topo
param_log(
  std::string const& id, std::string const& name, int index, int slot_count, int section,
  double min, double max, double default_, double midpoint, int precision, std::string const& unit,
  param_dir dir, param_rate rate, param_format format, gui_edit_type edit_type,
  gui_label_contents label_contents, gui_label_align label_align, gui_label_justify label_justify,
  gui_layout layout, gui_position const& position)
{
  param_topo result(param_base(
    id, name, index, slot_count, section, std::to_string(default_), 
    dir, rate, format, edit_type,
    label_contents, label_align, label_justify, layout, position));
  result.domain.precision = precision;
  result.domain.type = domain_type::log;
  result.domain.min = min;
  result.domain.max = max;
  result.domain.unit = unit;
  result.domain.exp = std::log((midpoint - min) / (max - min)) / std::log(0.5);
  return result;
}

}