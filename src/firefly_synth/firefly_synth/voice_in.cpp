#include <plugin_base/helpers/dsp.hpp>
#include <plugin_base/topo/plugin.hpp>
#include <plugin_base/topo/support.hpp>
#include <plugin_base/shared/state.hpp>
#include <plugin_base/dsp/engine.hpp>
#include <plugin_base/dsp/utility.hpp>
#include <plugin_base/dsp/graph_engine.hpp>
#include <plugin_base/shared/io_plugin.hpp>

#include <firefly_synth/synth.hpp>
#include <cmath>

using namespace plugin_base;

namespace firefly_synth {

enum { output_pitch_offset };
enum { over_1, over_2, over_4 };
enum { porta_off, porta_on, porta_auto };
enum { scratch_pb, scratch_cent, scratch_pitch, scratch_count };
enum { section_left, section_sync, section_mid, section_right };

enum {
  param_mode, param_porta_sync, param_porta, param_porta_time, param_porta_tempo,
  param_oversmp, param_note, param_cent, param_pitch, param_pb, param_count };

extern int const voice_in_param_mode = param_mode;
extern int const voice_in_param_oversmp = param_oversmp;
extern int const voice_in_output_pitch_offset = output_pitch_offset;

static std::vector<list_item>
mode_items()
{
  std::vector<list_item> result;
  result.emplace_back("{88F746C4-1A70-4A64-A11D-584D87D3059C}", "Poly");
  result.emplace_back("{6ABA8E48-F284-40A4-A0E2-C263B536D493}", "Mono");
  result.emplace_back("{519341B0-4F79-4433-9449-1386F927E88B}", "Release");
  return result;
}

static std::vector<list_item>
porta_items()
{
  std::vector<list_item> result;
  result.emplace_back("{51C360E5-967A-4218-B375-5052DAC4FD02}", "Off");
  result.emplace_back("{112A9728-8564-469E-95A7-34FE5CC7C8FC}", "On");
  result.emplace_back("{0E3AF80A-F242-4176-8C72-C0C91D72AEBB}", "Auto");
  return result;
}

static std::vector<list_item>
over_items()
{
  std::vector<list_item> result;
  result.emplace_back("{F9C54B64-3635-417F-86A9-69B439548F3C}", "1X");
  result.emplace_back("{937686E8-AC03-420B-A3FF-0ECE1FF9B23E}", "2X");
  result.emplace_back("{64F2A767-DE91-41DF-B2F1-003FCC846384}", "4X");
  return result;
}

class voice_in_state_converter :
public state_converter
{
  plugin_desc const* const _desc;
public:
  voice_in_state_converter(plugin_desc const* const desc) : _desc(desc) {}
  void post_process(load_handler const& handler, plugin_state& new_state) override {}

  bool handle_invalid_param_value(
    std::string const& new_module_id, int new_module_slot,
    std::string const& new_param_id, int new_param_slot,
    std::string const& old_value, load_handler const& handler,
    plain_value& new_value) override;
};

class voice_in_engine :
public module_engine {
  int _position = -1;
  int _porta_samples = -1;
  float _to_note_pitch = -1;
  float _from_note_pitch = -1;
  float _mono_porta_time = -1;
  int _mono_porta_samples = -1;
  
  template <bool Monophonic>
  void process_mode(plugin_block& block);
  template <bool Monophonic, bool GlobalUnison>
  void process_mode_unison(plugin_block& block);

public:
  void reset(plugin_block const*) override;
  void process(plugin_block& block) override;
  PB_PREVENT_ACCIDENTAL_COPY_DEFAULT_CTOR(voice_in_engine);
};

static graph_data
render_graph(plugin_state const& state, graph_engine* engine, int param, param_topo_mapping const& mapping)
{
  bool bipolar = mapping.param_index == param_cent;
  std::string partition = state.desc().params[param]->info.name;
  if (mapping.param_index == param_cent || mapping.param_index == param_porta_time)
    return graph_data(state.get_plain_at(mapping).real(), bipolar, { partition });
  return graph_data(graph_data_type::na, {});
}

bool
voice_in_state_converter::handle_invalid_param_value(
  std::string const& new_module_id, int new_module_slot,
  std::string const& new_param_id, int new_param_slot,
  std::string const& old_value, load_handler const& handler,
  plain_value& new_value)
{
  // Max osc oversampling got reduced from 8x to 4x.
  if (handler.old_version() < plugin_version{ 1, 7, 2 })
    if (new_param_id == _desc->plugin->modules[module_voice_in].params[param_oversmp].info.tag.id)
      if (old_value == "{9C6E560D-4999-40D9-85E4-C02468296206}")
      {
        new_value = _desc->raw_to_plain_at(module_voice_in, param_oversmp, over_4);
        return true;
      }

  return false;
}

module_topo
voice_in_topo(int section, gui_position const& pos)
{
  module_topo result(make_module(
    make_topo_info("{524138DF-1303-4961-915A-3CAABA69D53A}", true, "Voice In", "Voice In", "VIn", module_voice_in, 1),
    make_module_dsp(module_stage::voice, module_output::cv, scratch_count, {
      make_module_dsp_output(false, make_topo_info_basic("{58E73C3A-CACD-48CC-A2B6-25861EC7C828}", "Pitch", 0, 1)) }),
    make_module_gui(section, pos, { { 1 }, { 32, 13, 34, 63 } } )));
  result.info.description = "Oscillator common module. Controls portamento, oversampling and base pitch for all oscillators.";
  
  result.graph_renderer = render_graph;
  result.force_rerender_on_param_hover = true;
  result.gui.menu_handler_factory = make_cv_routing_menu_handler;
  result.engine_factory = [](auto const&, int, int) { return std::make_unique<voice_in_engine>(); };
  result.state_converter_factory = [](auto desc) { return std::make_unique<voice_in_state_converter>(desc); };

  result.sections.emplace_back(make_param_section(section_left,
    make_topo_tag_basic("{C85AA7CC-FBD1-4631-BB7A-831A2E084E9E}", "Left"),
    make_param_section_gui({ 0, 0 }, gui_dimension({ { 1 } }, { { 1 } }))));
  auto& voice_mode = result.params.emplace_back(make_param(
    make_topo_info("{F26D6913-63E8-4A23-97C0-9A17D859ED93}", true, "Voice Mode", "Mode", "Mode", param_mode, 1),
    make_param_dsp_voice(param_automate::automate), make_domain_item(mode_items(), ""),
    make_param_gui_single(section_left, gui_edit_type::list, { 0, 0 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  voice_mode.info.description = std::string("Selects poly/mono mode.<br/>") +
    "Poly - regular polyphonic mode.<br/>" +
    "Mono - true monophonic mode, may cause clicks.<br/>" +
    "Release - monophonic untill a mono section is released. So, multiple mono sections may overlap.<br/>"
    "To avoid clicks it is best to use release-monophonic mode with multi-triggered envelopes.";

  result.sections.emplace_back(make_param_section(section_sync,
    make_topo_tag_basic("{11E4DE4C-A824-424E-BC5E-014240518C0F}", "Sync"),
    make_param_section_gui({ 0, 1 }, gui_dimension({ { 1 }, { { 1 } } }), gui_label_edit_cell_split::no_split)));
  auto& sync = result.params.emplace_back(make_param(
    make_topo_info("{FE70E21D-2104-4EB6-B852-6CD9690E5F72}", true, "Porta Tempo Sync", "Sync", "Sync", param_porta_sync, 1),
    make_param_dsp_voice(param_automate::automate), make_domain_toggle(false),
    make_param_gui_single(section_sync, gui_edit_type::toggle, { 0, 0 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  sync.gui.bindings.enabled.bind_params({ param_porta }, [](auto const& vs) { return vs[0] != porta_off; });
  sync.info.description = "Selects time or tempo-synced mode.";

  result.sections.emplace_back(make_param_section(section_mid,
    make_topo_tag_basic("{1C5D7493-AD1C-4F89-BF32-2D0092CB59EF}", "Mid"),
    make_param_section_gui({ 0, 2 }, gui_dimension({ { 1 }, { { gui_dimension::auto_size, 1 } } }))));
  auto& porta = result.params.emplace_back(make_param(
    make_topo_info("{586BEE16-430A-483E-891B-48E89C4B8FC1}", true, "Porta Mode", "Porta", "Porta", param_porta, 1),
    make_param_dsp_voice(param_automate::automate), make_domain_item(porta_items(), ""),
    make_param_gui_single(section_mid, gui_edit_type::autofit_list, { 0, 0 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  porta.info.description = std::string("Selects portamento mode.<br/>") + 
    "Off - no portamento.<br/>" + 
    "On - glides 1 semitone in the specified time, so glide pitch is constant and glide time is variable.<br/>" +
    "Auto - glides pitch difference between old and new note in the specified time, so glide pitch is variable and glide time is constant.";
  auto& time = result.params.emplace_back(make_param(
    make_topo_info("{E8301E86-B6EE-4F87-8181-959A05384866}", true, "Porta Time", "Time", "Time", param_porta_time, 1),
    make_param_dsp_voice(param_automate::automate), make_domain_log(0.001, 10, 0.1, 1, 3, "Sec"),
    make_param_gui_single(section_mid, gui_edit_type::hslider, { 0, 1 }, make_label_none())));
  time.gui.bindings.enabled.bind_params({ param_porta }, [](auto const& vs) { return vs[0] != porta_off; });
  time.gui.bindings.visible.bind_params({ param_porta, param_porta_sync }, [](auto const& vs) { return vs[1] == 0; });
  time.info.description = "Pitch glide time in seconds.";
  auto& tempo = result.params.emplace_back(make_param(
    make_topo_info("{15271CBC-9876-48EC-BD3C-480FF68F9ACC}", true, "Porta Tempo", "Tempo", "Tempo", param_porta_tempo, 1),
    make_param_dsp_voice(param_automate::automate), make_domain_timesig_default(false, {4, 1}, {1, 16}),
    make_param_gui_single(section_mid, gui_edit_type::list, { 0, 1 }, make_label_none())));
  tempo.gui.submenu = make_timesig_submenu(tempo.domain.timesigs);
  tempo.gui.bindings.enabled.bind_params({ param_porta }, [](auto const& vs) { return vs[0] != porta_off; });
  tempo.gui.bindings.visible.bind_params({ param_porta, param_porta_sync }, [](auto const& vs) { return vs[1] == 1; });
  tempo.info.description = "Pitch glide time in bars.";
 
  result.sections.emplace_back(make_param_section(section_right,
    make_topo_tag_basic("{3EB05593-E649-4460-929C-993B6FB7BBD3}", "Right"),
    make_param_section_gui({ 0, 3 }, gui_dimension({ 1 }, { gui_dimension::auto_size, gui_dimension::auto_size, 1 }))));
  auto& oversmp = result.params.emplace_back(make_param(
    make_topo_info("{0A866D59-E7C1-4D45-9DAF-D0C62EA03E93}", true, "Osc Oversampling", "Osc OvrSmp", "Osc OvrSmp", param_oversmp, 1),
    make_param_dsp_voice(param_automate::automate), make_domain_item(over_items(), ""),
    make_param_gui_single(section_right, gui_edit_type::autofit_list, { 0, 0 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  oversmp.info.description = std::string("Oversampling for those rare cases where it makes a positive difference. ") +
    "Only affects FM and hardsync, but not AM. " +
    "Oversampling is per unison voice, so setting both this and unison to 8 results in an oscillator being 64 times as expensive to calculate.";
  auto& note = result.params.emplace_back(make_param(
    make_topo_info_basic("{CB6D7BC8-5DE6-4A84-97C9-4E405A96E0C8}", "Note", param_note, 1),
    make_param_dsp_voice(param_automate::automate), make_domain_item(make_midi_note_list(), "C4"),
    make_param_gui_single(section_right, gui_edit_type::autofit_list, { 0, 1 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  note.gui.submenu = make_midi_note_submenu();
  note.info.description = "Oscillator base pitch adjustment for all Oscs, C4 is no adjustment.";
  auto& cent = result.params.emplace_back(make_param(
    make_topo_info_basic("{57A908CD-ED0A-4FCD-BA5F-92257175A9DE}", "Cent", param_cent, 1),
    make_param_dsp_accurate(param_automate::modulate), make_domain_percentage(-1, 1, 0, 0, false),
    make_param_gui_single(section_right, gui_edit_type::hslider, { 0, 2 },
      make_label(gui_label_contents::value, gui_label_align::left, gui_label_justify::near))));
  cent.info.description = "Oscillator pitch cents adjustment for all Oscs.";

  auto& pitch = result.params.emplace_back(make_param(
    make_topo_info_basic("{034AE513-9AB6-46EE-8246-F6ECCC11CAE0}", "Pitch", param_pitch, 1),
    make_param_dsp_accurate(param_automate::modulate), make_domain_linear(-128, 128, 0, 0, ""),
    make_param_gui_none()));
  pitch.info.description = "Absolute pitch modulation target for all Oscs.";
  auto& pb = result.params.emplace_back(make_param(
    make_topo_info("{BF20BA77-A162-401B-9F32-92AE34841AB2}", true, "Pitch Bend", "PB", "PB", param_pb, 1),
    make_param_dsp_accurate(param_automate::modulate), make_domain_percentage(-1, 1, 0, 0, true),
    make_param_gui_none()));
  pb.info.description = "Pitch-bend modulation target for all Oscs. Reacts to master pitchbend range.";
  return result;
}

void
voice_in_engine::reset(plugin_block const* block)
{
  _position = 0;
  _to_note_pitch = block->voice->state.note_id_.key;
  _from_note_pitch = block->voice->state.note_id_.key;
  if (block->voice->state.note_id_.channel == block->voice->state.last_note_channel)
    _from_note_pitch = block->voice->state.last_note_key;

  auto const& block_auto = block->state.own_block_automation;
  int porta_mode = block_auto[param_porta][0].step();
  bool porta_sync = block_auto[param_porta_sync][0].step() != 0;
  float porta_time_time = block_auto[param_porta_time][0].real();
  float porta_time_tempo = get_timesig_time_value(*block, block->host.bpm, module_voice_in, param_porta_tempo);
  float porta_time = porta_sync? porta_time_tempo: porta_time_time;
  switch (porta_mode)
  {
  case porta_off: _porta_samples = 0; break;
  case porta_auto: _porta_samples = porta_time * block->sample_rate; break;
  case porta_on: _porta_samples = porta_time * block->sample_rate * std::abs(_from_note_pitch - _to_note_pitch); break;
  default: assert(false); break;
  }

  // in monophonic mode we do not glide the first note in 
  // monophonic section. instead the first note is "plain"
  // and all subsequent notes in mono section apply portamento
  if(block_auto[param_mode][0].step() != engine_voice_mode_poly)
  {
    _from_note_pitch = _to_note_pitch;
    _mono_porta_time = porta_time;
    _mono_porta_samples = _porta_samples;
    _porta_samples = 0;
  }
}

void
voice_in_engine::process(plugin_block& block)
{
  auto const& block_auto = block.state.own_block_automation;
  if(block_auto[param_mode][0].step() == engine_voice_mode_poly)
    process_mode<false>(block);
  else
    process_mode<true>(block);
}

template <bool Monophonic> void
voice_in_engine::process_mode(plugin_block& block)
{
  if (block.voice->state.sub_voice_count > 1)
    process_mode_unison<Monophonic, true>(block);
  else
    process_mode_unison<Monophonic, false>(block);
}

template <bool Monophonic, bool GlobalUnison> void
voice_in_engine::process_mode_unison(plugin_block& block)
{  
  auto const& block_auto = block.state.own_block_automation;
  int note = block_auto[param_note][0].step();
  int porta_mode = block_auto[param_porta][0].step();

  auto const& modulation = get_cv_audio_matrix_mixdown(block, false);
  int master_pb_range = block.state.all_block_automation[module_master_in][0][master_in_param_pb_range][0].step();
  auto const& glob_uni_dtn_curve = block.state.all_accurate_automation[module_master_in][0][master_in_param_glob_uni_dtn][0];

  auto const& pb_curve_norm = *(modulation)[module_voice_in][0][param_pb][0];
  auto& pb_curve = block.state.own_scratch[scratch_pb];
  block.normalized_to_raw_block<domain_type::linear>(module_voice_in, param_pb, pb_curve_norm, pb_curve);

  auto const& cent_curve_norm = *(modulation)[module_voice_in][0][param_cent][0];
  auto& cent_curve = block.state.own_scratch[scratch_cent];
  block.normalized_to_raw_block<domain_type::linear>(module_voice_in, param_cent, cent_curve_norm, cent_curve);

  auto const& pitch_curve_norm = *(modulation)[module_voice_in][0][param_pitch][0];
  auto& pitch_curve = block.state.own_scratch[scratch_pitch];
  block.normalized_to_raw_block<domain_type::linear>(module_voice_in, param_pitch, pitch_curve_norm, pitch_curve);
  
  for(int f = block.start_frame; f < block.end_frame; f++)
  {
    if constexpr (Monophonic)
    {
      if (block.state.mono_note_stream[f].note_on)
      {
        if (porta_mode == porta_off)
        {
          // pitch switch, will be picked up by the oscs
          _position = 0;
          _porta_samples = 0;
          _to_note_pitch = block.state.mono_note_stream[f].midi_key;
          _from_note_pitch = _to_note_pitch;
        }
        else 
        {
          // start a new porta section within the current voice
          _position = 0;
          _from_note_pitch = _to_note_pitch;
          _to_note_pitch = block.state.mono_note_stream[f].midi_key;

          // need to recalc total glide time
          if (porta_mode == porta_on)
            _porta_samples = _mono_porta_time * block.sample_rate * std::abs(_from_note_pitch - _to_note_pitch);
          _porta_samples = _mono_porta_samples;
        }
      }
    }

    // global unison detuning for this voice
    float glob_uni_detune = 0;
    if constexpr (!Monophonic && GlobalUnison)
    {
      float voice_pos = (float)block.voice->state.sub_voice_index / (block.voice->state.sub_voice_count - 1.0f);
      glob_uni_detune = (voice_pos - 0.5f) * glob_uni_dtn_curve[f];
    }

    float porta_note = 0;
    if(_position == _porta_samples) porta_note = _to_note_pitch;
    else porta_note = _from_note_pitch + (_position++ / (float)_porta_samples * (_to_note_pitch - _from_note_pitch));
    block.state.own_cv[output_pitch_offset][0][f] = 
      (note + cent_curve[f] + glob_uni_detune - midi_middle_c) + 
      (porta_note - midi_middle_c) + pitch_curve[f] + pb_curve[f] * master_pb_range;
  }
}

}
