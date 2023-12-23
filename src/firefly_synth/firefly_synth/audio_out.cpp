#include <plugin_base/dsp/engine.hpp>
#include <plugin_base/topo/plugin.hpp>
#include <plugin_base/topo/support.hpp>

#include <firefly_synth/synth.hpp>
#include <cmath>

using namespace plugin_base;

namespace firefly_synth {

enum { section_main };
enum { param_gain, param_bal };

class audio_out_engine :
public module_engine {
  bool const _global;
public:
  void process(plugin_block& block) override;
  void reset(plugin_block const*) override {}
  PB_PREVENT_ACCIDENTAL_COPY(audio_out_engine);
  audio_out_engine(bool global): _global(global) {}
};

static graph_data
render_graph(plugin_state const& state, param_topo_mapping const& mapping)
{
  jarray<float, 1> result;
  result.push_back(0);
  float bal = state.get_plain_at(mapping.module_index, mapping.module_slot, param_bal, 0).real();
  float gain = state.get_plain_at(mapping.module_index, mapping.module_slot, param_gain, 0).real();
  float l = stereo_balance(0, bal) * gain;
  float r = stereo_balance(1, bal) * gain;
  return graph_data({ { l, r } }, {});
}

module_topo
audio_out_topo(int section, gui_colors const& colors, gui_position const& pos, bool global)
{
  auto voice_info(make_topo_info("{D5E1D8AF-8263-4976-BF68-B52A5CB82774}", "Voice Out", "V.Out", true, module_voice_out, 1));
  auto master_info(make_topo_info("{3EEB56AB-FCBC-4C15-B6F3-536DB0D93E67}", "Master Out", "M.Out", true, module_master_out, 1));
  module_stage stage = global ? module_stage::output : module_stage::voice;
  auto const info = topo_info(global ? master_info : voice_info);

  module_topo result(make_module(info,
    make_module_dsp(stage, module_output::none, 0, {}),
    make_module_gui(section, colors, pos, { 1, 1 })));

  result.graph_renderer = render_graph;
  result.engine_factory = [global](auto const&, int, int) { return std::make_unique<audio_out_engine>(global); };
  result.gui.menu_handler_factory = [global](plugin_state* state) { return make_audio_routing_menu_handler(state, global); };

  result.sections.emplace_back(make_param_section(section_main,
    make_topo_tag("{34BF24A3-696C-48F5-A49F-7CA445DEF38E}", "Main"),
    make_param_section_gui({ 0, 0 }, gui_dimension({ 1 }, { 1, 1 }))));

  double default_ = global? 0.5: 1.0;
  result.params.emplace_back(make_param(
    make_topo_info("{2156DEE6-A147-4B93-AEF3-ABE69F53DBF9}", "Gain", param_gain, 1),
    make_param_dsp_accurate(param_automate::automate_modulate), make_domain_percentage(0, 1, default_, 0, true),
    make_param_gui_single(section_main, gui_edit_type::knob, { 0, 0 }, gui_label_contents::value,
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::center))));
  result.params.emplace_back(make_param(
    make_topo_info("{7CCD4A32-FD84-402E-B099-BB94AAAD3C9E}", "Bal", param_bal, 1),
    make_param_dsp_accurate(param_automate::automate_modulate), make_domain_percentage(-1, 1, 0, 0, true),
    make_param_gui_single(section_main, gui_edit_type::knob, { 0, 1 }, gui_label_contents::value,
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::center))));

  return result;
}

void
audio_out_engine::process(plugin_block& block)
{
  float* audio_out[2];
  int module = _global? module_master_out: module_voice_out;
  auto& mixer = get_audio_matrix_mixer(block, _global);
  auto const& audio_in = mixer.mix(block, module, 0);
  auto const& modulation = get_cv_matrix_mixdown(block, _global);
  auto const& bal_curve = *modulation[module][0][param_bal][0];
  auto const& gain_curve = *modulation[module][0][param_gain][0];

  if (_global)
  {
    audio_out[0] = block.out->host_audio[0];
    audio_out[1] = block.out->host_audio[1];
  }
  else
  {
    audio_out[0] = block.voice->result[0].data().data();
    audio_out[1] = block.voice->result[1].data().data();
  }

  for (int f = block.start_frame; f < block.end_frame; f++)
  {
    float bal = block.normalized_to_raw(module, param_bal, bal_curve[f]);
    for(int c = 0; c < 2; c++)
      audio_out[c][f] = audio_in[c][f] * gain_curve[f] * stereo_balance(c, bal);
  }
}

}
