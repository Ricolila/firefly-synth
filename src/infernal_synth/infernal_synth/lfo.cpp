#include <plugin_base/dsp/engine.hpp>
#include <plugin_base/dsp/utility.hpp>
#include <plugin_base/topo/plugin.hpp>
#include <plugin_base/topo/support.hpp>

#include <infernal_synth/synth.hpp>
#include <cmath>

using namespace plugin_base;

namespace infernal_synth {

enum { section_main };
enum { param_sync, param_rate, param_num, param_denom };

class lfo_engine: 
public module_engine {
  float _phase;

public:
  lfo_engine() { initialize(); }
  INF_PREVENT_ACCIDENTAL_COPY(lfo_engine);
  void initialize() override { _phase = 0; }
  void process(plugin_block& block) override;
};

module_topo
lfo_topo()
{
  module_topo result(make_module(
    make_topo_info("{FAF92753-C6E4-4D78-BD7C-584EF473E29F}", "Global LFO", module_lfo, 3), 
    make_module_dsp(module_stage::input, module_output::cv, 1, 0),
    make_module_gui(gui_layout::tabbed, { 0, 0 }, { 1, 1 })));
  result.sections.emplace_back(make_section(section_main,
    make_topo_tag("{F0002F24-0CA7-4DF3-A5E3-5B33055FD6DC}", "Main"),
    make_section_gui({ 0, 0 }, { 1, 4 })));
  result.engine_factory = [](auto const&, int, int) ->
    std::unique_ptr<module_engine> { return std::make_unique<lfo_engine>(); };

  result.params.emplace_back(make_param(
    make_topo_info("{2A9CAE77-13B0-406F-BA57-1A30ED2F5D80}", "Sync", param_sync, 1),
    make_param_dsp_block(param_automate::none), make_domain_toggle(false),
    make_param_gui_single(section_main, gui_edit_type::toggle, { 0, 0 }, 
      make_label_default(gui_label_contents::name))));
  
  auto& rate = result.params.emplace_back(make_param(
    make_topo_info("{EE68B03D-62F0-4457-9918-E3086B4BCA1C}", "Rate", param_rate, 1),
    make_param_dsp_accurate(param_automate::modulate), make_domain_linear(0.1, 20, 1, 2, "Hz"),
    make_param_gui_single(section_main, gui_edit_type::knob, { 0, 1 }, 
      make_label_default(gui_label_contents::both))));
  rate.gui.bindings.enabled.params = { param_sync };
  rate.gui.bindings.enabled.selector = [] (auto const& vs) { return vs[0] == 0; };

  auto& num = result.params.emplace_back(make_param(
    make_topo_info("{5D05DF07-9B42-46BA-A36F-E32F2ADA75E0}", "Num", param_num, 1),
    make_param_dsp_block(param_automate::none), make_domain_step(1, 16, 1, 0),
    make_param_gui_single(section_main, gui_edit_type::list, { 0, 2 }, 
      make_label_default(gui_label_contents::name))));
  num.gui.bindings.enabled.params = { param_sync };
  num.gui.bindings.enabled.selector = [](auto const& vs) { return vs[0] != 0; };

  auto& denom = result.params.emplace_back(make_param(
    make_topo_info("{84B58AC9-C401-4580-978C-60591AFB757B}", "Denom", param_denom, 1),
    make_param_dsp_block(param_automate::none), make_domain_step(1, 16, 4, 0),
    make_param_gui_single(section_main, gui_edit_type::list, { 0, 3 }, 
      make_label_default(gui_label_contents::name))));
  denom.gui.bindings.enabled.params = { param_sync };
  denom.gui.bindings.enabled.selector = [](auto const& vs) { return vs[0] != 0; };

  return result;
}

void
lfo_engine::process(plugin_block& block)
{
  // TODO make per-voice lfo
  // TODO rate/sync
  auto const& rate_curve = block.state.own_accurate_automation[param_rate][0];
  for (int f = block.start_frame; f < block.end_frame; f++)
  {
    block.state.own_cv[0][f] = (std::sin(2.0f * pi32 * _phase) + 1.0f) * 0.5f;
    _phase += block.normalized_to_raw(module_lfo, param_rate, rate_curve[f]) / block.sample_rate;
    if(_phase >= 1.0f) _phase = 0.0f;
  }
}

}
