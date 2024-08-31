#include <plugin_base/gui/graph.hpp>
#include <plugin_base/dsp/utility.hpp>
#include <plugin_base/dsp/graph_engine.hpp>
#include <plugin_base/shared/utility.hpp>

using namespace juce;

namespace plugin_base {

module_graph::
~module_graph() 
{ 
  _done = true;
  stopTimer();
  if(_module_params.render_on_tweak) _gui->gui_state()->remove_any_listener(this);
  if(_module_params.render_on_tab_change) _gui->remove_tab_selection_listener(this);
  if (_module_params.render_on_modulation_output_change) _gui->remove_modulation_output_listener(this);
  if (_module_params.render_on_module_mouse_enter || _module_params.render_on_param_mouse_enter_modules.size())
    _gui->remove_gui_mouse_listener(this);
}

module_graph::
module_graph(plugin_gui* gui, lnf* lnf, graph_params const& params, module_graph_params const& module_params):
graph(lnf, params), _gui(gui), _module_params(module_params)
{ 
  assert(_module_params.fps > 0);
  assert(_module_params.render_on_tweak || _module_params.render_on_tab_change ||
    _module_params.render_on_module_mouse_enter || _module_params.render_on_param_mouse_enter_modules.size());
  if(_module_params.render_on_tab_change) assert(_module_params.module_index != -1);
  if (_module_params.render_on_tweak) gui->gui_state()->add_any_listener(this);
  if(_module_params.render_on_module_mouse_enter || _module_params.render_on_param_mouse_enter_modules.size())
    gui->add_gui_mouse_listener(this);
  if (_module_params.render_on_modulation_output_change)
    _gui->add_modulation_output_listener(this);
  if (_module_params.render_on_tab_change)
  {
    gui->add_tab_selection_listener(this);
    module_tab_changed(_module_params.module_index, 0);
  }
  startTimerHz(_module_params.fps);
}

void
module_graph::paint(Graphics& g)
{
  render_if_dirty();
  graph::paint(g);
}

void
module_graph::timerCallback()
{
  if(_done || !_render_dirty) return;
  if(render_if_dirty())
    repaint();
}

void 
module_graph::modulation_outputs_changed(std::vector<modulation_output> const& outputs)
{
  if (_data.type() != graph_data_type::series)
    return;

  if (_hovered_or_tweaked_param == -1)
    return;

  float w = getWidth();
  float h = getHeight();
  int count = _data.series().size();

  int current_module_slot = -1;
  int current_module_index = -1;

  auto const& desc = _gui->gui_state()->desc();
  auto const& topo = *desc.plugin;
  auto const& mappings = desc.param_mappings.params;
  param_topo_mapping mapping = mappings[_hovered_or_tweaked_param].topo;

  if (_module_params.module_index != -1)
  {
    current_module_slot = _activated_module_slot;
    current_module_index = _module_params.module_index;
  }
  else
  {
    current_module_slot = desc.param_mappings.params[_hovered_or_tweaked_param].topo.module_slot;
    current_module_index = desc.param_mappings.params[_hovered_or_tweaked_param].topo.module_index;
  }

  if (topo.modules[current_module_index].mod_output_source_selector != nullptr)
  {
    auto selected = topo.modules[current_module_index].mod_output_source_selector(*_gui->gui_state(), mapping);
    if (selected.module_index != -1 && selected.module_slot != -1)
    {
      current_module_slot = selected.module_slot;
      current_module_index = selected.module_index;
    }
  }

  int current_output = 0;
  int current_module_global = desc.module_topo_to_index.at(current_module_index) + current_module_slot;
  for (int i = 0; i < outputs.size() && current_output < max_mod_outputs; i++)
    if (current_module_global == outputs[i].data.module_global && outputs[i].data.param_global == -1)
    {
      float output_pos = outputs[i].data.value;
      float x = output_pos * w;
      int point = std::clamp((int)(output_pos * (count - 1)), 0, count - 1);
      float y = (1 - std::clamp(_data.series()[point], 0.0f, 1.0f)) * h;
      _mod_outputs[current_output]->activate();
      _mod_outputs[current_output]->setBounds(x - 3, y - 3, 6, 6);
      _mod_outputs[current_output]->repaint();
      current_output++;
    }

  if (current_output > 0)
  {
    // if any data found for this round, invalidate stuff from the previous round
    for (int i = current_output; i < max_mod_outputs; i++)
      _mod_outputs[i]->setVisible(false);
  }
  else
  {
    // if no data found for this round, invalidate stuff that expired 
    double invalidate_after = 0.05;
    double time_now = seconds_since_epoch();
    for (int i = 0; i < max_mod_outputs; i++)
      if (_mod_outputs[i]->activated_time_seconds() < time_now - invalidate_after)
        _mod_outputs[i]->setVisible(false);
  }
}

void 
module_graph::module_tab_changed(int module, int slot)
{
  // trigger re-render based on first new module param
  auto const& desc = _gui->gui_state()->desc();
  if(_module_params.module_index != -1 && _module_params.module_index != module) return;
  _activated_module_slot = slot;
  int index = desc.module_topo_to_index.at(module) + slot;
  _last_rerender_cause_param = desc.modules[index].params[0].info.global;
  request_rerender(_last_rerender_cause_param);
}

void 
module_graph::any_state_changed(int param, plain_value plain) 
{
  auto const& desc = _gui->gui_state()->desc();
  auto const& mapping = desc.param_mappings.params[param];
  if(_module_params.module_index == -1 || _module_params.module_index == mapping.topo.module_index)
  {
    if (_activated_module_slot == mapping.topo.module_slot)
    {
      if(_module_params.module_index != -1)
        _last_rerender_cause_param = param;
      request_rerender(param);
    }
    return;
  }

  // someone else changed a param and we depend on it
  // request re-render for first param in own topo or last changed in own topo
  if(std::find(
    _module_params.dependent_module_indices.begin(),
    _module_params.dependent_module_indices.end(),
    mapping.topo.module_index) == _module_params.dependent_module_indices.end())
    return;

  if(_last_rerender_cause_param != -1)
  {
    request_rerender(_last_rerender_cause_param);
    return;
  }  
  int index = desc.module_topo_to_index.at(_module_params.module_index) + _activated_module_slot;
  request_rerender(desc.modules[index].params[0].info.global);
}

void
module_graph::module_mouse_enter(int module)
{
  // trigger re-render based on first new module param
  auto const& desc = _gui->gui_state()->desc().modules[module];
  if (_module_params.module_index != -1 && _module_params.module_index != desc.module->info.index) return;
  if(desc.params.size() == 0) return;
  if(_module_params.render_on_module_mouse_enter && !desc.module->force_rerender_on_param_hover)
    request_rerender(desc.params[0].info.global);
}

void
module_graph::param_mouse_enter(int param)
{
  // trigger re-render based on specific param
  auto const& mapping = _gui->gui_state()->desc().param_mappings.params[param];
  if (_module_params.module_index != -1 && _module_params.module_index != mapping.topo.module_index) return;
  auto end = _module_params.render_on_param_mouse_enter_modules.end();
  auto begin = _module_params.render_on_param_mouse_enter_modules.begin();
  if (std::find(begin, end, mapping.topo.module_index) != end ||
    std::find(begin, end, -1) != end)
    request_rerender(param);
}

void
module_graph::request_rerender(int param)
{
  auto const& desc = _gui->gui_state()->desc();
  auto const& mapping = desc.param_mappings.params[param];
  int m = mapping.topo.module_index;
  int p = mapping.topo.param_index;
  if (desc.plugin->modules[m].params[p].dsp.direction == param_direction::output) return;
  
  _render_dirty = true;
  _hovered_or_tweaked_param = param;

  // will be picked up on the next round, 
  // need them to disappear first otherwise they may hang around on module switch
  for (int i = 0; i < max_mod_outputs; i++)
    _mod_outputs[i]->setVisible(false);
}

bool
module_graph::render_if_dirty()
{
  if (!_render_dirty) return false;
  if (_hovered_or_tweaked_param == -1) return false;

  auto const& mappings = _gui->gui_state()->desc().param_mappings.params;
  param_topo_mapping mapping = mappings[_hovered_or_tweaked_param].topo;
  auto const& module = _gui->gui_state()->desc().plugin->modules[mapping.module_index];
  if(module.graph_renderer != nullptr)
    render(module.graph_renderer(
      *_gui->gui_state(), _gui->get_module_graph_engine(module), _hovered_or_tweaked_param, mapping));
  _render_dirty = false;
  return true;
}

graph_mod_output::
graph_mod_output(lnf* lnf) : _lnf(lnf)
{
  setSize(6, 6);
  setVisible(false);
}

void
graph_mod_output::paint(Graphics& g)
{
  g.setColour(_lnf->colors().graph_modulation_bubble);
  g.fillEllipse(0, 0, 6, 6);
}

void 
graph_mod_output::activate()
{
  setVisible(true);
  _activated_time_seconds = seconds_since_epoch();
}

graph::
graph(lnf* lnf, graph_params const& params) :
  _lnf(lnf), _data(graph_data_type::na, {}), _params(params)
{
  _mod_outputs.resize(max_mod_outputs);
  for (int i = 0; i < max_mod_outputs; i++)
  {
    _mod_outputs[i] = std::make_unique<graph_mod_output>(_lnf);
    addChildComponent(_mod_outputs[i].get());
  }
}

void 
graph::render(graph_data const& data)
{
  _data = data;
  repaint();
}

void 
graph::paint_series(
  Graphics& g, jarray<float, 1> const& series,
  bool bipolar, float stroke_thickness, float midpoint)
{
  Path pFill;
  Path pStroke;
  float w = getWidth();
  float h = getHeight();
  float count = series.size();

  float y0 = (1 - std::clamp(series[0], 0.0f, 1.0f)) * h;
  pFill.startNewSubPath(0, bipolar? h * midpoint : h);
  pFill.lineTo(0, y0);
  pStroke.startNewSubPath(0, y0);
  for (int i = 1; i < series.size(); i++)
  {
    float yn = (1 - std::clamp(series[i], 0.0f, 1.0f)) * h;
    pFill.lineTo(i / count * w, yn);
    pStroke.lineTo(i / count * w, yn);
  }
  pFill.lineTo(w, bipolar? h * midpoint : h);
  pFill.closeSubPath();

  g.setColour(_lnf->colors().graph_area);
  g.fillPath(pFill);
  if (!_data.stroke_with_area())
    g.setColour(_lnf->colors().graph_line);
  g.strokePath(pStroke, PathStrokeType(stroke_thickness));
}

void
graph::paint(Graphics& g)
{
  Path p;
  float w = getWidth();
  float h = getHeight();
  g.fillAll(_lnf->colors().graph_background);

  // figure out grid box size such that row count is even and line 
  // count is uneven because we want a horizontal line in the middle
  float preferred_box_size = 9;
  int row_count = std::round(h / preferred_box_size);
  if(row_count % 2 != 0) row_count++;
  float box_size = h / row_count;
  int col_count = std::round(w / box_size);
  g.setColour(_lnf->colors().graph_grid);
  for(int i = 1; i < row_count; i++)
    g.fillRect(0.0f, i / (float)(row_count) * h, w, 1.0f);
  for (int i = 1; i < col_count; i++)
    g.fillRect(i / (float)(col_count) * w, 0.0f, 1.0f, h);

  // draw background partitions
  for (int part = 0; part < _data.partitions().size(); part++)
  {
    Rectangle<float> area(part / (float)_data.partitions().size() * w, 0.0f, w / _data.partitions().size(), h);
    if (part % 2 == 1)
    {
      g.setColour(_lnf->colors().graph_area.withAlpha(0.33f));
      g.fillRect(area);
    }
    g.setColour(_lnf->colors().graph_text);
    if (_params.scale_type == graph_params::scale_h)
      g.setFont(_lnf->font().withHeight(h * _params.partition_scale));
    else
      g.setFont(_lnf->font().withHeight(w * _params.partition_scale));
    g.drawText(_data.partitions()[part], area, Justification::centred, false);
  }
       
  if (_data.type() == graph_data_type::off || _data.type() == graph_data_type::na)
  {
    g.setColour(_lnf->colors().graph_text);
    auto text = _data.type() == graph_data_type::off ? "OFF" : "N/A";
    g.setFont(dynamic_cast<plugin_base::lnf&>(getLookAndFeel()).font().boldened());
    g.drawText(text, getLocalBounds().toFloat(), Justification::centred, false);
    return;
  }

  if (_data.type() == graph_data_type::multi_stereo)
  {
    auto area = _lnf->colors().graph_area;
    for (int i = 0; i < _data.multi_stereo().size(); i++)
    {
      float l = 1 - _data.multi_stereo()[i].first;
      float r = 1 - _data.multi_stereo()[i].second;
      g.setColour(area.withAlpha(0.67f / _data.multi_stereo().size()));
      g.fillRect(0.0f, l * h, w * 0.5f, (1 - l) * h);
      g.fillRect(w * 0.5f, r * h, w * 0.5f, (1 - r) * h);
      g.setColour(area);
      g.fillRect(0.0f, l * h, w * 0.5f, 1.0f);
      g.fillRect(w * 0.5f, r * h, w * 0.5f, 1.0f);
    }
    return;
  }

  if (_data.type() == graph_data_type::scalar)
  {
    auto area = _lnf->colors().graph_area;
    float scalar = _data.scalar();
    if (_data.bipolar())
    {
      scalar = 1.0f - bipolar_to_unipolar(scalar);
      g.setColour(area.withAlpha(0.67f));
      if (scalar <= 0.5f)
        g.fillRect(0.0f, scalar * h, w, (0.5f - scalar) * h);
      else
        g.fillRect(0.0f, 0.5f * h, w, (scalar - 0.5f) * h);
      g.setColour(area);
      g.fillRect(0.0f, scalar * h, w, 1.0f);
    }
    else
    {
      scalar = 1.0f - scalar;
      g.setColour(area.withAlpha(0.67f));
      g.fillRect(0.0f, scalar * h, w, (1 - scalar) * h);
      g.setColour(area);
      g.fillRect(0.0f, scalar * h, w, 1.0f);
    }
    return;
  }

  if (_data.type() == graph_data_type::audio)
  {
    jarray<float, 2> audio(_data.audio());
    for(int c = 0; c < 2; c++)
      for (int i = 0; i < audio[c].size(); i++)
        audio[c][i] = ((1 - c) + bipolar_to_unipolar(std::clamp(audio[c][i], -1.0f, 1.0f))) * 0.5f;
    paint_series(g, audio[0], true, _data.stroke_thickness(), 0.25f);
    paint_series(g, audio[1], true, _data.stroke_thickness(), 0.75f);
    return;
  }  

  assert(_data.type() == graph_data_type::series);
  jarray<float, 1> series(_data.series());
  if(_data.bipolar())
    for(int i = 0; i < series.size(); i++)
      series[i] = bipolar_to_unipolar(series[i]);
  paint_series(g, series, _data.bipolar(), _data.stroke_thickness(), 0.5f);
}

}