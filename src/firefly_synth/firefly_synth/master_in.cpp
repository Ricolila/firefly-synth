#include <plugin_base/helpers/dsp.hpp>
#include <plugin_base/topo/plugin.hpp>
#include <plugin_base/topo/support.hpp>
#include <plugin_base/desc/param.hpp>
#include <plugin_base/shared/state.hpp>
#include <plugin_base/dsp/engine.hpp>
#include <plugin_base/dsp/utility.hpp>
#include <plugin_base/dsp/graph_engine.hpp>

#include <firefly_synth/synth.hpp>
#include <cmath>

using namespace plugin_base;

namespace firefly_synth {

static int const aux_count = 6;

enum { output_aux, output_mod, output_pb };
enum { section_aux, section_linked, section_linked_pbrange, section_glob_uni_prms, section_glob_uni_count };

enum { 
  param_aux, param_mod, param_pb, param_pb_range, 
  param_glob_uni_dtn, param_glob_uni_sprd, param_glob_uni_lfo_phase, 
  param_glob_uni_lfo_dtn, param_glob_uni_osc_phase, param_glob_uni_env_dtn, 
  param_glob_uni_voices, param_count };

// we provide the buttons, everyone else needs to implement it
extern int const master_in_param_pb_range = param_pb_range;
extern int const master_in_param_glob_uni_dtn = param_glob_uni_dtn;
extern int const master_in_param_glob_uni_sprd = param_glob_uni_sprd;
extern int const master_in_param_glob_uni_voices = param_glob_uni_voices; 
extern int const master_in_param_glob_uni_env_dtn = param_glob_uni_env_dtn;
extern int const master_in_param_glob_uni_lfo_dtn = param_glob_uni_lfo_dtn;
extern int const master_in_param_glob_uni_lfo_phase = param_glob_uni_lfo_phase;
extern int const master_in_param_glob_uni_osc_phase = param_glob_uni_osc_phase;

class master_in_engine :
public module_engine {
public:
  void reset(plugin_block const*) override {}
  void process(plugin_block& block) override;
  PB_PREVENT_ACCIDENTAL_COPY_DEFAULT_CTOR(master_in_engine);
};

static graph_data
render_graph(plugin_state const& state, graph_engine* engine, int param, param_topo_mapping const& mapping)
{
  if (mapping.param_index == param_pb_range)
    return graph_data(graph_data_type::na, {});
  float value = state.get_plain_at(mapping).real();
  bool bipolar = mapping.param_index == param_pb;
  std::string partition = state.desc().params[param]->info.name;
  return graph_data(value, bipolar, { partition });
}

module_topo
master_in_topo(int section, bool is_fx, gui_position const& pos)
{
  std::vector<int> row_distribution = { 1, 1 };
  std::vector<int> column_distribution = { 37, 27, 26, 40, 28, 18, 92, 16 };
  if(is_fx) 
  {
    row_distribution = { 1 };
    column_distribution = { 64, 26, 30, 38, 73, 53 };
  }
  module_topo result(make_module(
    make_topo_info("{E22B3B9D-2337-4DE5-AA34-EB3351948D6A}", true, "Master In", "Master In", "MIn", module_master_in, 1),
    make_module_dsp(module_stage::input, module_output::cv, 0, {
      make_module_dsp_output(true, make_topo_info_basic("{9D36E713-80F9-49CA-9E81-17E424FF66EE}", "Aux", output_aux, aux_count)),
      make_module_dsp_output(true, make_topo_info("{91B915D6-0DCA-4F59-A396-6AF31DA28DBB}", true, "Mod Wheel", "Mod", "Mod", output_mod, 1)),
      make_module_dsp_output(true, make_topo_info("{EB8CBA31-212A-42EA-956E-69063BF93C58}", true, "Pitch Bend", "PB", "PB", output_pb, 1)) }),
      make_module_gui(section, pos, { row_distribution, column_distribution } )));
  result.gui.tabbed_name = "Master In";
  result.info.description = "Master CV module with automation, MIDI and BPM smoothing, MIDI-linked modwheel and pitchbend plus some additional freely-assignable parameters.";

  result.graph_renderer = render_graph;
  result.force_rerender_on_param_hover = true;
  result.gui.menu_handler_factory = make_cv_routing_menu_handler;
  result.engine_factory = [](auto const&, int, int) { return std::make_unique<master_in_engine>(); };

  auto section_aux_gui = make_param_section_gui({ 0, 0, 2, 4 }, gui_dimension({ 1, 1 }, {
      gui_dimension::auto_size_all, 1,
      gui_dimension::auto_size_all, 1,
      gui_dimension::auto_size_all, 1, }), 
      gui_label_edit_cell_split::horizontal);
  // TODO
  if(is_fx) section_aux_gui = make_param_section_gui({ 0, 0, 1, 3 }, gui_dimension({ 1 }, { 
      gui_dimension::auto_size_all, 1,
      gui_dimension::auto_size_all, 1,
      gui_dimension::auto_size_all, 1,
      gui_dimension::auto_size_all, 1,
      gui_dimension::auto_size_all, 1,
      gui_dimension::auto_size_all, 1, }),
      gui_label_edit_cell_split::horizontal);
  result.sections.emplace_back(make_param_section(section_aux,
    make_topo_tag_basic("{BB12B605-4EEF-4FEA-9F2C-FACEEA39644A}", "Aux"), section_aux_gui));
  auto& aux = result.params.emplace_back(make_param(
    make_topo_info_basic("{9EC93CE9-6BD6-4D17-97A6-403ED34BBF38}", "Aux", param_aux, aux_count),
    make_param_dsp_accurate(param_automate::modulate), make_domain_percentage_identity(0, 0, true),
    make_param_gui(section_aux, gui_edit_type::knob, param_layout::single_grid, { 0, 0 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  aux.info.description = "Auxilliary controls to be used through automation and the CV matrices.";
  aux.gui.alternate_drag_output_id = result.dsp.outputs[output_aux].info.tag.id;
  aux.gui.display_formatter = [is_fx](auto const& desc) { return (desc.info.slot == 0 || (!is_fx && desc.info.slot == 3))? desc.info.name: std::to_string(desc.info.slot + 1); };

  auto linked_gui = make_param_section_gui(
    { 0, 4, 2, 1 }, gui_dimension({ 1, 1 }, { gui_dimension::auto_size_all, 1 }), gui_label_edit_cell_split::horizontal);
  if(is_fx) linked_gui = make_param_section_gui(
    { 0, 5, 1, 1 }, gui_dimension({ 1 }, { 1, 1 }), gui_label_edit_cell_split::no_split);
  result.sections.emplace_back(make_param_section(section_linked,
    make_topo_tag_basic("{56FD2FEB-3084-4E28-B56C-06D31406EB42}", "Linked"), linked_gui));
  auto& mod_wheel = result.params.emplace_back(make_param(
    make_topo_info("{7696305C-28F3-4C54-A6CA-7C9DB5635153}", true, "Mod Wheel", "Mod", "Mod", param_mod, 1),
    make_param_dsp_midi({ module_midi, 0, 1 }), make_domain_percentage_identity(0, 0, true),
    make_param_gui_single(section_linked, gui_edit_type::knob, { 0, 0 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  mod_wheel.info.description = "Linked to MIDI mod wheel, updates on incoming MIDI events.";
  mod_wheel.gui.alternate_drag_output_id = result.dsp.outputs[output_mod].info.tag.id;
  auto& pitch_bend = result.params.emplace_back(make_param(
    make_topo_info("{D1B334A6-FA2F-4AE4-97A0-A28DD0C1B48D}", true, "Pitch Bend", "PB", "PB", param_pb, 1),
    make_param_dsp_midi({ module_midi, 0, midi_source_pb }), make_domain_percentage(-1, 1, 0, 0, true),
    make_param_gui_single(section_linked, gui_edit_type::knob, { is_fx? 0: 1, is_fx? 1: 0 },
    make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  pitch_bend.info.description = "Linked to MIDI pitch bend, updates on incoming MIDI events.";
  pitch_bend.gui.alternate_drag_output_id = result.dsp.outputs[output_pb].info.tag.id;

  if(is_fx) return result;

  result.sections.emplace_back(make_param_section(section_linked_pbrange,
    make_topo_tag_basic("{12EAD382-DF92-486C-A451-E19EC1C009BD}", "Linked PB Range"),
    make_param_section_gui({ 0, 5, is_fx? 1: 2, 1 }, gui_dimension({ 1, 1 }, { 1 }),
      gui_label_edit_cell_split::vertical)));
  auto& pb_range = result.params.emplace_back(make_param(
    make_topo_info("{79B7592A-4911-4B04-8F71-5DD4B2733F4F}", true, "PB Range", "Rng", "Range", param_pb_range, 1),
    make_param_dsp_block(param_automate::automate), make_domain_step(1, 24, 12, 0),
    make_param_gui_single(section_linked_pbrange, gui_edit_type::list, { 0, 0 },
      make_label(gui_label_contents::name, gui_label_align::top, gui_label_justify::center))));
  pb_range.info.description = "Pitch bend range. Together with Pitch Bend this affects the base pitch of all oscillators.";

  result.sections.emplace_back(make_param_section(section_glob_uni_prms,
    make_topo_tag_basic("{7DCA43C8-CD48-4414-9017-EC1B982281FF}", "Global Unison Params"),
    make_param_section_gui({ 0, 6, is_fx? 1: 2, 1 }, gui_dimension({ 1, 1 }, { 
      gui_dimension::auto_size_all, 1, gui_dimension::auto_size_all, 1, gui_dimension::auto_size_all, 1 }), 
        gui_label_edit_cell_split::horizontal)));
  auto& glob_uni_dtn = result.params.emplace_back(make_param(
    make_topo_info("{2F0E199D-7B8A-497E-BED4-BC0FC55F1720}", true, "Global Unison Detune", "Dtn", "Uni Dtn", param_glob_uni_dtn, 1),
    make_param_dsp_accurate(param_automate::automate), make_domain_percentage_identity(0.33, 0, true),
    make_param_gui_single(section_glob_uni_prms, gui_edit_type::knob, { 0, 0 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  glob_uni_dtn.info.description = "Global unison voice pitch detune amount.";
  glob_uni_dtn.gui.bindings.enabled.bind_params({ param_glob_uni_voices }, [](auto const& vs) { return vs[0] > 1; });
  glob_uni_dtn.gui.bindings.global_enabled.bind_param(module_voice_in, voice_in_param_mode, [](int v) { return v == engine_voice_mode_poly; });
  auto& glob_uni_spread = result.params.emplace_back(make_param(
    make_topo_info("{356468BC-59A0-40D0-AC14-C7DDBB16F4CE}", true, "Global Unison Spread", "Spr", "Uni Spr", param_glob_uni_sprd, 1),
    make_param_dsp_accurate(param_automate::automate), make_domain_percentage_identity(0.5, 0, true),
    make_param_gui_single(section_glob_uni_prms, gui_edit_type::knob, { 1, 0 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  glob_uni_spread.info.description = "Global unison stereo spread.";
  glob_uni_spread.gui.bindings.enabled.bind_params({ param_glob_uni_voices }, [](auto const& vs) { return vs[0] > 1; });
  glob_uni_spread.gui.bindings.global_enabled.bind_param(module_voice_in, voice_in_param_mode, [](int v) { return v == engine_voice_mode_poly; });
  auto& glob_uni_lfo_phase = result.params.emplace_back(make_param(
    make_topo_info("{1799D722-B551-485F-A7F1-0590D97514EF}", true, "Global Unison LFO Phase Offset", "LFO Phs", "Uni LFO Phs", param_glob_uni_lfo_phase, 1),
    make_param_dsp_block(param_automate::automate), make_domain_percentage_identity(0.0, 0, true),
    make_param_gui_single(section_glob_uni_prms, gui_edit_type::knob, { 0, 2 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  glob_uni_lfo_phase.info.description = "Global unison voice LFO phase offset.";
  glob_uni_lfo_phase.gui.bindings.enabled.bind_params({ param_glob_uni_voices }, [](auto const& vs) { return vs[0] > 1; });
  glob_uni_lfo_phase.gui.bindings.global_enabled.bind_param(module_voice_in, voice_in_param_mode, [](int v) { return v == engine_voice_mode_poly; });
  auto& glob_uni_lfo_dtn = result.params.emplace_back(make_param(
    make_topo_info("{1B61F48D-7995-4295-A8DB-3AA44E1BF346}", true, "Global Unison LFO Detune", "LFO Dtn", "Uni LFO Dtn", param_glob_uni_lfo_dtn, 1),
    make_param_dsp_accurate(param_automate::automate), make_domain_percentage_identity(0.0, 0, true),
    make_param_gui_single(section_glob_uni_prms, gui_edit_type::knob, { 1, 2 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  glob_uni_lfo_dtn.info.description = "Global unison voice LFO detune amount.";
  glob_uni_lfo_dtn.gui.bindings.enabled.bind_params({ param_glob_uni_voices }, [](auto const& vs) { return vs[0] > 1; });
  glob_uni_lfo_dtn.gui.bindings.global_enabled.bind_param(module_voice_in, voice_in_param_mode, [](int v) { return v == engine_voice_mode_poly; });
  auto& glob_uni_osc_phase = result.params.emplace_back(make_param(
    make_topo_info("{35D94C8A-3986-44EC-A4D6-485ACF199C4C}", true, "Global Unison Osc Phase Offset", "Osc Phs", "Uni Osc Phs", param_glob_uni_osc_phase, 1),
    make_param_dsp_block(param_automate::automate), make_domain_percentage_identity(0.0, 0, true),
    make_param_gui_single(section_glob_uni_prms, gui_edit_type::knob, { 0, 4 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  glob_uni_osc_phase.info.description = "Global unison voice osc phase offset.";
  glob_uni_osc_phase.gui.bindings.enabled.bind_params({ param_glob_uni_voices }, [](auto const& vs) { return vs[0] > 1; });
  glob_uni_osc_phase.gui.bindings.global_enabled.bind_param(module_voice_in, voice_in_param_mode, [](int v) { return v == engine_voice_mode_poly; });
  auto& glob_uni_env_dtn = result.params.emplace_back(make_param(
    make_topo_info("{52E0A939-296F-4F2A-A1E4-F283556B0BFD}", true, "Global Unison Env Detune", "Env Dtn", "Uni Env Dtn", param_glob_uni_env_dtn, 1),
    make_param_dsp_block(param_automate::automate), make_domain_percentage_identity(0.0, 0, true),
    make_param_gui_single(section_glob_uni_prms, gui_edit_type::knob, { 1, 4 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  glob_uni_env_dtn.info.description = "Global unison voice envelope detune amount.";
  glob_uni_env_dtn.gui.bindings.enabled.bind_params({ param_glob_uni_voices }, [](auto const& vs) { return vs[0] > 1; });
  glob_uni_env_dtn.gui.bindings.global_enabled.bind_param(module_voice_in, voice_in_param_mode, [](int v) { return v == engine_voice_mode_poly; });

  result.sections.emplace_back(make_param_section(section_glob_uni_count,
    make_topo_tag_basic("{550AAF78-C95A-4D4E-814C-0C5CC26C6457}", "Global Unison Voices"),
    make_param_section_gui({ 0, 7, is_fx? 1: 2, 1 }, gui_dimension({ 1, 1 }, { 1 }), gui_label_edit_cell_split::vertical)));
  auto& glob_uni_voices = result.params.emplace_back(make_param(
    make_topo_info("{C2B06E63-0283-4564-BABB-F20D9B30AD68}", true, "Global Unison Voices", "Uni", "Uni", param_glob_uni_voices, 1),
    make_param_dsp_block(param_automate::automate), make_domain_step(1, max_global_unison_voices, 1, 0),
    make_param_gui_single(section_glob_uni_count, gui_edit_type::list, { 0, 0 },
      make_label(gui_label_contents::name, gui_label_align::top, gui_label_justify::center))));
  glob_uni_voices.info.description = "Global unison voice count. Global unison spawns an entire polyphonic synth voice per unison voice. This includes per-voice oscillators, effects, lfo's and envelopes.";
  glob_uni_voices.gui.bindings.global_enabled.bind_param(module_voice_in, voice_in_param_mode, [](int v) { return v == engine_voice_mode_poly; });

  return result;
}

void
master_in_engine::process(plugin_block& block)
{
  auto& own_cv = block.state.own_cv;  
  auto const& accurate = block.state.own_accurate_automation;
  accurate[param_pb][0].copy_to(block.start_frame, block.end_frame, own_cv[output_pb][0]);
  accurate[param_mod][0].copy_to(block.start_frame, block.end_frame, own_cv[output_mod][0]);
  for(int i = 0; i < aux_count; i++)
    accurate[param_aux][i].copy_to(block.start_frame, block.end_frame, own_cv[output_aux][i]);
}

}
