#include <plugin_base/dsp/engine.hpp>
#include <plugin_base/dsp/utility.hpp>
#include <plugin_base/dsp/block/host.hpp>
#include <plugin_base/desc/frame_dims.hpp>
#include <plugin_base/shared/logger.hpp>

#include <limits>
#include <algorithm>

namespace plugin_base {

static float const default_auto_filter_millis = 1.0f;
static float const default_bpm_filter_millis = 200;
static float const default_midi_filter_millis = 50;

static int 
topo_polyphony(plugin_desc const* desc, bool graph)
{
  if(graph) return desc->plugin->graph_polyphony;
  return desc->plugin->audio_polyphony;
}
 
plugin_engine::
plugin_engine(
  plugin_desc const* desc, bool graph,
  thread_pool_voice_processor voice_processor,
  void* voice_processor_context) :
_graph(graph),
_polyphony(topo_polyphony(desc, graph)),
_state(desc, false), 
_block_automation(desc, false),
_voice_automation(_polyphony, plugin_state(desc, false)),
_dims(*desc->plugin, _polyphony),
_host_block(std::make_unique<host_block>()),
_voice_processor(voice_processor),
_voice_thread_ids(_polyphony, std::thread::id()),
_voice_processor_context(voice_processor_context)
{
  assert(_polyphony >= 0);

  // validate here instead of plugin_desc ctor 
  // since that runs on module init so is hard to debug
  desc->validate();

  // init everything that is not frame-count dependent
  _global_module_process_duration_sec.resize(_dims.module_slot);
  _voice_module_process_duration_sec.resize(_dims.voice_module_slot);
  _voice_states.resize(_polyphony);
  _global_context.resize(_dims.module_slot);
  _voice_context.resize(_dims.voice_module_slot);
  _output_values.resize(_dims.module_slot_param_slot);
  _input_engines.resize(_dims.module_slot);
  _output_engines.resize(_dims.module_slot);
  _voice_engines.resize(_dims.voice_module_slot);
  _midi_was_automated.resize(_state.desc().midi_count);
  _midi_active_selection.resize(_dims.module_slot_midi);
  _param_was_automated.resize(_dims.module_slot_param_slot);
  _current_modulation.resize(_dims.module_slot_param_slot);
  _automation_lerp_filters.resize(_dims.module_slot_param_slot);
  _automation_lp_filters.resize(_dims.module_slot_param_slot);
  _automation_state_last_round_end.resize(_dims.module_slot_param_slot);

  // microtuning support
  _current_voice_tuning_mode.resize(_polyphony);
  _current_voice_tuning_channel.resize(_polyphony);
}

engine_tuning_mode 
plugin_engine::get_current_tuning_mode()
{
  auto const& topo = *_state.desc().plugin;
  engine_tuning_mode result = (engine_tuning_mode)-1;
  if (topo.tuning_mode_module == -1 || topo.tuning_mode_param == -1) return engine_tuning_mode_off;
  result = (engine_tuning_mode)_state.get_plain_at(topo.tuning_mode_module, 0, topo.tuning_mode_param, 0).step();
  assert(engine_tuning_mode_off <= result && result <= engine_tuning_mode_continuous_after_mod);
  return result;
}

void
plugin_engine::query_mts_esp_tuning(std::array<note_tuning, 128>& tuning, int channel)
{
  // see if the IPC-friendly idea turns out to be good
  assert(channel >= -1);
  for (int i = 0; i < 128; i++)
  {
    tuning[i].frequency = MTS_NoteToFrequency(_host_block->mts_client, (char)i, (char)channel);
    tuning[i].is_mapped = !MTS_ShouldFilterNote(_host_block->mts_client, (char)i, (char)channel);
  }
}

plugin_voice_block 
plugin_engine::make_voice_block(
  int v, int release_frame, note_id id, 
  int sub_voice_count, int sub_voice_index, 
  int last_note_key, int last_note_channel, float last_retuned_pitch)
{
  _voice_states[v].note_id_ = id;
  _voice_states[v].release_frame = release_frame;
  _voice_states[v].last_note_key = last_note_key;
  _voice_states[v].last_note_channel = last_note_channel;
  _voice_states[v].last_retuned_pitch = last_retuned_pitch;
  _voice_states[v].sub_voice_count = sub_voice_count;
  _voice_states[v].sub_voice_index = sub_voice_index;
  return {
    false, _voice_results[v], _voice_states[v], 
    _voice_cv_state[v], _voice_audio_state[v], _voice_context[v]
  };
};

plugin_block
plugin_engine::make_plugin_block(
  int voice, int voice_channel, int module, int slot, 
  engine_tuning_mode tuning_mode, int start_frame, int end_frame)
{
  assert(voice == -1 && voice_channel == -1 || voice >= 0 && voice_channel >= 0 && voice_channel < 16);

  void** context_out = voice < 0
    ? &_global_context[module][slot]
    : &_voice_context[voice][module][slot];
  jarray<float, 3>& cv_out = voice < 0
    ? _global_cv_state[module][slot]
    : _voice_cv_state[voice][module][slot];
  jarray<float, 2>& scratch = voice < 0
    ? _global_scratch_state[module][slot]
    : _voice_scratch_state[voice][module][slot];
  jarray<float, 4>& audio_out = voice < 0
    ? _global_audio_state[module][slot] 
    : _voice_audio_state[voice][module][slot];

  // fix param_rate::voice values to voice start
  jarray<plain_value, 2> const& own_block_auto = voice < 0
    ? _block_automation.state()[module][slot]
    : _voice_automation[voice].state()[module][slot];
  jarray<plain_value, 4> const& all_block_auto = voice < 0
    ? _block_automation.state()
    : _voice_automation[voice].state();

  plugin_block_state state = {
    _last_note_key, context_out, _mono_note_stream,
    cv_out, audio_out, scratch, _bpm_automation,
    _global_cv_state, _global_audio_state, _global_context, 
    _midi_automation[module][slot], _midi_automation,
    _midi_active_selection[module][slot], _midi_active_selection,
    _accurate_automation[module][slot], _accurate_automation,
    own_block_auto, all_block_auto
  };

  // even for per-voice tuning we need to fallback to per-block tuning for global stuff
  // also note the correct per-block/per-voice tuning is already filled by querying mts-esp!
  std::array<note_tuning, 128>* current_tuning = nullptr;
  switch (tuning_mode)
  {
  case engine_tuning_mode_off:
  // for these we don't need the tables -- does RetuningInSemis before after-mod interpolation
  case engine_tuning_mode_on_note_before_mod:
  case engine_tuning_mode_continuous_before_mod:
    current_tuning = nullptr;
    break;
  // note! not only on_note_after needs the per-voice-per-channel tables,
  // but also continuous_after needs them, because continuous_after may very well affect per-voice stuff
  case engine_tuning_mode_on_note_after_mod:
    if (voice < 0) current_tuning = &_current_block_tuning_global; // fallback -- global got no midi channel
    else current_tuning = &_current_voice_tuning_channel[voice]; // here's the gist! per-voice-fixed-per-channel
    break;
  case engine_tuning_mode_continuous_after_mod:
    if (voice < 0) current_tuning = &_current_block_tuning_global; // fallback -- global got no midi channel
    else current_tuning = &_current_block_tuning_channel[voice_channel]; // here's the gist! continuous-per-channel
    break;
  default:
    assert(false);
  }

  return {
    _graph,
    _host_block->mts_client,
    current_tuning,
    tuning_mode,
    start_frame, end_frame, slot,
    _sample_rate, state, nullptr, nullptr, 
    _host_block->shared, *_state.desc().plugin, 
    _state.desc().plugin->modules[module]
  };
}

void
plugin_engine::init_from_state(plugin_state const* state)
{
  _state.copy_from(state->state());
  automation_state_dirty();
  init_automation_from_state();
}        

void
plugin_engine::release_block()
{
  double now = seconds_since_epoch();
  double process_time_sec = now - _block_start_time_sec;
  double block_time_sec = _host_block->frame_count / _sample_rate;
  _cpu_usage = process_time_sec / block_time_sec;

  // total can exceed processing time when using clap threadpool
  double max_module_duration = 0;
  double total_module_duration = 0;
  for (int m = 0; m < _state.desc().plugin->modules.size(); m++)
  {
    auto const& module = _state.desc().plugin->modules[m];
    for (int mi = 0; mi < module.info.slot_count; mi++)
    {
      double this_module_duration = 0;
      if(module.dsp.stage != module_stage::voice)
        this_module_duration = _global_module_process_duration_sec[m][mi];
      else
        for(int v = 0; v < _polyphony; v++)
          this_module_duration += _voice_module_process_duration_sec[v][m][mi];
      total_module_duration += this_module_duration;
      if (this_module_duration > max_module_duration)
      {
        _high_cpu_module = _state.desc().module_topo_to_index.at(m) + mi;
        max_module_duration = this_module_duration;
      }
    }
  }
  _high_cpu_module_usage = max_module_duration / total_module_duration;
}

host_block&
plugin_engine::prepare_block()
{
  // host calls this and should provide the current block values
  _host_block->prepare();
  _block_start_time_sec = seconds_since_epoch();
  return *_host_block;
}

void
plugin_engine::deactivate()
{
  _cpu_usage = 0;
  _sample_rate = 0;
  _stream_time = 0;
  _blocks_processed = 0;
  _max_frame_count = 0;
  _output_updated_sec = 0;
  _block_start_time_sec = 0;
  _last_note_key = -1;
  _last_note_channel = -1;

  _high_cpu_module = 0;
  _high_cpu_module_usage = 0;

  std::fill(_voice_states.begin(), _voice_states.end(), voice_state());
  for (int m = 0; m < _state.desc().plugin->modules.size(); m++)
  {
    auto const& module = _state.desc().plugin->modules[m];
    for (int mi = 0; mi < module.info.slot_count; mi++)
      if(module.dsp.stage != module_stage::voice)
        _global_module_process_duration_sec[m][mi] = 0;
      else
        for(int v = 0; v < _polyphony; v++)
          _voice_module_process_duration_sec[v][m][mi] = 0;
  }

  // drop frame-count dependent memory
  _voice_results = {};
  _voices_mixdown = {};
  _voice_cv_state = {};
  _voice_audio_state = {};
  _global_cv_state = {};
  _global_audio_state = {};
  _midi_automation = {};
  _midi_filters = {};
  _bpm_automation = {};
  _bpm_filter = {};
  _accurate_automation = {};
  _voice_scratch_state = {};
  _global_scratch_state = {};
  _host_block->events.deactivate();

  for(int m = 0; m < _state.desc().module_voice_start; m++)
    for (int mi = 0; mi < _state.desc().plugin->modules[m].info.slot_count; mi++)
      _input_engines[m][mi].reset();
  for (int m = _state.desc().module_voice_start; m < _state.desc().module_output_start; m++)
    for (int mi = 0; mi < _state.desc().plugin->modules[m].info.slot_count; mi++)
      for (int v = 0; v < _polyphony; v++)
        _voice_engines[v][m][mi].reset();
  for (int m = _state.desc().module_output_start; m < _state.desc().plugin->modules.size(); m++)
    for (int mi = 0; mi < _state.desc().plugin->modules[m].info.slot_count; mi++)
      _output_engines[m][mi].reset();
}

void
plugin_engine::activate(int max_frame_count)
{
  deactivate();
  _stream_time = 0;
  _blocks_processed = 0;
  _last_note_key = -1;
  _last_note_channel = -1;
  _max_frame_count = max_frame_count;
  _output_updated_sec = seconds_since_epoch();
  std::fill(_voice_states.begin(), _voice_states.end(), voice_state());

  // init frame-count dependent memory
  plugin_frame_dims frame_dims(*_state.desc().plugin, _polyphony, max_frame_count);
  _voices_mixdown.resize(frame_dims.audio);
  _voice_results.resize(frame_dims.voices_audio);
  _voice_cv_state.resize(frame_dims.module_voice_cv);
  _voice_scratch_state.resize(frame_dims.module_voice_scratch);
  _voice_audio_state.resize(frame_dims.module_voice_audio);
  _global_cv_state.resize(frame_dims.module_global_cv);
  _global_scratch_state.resize(frame_dims.module_global_scratch);
  _global_audio_state.resize(frame_dims.module_global_audio);
  _midi_automation.resize(frame_dims.midi_automation);
  _accurate_automation.resize(frame_dims.accurate_automation);
  _bpm_automation.resize(max_frame_count);
  _mono_note_stream.resize(max_frame_count);
  _host_block->events.activate(_graph, _state.desc().param_count, _state.desc().midi_count, _polyphony, max_frame_count);

  // set automation values to current state, events may overwrite
  automation_state_dirty();
  init_automation_from_state();
}

void
plugin_engine::automation_state_dirty()
{
  for (int m = 0; m < _state.desc().plugin->modules.size(); m++)
    for (int mi = 0; mi < _state.desc().plugin->modules[m].info.slot_count; mi++)
      for (int p = 0; p < _state.desc().plugin->modules[m].params.size(); p++)
        for (int pi = 0; pi < _state.desc().plugin->modules[m].params[p].info.slot_count; pi++)
        {
          // mod is transient
          _current_modulation[m][mi][p][pi] = 0;
          _param_was_automated[m][mi][p][pi] = 1;
        }
}

void
plugin_engine::activate_modules()
{
  PB_LOG_FUNC_ENTRY_EXIT();

  assert(_sample_rate > 0);
  assert(_max_frame_count > 0);

  // smoothing filters are SR dependent
  _bpm_filter = block_filter(_sample_rate, default_bpm_filter_millis * 0.001, 120);
  for(int ms = 0; ms < _state.desc().midi_count; ms++)
    _midi_filters.push_back(block_filter(_sample_rate, default_midi_filter_millis * 0.001, _state.desc().midi_sources[ms]->source->default_));

  for(int m = 0; m < _state.desc().plugin->modules.size(); m++)
    for (int mi = 0; mi < _state.desc().plugin->modules[m].info.slot_count; mi++)
      for (int p = 0; p < _state.desc().plugin->modules[m].params.size(); p++)
        if(_state.desc().plugin->modules[m].params[p].dsp.rate == param_rate::accurate)
          for (int pi = 0; pi < _state.desc().plugin->modules[m].params[p].info.slot_count; pi++)
          {
            _automation_lerp_filters[m][mi][p][pi].init(_sample_rate, default_auto_filter_millis * 0.001f);
            _automation_lerp_filters[m][mi][p][pi].current((float)_state.get_normalized_at(m, mi, p, pi).value());
            _automation_lerp_filters[m][mi][p][pi].set((float)_state.get_normalized_at(m, mi, p, pi).value());
            _automation_lp_filters[m][mi][p][pi].init(_sample_rate, default_auto_filter_millis * 0.001f);
            _automation_lp_filters[m][mi][p][pi].current((float)_state.get_normalized_at(m, mi, p, pi).value());
          }

  for (int m = 0; m < _state.desc().module_voice_start; m++)
    for (int mi = 0; mi < _state.desc().plugin->modules[m].info.slot_count; mi++)
    {
      auto& factory = _state.desc().plugin->modules[m].engine_factory;
      if (factory)
      {
        plugin_block block(make_plugin_block(-1, -1, m, mi, engine_tuning_mode_off, 0, 0));
        _input_engines[m][mi] = factory(*_state.desc().plugin, _sample_rate, _max_frame_count);
        _input_engines[m][mi]->reset(&block);
      }
    }
  
  if(_state.desc().plugin->type == plugin_type::synth)
    for (int m = _state.desc().module_voice_start; m < _state.desc().module_output_start; m++)
      for (int mi = 0; mi < _state.desc().plugin->modules[m].info.slot_count; mi++)
        for (int v = 0; v < _polyphony; v++)
        {
          auto& factory = _state.desc().plugin->modules[m].engine_factory;
          if(factory) _voice_engines[v][m][mi] = factory(*_state.desc().plugin, _sample_rate, _max_frame_count);
        }

  for (int m = _state.desc().module_output_start; m < _state.desc().plugin->modules.size(); m++)
    for (int mi = 0; mi < _state.desc().plugin->modules[m].info.slot_count; mi++)
    {
      auto& factory = _state.desc().plugin->modules[m].engine_factory;
      if(factory)
      {
        plugin_block block(make_plugin_block(-1, -1, m, mi, engine_tuning_mode_off, 0, 0));
        _output_engines[m][mi] = factory(*_state.desc().plugin, _sample_rate, _max_frame_count);
        _output_engines[m][mi]->reset(&block);
      }
    }
}

void
plugin_engine::init_bpm_automation(float bpm)
{
  for(int f = 0; f < _max_frame_count; f++)
    _bpm_automation[f] = bpm;
}

void
plugin_engine::voice_block_params_snapshot(int v)
{
  // take a snapshot of current block automation values into once per voice automation
  for (int m = 0; m < _state.desc().plugin->modules.size(); m++)
  {
    auto const& module = _state.desc().plugin->modules[m];
    if (module.dsp.stage != module_stage::output)
      for (int p = 0; p < module.params.size(); p++)
        if (module.params[p].dsp.rate != param_rate::accurate)
          for (int mi = 0; mi < module.info.slot_count; mi++)
            for (int pi = 0; pi < module.params[p].info.slot_count; pi++)
              _voice_automation[v].set_plain_at(m, mi, p, pi, _block_automation.get_plain_at(m, mi, p, pi));
  }
}

void
plugin_engine::init_automation_from_state()
{
  // set automation values to state, automation may overwrite
  // note that we cannot initialize current midi state since it may be anything
  for (int m = 0; m < _state.desc().plugin->modules.size(); m++)
  {
    auto const& module = _state.desc().plugin->modules[m];
    for (int mi = 0; mi < module.info.slot_count; mi++)
      for (int p = 0; p < module.params.size(); p++)
      {
        auto const& param = module.params[p];
        if (param.dsp.rate != param_rate::accurate)
        {
          for (int pi = 0; pi < param.info.slot_count; pi++)
            if(_param_was_automated[m][mi][p][pi] != 0)
            {
              _param_was_automated[m][mi][p][pi] = 0;
              _block_automation.set_plain_at(m, mi, p, pi, _state.get_plain_at(m, mi, p, pi));
            }
        }
        else
        {
          // NOTE! We *really* need max frame count here, not current block size.
          // This is because if the parameter is *not* automated in the current
          // block, and the host comes at us with a larger block size on the next round,
          // we end up with invalid values between current block size and next round block
          // size. This happens only on hosts which employ variable block sizes (e.g. FLStudio).
          // However this is completely within the vst3 spec so we should accomodate it.
          // Note to self: this was a full day not fun debugging session. Please keep
          // variable block sizes in mind.
          
          for (int pi = 0; pi < param.info.slot_count; pi++)
            if (_blocks_processed == 0)
            {
              // First time around!
              // Fill all automation buffers with plugin state.
              // Do NOT mark as un-automated, filters need to catch up.
              std::fill(
                _accurate_automation[m][mi][p][pi].begin(),
                _accurate_automation[m][mi][p][pi].begin() + _max_frame_count,
                (float)_state.get_normalized_at(m, mi, p, pi).value());
            }
            else if (_automation_lp_filters[m][mi][p][pi].active())
            {
              // filter needs run-off
              std::fill(
                _accurate_automation[m][mi][p][pi].begin(),
                _accurate_automation[m][mi][p][pi].begin() + _max_frame_count,
                _automation_lp_filters[m][mi][p][pi].current());
            }
            else if (_param_was_automated[m][mi][p][pi] != 0)
            {
              // filter ran to completion but new events came in
              _param_was_automated[m][mi][p][pi] = 0;
              std::fill(
                _accurate_automation[m][mi][p][pi].begin(),
                _accurate_automation[m][mi][p][pi].begin() + _max_frame_count,
                std::clamp((float)_state.get_normalized_at(m, mi, p, pi).value() + _current_modulation[m][mi][p][pi], 0.0f, 1.0f));
            }
        }
      }
  }
}

void 
plugin_engine::process_voices_single_threaded()
{
  for (int v = 0; v < _voice_states.size(); v++)
    if (_voice_states[v].stage != voice_stage::unused)
      process_voice(v, false);
}

void
plugin_engine::process_voice(int v, bool threaded)
{
  // matching release-fence before _voice_processor on the main thread
  if (threaded) 
    std::atomic_thread_fence(std::memory_order_acquire);

  // simplifies threadpool
  // so we can just push (polyphony) tasks each time
  if (_voice_states[v].stage == voice_stage::unused) return;

  auto& state = _voice_states[v];
  std::pair<std::uint32_t, std::uint32_t> denormal_state;
  if(threaded) denormal_state = disable_denormals();

  for (int m = _state.desc().module_voice_start; m < _state.desc().module_output_start; m++)
    for (int mi = 0; mi < _state.desc().plugin->modules[m].info.slot_count; mi++)
      if(_voice_engines[v][m][mi])
      {
        // state has already been copied from note event to 
        // _voice_states during voice stealing (to allow per-voice init)
        plugin_voice_block voice_block(make_voice_block(v, _voice_states[v].release_frame,
          _voice_states[v].note_id_, _voice_states[v].sub_voice_count, _voice_states[v].sub_voice_index,
          _voice_states[v].last_note_key, _voice_states[v].last_note_channel, _voice_states[v].last_retuned_pitch));
        plugin_block block(make_plugin_block(v, _voice_states[v].note_id_.channel, m, mi, _current_block_tuning_mode, state.start_frame, state.end_frame));
        block.voice = &voice_block;

        double start_time = seconds_since_epoch();
        _voice_module_process_duration_sec[v][m][mi] = start_time;
        _voice_engines[v][m][mi]->process(block);
        _voice_module_process_duration_sec[v][m][mi] = seconds_since_epoch() - start_time;

        // plugin completed its envelope
        if (block.voice->finished)
          _voice_states[v].stage = voice_stage::finishing;
      }

  // plugin should have initiated release state
  state.release_frame = -1;

  // process() call will acquire
  if (threaded)
  {
    _voice_thread_ids[v] = std::this_thread::get_id();
    std::atomic_thread_fence(std::memory_order_release);
    restore_denormals(denormal_state);
  }
}

// recycle by age
int
plugin_engine::find_best_voice_slot()
{
  int slot = -1;
  std::int64_t min_time = std::numeric_limits<std::int64_t>::max();
  for (int i = 0; i < _voice_states.size(); i++)
    if (_voice_states[i].stage == voice_stage::unused)
    {
      slot = i;
      break;
    }
    else if (_voice_states[i].time < min_time)
    {
      slot = i;
      min_time = _voice_states[i].time;
    }
  assert(slot != -1);
  return slot;
}

void 
plugin_engine::activate_voice(
  note_event const& event, int slot, engine_tuning_mode tuning_mode, 
  int sub_voice_count, int sub_voice_index, int frame_count)
{
  assert(slot >= 0);
  auto& state = _voice_states[slot];
  state.note_id_ = event.id;
  state.release_id = event.id;
  state.end_frame = frame_count;
  state.start_frame = event.frame;
  state.velocity = event.velocity;
  state.stage = voice_stage::active;
  state.time = _stream_time + event.frame;
  state.sub_voice_count = sub_voice_count;
  state.sub_voice_index = sub_voice_index;
  assert(0 <= state.start_frame && state.start_frame <= state.end_frame && state.end_frame <= frame_count);

  // microtuning support
  state.retuned_pitch = event.id.key;
  if (tuning_mode == engine_tuning_mode_on_note_before_mod || tuning_mode == engine_tuning_mode_continuous_before_mod)
    state.retuned_pitch = std::clamp(event.id.key + (float)MTS_RetuningInSemitones(_host_block->mts_client, (char)event.id.key, (char)event.id.channel), 0.0f, 127.0f);
  else if (tuning_mode == engine_tuning_mode_on_note_after_mod || tuning_mode == engine_tuning_mode_continuous_after_mod)
  {
    // for these cases we need to warp pitchmods after modulation, so need the entire mts table
    _current_voice_tuning_mode[slot] = tuning_mode;
    for (int i = 0; i < 128; i++)
    {
      _current_voice_tuning_channel[slot][i].frequency = _current_block_tuning_channel[event.id.channel][i].frequency;
      _current_voice_tuning_channel[slot][i].is_mapped = _current_block_tuning_channel[event.id.channel][i].is_mapped;
    }
  }

  // allow module engine to do once-per-voice init
  voice_block_params_snapshot(slot);
  for (int m = _state.desc().module_voice_start; m < _state.desc().module_output_start; m++)
    for (int mi = 0; mi < _state.desc().plugin->modules[m].info.slot_count; mi++)
      if(_voice_engines[slot][m][mi])
      {
        plugin_voice_block voice_block(make_voice_block(
          slot, _voice_states[slot].release_frame, event.id, _voice_states[slot].sub_voice_count, 
          _voice_states[slot].sub_voice_index, _last_note_key, _last_note_channel, _last_note_retuned_pitch));
        plugin_block block(make_plugin_block(slot, _voice_states[slot].note_id_.channel, m, mi, tuning_mode, state.start_frame, state.end_frame));
        block.voice = &voice_block;
        _voice_engines[slot][m][mi]->reset(&block);
      }
}

void
plugin_engine::automation_sanity_check(int frame_count)
{
  // This is a nice debugging tool but it does sometimes
  // also fire assertions on fast smoothing changes, which are fine.
#if 0
  for (int m = 0; m < _state.desc().plugin->modules.size(); m++)
  {
    auto const& module = _state.desc().plugin->modules[m];
    for (int mi = 0; mi < module.info.slot_count; mi++)
      for (int p = 0; p < module.params.size(); p++)
      {
        auto const& param = module.params[p];
        if (param.dsp.rate == param_rate::accurate)
        {
          for (int pi = 0; pi < param.info.slot_count; pi++)
          {
            auto const& curve = _accurate_automation[m][mi][p][pi];
            (void)curve; 
            for (int f = 1; f < frame_count; f++)
              assert(std::fabs(curve[f] - curve[f - 1]) < 0.01f);
            if (_blocks_processed > 0)
              assert(std::fabs(curve[0] - _automation_state_last_round_end[m][mi][p][pi]) < 0.01f);
          }
        }
      }
  }
#endif
}

void 
plugin_engine::process()
{
  int voice_count = 0;
  int frame_count = _host_block->frame_count;

  _host_block->events.out.clear();
  std::pair<std::uint32_t, std::uint32_t> denormal_state = disable_denormals();  

  // set automation values to current state, events may overwrite
  init_automation_from_state();

  /***************************************/
  /* STEP 1: Set up per-block automation */
  /***************************************/

  // microtuning mode
  auto const& topo = *_state.desc().plugin;
  _current_block_tuning_mode = get_current_tuning_mode();
  if (!MTS_HasMaster(_host_block->mts_client)) _current_block_tuning_mode = engine_tuning_mode_off;
  if (_current_block_tuning_mode != engine_tuning_mode_off)
  {
    query_mts_esp_tuning(_current_block_tuning_global, -1);
    for(int i = 0; i < 16; i++)
      query_mts_esp_tuning(_current_block_tuning_channel[i], i);
  }

  // smoothing per-block bpm values
  _bpm_filter.set(_host_block->shared.bpm);
  if(topo.bpm_smooth_module >= 0 && topo.bpm_smooth_param >= 0)
    _bpm_filter.init(_sample_rate, _state.get_plain_at(topo.bpm_smooth_module, 0, topo.bpm_smooth_param, 0).real() * 0.001);
  for(int f = 0; f < frame_count; f++)
    _bpm_automation[f] = _bpm_filter.next().first;
   
  // host automation
  for (int e = 0; e < _host_block->events.block.size(); e++)
  {
    // we update state right here so no need to mark as automated
    auto const& event = _host_block->events.block[e];
    _state.set_normalized_at_index(event.param, event.normalized);
    _block_automation.set_normalized_at_index(event.param, event.normalized);
  }

  /*********************************************/
  /* STEP 2: Set up sample-accurate automation */
  /*********************************************/

  // The idea is to construct dense buffers from incoming events
  // *without* smoothing stuff that has not changed.
  // Init_automation_from_state will just dump current
  // plugin state into these buffers, taking into account
  // any filters that are still "active".

  // param smoothing control
  float auto_filter_millis = default_auto_filter_millis;
  if (topo.auto_smooth_module >= 0 && topo.auto_smooth_param >= 0)
    auto_filter_millis = _state.get_plain_at(topo.auto_smooth_module, 0, topo.auto_smooth_param, 0).real();

  // deal with unfinished filters from the previous round
  // automation events may overwrite below but i think thats ok
  // also need to run filter to completion for the entire block
  // i.e. even when filter is inactive rest of the block needs the end value
  for (int m = 0; m < _state.desc().plugin->modules.size(); m++)
    for (int mi = 0; mi < _state.desc().plugin->modules[m].info.slot_count; mi++)
      for (int p = 0; p < _state.desc().plugin->modules[m].params.size(); p++)
        if (_state.desc().plugin->modules[m].params[p].dsp.rate == param_rate::accurate)
          for (int pi = 0; pi < _state.desc().plugin->modules[m].params[p].info.slot_count; pi++)
            if (_automation_lerp_filters[m][mi][p][pi].active() || _automation_lp_filters[m][mi][p][pi].active())
            {   
              _automation_lerp_filters[m][mi][p][pi].init(_sample_rate, auto_filter_millis * 0.001f);
              _automation_lp_filters[m][mi][p][pi].init(_sample_rate, auto_filter_millis * 0.001f);
              auto& curve = _accurate_automation[m][mi][p][pi];
              for(int f = 0; f < frame_count; f++)
                curve[f] = _automation_lp_filters[m][mi][p][pi].next(_automation_lerp_filters[m][mi][p][pi].next().first);
              (void)curve;
            }

  // debug make sure theres no jumps in the curve
  automation_sanity_check(frame_count);

  // deal with new events from the current round
  // interpolate auto and mod together
  auto& auto_and_mod = _host_block->events.accurate_automation_and_modulation;
  auto_and_mod.clear();
  auto_and_mod.insert(auto_and_mod.begin(),
    _host_block->events.accurate_automation.begin(),
    _host_block->events.accurate_automation.end());
  auto_and_mod.insert(auto_and_mod.begin(),
    _host_block->events.accurate_modulation.begin(),
    _host_block->events.accurate_modulation.end());
  auto comp = [](auto const& l, auto const& r) {
    if (l.param < r.param) return true;
    if (l.param > r.param) return false;
    if (l.frame < r.frame) return true;
    if (l.frame > r.frame) return false;
    if (l.is_mod < r.is_mod) return true;
    if (l.is_mod > r.is_mod) return false;
    return false; };
  std::sort(auto_and_mod.begin(), auto_and_mod.end(), comp);

  for (int e = 0; e < auto_and_mod.size(); e++)
  {
    // sorting should be param first, frame second
    // see splice_engine
    auto const& event = auto_and_mod[e];
    bool is_last_event = e == auto_and_mod.size() - 1;

    assert(is_last_event ||
      auto_and_mod[e + 1].param > event.param || (
      auto_and_mod[e + 1].param == event.param &&
      auto_and_mod[e + 1].frame >= event.frame));

    // run the automation curve untill the next event
    // which may reside in the next block, incase we'll pick it up later
    int next_event_pos = frame_count - 1;
    auto const& mapping = _state.desc().param_mappings.params[event.param];
    auto& curve = mapping.topo.value_at(_accurate_automation);
    auto& lerp_filter = mapping.topo.value_at(_automation_lerp_filters);
    auto& lp_filter = mapping.topo.value_at(_automation_lp_filters);
    if(!is_last_event && event.param == auto_and_mod[e + 1].param)
      next_event_pos = auto_and_mod[e + 1].frame;

    // update patch state or mod state and figure out new lerp target
    double new_target_value;
    if (event.is_mod)
    {
      new_target_value = _state.get_normalized_at_index(event.param).value();
      new_target_value += check_bipolar(event.value_or_offset);
      mapping.topo.value_at(_current_modulation) = event.value_or_offset;
    }
    else
    {
      new_target_value = mapping.topo.value_at(_current_modulation);
      new_target_value += check_unipolar(event.value_or_offset);
      _state.set_normalized_at_index(event.param, normalized_value(event.value_or_offset));
    }
    new_target_value = std::clamp(new_target_value, 0.0, 1.0);

    // start tracking the next value - lerp with delay + lp filter
    // may cross block boundary, see init_automation_from_state
    // need to restore current filter value to one-before-event-frame 
    // since filters are already run to completion above
    if(event.frame == 0)
    {
      lp_filter.current(mapping.topo.value_at(_automation_state_last_round_end));
      lerp_filter.current(mapping.topo.value_at(_automation_state_last_round_end));
    }
    else
    {
      lp_filter.current(curve[event.frame - 1]);
      lerp_filter.current(curve[event.frame - 1]);
    }

    lerp_filter.set(new_target_value);
    for(int f = event.frame; f <= next_event_pos; f++)
      curve[f] = lp_filter.next(lerp_filter.next().first);

    // make sure to re-fill the automation buffer on the next round
    mapping.topo.value_at(_param_was_automated) = 1;

    // This is a nice debugging tool but it does sometimes
    // also fire assertions on fast smoothing changes, which are fine.
#if 0
    for (int f = 1; f <= next_event_pos; f++)
      assert(std::fabs(curve[f] - curve[f - 1]) < 0.01f);
    if (_blocks_processed > 0)
      assert(std::fabs(curve[0] - mapping.topo.value_at(_automation_state_last_round_end)) < 0.01f);
#endif
  }

  // debug make sure theres no jumps in the curve
  automation_sanity_check(frame_count);

  // need to remember last value so we can restart filtering from there
  // in case an event comes in on the next round
  for (int m = 0; m < _state.desc().plugin->modules.size(); m++)
  {
    auto const& module = _state.desc().plugin->modules[m];
    for (int mi = 0; mi < module.info.slot_count; mi++)
      for (int p = 0; p < module.params.size(); p++)
      {
        auto const& param = module.params[p];
        if (param.dsp.rate == param_rate::accurate)
        {
          for (int pi = 0; pi < param.info.slot_count; pi++)
          {
            auto const& curve = _accurate_automation[m][mi][p][pi];
            _automation_state_last_round_end[m][mi][p][pi] = curve[frame_count - 1];
          }
        }
      } 
  }

  /***************************************************************/
  /* STEP 3: Set up MIDI automation (treated as sample-accurate) */
  /***************************************************************/

  // midi smoothing control
  float midi_filter_millis = default_midi_filter_millis;
  if (topo.midi_smooth_module >= 0 && topo.midi_smooth_param >= 0)
    midi_filter_millis = _state.get_plain_at(topo.midi_smooth_module, 0, topo.midi_smooth_param, 0).real();

  // find out which midi sources are actually used
  // since it is quite expensive to just keep them all active
  // note: midi_active_selector probably depends on per-block automation values
  for (int m = 0; m < _state.desc().plugin->modules.size(); m++)
  {
    auto const& module = _state.desc().plugin->modules[m];
    for (int mi = 0; mi < module.info.slot_count; mi++)
    {
      std::fill(_midi_active_selection[m][mi].begin(), _midi_active_selection[m][mi].end(), 0);
      if (module.midi_active_selector_ != nullptr)
        module.midi_active_selector_(_state, mi, _midi_active_selection);
    }
  }

  // process midi automation values
  // plugin gui may provide smoothing amount params
  auto frame_comp = [](auto const& l, auto const& r) { return l.frame < r.frame; };
  std::sort(_host_block->events.midi.begin(), _host_block->events.midi.end(), frame_comp);
  std::fill(_midi_was_automated.begin(), _midi_was_automated.end(), 0);

  // note: midi_source * frame_count loop rather than frame_count * midi_source loop for performance
  for (int ms = 0; ms < _midi_filters.size(); ms++)
  {
    int event_index = 0;
    auto const& mapping = _state.desc().midi_mappings.midi_sources[ms];
    if(!mapping.topo.value_at(_midi_active_selection)) continue;

    auto& curve = mapping.topo.value_at(_midi_automation);
    for (int f = 0; f < frame_count; f++)
    {
      for (; event_index < _host_block->events.midi.size() && _host_block->events.midi[event_index].frame == f; event_index++)
      {
        // if midi event is mapped, set the next target value for interpolation
        auto const& event = _host_block->events.midi[event_index];
        auto const& id_mapping = _state.desc().midi_mappings.id_to_index;
        auto iter = id_mapping.find(event.id);
        if (iter == id_mapping.end()) continue;
        int midi_index = iter->second;
        if(midi_index == ms)
        {
          _midi_filters[midi_index].set(event.normalized.value());
          _midi_filters[midi_index].init(_sample_rate, midi_filter_millis * 0.001);
        }
      }

      auto filter_result = _midi_filters[ms].next();
      curve[f] = filter_result.first;
      _midi_was_automated[ms] |= filter_result.second ? 1 : 0;
    }
  }

  // take care of midi linked parameters
  for(int ms = 0; ms < _midi_filters.size(); ms++)
  {
    auto const& midi_mapping = _state.desc().midi_mappings.midi_sources[ms];
    if(!midi_mapping.topo.value_at(_midi_active_selection)) 
    {
      // midi source linked to a parameter should always be active
      assert(!midi_mapping.linked_params.size());
      continue;
    }

    if (!_midi_was_automated[ms]) continue;
    auto last_value = midi_mapping.topo.value_at(_midi_automation)[frame_count - 1];
    for (int lp = 0; lp < midi_mapping.linked_params.size(); lp++)
    {
      int param_index = midi_mapping.linked_params[lp];
      auto const& param_mapping = _state.desc().param_mappings.params[param_index];
      assert(param_mapping.midi_source_global == ms);
      auto const& mt = midi_mapping.topo;
      auto const& pt = param_mapping.topo;
      std::copy(
        _midi_automation[mt.module_index][mt.module_slot][mt.midi_index].begin(),
        _midi_automation[mt.module_index][mt.module_slot][mt.midi_index].begin() + frame_count,
        _accurate_automation[pt.module_index][pt.module_slot][pt.param_index][pt.param_slot].begin());
      _state.set_normalized_at_index(param_index, normalized_value(last_value));
      pt.value_at(_param_was_automated) = 1;

      // have the host update the gui with the midi value
      // note that we don't handle the other direction (i.e. no midi cc out)
      block_event out_event;
      out_event.param = param_index;
      out_event.normalized = normalized_value(last_value);
      _host_block->events.out.push_back(out_event);
    }
  }

  /*********************************************************/
  /* STEP 4: Run global input modules (typically CV stuff) */
  /*********************************************************/

  // run input modules in order
  for (int m = 0; m < _state.desc().module_voice_start; m++)
    for (int mi = 0; mi < _state.desc().plugin->modules[m].info.slot_count; mi++)
      if(_input_engines[m][mi])
      {
        plugin_block block(make_plugin_block(-1, -1, m, mi, _current_block_tuning_mode, 0, frame_count));
        double start_time = seconds_since_epoch();
        _global_module_process_duration_sec[m][mi] = start_time;
        _input_engines[m][mi]->process(block);
        _global_module_process_duration_sec[m][mi] = seconds_since_epoch() - start_time;
      }

  /********************************************************/
  /* STEP 5: Voice management in case of polyphonic synth */
  /********************************************************/

  if(_state.desc().plugin->type == plugin_type::synth)
  {
    // always take a voice for an entire block,
    // module processor is handed appropriate start/end_frame.
    // and return voices completed the previous block
    for (int i = 0; i < _voice_states.size(); i++)
    {
      auto& state = _voice_states[i];
      if (state.stage == voice_stage::active || state.stage == voice_stage::releasing)
      {
        voice_count++;
        state.start_frame = 0;
        state.end_frame = frame_count;
        state.release_frame = frame_count;
      }
      else if (state.stage == voice_stage::finishing)
        state = voice_state();
    }

    int voice_mode = -1;
    assert(topo.voice_mode_module >= 0);
    assert(topo.voice_mode_param >= 0);
    voice_mode = _state.get_plain_at(topo.voice_mode_module, 0, topo.voice_mode_param, 0).step();
    assert(voice_mode == engine_voice_mode_mono || voice_mode == engine_voice_mode_poly || voice_mode == engine_voice_mode_release);

    // for mono mode
    std::fill(_mono_note_stream.begin(), _mono_note_stream.end(), mono_note_state { _last_note_key, false });

    if(voice_mode == engine_voice_mode_poly)
    {
      // figure out subvoice count for global unison
      int sub_voice_count = 1;
      if (topo.sub_voice_counter) sub_voice_count = topo.sub_voice_counter(_graph, _state);

      // poly mode: steal voices for incoming notes by age
      for (int e = 0; e < _host_block->events.notes.size(); e++)
      {
        auto const& event = _host_block->events.notes[e];
        if (event.type != note_event_type::on) continue;

        // mts-esp support
        if (_current_block_tuning_mode != engine_tuning_mode_off && 
          !_current_block_tuning_channel[event.id.channel][event.id.key].is_mapped) continue;

        for(int sv = 0; sv < sub_voice_count; sv++)
        {
          int slot = find_best_voice_slot();
          activate_voice(event, slot, _current_block_tuning_mode, sub_voice_count, sv, frame_count);
        }
         
        // for portamento
        _last_note_key = event.id.key;
        _last_note_channel = event.id.channel;
        _last_note_retuned_pitch = event.id.key;
        if (_current_block_tuning_mode == engine_tuning_mode_on_note_before_mod || _current_block_tuning_mode == engine_tuning_mode_continuous_before_mod)
          _last_note_retuned_pitch = std::clamp(event.id.key + (float)MTS_RetuningInSemitones(_host_block->mts_client, (char)event.id.key, (char)event.id.channel), 0.0f, 127.0f);
      }
    }
    else
    {
      // true mono mode: recycle the first active or releasing voice, or set up a new one
      // release mono mode: recycle the first active but not releasing voice, or set up a new one
      // note: we do not account for triggering more than 1 voice per block
      // note: need to play *all* incoming notes into this voice
      // note: subvoice/global uni does NOT apply to mono mode!
      // because subvoices may have unequal length, then stuff becomes too complicated

      int first_note_on_index = -1;
      for (int e = 0; e < _host_block->events.notes.size(); e++)
      {
        auto const& event = _host_block->events.notes[e];
        if (event.type == note_event_type::on)
        {
          // mts-esp support
          // in mono-mode + tuning-mode, we trigger the first key that is mapped
          if (_current_block_tuning_mode == engine_tuning_mode_off ||
            _current_block_tuning_channel[event.id.channel][event.id.key].is_mapped)
          {
            first_note_on_index = e;
            break;
          }
        }
      }

      if(first_note_on_index != -1)
      {
        auto& first_event = _host_block->events.notes[first_note_on_index];

        int slot = -1;
        for(int v = 0; v < _voice_states.size(); v++)
          if (_voice_states[v].stage == voice_stage::active ||
          (_voice_states[v].stage == voice_stage::releasing && voice_mode == engine_voice_mode::engine_voice_mode_mono))
          {
            slot = v;
            break;
          }

        if (voice_mode == engine_voice_mode_mono)
        {
          // no slot found, for real mono mode take slot 0
          if(slot == -1)
          {
            slot = 0;
            activate_voice(first_event, 0, _current_block_tuning_mode, 1, 0, frame_count);
          }
          else 
          {
            // needed for later release by id or pck
            _voice_states[slot].release_id = first_event.id;
            _voice_states[slot].time = _stream_time + first_event.frame;
          }
        }

        // for release mono mode, need to do the recycling again
        if (voice_mode == engine_voice_mode_release)
        {
          if (slot == -1)
          {
            slot = find_best_voice_slot();
            activate_voice(first_event, slot, _current_block_tuning_mode, 1, 0, frame_count);
          }
          else
          {
            // needed for later release by id or pck
            _voice_states[slot].release_id = first_event.id;
            _voice_states[slot].time = _stream_time + first_event.frame;
          }
        }

        // set up note stream, plugin will have to do something with it
        for(int e = 0; e < _host_block->events.notes.size(); e++)
          if(_host_block->events.notes[e].type == note_event_type::on)
          {
            auto const& event = _host_block->events.notes[e]; 
            _last_note_key = event.id.key;
            _last_note_channel = event.id.channel;
            _last_note_retuned_pitch = event.id.key;
            if (_current_block_tuning_mode == engine_tuning_mode_on_note_before_mod || _current_block_tuning_mode == engine_tuning_mode_continuous_before_mod)
              _last_note_retuned_pitch = std::clamp(event.id.key + (float)MTS_RetuningInSemitones(_host_block->mts_client, (char)event.id.key, (char)event.id.channel), 0.0f, 127.0f);
            std::fill(_mono_note_stream.begin() + event.frame, _mono_note_stream.end(), mono_note_state { event.id.key, false });
            _mono_note_stream[event.frame].note_on = true;
          }
      }
    }

    // mark voices for completion the next block
    // be sure to check the note was on (earlier in time than the event) before we turn it off
    // not sure how to handle note id vs pck: 
    // clap on bitwig hands us note-on events with note id and note-off events without them
    // so i choose to allow note-off with note-id to only kill the same note-id
    // but allow note-off without note-id to kill any matching pck regardless of note-id
    for (int e = 0; e < _host_block->events.notes.size(); e++)
    {
      auto const& event = _host_block->events.notes[e];
      if (event.type == note_event_type::on) continue;
      for (int v = 0; v < _voice_states.size(); v++)
      {
        auto& state = _voice_states[v];
        if (state.stage == voice_stage::active &&
          state.time < _stream_time + event.frame &&
          ((event.id.id != -1 && state.release_id.id == event.id.id) ||
            (event.id.id == -1 && (state.release_id.key == event.id.key && state.release_id.channel == event.id.channel))))
        {
          if (event.type == note_event_type::cut)
          {
            state.end_frame = event.frame;
            state.stage = voice_stage::finishing;
            assert(0 <= state.start_frame && state.start_frame <= state.end_frame && state.end_frame < frame_count);
          }
          else {
            state.release_frame = event.frame;
            state.stage = voice_stage::releasing;
            assert(0 <= state.start_frame && state.start_frame <= state.release_frame && state.release_frame < frame_count);
          }
        }
      }
    }
  }

  /*************************************************************/
  /* STEP 6: Run per-voice modules in case of polyphonic synth */
  /*************************************************************/

  int thread_count = 1;
  if (_state.desc().plugin->type == plugin_type::synth)
  {
    // run voice modules in order taking advantage of host threadpool if possible
    // note: multithreading over voices, not anything within a single voice
    if (!_voice_processor)
      process_voices_single_threaded();
    else
    {
      for (int v = 0; v < _polyphony; v++)
        _voice_thread_ids[v] = std::thread::id();
      std::atomic_thread_fence(std::memory_order_release);
      if (!_voice_processor(*this, _voice_processor_context))
        process_voices_single_threaded();
      else 
      {
        std::atomic_thread_fence(std::memory_order_acquire);
        std::sort(_voice_thread_ids.begin(), _voice_thread_ids.end());
        thread_count = std::unique(_voice_thread_ids.begin(), _voice_thread_ids.end()) - _voice_thread_ids.begin();
        thread_count = std::max(1, thread_count);
      }
    }

    // mixdown voices output
    _voices_mixdown[0].fill(0, frame_count, 0.0f);
    _voices_mixdown[1].fill(0, frame_count, 0.0f);
    for (int v = 0; v < _voice_states.size(); v++)
      if (_voice_states[v].stage != voice_stage::unused)
        for(int c = 0; c < 2; c++)
          for(int f = _voice_states[v].start_frame; f < _voice_states[v].end_frame; f++)
            _voices_mixdown[c][f] += _voice_results[v][c][f];
  }

  /****************************************************************************************/
  /* STEP 7: Run global output modules (at least one is expected to write host audio out) */
  /****************************************************************************************/

  // run output modules in order
  for (int m = _state.desc().module_output_start; m < _state.desc().plugin->modules.size(); m++)
    for (int mi = 0; mi < _state.desc().plugin->modules[m].info.slot_count; mi++)
      if(_output_engines[m][mi])
      {
        // output params are written to intermediate buffer first
        plugin_output_block out_block = {
          voice_count, thread_count,
          _cpu_usage, _high_cpu_module, _high_cpu_module_usage,
          _host_block->audio_out, _output_values[m][mi], _voices_mixdown
        };
        plugin_block block(make_plugin_block(-1, -1, m, mi, _current_block_tuning_mode, 0, frame_count));
        block.out = &out_block;
        double start_time = seconds_since_epoch();
        _global_module_process_duration_sec[m][mi] = start_time;
        _output_engines[m][mi]->process(block);
        _global_module_process_duration_sec[m][mi] = seconds_since_epoch() - start_time;

        // copy back output parameter values
        for(int p = 0; p < _state.desc().plugin->modules[m].params.size(); p++)
          if(_state.desc().plugin->modules[m].params[p].dsp.direction == param_direction::output)
            for(int pi = 0; pi < _state.desc().plugin->modules[m].params[p].info.slot_count; pi++)
              _state.set_plain_at(m, mi, p, pi, _output_values[m][mi][p][pi]);
      }

  /*************************************/
  /* STEP 8: Process output parameters */
  /*************************************/

  // update output params 3 times a second
  // push all out params - we don't check for changes
  auto now_sec = seconds_since_epoch();
  if(now_sec - _output_updated_sec > 0.33)
  {
    int param_global = 0;
    _output_updated_sec = now_sec;
    for (int m = 0; m < _state.desc().plugin->modules.size(); m++)
    {
      auto const& module = _state.desc().plugin->modules[m];
      for (int mi = 0; mi < module.info.slot_count; mi++)
        for (int p = 0; p < module.params.size(); p++)
          for(int pi = 0; pi < module.params[p].info.slot_count; pi++)
          {
            if (module.params[p].dsp.direction == param_direction::output)
            {
              block_event out_event;
              out_event.param = param_global;
              out_event.normalized = _state.get_normalized_at(m, mi, p, pi);
              _host_block->events.out.push_back(out_event);
            }
            param_global++;
          }
    }
  }

  /*******************/
  /* STEP 9: Wrap-up */
  /*******************/

  // keep track of running time in frames
  _stream_time += frame_count;
  _blocks_processed++;
  restore_denormals(denormal_state);
}

}