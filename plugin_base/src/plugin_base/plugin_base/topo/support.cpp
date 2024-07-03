#include <plugin_base/dsp/utility.hpp>
#include <plugin_base/topo/support.hpp>

#include <map>
#include <cmath>
#include <numeric>

namespace plugin_base {

static default_selector
simple_default(std::string value)
{ return [value](int, int) { return value; }; }

static std::vector<std::string> const note_names = { 
  "C", "C#", "D", "D#", 
  "E", "F", "F#", "G", 
  "G#", "A", "A#", "B" 
};

std::shared_ptr<gui_submenu>
make_midi_note_submenu()
{
  auto result = std::make_shared<gui_submenu>();
  for (int i = 0; i < 12; i++)
  {
    auto note_sub = result->add_submenu(note_names[i]);
    for(int j = 0; j < 128; j++)
      if(j % 12 == i)
        note_sub->indices.push_back(j);
  }
  return result;
}

std::vector<list_item>
make_midi_note_list()
{
  std::vector<list_item> result;
  std::string id = "{97B77668-0B4D-4678-BBD9-842AE601E815}";
  for (int i = 0; i < 128; i++)
    result.push_back(list_item(id + "-" + std::to_string(i), note_names[i % 12] + std::to_string(i / 12 - 1)));
  return result;
}

topo_tag
make_topo_tag(
  std::string const& id, bool name_one_based,
  std::string const& full_name, std::string const& display_name,
  std::string const& menu_display_name, std::string const& tabular_display_name)
{
  topo_tag result = {};
  result.id = id;
  result.full_name = full_name;
  result.display_name = display_name;
  result.name_one_based = name_one_based;
  result.menu_display_name = menu_display_name;
  result.tabular_display_name = tabular_display_name;
  return result;
}

topo_info
make_topo_info(
  std::string const& id, bool name_one_based,
  std::string const& full_name, std::string const& display_name,
  std::string const& menu_display_name,
  int index, int slot_count)
{
  topo_info result = {};
  result.index = index;
  result.slot_count = slot_count;
  result.tag = make_topo_tag(id, name_one_based, full_name, display_name, menu_display_name, "");
  return result;
}

topo_info
make_topo_info_tabular(
  std::string const& id, std::string const& name,
  std::string const& tabular_name, int index, int slot_count)
{
  topo_info result = {};
  result.index = index;
  result.slot_count = slot_count;
  result.tag = make_topo_tag(id, true, name, name, name, tabular_name);
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

param_section
make_param_section(int index, topo_tag const& tag, param_section_gui const& gui)
{
  param_section result = {};
  result.index = index;
  result.tag = topo_tag(tag);
  result.gui = param_section_gui(gui);
  return result;
}

param_section_gui
make_param_section_gui(gui_position const& position, gui_dimension const& dimension, gui_label_edit_cell_split split)
{
  param_section_gui result = {};
  result.cell_split = split;
  result.position = position;
  result.dimension = dimension;
  return result;
}

custom_section_gui
make_custom_section_gui(int index, std::string const& full_name, gui_position const& position, custom_gui_factory factory)
{
  custom_section_gui result = {};
  result.index = index;
  result.position = position;
  result.gui_factory = factory;
  result.full_name = full_name;
  return result;
}

midi_source
make_midi_source(topo_tag const& tag, int id, float default_)
{
  midi_source result;
  result.id = id;
  result.default_ = default_;
  result.tag = topo_tag(tag);
  return result;
}

module_section_gui
make_module_section_gui_none(std::string const& id, int index)
{
  module_section_gui result = {};
  result.id = id;
  result.index = index;
  result.visible = false;
  return result;
}

module_section_gui
make_module_section_gui(std::string const& id, int index, gui_position const& position, gui_dimension const& dimension)
{
  module_section_gui result = {};
  result.id = id;
  result.index = index;
  result.tabbed = false;
  result.visible = true;
  result.position = position;
  result.dimension = dimension;
  return result; 
}

module_section_gui
make_module_section_gui_tabbed(std::string const& id, int index, gui_position const& position, std::vector<int> const& tab_order)
{
  module_section_gui result = {};
  result.id = id;
  result.index = index;
  result.tabbed = true;
  result.visible = true;
  result.position = position;
  result.tab_order = tab_order;
  result.dimension = { 1, 1 };
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

module_dsp_output
make_module_dsp_output(bool is_modulation_source, topo_info const& info)
{
  module_dsp_output result;
  result.info = topo_info(info);
  result.is_modulation_source = is_modulation_source;
  return result;
}

module_dsp
make_module_dsp(module_stage stage, module_output output, int scratch_count, std::vector<module_dsp_output> const& outputs)
{
  module_dsp result = {};
  result.stage = stage;
  result.output = output;
  result.scratch_count = scratch_count;
  result.outputs = vector_explicit_copy(outputs);
  return result;
}

module_topo_gui
make_module_gui_none(int section)
{
  module_topo_gui result = {};
  result.visible = false;
  result.section = section;
  return result;
}

module_topo_gui
make_module_gui(int section, gui_position const& position, gui_dimension const& dimension)
{
  module_topo_gui result = {};
  result.tabbed = false;
  result.visible = true;
  result.section = section;
  result.position = position;
  result.dimension = dimension;
  return result;
}

module_topo_gui
make_module_gui_tabbed(int section, gui_position const& position, std::vector<int> const& tab_order)
{
  module_topo_gui result = {};
  result.tabbed = true;
  result.visible = true;
  result.section = section;
  result.position = position;
  result.tab_order = tab_order;
  result.dimension = { 1, 1 };
  return result;
}

param_domain
make_domain_toggle(bool default_)
{
  param_domain result = {};
  result.min = 0;
  result.max = 1;
  result.type = domain_type::toggle;
  result.default_selector_ = simple_default(default_ ? "On" : "Off");
  return result;
}

param_domain
make_domain_step(int min, int max, int default_, int display_offset)
{
  param_domain result = {};
  result.min = min;
  result.max = max;
  result.type = domain_type::step;
  result.display_offset = display_offset;
  result.default_selector_ = simple_default(std::to_string(default_));
  return result;
}

param_domain
make_domain_timesig_default(bool with_zero, timesig const& max, timesig const& default_)
{
  auto defaults = make_default_timesigs(with_zero, { with_zero ? 0 : 1, 128, }, max);
  return make_domain_timesig(defaults, default_);
}

param_domain
make_domain_timesig(std::vector<timesig> const& sigs, timesig const& default_)
{
  param_domain result = {};
  result.min = 0;
  result.timesigs = sigs;
  result.max = sigs.size() - 1;
  result.type = domain_type::timesig;
  result.default_selector_ = simple_default(default_.to_text());
  return result;
}

param_domain
make_domain_item(std::vector<list_item> const& items, std::string const& default_)
{
  param_domain result = {};
  result.min = 0;
  result.max = items.size() - 1;
  result.type = domain_type::item;
  result.items = std::vector(items);
  result.default_selector_ = simple_default(default_.size() ? default_ : result.items[0].name);
  return result;
}

param_domain
make_domain_name(std::vector<std::string> const& names, std::string const& default_)
{
  param_domain result = {};
  result.min = 0;
  result.names = names;
  result.max = names.size() - 1;
  result.type = domain_type::name;
  result.default_selector_ = simple_default(default_.size() ? default_ : result.names[0]);
  return result;
}

param_domain
make_domain_percentage_identity(double default_, int precision, bool unit)
{
  auto result = make_domain_percentage(0, 1, default_, precision, unit);
  result.type = domain_type::identity;
  return result;
}

param_domain
make_domain_percentage(double min, double max, double default_, int precision, bool unit)
{
  param_domain result = {};
  result.min = min;
  result.max = max;
  result.precision = precision;
  result.unit = unit ? "%" : "";
  result.type = domain_type::linear;
  result.display = domain_display::percentage;
  result.default_selector_ = simple_default(std::to_string(default_ * 100));
  return result;
}

param_domain
make_domain_linear(double min, double max, double default_, int precision, std::string const& unit)
{
  param_domain result = {};  
  result.min = min;
  result.max = max;
  result.unit = unit;
  result.precision = precision;
  result.type = domain_type::linear;
  result.default_selector_ = simple_default(std::to_string(default_));
  return result;
}

param_domain
make_domain_log(double min, double max, double default_, double midpoint, int precision, std::string const& unit)
{
  param_domain result = {};
  result.min = min;
  result.max = max;
  result.unit = unit;
  result.midpoint = midpoint;
  result.precision = precision;
  result.type = domain_type::log;
  result.default_selector_ = simple_default(std::to_string(default_));
  result.exp = std::log((midpoint - min) / (max - min)) / std::log(0.5);
  return result;
}

param_dsp
make_param_dsp_midi(midi_topo_mapping const& source)
{
  param_dsp result = {};
  result.midi_source = source;
  result.rate = param_rate::accurate;
  result.direction = param_direction::input;
  result.automate_selector_ = [](int) { return param_automate::midi; };
  return result;
}

param_dsp
make_param_dsp(param_direction direction, param_rate rate, param_automate automate)
{
  param_dsp result = {};
  result.rate = rate;
  result.direction = direction;
  result.automate_selector_ = [automate](int) { return automate; };
  return result;
}

param_topo_gui
make_param_gui_none()
{
  param_topo_gui result = {};
  result.visible = false;
  return result;
}

param_topo_gui
make_param_gui(int section, gui_edit_type edit_type, param_layout layout, gui_position position, gui_label label)
{
  param_topo_gui result = {};
  result.visible = true;
  result.layout = layout;
  result.section = section;
  result.position = position;
  result.edit_type = edit_type;
  result.label = gui_label(label);
  return result;
}

param_topo
make_param(topo_info const& info, param_dsp const& dsp, param_domain const& domain, param_topo_gui const& gui)
{
  param_topo result = {};
  result.dsp = param_dsp(dsp);
  result.info = topo_info(info);
  result.gui = param_topo_gui(gui);
  result.domain = param_domain(domain);
  return result;
}

std::shared_ptr<gui_submenu>
make_timesig_submenu(std::vector<timesig> const& sigs)
{
  std::map<int, std::vector<int>> sigs_by_num;
  for (int i = 0; i < sigs.size(); i++)
    sigs_by_num[sigs[i].num].push_back(i);
  auto result = std::make_shared<gui_submenu>();
  for(auto const& sbn: sigs_by_num)
  {
    auto sig_sub = std::make_shared<gui_submenu>();
    sig_sub->name = std::to_string(sbn.first);
    sig_sub->indices = sbn.second;
    result->children.push_back(sig_sub);
  }
  return result;
}

std::vector<timesig>
make_default_timesigs(bool with_zero, timesig low, timesig high)
{
  std::vector<int> steps({ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 20, 24, 32, 48, 64, 96, 128 });
  if (with_zero) steps.insert(steps.begin(), 0);
  return make_timesigs(steps, low, high);
}

std::vector<timesig>
make_timesigs(std::vector<int> const& steps, timesig low, timesig high)
{
  assert(low.den > 0);
  assert(high.den > 0);
  assert(steps.size());

  bool with_zero = false;
  std::vector<timesig> result;
  for (int n = 0; n < steps.size(); n++)
    if(steps[n] == 0) 
      with_zero = true;
    else
      for (int d = 0; d < steps.size(); d++)
        if(steps[d] != 0)
          result.push_back({ steps[n], steps[d] });
  if(with_zero)
    result.insert(result.begin(), { 0, 1 });

  auto filter_low = [low](auto const& s) { return (float)s.num / s.den >= (float)low.num / low.den; };
  result = vector_filter(result, filter_low);
  auto filter_high = [high](auto const& s) { return (float)s.num / s.den <= (float)high.num / high.den; };
  result = vector_filter(result, filter_high);
  auto filter_gcd = [](auto const& s) { return s.num == 0 || std::gcd(s.num, s.den) == 1; };
  return vector_filter(result, filter_gcd);
}

}