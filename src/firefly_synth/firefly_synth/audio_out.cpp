#include <plugin_base/dsp/engine.hpp>
#include <plugin_base/topo/plugin.hpp>
#include <plugin_base/topo/support.hpp>

#include <firefly_synth/synth.hpp>
#include <cmath>

using namespace plugin_base;

namespace firefly_synth {

enum { section_main };
enum { param_gain, param_bal };
enum { scratch_bal, scratch_count };

class global_audio_out_engine :
public module_engine {
public:
  void reset(plugin_block const*) override {}
  void process(plugin_block& block) override;
  PB_PREVENT_ACCIDENTAL_COPY_DEFAULT_CTOR(global_audio_out_engine);
};

class voice_audio_out_engine :
public module_engine {

  template <bool GlobalUnison>
  void process_unison(plugin_block& block);
public:
  void reset(plugin_block const*) override {}
  void process(plugin_block& block) override;
  PB_PREVENT_ACCIDENTAL_COPY_DEFAULT_CTOR(voice_audio_out_engine);
};

static graph_data
render_graph(
  plugin_state const& state, graph_engine* engine, int param, param_topo_mapping const& mapping, 
  bool overlay, std::vector<mod_indicator_state> const& mod_indicators)
{
  std::string partition = mapping.module_index == module_global_out? "Global": "Voice";
  float bal = state.get_plain_at(mapping.module_index, mapping.module_slot, param_bal, 0).real();
  float gain = state.get_plain_at(mapping.module_index, mapping.module_slot, param_gain, 0).real();
  float l = stereo_balance<0>(bal) * gain;
  float r = stereo_balance<1>(bal) * gain;
  return graph_data({ { l, r } }, { partition });
}

module_topo
audio_out_topo(int section, gui_position const& pos, bool global)
{
  auto voice_info(make_topo_info("{D5E1D8AF-8263-4976-BF68-B52A5CB82774}", true, "Voice Out", "Voice Out", "VOut", module_voice_out, 1));
  voice_info.description = "Controls gain and balance of individual voices.";
  auto global_info(make_topo_info("{3EEB56AB-FCBC-4C15-B6F3-536DB0D93E67}", true, "Global Out", "Global Out", "GOut", module_global_out, 1));
  global_info.description = "Controls gain and balance of global audio output.";
  module_stage stage = global ? module_stage::output : module_stage::voice;
  auto const info = topo_info(global ? global_info : voice_info);

  module_topo result(make_module(info,
    make_module_dsp(stage, module_output::none, scratch_count, {}),
    make_module_gui(section, pos, { 1, 1 })));
  result.gui.alternate_drag_source_id = info.tag.id;
  result.gui.is_drag_mod_source = true;
  result.gui.tabbed_name = "OUT";

  result.graph_renderer = render_graph;
  result.gui.menu_handler_factory = [global](plugin_state* state) {
    return make_audio_routing_menu_handler(state, global); };
  if(global)
    result.engine_factory = [](auto const&, int, int) { 
      return std::make_unique<global_audio_out_engine>(); };
  else
    result.engine_factory = [](auto const&, int, int) { 
      return std::make_unique<voice_audio_out_engine>(); };

  int gain_row = 0;
  int gain_col = 0;
  int bal_row = 1;
  int bal_col = 0;
  double gain_default_ = 1.0;
  gui_dimension dimension({ 1, 1 }, { gui_dimension::auto_size_all, 1 });
  gui_edit_type edit_type = gui_edit_type::hslider;
  if (global)
  {
    bal_row = 0;
    bal_col = 1;
    gain_default_ = 0.33;
    edit_type = gui_edit_type::knob;
    dimension = gui_dimension({ 1 }, { 1, 1 });
  } 

  result.sections.emplace_back(make_param_section(section_main,
    make_topo_tag_basic("{34BF24A3-696C-48F5-A49F-7CA445DEF38E}", "Main"),
    make_param_section_gui({ 0, 0 }, dimension, global ? gui_label_edit_cell_split::no_split : gui_label_edit_cell_split::horizontal)));
  auto& gain = result.params.emplace_back(make_param(
    make_topo_info_basic("{2156DEE6-A147-4B93-AEF3-ABE69F53DBF9}", "Gain", param_gain, 1),
    make_param_dsp_accurate(param_automate::modulate), make_domain_percentage_identity(gain_default_, 0, true),
    make_param_gui_single(section_main, edit_type, { gain_row, gain_col },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  gain.info.description = "Output gain.";
  auto& bal = result.params.emplace_back(make_param(
    make_topo_info("{7CCD4A32-FD84-402E-B099-BB94AAAD3C9E}", true, "Balance", "Bal", "Bal", param_bal, 1),
    make_param_dsp_accurate(param_automate::modulate), make_domain_percentage(-1, 1, 0, 0, true),
    make_param_gui_single(section_main, edit_type, { bal_row, bal_col },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  bal.info.description = "Output stereo balance.";
  return result;
} 

void
global_audio_out_engine::process(plugin_block& block)
{
  auto& mixer = get_audio_audio_matrix_mixer(block, true);
  auto const& audio_in = mixer.mix(block, module_global_out, 0);
  auto const& modulation = get_cv_audio_matrix_mixdown(block, true);
  auto const& bal_curve_norm = *modulation[module_global_out][0][param_bal][0];
  auto const& gain_curve = *modulation[module_global_out][0][param_gain][0];

  auto& bal_curve = block.state.own_scratch[scratch_bal];
  block.normalized_to_raw_block<domain_type::linear>(module_global_out, param_bal, bal_curve_norm, bal_curve);
  for (int f = block.start_frame; f < block.end_frame; f++)
  {
    block.out->host_audio[0][f] = audio_in[0][f] * gain_curve[f] * stereo_balance<0>(bal_curve[f]);
    block.out->host_audio[1][f] = audio_in[1][f] * gain_curve[f] * stereo_balance<1>(bal_curve[f]);
  }
}


void
voice_audio_out_engine::process(plugin_block& block)
{
  if(block.voice->state.sub_voice_count > 1)
    process_unison<true>(block);
  else
    process_unison<false>(block);
}

template <bool GlobalUnison> void 
voice_audio_out_engine::process_unison(plugin_block& block)
{
  auto& mixer = get_audio_audio_matrix_mixer(block, false);
  auto const& audio_in = mixer.mix(block, module_voice_out, 0);
  auto const& modulation = get_cv_audio_matrix_mixdown(block, false);
  auto const& amp_env = block.voice->all_cv[module_env][0][0][0];
  auto const& gain_curve = *modulation[module_voice_out][0][param_gain][0];
  auto const& bal_curve_norm = *modulation[module_voice_out][0][param_bal][0];
  jarray<float, 1> const* glob_uni_sprd_curve = nullptr;

  float attn = 1.0f;
  float voice_pos = 0.0f;
  float voice_bal = 0.0f;
  if constexpr (GlobalUnison)
  {
    attn = std::sqrt(block.voice->state.sub_voice_count);
    voice_pos = (float)block.voice->state.sub_voice_index / (block.voice->state.sub_voice_count - 1.0f);
    voice_pos = unipolar_to_bipolar(voice_pos);
    glob_uni_sprd_curve = modulation[module_voice_in][0][voice_in_param_uni_sprd][0];
  }

  auto& bal_curve = block.state.own_scratch[scratch_bal];
  block.normalized_to_raw_block<domain_type::linear>(module_global_out, param_bal, bal_curve_norm, bal_curve);
  for (int f = block.start_frame; f < block.end_frame; f++)
  {
    if constexpr (GlobalUnison)
      voice_bal = voice_pos * (*glob_uni_sprd_curve)[f];
    block.voice->result[0][f] = audio_in[0][f] * gain_curve[f] * amp_env[f] * stereo_balance<0>(bal_curve[f]) * stereo_balance<0>(voice_bal) / attn;
    block.voice->result[1][f] = audio_in[1][f] * gain_curve[f] * amp_env[f] * stereo_balance<1>(bal_curve[f]) * stereo_balance<1>(voice_bal) / attn;
  }
}

}
