#include <plugin_base/topo/support.hpp>
#include <cmath>

namespace plugin_base {

static param_topo
param_base(
  std::string const& id, std::string const& name, int index, int slot_count, int section, std::string const& default_,
  param_direction direction, param_rate rate, param_format format, gui_edit_type edit_type,
  gui_label_contents label_contents, gui_label_align label_align, gui_label_justify label_justify,
  gui_layout layout, gui_position const& position)
{
  param_topo result = {};
  result.info.tag.id = id;
  result.dsp.direction = direction;
  result.info.tag.name = name;
  result.dsp.rate = rate;
  result.info.index = index;
  result.dsp.format = format;
  result.gui.section = section;
  result.info.slot_count = slot_count;
  result.domain.default_ = default_;
  result.gui.layout = layout;
  result.gui.position = position;
  result.gui.edit_type = edit_type;
  result.gui.label.align = label_align;
  result.gui.label.justify = label_justify;
  result.gui.label.contents = label_contents;
  return result;
}

int
index_of_item_tag(std::vector<list_item> const& items, int tag)
{
  for(int i = 0; i < items.size(); i++)
    if(items[i].tag == tag)
      return i;
  assert(false);
  return -1;
}

topo_tag
make_topo_tag(std::string const& id, std::string const& name)
{
  topo_tag result = {};
  result.id = id;
  result.name = name;
  return result;
}

topo_info
make_topo_info(std::string const& id, std::string const& name, int index, int slot_count)
{
  topo_info result = {};
  result.tag.id = id;
  result.tag.name = name;
  result.index = index;
  result.slot_count = slot_count;
  return result;
}

gui_label
make_label(gui_label_contents contents, gui_label_align align, gui_label_justify justify)
{
  gui_label result = {};
  result.align = align;
  result.justify = justify;
  result.contents = contents;
  return result;
}

section_topo
make_section(int index, topo_tag const& tag, section_topo_gui const& gui)
{
  section_topo result = {};
  result.index = index;
  result.tag = topo_tag(tag);
  result.gui = section_topo_gui(gui);
  return result;
}

section_topo_gui
make_section_gui(gui_position const& position, gui_dimension const& dimension)
{
  section_topo_gui result = {};
  result.position = position;
  result.dimension = dimension;
  return result;
}

module_dsp
make_module_dsp(module_stage stage, module_output output, int output_count)
{
  module_dsp result = {};
  result.stage = stage;
  result.output = output;
  result.output_count = output_count;
  return result;
}

module_topo
make_module(topo_info const& info, module_dsp const& dsp, module_topo_gui const& gui)
{
  module_topo result = {};
  result.dsp = module_dsp(dsp);
  result.info = topo_info(info);
  result.gui = module_topo_gui(gui);
  return result;
}

module_topo_gui
make_module_gui(gui_layout layout, gui_position const& position, gui_dimension const& dimension)
{
  module_topo_gui result = {};
  result.layout = layout;
  result.position = position;
  result.dimension = dimension;
  return result;
}

param_topo_gui
make_param_gui(int section, gui_layout layout, gui_position position, gui_edit_type edit_type, gui_label label)
{
  param_topo_gui result = {};
  result.layout = layout;
  result.section = section;
  result.position = position;
  result.edit_type = edit_type;
  result.label = gui_label(label);
  return result;
}

param_topo
param_toggle(
  std::string const& id, std::string const& name, int index, int slot_count, int section,
  bool default_,
  param_direction direction,
  gui_label_contents label_contents, gui_label_align label_align, gui_label_justify label_justify,
  gui_layout layout, gui_position const& position)
{
  param_topo result(param_base(
    id, name, index, slot_count, section, default_? "On": "Off", 
    direction, param_rate::block, param_format::plain, gui_edit_type::toggle, 
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
  param_direction direction, gui_edit_type edit_type,
  gui_label_contents label_contents, gui_label_align label_align, gui_label_justify label_justify,
  gui_layout layout, gui_position const& position)
{
  param_topo result(param_base(
    id, name, index, slot_count, section, std::to_string(default_), 
    direction, param_rate::block, param_format::plain, edit_type,
    label_contents, label_align, label_justify, layout, position));
  result.domain.min = min;
  result.domain.max = max;
  result.domain.type = domain_type::step;
  return result;
}

param_topo
param_items(
  std::string const& id, std::string const& name, int index, int slot_count, int section,
  std::vector<list_item>&& items, std::string const& default_,
  param_direction direction, gui_edit_type edit_type,
  gui_label_contents label_contents, gui_label_align label_align, gui_label_justify label_justify,
  gui_layout layout, gui_position const& position)
{
  param_topo result(param_base(
    id, name, index, slot_count, section, default_.size()? default_: items[0].name,
    direction, param_rate::block, param_format::plain, edit_type,
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
  param_direction direction, gui_edit_type edit_type,
  gui_label_contents label_contents, gui_label_align label_align, gui_label_justify label_justify,
  gui_layout layout, gui_position const& position)
{
  param_topo result(param_base(
    id, name, index, slot_count, section, default_.size()? default_: names[0],
    direction, param_rate::block, param_format::plain, edit_type,
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
  param_direction direction, param_rate rate, param_format format, bool unit, gui_edit_type edit_type,
  gui_label_contents label_contents, gui_label_align label_align, gui_label_justify label_justify,
  gui_layout layout, gui_position const& position)
{
  param_topo result(param_base(
    id, name, index, slot_count, section, std::to_string(default_ * 100), 
    direction, rate, format, edit_type,
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
  param_direction direction, param_rate rate, param_format format, gui_edit_type edit_type,
  gui_label_contents label_contents, gui_label_align label_align, gui_label_justify label_justify,
  gui_layout layout, gui_position const& position)
{
  param_topo result(param_base(
    id, name, index, slot_count, section, std::to_string(default_), 
    direction, rate, format, edit_type,
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
  param_direction direction, param_rate rate, param_format format, gui_edit_type edit_type,
  gui_label_contents label_contents, gui_label_align label_align, gui_label_justify label_justify,
  gui_layout layout, gui_position const& position)
{
  param_topo result(param_base(
    id, name, index, slot_count, section, std::to_string(default_), 
    direction, rate, format, edit_type,
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