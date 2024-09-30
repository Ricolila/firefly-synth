#include <plugin_base/dsp/engine.hpp>
#include <plugin_base/topo/plugin.hpp>
#include <plugin_base/topo/support.hpp>
#include <plugin_base/helpers/dsp.hpp>

#include <firefly_synth/synth.hpp>
#include <cmath>
#include <array>

using namespace plugin_base;
 
namespace firefly_synth {

enum { section_main };
enum { 
  param_type, param_mode,
  ///* TODO param_cv_source, */ param_oct_down, param_oct_up//, 
  param_sync, param_rate_hz, param_rate_tempo, param_length_time, param_length_tempo
  /* TODO param_rate_offset_source, amt, length_offset_source, amt */
};

enum { mode_up, mode_down, mode_up_down, mode_down_up };
enum { type_off, type_straight, type_p1_oct, type_p2_oct, type_m1p1_oct, type_m2p2_oct };

struct arp_note_state
{
  bool on = {};
  float velocity = {};
};

struct arp_active_note
{
  int midi_key;
  float velocity;
};

static std::vector<list_item>
type_items()
{
  std::vector<list_item> result;
  result.emplace_back("{70109417-1525-48A6-AE1D-7AB0E5765310}", "Off");
  result.emplace_back("{85A091D9-1283-4E67-961E-48C57BC68EB7}", "Straight");
  result.emplace_back("{20CDFF90-9D2D-4AFD-8138-1BCB61370F23}", "+1 Oct");
  result.emplace_back("{4935E002-3745-4097-887F-C8ED52213658}", "+2 Oct");
  result.emplace_back("{018FFDF0-AA2F-40BB-B449-34C8E93BCEB2}", "+/-1 Oct");
  result.emplace_back("{E802C511-30E0-4B9B-A548-173D4C807AFF}", "+/-2 Oct");
  return result;
}

static std::vector<list_item>
mode_items()
{
  std::vector<list_item> result;
  result.emplace_back("{25F4EF71-60E4-4F60-B613-8549C1BA074B}", "Up");
  result.emplace_back("{1772EDDE-6EC2-4F72-AC98-5B521AFB0EF1}", "Down");
  result.emplace_back("{1ECA59EC-B4B5-4EE9-A1E8-0169E7F32BCC}", "UpDown");
  result.emplace_back("{B48727A2-E886-43D4-906D-D87F5E7EE3CD}", "DownUp");
  return result;
}

class arpeggiator_engine :
public arp_engine_base
{
  int _prev_type = -1;
  int _prev_mode = -1;

  // todo stuff with velo
  int _table_pos = -1;
  int _note_remaining = 0;
  std::int64_t _start_time = 0;
  std::vector<arp_active_note> _active_notes = {};
  std::array<arp_note_state, 128> _prev_chord = {};
  std::array<arp_note_state, 128> _current_chord = {};

  void hard_reset(std::vector<note_event>& out);

public:

  arpeggiator_engine();

  void process_notes(
    plugin_block const& block,
    std::vector<note_event> const& in,
    std::vector<note_event>& out) override;
};

std::unique_ptr<arp_engine_base>
make_arpeggiator() { return std::make_unique<arpeggiator_engine>(); }

module_topo
arpeggiator_topo(int section, gui_position const& pos)
{
  module_topo result(make_module(
    make_topo_info_basic("{8A09B4CD-9768-4504-B9FE-5447B047854B}", "ARP / SEQ", module_arpeggiator, 1),
    make_module_dsp(module_stage::input, module_output::none, 0, {}),
    make_module_gui(section, pos, { 1, 1 })));
  result.info.description = "Arpeggiator / Sequencer.";

  result.sections.emplace_back(make_param_section(section_main,
    make_topo_tag_basic("{6779AFA8-E0FE-482F-989B-6DE07263AEED}", "Main"),
    make_param_section_gui({ 0, 0 }, { { 1 }, { 
      gui_dimension::auto_size, gui_dimension::auto_size, 
      gui_dimension::auto_size, gui_dimension::auto_size, gui_dimension::auto_size } })));
  auto& type = result.params.emplace_back(make_param(
    make_topo_info_basic("{FF418A06-2017-4C23-BC65-19FAF226ABE8}", "Type", param_type, 1),
    make_param_dsp_block(param_automate::automate), make_domain_item(type_items(), "Off"),
    make_param_gui_single(section_main, gui_edit_type::autofit_list, { 0, 0 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  type.info.description = "TODO";
  auto& mode = result.params.emplace_back(make_param(
    make_topo_info_basic("{BCE75C3A-85B8-4946-A06B-68F8F5F36785}", "Mode", param_mode, 1),
    make_param_dsp_block(param_automate::automate), make_domain_item(mode_items(), "Up"),
    make_param_gui_single(section_main, gui_edit_type::autofit_list, { 0, 1 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  mode.info.description = "TODO";
  mode.gui.bindings.enabled.bind_params({ param_type }, [](auto const& vs) { return vs[0] != type_off; });
  auto& sync = result.params.emplace_back(make_param(
    make_topo_info_basic("{8DE4D902-946C-41AA-BA1B-E0B645F8C87D}", "Sync", param_sync, 1),
    make_param_dsp_block(param_automate::automate), make_domain_toggle(true),
    make_param_gui_single(section_main, gui_edit_type::toggle, { 0, 2 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  sync.info.description = "TODO";
  sync.gui.bindings.enabled.bind_params({ param_type }, [](auto const& vs) { return vs[0] != type_off; });
  auto& rate_hz = result.params.emplace_back(make_param(
    make_topo_info_basic("{EE305C60-8D37-492D-A2BE-5BD9C80DC59D}", "Rate", param_rate_hz, 1),
    make_param_dsp_block(param_automate::automate), make_domain_log(0.25, 20, 4, 4, 2, "Hz"),
    make_param_gui_single(section_main, gui_edit_type::knob, { 0, 3 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  rate_hz.gui.bindings.enabled.bind_params({ param_type, param_sync }, [](auto const& vs) { return vs[0] != type_off && vs[1] == 0; });
  rate_hz.gui.bindings.visible.bind_params({ param_type, param_sync }, [](auto const& vs) { return vs[1] == 0; });
  rate_hz.info.description = "TODO";
  auto& rate_tempo = result.params.emplace_back(make_param(
    make_topo_info_basic("{B1727889-9B55-4F93-8E0A-E2D4B791568B}", "Rate", param_rate_tempo, 1),
    make_param_dsp_block(param_automate::automate), make_domain_timesig_default(false, { 16, 1 }, { 1, 4 }), // TODO tune this
    make_param_gui_single(section_main, gui_edit_type::autofit_list, { 0, 3 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  rate_tempo.gui.submenu = make_timesig_submenu(rate_tempo.domain.timesigs);
  rate_tempo.gui.bindings.enabled.bind_params({ param_type, param_sync }, [](auto const& vs) { return vs[0] != type_off && vs[1] != 0; });
  rate_tempo.gui.bindings.visible.bind_params({ param_type, param_sync }, [](auto const& vs) { return vs[1] != 0; });
  rate_tempo.info.description = "TODO";
  auto& length_sec = result.params.emplace_back(make_param(
    make_topo_info_basic("{157B9BF0-4880-4E91-9E91-72DBDF15595B}", "Length", param_length_time, 1),
    make_param_dsp_block(param_automate::automate), make_domain_log(0.05, 4, 0.25, 0.25, 2, "Sec"),
    make_param_gui_single(section_main, gui_edit_type::knob, { 0, 4 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  length_sec.gui.bindings.enabled.bind_params({ param_type, param_sync }, [](auto const& vs) { return vs[0] != type_off && vs[1] == 0; });
  length_sec.gui.bindings.visible.bind_params({ param_type, param_sync }, [](auto const& vs) { return vs[1] == 0; });
  length_sec.info.description = "TODO";
  auto& length_tempo = result.params.emplace_back(make_param(
    make_topo_info_basic("{67178302-F2F3-449D-99B8-C769DC189B90}", "Release", param_length_tempo, 1),
    make_param_dsp_block(param_automate::automate), make_domain_timesig_default(false, { 16, 1 }, { 1, 4 }), // TODO tune this
    make_param_gui_single(section_main, gui_edit_type::autofit_list, { 0, 4 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  length_tempo.gui.submenu = make_timesig_submenu(length_tempo.domain.timesigs);
  length_tempo.gui.bindings.enabled.bind_params({ param_type, param_sync }, [](auto const& vs) { return vs[0] != type_off && vs[1] != 0; });
  length_tempo.gui.bindings.visible.bind_params({ param_type, param_sync }, [](auto const& vs) { return vs[1] != 0; });
  length_tempo.info.description = "TODO";

  return result;
}         

arpeggiator_engine::
arpeggiator_engine()
{ _active_notes.reserve(128); /* guess */ }

void 
arpeggiator_engine::hard_reset(std::vector<note_event>& out)
{
  for (int i = 0; i < 128; i++)
  {
    note_event off;
    off.frame = 0;
    off.id.id = 0;
    off.id.key = i;
    off.id.channel = 0; // TODO need this?
    off.velocity = 0.0f;
    off.type = note_event_type::off;
    out.push_back(off);
  }
}

void 
arpeggiator_engine::process_notes(
  plugin_block const& block,
  std::vector<note_event> const& in,
  std::vector<note_event>& out)
{
  // plugin_base clears on each round
  assert(out.size() == 0);

  auto const& block_auto = block.state.own_block_automation;
  int type = block_auto[param_type][0].step();
  int mode = block_auto[param_mode][0].step();

  // TODO for all params ??
  // maybe, but more probably only those that generate notes
  if (type != _prev_type || mode != _prev_mode)
  {
    _prev_type = type;
    _prev_mode = mode;
    hard_reset(out);
  }

  if (type == type_off)
  {
    out.insert(out.end(), in.begin(), in.end());
    return;
  }
  
  // todo do the reset-on-event-thing
  // todo do the lookahead window thing / allow some time to accumulate the table
  // buildup thing is for hw keyboards as i doubt anyone can hit keys sample-accurate
  // TODO allow to cut vs off
  
  // this assumes notes are ordered by stream pos
  int table_changed_frame = 0; // TODO something with this
  bool table_changed = false; // make this TODO a bit more lenient
  for (int i = 0; i < in.size(); i++)
  {
    table_changed = true;
    table_changed_frame = in[i].frame;
    if (in[i].type == note_event_type::on)
    {
      _current_chord[in[i].id.key].on = true;
      _current_chord[in[i].id.key].velocity = in[i].velocity;
    }
    else
    {
      _current_chord[in[i].id.key].on = false;
      _current_chord[in[i].id.key].velocity = 0.0f;
    }
  }

  if (table_changed)
  {
    // STEP 0: off all notes
    // just active notes is not enough, 
    // user may have been playing with voice/osc note/oct in the meantime
    hard_reset(out);

    _table_pos = -1; // before start, will get picked up
    _note_remaining = 0;
    _active_notes.clear();

    // STEP 1: build up the base chord table
    for (int i = 0; i < 128; i++)
      if(_current_chord[i].on)
      {
        arp_active_note aan;
        aan.midi_key = i;
        aan.velocity = _current_chord[i].velocity;
        _active_notes.push_back(aan);
      }

    if (_active_notes.size() == 0)
      return;

    // STEP 2: take the +/- oct into account
    int base_note_count = _active_notes.size();
    switch (type)
    {
    case type_straight: 
      break;
    case type_p1_oct:
      for (int i = 0; i < base_note_count; i++)
        _active_notes.push_back({ _active_notes[i].midi_key + 12, _active_notes[i].velocity });
      break;
    case type_p2_oct:
      for (int i = 0; i < base_note_count; i++)
        _active_notes.push_back({ _active_notes[i].midi_key + 12, _active_notes[i].velocity });
      for (int i = 0; i < base_note_count; i++)
        _active_notes.push_back({ _active_notes[i].midi_key + 24, _active_notes[i].velocity });
      break;
    case type_m1p1_oct:
      for (int i = 0; i < base_note_count; i++)
        _active_notes.push_back({ _active_notes[i].midi_key - 12, _active_notes[i].velocity });
      for (int i = 0; i < base_note_count; i++)
        _active_notes.push_back({ _active_notes[i].midi_key + 12, _active_notes[i].velocity });
      break;
    case type_m2p2_oct:
      for (int i = 0; i < base_note_count; i++)
        _active_notes.push_back({ _active_notes[i].midi_key - 12, _active_notes[i].velocity });
      for (int i = 0; i < base_note_count; i++)
        _active_notes.push_back({ _active_notes[i].midi_key - 24, _active_notes[i].velocity });
      for (int i = 0; i < base_note_count; i++)
        _active_notes.push_back({ _active_notes[i].midi_key + 12, _active_notes[i].velocity });
      for (int i = 0; i < base_note_count; i++)
        _active_notes.push_back({ _active_notes[i].midi_key + 24, _active_notes[i].velocity });
      break;
    default:
      assert(false);
      break;
    }

    // make sure all is in check
    // we cannot go out of bounds in pitch
    // pitch 2 freq translator allows it, but we won't be able to kill MIDI -3 or MIDI 134 etc
    for (int i = 0; i < _active_notes.size(); i++)
      _active_notes[i].midi_key = std::clamp(_active_notes[i].midi_key, 0, 127);

    // and sort the thing
    auto comp = [](auto const& l, auto const& r) { return l.midi_key < r.midi_key; };
    std::sort(_active_notes.begin(), _active_notes.end(), comp);

    // STEP 3: take the up-down into account
    int note_set_count = _active_notes.size();
    switch (mode)
    {
    case mode_up:
      break; // already sorted
    case mode_down:
      std::reverse(_active_notes.begin(), _active_notes.end());
      break;
    case mode_up_down:
      for (int i = note_set_count - 2; i >= 1; i--)
        _active_notes.push_back({ _active_notes[i].midi_key, _active_notes[i].velocity });
      break;
    case mode_down_up:
      std::reverse(_active_notes.begin(), _active_notes.end());
      for (int i = note_set_count - 2; i >= 1; i--)
        _active_notes.push_back({ _active_notes[i].midi_key, _active_notes[i].velocity });
      break;
    default:
      assert(false);
      break;
    }
  }

  if (_active_notes.size() == 0)
    return;

  // how often to output new notes
  float rate_hz;
  if (block_auto[param_sync][0].step() == 0)
    rate_hz = block_auto[param_rate_hz][0].real();
  else
    rate_hz = timesig_to_freq(block.host.bpm, get_timesig_param_value(block, module_arpeggiator, param_rate_tempo));
  int rate_frames = block.sample_rate / rate_hz;

  // hold time for notes (when to output off event)
  float length_sec;
  if (block_auto[param_sync][0].step() == 0)
    length_sec = block_auto[param_length_time][0].real();
  else
    length_sec = timesig_to_time(block.host.bpm, get_timesig_param_value(block, module_arpeggiator, param_length_tempo));
  int length_frames = block.sample_rate * length_sec;

  // TODO what would actually happen when "play chord", release, wait, "play chord"
  // TODO stuff with actual start pos of the table as based on note event frame ??
  for (int f = block.start_frame; f < block.end_frame; f++)
  {
    if (_note_remaining == 0) // TODO make sure this is always 1+ frames
    {
      if (_table_pos != -1)
      {
        note_event end_old = {};
        end_old.id.id = 0;
        end_old.id.channel = 0; // TODO maybe ?
        end_old.id.key = _active_notes[_table_pos].midi_key;
        end_old.frame = f;
        end_old.velocity = 0.0f;
        end_old.type = note_event_type::off; // TODO
        out.push_back(end_old);
      }

      _note_remaining = rate_frames; // TODO
      _table_pos = (_table_pos + 1) % _active_notes.size();

      note_event start_new = {};
      start_new.id.id = 0;
      start_new.id.channel = 0;
      start_new.id.key = _active_notes[_table_pos].midi_key;
      start_new.frame = f;
      start_new.type = note_event_type::on;
      start_new.velocity = _active_notes[_table_pos].velocity;
      out.push_back(start_new);
    }
    
    --_note_remaining;
  }
}

}
