#include <plugin_base/topo/plugin.hpp>
#include <plugin_base/topo/support.hpp>
#include <plugin_base/helpers/dsp.hpp>
#include <plugin_base/dsp/engine.hpp>
#include <plugin_base/dsp/utility.hpp>
#include <plugin_base/shared/io_plugin.hpp>
#include <plugin_base/dsp/graph_engine.hpp>
#include <plugin_base/gui/mseg_editor.hpp>

#include <firefly_synth/synth.hpp>
#include <firefly_synth/waves.hpp>

#include <cmath>
#include <sstream>

using namespace plugin_base;

namespace firefly_synth {

static int const mseg_max_point_count = 16;
static float const log_half = std::log(0.5f); 
static float const max_filter_time_ms = 500;

enum { custom_tag_total_pos, custom_tag_multitrig_level };
enum class env_stage { delay, attack, hold, decay, sustain, release, filter, end };

enum { type_sustain, type_follow, type_release };
enum { trigger_legato, trigger_retrig, trigger_multi };
enum { mode_linear, mode_exp_uni, mode_exp_bi, mode_exp_split, mode_mseg };
enum { section_on, section_type, section_sync, section_trigger, section_dahdr, section_mseg };
enum {
  param_on, param_type, param_mode, param_sync, param_filter, param_trigger, param_sustain,
  param_delay_time, param_delay_tempo, param_hold_time, param_hold_tempo,
  param_attack_time, param_attack_tempo, param_attack_slope, 
  param_decay_time, param_decay_tempo, param_decay_slope, 
  param_release_time, param_release_tempo, param_release_slope,
  param_mseg_start_y, param_mseg_end_y, param_mseg_on, param_mseg_x, param_mseg_y };

static constexpr bool is_exp_slope(int mode) { 
  return mode == mode_exp_uni || mode == mode_exp_bi || mode == mode_exp_split; }

static std::vector<list_item>
trigger_items()
{
  std::vector<list_item> result;
  result.emplace_back("{5600C9FE-6122-47B3-A625-9E059E56D949}", "Legato");
  result.emplace_back("{0EAFA1E3-4707-4E8C-B16D-5E16955F8962}", "Retrigger");
  result.emplace_back("{A49D48D7-D664-4A52-BD82-07A488BDB4C8}", "Multi-trigger");
  return result;
}

static std::vector<list_item>
type_items()
{
  std::vector<list_item> result;
  result.emplace_back("{D6F91E8A-D682-4EE7-BBDC-5147E62BC4F4}", "Sustain");
  result.emplace_back("{2E644E98-2F57-4E91-881C-51700F32D804}", "Follow");
  result.emplace_back("{5A38B671-FE8F-4C0E-B628-985AF6F1F822}", "Release");
  return result;
}

static std::vector<list_item>
mode_items()
{
  std::vector<list_item> result;
  result.emplace_back("{B3310A09-6A49-4EB6-848C-1F61A1028126}", "Linear", "Linear");
  result.emplace_back("{924FB84C-7509-446F-82E7-B9E39DE399A5}", "Exp Uni", "Exponential Unipolar");
  result.emplace_back("{35EDA297-B042-41C0-9A1C-9502DBDAF633}", "Exp Bi", "Exponential Bipolar");
  result.emplace_back("{666FEFDF-3BC5-4FDA-8490-A8980741D6E7}", "Exp Split", "Exponential Split");
  result.emplace_back("{CD3E67A3-80CC-4419-9E1F-E7A5FF9ABE1E}", "MSEG", "Multi-Segment Envelope Generator");
  return result;
}

class env_state_converter :
public state_converter
{
  plugin_desc const* const _desc;
public:
  env_state_converter(plugin_desc const* const desc) : _desc(desc) {}
  void post_process_always(load_handler const& handler, plugin_state& new_state) override {}
  void post_process_existing(load_handler const& handler, plugin_state& new_state) override;

  bool handle_invalid_param_value(
    std::string const& new_module_id, int new_module_slot,
    std::string const& new_param_id, int new_param_slot,
    std::string const& old_value, load_handler const& handler,
    plain_value& new_value) override;
};

class env_engine:
public module_engine {

  void process_internal(plugin_block& block, cv_cv_matrix_mixdown const* modulation);

public:
  PB_PREVENT_ACCIDENTAL_COPY_DEFAULT_CTOR(env_engine);

  void reset_audio(plugin_block const* block,
    std::vector<note_event> const* in_notes,
    std::vector<note_event>* out_notes) override;
  void process_audio(plugin_block& block,
    std::vector<note_event> const* in_notes,
    std::vector<note_event>* out_notes) override { process_internal(block, nullptr); }

  void process_graph(plugin_block& block,
    std::vector<note_event> const* in_notes,
    std::vector<note_event>* out_notes, 
    std::vector<mod_out_custom_state> const& custom_outputs, void* context) override;
  void reset_graph(plugin_block const* block,
    std::vector<note_event> const* in_notes,
    std::vector<note_event>* out_notes, std::vector<mod_out_custom_state> const& custom_outputs, void* context) override;

private:

  double _stn = 0;
  double _hld = 0;
  double _dcy = 0;
  double _dly = 0;
  double _att = 0;
  double _rls = 0;
  
  env_stage _stage = {};
  cv_filter _filter = {};

  double _stage_pos = 0;
  double _total_pos = 0;
  double _current_level = 0;
  double _multitrig_level = 0;

  double _slp_att_exp = 0;
  double _slp_dcy_exp = 0;
  double _slp_rls_exp = 0;
  double _slp_att_splt_bnd = 0;
  double _slp_dcy_splt_bnd = 0;
  double _slp_rls_splt_bnd = 0;

  bool _voice_initialized = false;

  // graphing
  int _ffwd_stopped_at_samples = -1;
  bool _is_ffwd_run_for_graph = false;
  float _ffwd_target_total_pos = 0.0f;

  static inline double const slope_min = 0.0001;
  static inline double const slope_max = 0.9999;
  static inline double const slope_range = slope_max - slope_min;

  void init_slope_exp(double slope, double& exp);
  void init_slope_exp_splt(double slope, double split_pos, double& exp, double& splt_bnd);

  template <bool Monophonic> 
  void process_mono(plugin_block& block, cv_cv_matrix_mixdown const* modulation);
  template <bool Monophonic, int Type>
  void process_mono_type(plugin_block& block, cv_cv_matrix_mixdown const* modulation);
  template <bool Monophonic, int Type, bool Sync>
  void process_mono_type_sync(plugin_block& block, cv_cv_matrix_mixdown const* modulation);
  template <bool Monophonic, int Type, bool Sync, int Trigger>
  void process_mono_type_sync_trigger(plugin_block& block, cv_cv_matrix_mixdown const* modulation);
  template <bool Monophonic, int Type, bool Sync, int Trigger, int Mode, class CalcSlope>
  void process_mono_type_sync_trigger_mode(plugin_block& block, cv_cv_matrix_mixdown const* modulation, CalcSlope calc_slope);
};

static void
init_default(plugin_state& state)
{ state.set_text_at(module_env, 1, param_on, 0, "On"); }

void
env_plot_length_seconds(plugin_state const& state, int slot, float& dly, float& att, float& hld, float& dcy, float& rls, float& flt)
{
  float const bpm = 120;
  bool sync = state.get_plain_at(module_env, slot, param_sync, 0).step() != 0;
  flt = state.get_plain_at(module_env, slot, param_filter, 0).real() / 1000.0f;
  hld = sync_or_time_from_state(state, bpm, sync, module_env, slot, param_hold_time, param_hold_tempo);
  dly = sync_or_time_from_state(state, bpm, sync, module_env, slot, param_delay_time, param_delay_tempo);
  dcy = sync_or_time_from_state(state, bpm, sync, module_env, slot, param_decay_time, param_decay_tempo);
  att = sync_or_time_from_state(state, bpm, sync, module_env, slot, param_attack_time, param_attack_tempo);
  rls = sync_or_time_from_state(state, bpm, sync, module_env, slot, param_release_time, param_release_tempo);
}

static graph_engine_params
make_graph_engine_params()
{
  graph_engine_params result = {};
  result.bpm = 120;
  result.max_frame_count = 2000; // need to spot outliers
  return result;
}

static std::unique_ptr<graph_engine>
make_graph_engine(plugin_desc const* desc)
{
  auto params = make_graph_engine_params();
  return std::make_unique<graph_engine>(desc, params);
}

static graph_data
render_graph(
  plugin_state const& state, graph_engine* engine, int param, 
  param_topo_mapping const& mapping, std::vector<mod_out_custom_state> const& custom_outputs)
{
  if (state.get_plain_at(module_env, mapping.module_slot, param_on, 0).step() == 0) 
    return graph_data(graph_data_type::off, { state.desc().plugin->modules[mapping.module_index].info.tag.menu_display_name });

  float dly, att, hld, dcy, rls, flt;
  env_plot_length_seconds(state, mapping.module_slot, dly, att, hld, dcy, rls, flt);
  float dahd = dly + att + hld + dcy;
  float dahdrf = dahd + rls + flt;

  if(dahdrf < 1e-5) return graph_data({}, false, {"0 Sec"});
  
  std::string partition = float_to_string(dahdrf, 2) + " Sec";
  bool sync = state.get_plain_at(module_env, mapping.module_slot, param_sync, 0).step() != 0;
  if(sync) partition = float_to_string(dahdrf / timesig_to_time(120, { 1, 1 }), 2) + " Bar";

  auto const params = make_graph_engine_params();
  int sample_rate = params.max_frame_count / dahdrf;
  int voice_release_at = dahd / dahdrf * params.max_frame_count;

  engine->process_begin(&state, sample_rate, params.max_frame_count, voice_release_at);
  auto const* block = engine->process(module_env, mapping.module_slot, custom_outputs, nullptr, [mapping, &custom_outputs](plugin_block& block) {
    env_engine engine;
    engine.reset_graph(&block, nullptr, nullptr, custom_outputs, nullptr);
    cv_cv_matrix_mixdown modulation(make_static_cv_matrix_mixdown(block)[module_env][mapping.module_slot]);
    engine.process_graph(block, nullptr, nullptr, custom_outputs, &modulation);
  });
  engine->process_end();

  jarray<float, 1> series(block->state.own_cv[0][0]);
  return graph_data(series, false, 1.0f, false, { partition });
}

bool
env_state_converter::handle_invalid_param_value(
  std::string const& new_module_id, int new_module_slot,
  std::string const& new_param_id, int new_param_slot,
  std::string const& old_value, load_handler const& handler,
  plain_value& new_value)
{
  if (handler.old_version() < plugin_version{ 1, 2, 0 })
  {
    // type + mode split out to separate controls
    if (new_param_id == _desc->plugin->modules[module_env].params[param_type].info.tag.id)
    {
      if (old_value == "{021EA627-F467-4879-A045-3694585AD694}" || 
          old_value == "{A23646C9-047D-485A-9A31-54D78D85570E}" ||
          old_value == "{CB4C4B41-8165-4303-BDAC-29142DF871DC}" ||
          old_value == "{DB38D81F-A6DC-4774-BA10-6714EA43938F}")
      {
        // sustain
        new_value = _desc->raw_to_plain_at(module_env, param_type, type_sustain);
        return true;
      }
      if (old_value == "{927DBB76-A0F2-4007-BD79-B205A3697F31}" ||
          old_value == "{CB268F2B-8A33-49CF-9569-675159ACC0E1}" ||
          old_value == "{221089F7-A516-4BCE-AE9A-D0D4F80A6BC5}" ||
          old_value == "{93473324-66FB-422F-9160-72B175A81207}")
      {
        // follow
        new_value = _desc->raw_to_plain_at(module_env, param_type, type_follow);
        return true;
      }
      if (old_value == "{0AF743E3-9248-4FF6-98F1-0847BD5790FA}" ||
         old_value == "{05AACFCF-4A2F-4EC6-B5A3-0EBF5A8B2800}" ||
         old_value == "{5FBDD433-C4E2-47E4-B471-F7B19485B31E}" ||
         old_value == "{1ECF13C0-EE16-4226-98D3-570040E6DA9D}")
      {
        // release
        new_value = _desc->raw_to_plain_at(module_env, param_type, type_release);
        return true;
      }
    }
  }
  return false;
}

void
env_state_converter::post_process_existing(load_handler const& handler, plugin_state& new_state)
{
  std::string old_value;
  auto const& modules = new_state.desc().plugin->modules;
  std::string module_id = modules[module_env].info.tag.id;
  if (handler.old_version() < plugin_version{ 1, 2, 0 })
  {
    for (int i = 0; i < modules[module_env].info.slot_count; i++)
    {
      // pick up mode from old type + mode
      if (handler.old_param_value(modules[module_env].info.tag.id, i, modules[module_env].params[param_type].info.tag.id, 0, old_value))
      {
        // Linear
        if (old_value == "{021EA627-F467-4879-A045-3694585AD694}" ||
          old_value == "{927DBB76-A0F2-4007-BD79-B205A3697F31}" ||
          old_value == "{0AF743E3-9248-4FF6-98F1-0847BD5790FA}")
          new_state.set_plain_at(module_env, i, param_mode, 0,
            _desc->raw_to_plain_at(module_env, param_mode, mode_linear));
        // Exp uni
        if (old_value == "{A23646C9-047D-485A-9A31-54D78D85570E}" ||
          old_value == "{CB268F2B-8A33-49CF-9569-675159ACC0E1}" ||
          old_value == "{05AACFCF-4A2F-4EC6-B5A3-0EBF5A8B2800}")
          new_state.set_plain_at(module_env, i, param_mode, 0,
            _desc->raw_to_plain_at(module_env, param_mode, mode_exp_uni));
        // Exp bi
        if (old_value == "{CB4C4B41-8165-4303-BDAC-29142DF871DC}" ||
          old_value == "{221089F7-A516-4BCE-AE9A-D0D4F80A6BC5}" ||
          old_value == "{5FBDD433-C4E2-47E4-B471-F7B19485B31E}")
          new_state.set_plain_at(module_env, i, param_mode, 0,
            _desc->raw_to_plain_at(module_env, param_mode, mode_exp_bi));
        // Exp split
        if (old_value == "{DB38D81F-A6DC-4774-BA10-6714EA43938F}" ||
          old_value == "{93473324-66FB-422F-9160-72B175A81207}" ||
          old_value == "{1ECF13C0-EE16-4226-98D3-570040E6DA9D}")
          new_state.set_plain_at(module_env, i, param_mode, 0,
            _desc->raw_to_plain_at(module_env, param_mode, mode_exp_split));
      }
    }
  }
}

module_topo
env_topo(int section, gui_position const& pos)
{
  module_topo result(make_module(
    make_topo_info_basic("{DE952BFA-88AC-4F05-B60A-2CEAF9EE8BF9}", "Env", module_env, 10),
    make_module_dsp(module_stage::voice, module_output::cv, 0, { 
      make_module_dsp_output(true, -1, make_topo_info_basic("{2CDB809A-17BF-4936-99A0-B90E1035CBE6}", "Output", 0, 1)) }),
    make_module_gui(section, pos, { { 1, 1 }, { 6, 26, 13, 34, 63 } })));
  result.gui.autofit_column = 1;
  result.gui.tabbed_name = "ENV";
  result.gui.is_drag_mod_source = true;
  result.info.description = "DAHDSR envelope generator with optional tempo-syncing, linear and exponential slopes and smoothing control.";

  result.graph_renderer = render_graph;
  result.graph_engine_factory = make_graph_engine;
  result.default_initializer = init_default;
  result.gui.menu_handler_factory = make_cv_routing_menu_handler;
  result.engine_factory = [](auto const&, int, int) { return std::make_unique<env_engine>(); };
  result.state_converter_factory = [](auto desc) { return std::make_unique<env_state_converter>(desc); };

  auto& on_section = result.sections.emplace_back(make_param_section(section_on,
    make_topo_tag_basic("{6BC7E26F-C687-4F38-AC60-1661714A0538}", "On"),
    make_param_section_gui({ 0, 0, 2, 1 }, { { 1, 1 }, { gui_dimension::auto_size_all } }, gui_label_edit_cell_split::vertical)));
  on_section.gui.merge_with_section = section_type;
  auto& on = result.params.emplace_back(make_param(
    make_topo_info_basic("{5EB485ED-6A5B-4A91-91F9-15BDEC48E5E6}", "On", param_on, 1),
    make_param_dsp_voice(param_automate::automate), make_domain_toggle(false),
    make_param_gui_single(section_on, gui_edit_type::toggle, { 0, 0 },
      make_label(gui_label_contents::name, gui_label_align::top, gui_label_justify::center))));
  on.domain.default_selector_ = [](int s, int) { return s == 0 ? "On" : "Off"; };
  on.gui.bindings.enabled.bind_slot([](int slot) { return slot > 0; });
  on.info.description = "Toggles envelope on/off.";

  auto& type_section = result.sections.emplace_back(make_param_section(section_type,
    make_topo_tag_basic("{25E441B9-D023-4312-92C0-9B3E64D4DAF9}", "Type"),
    make_param_section_gui({ 0, 1, 2, 1 }, { { 1, 1 }, { gui_dimension::auto_size_all, 1 } }, gui_label_edit_cell_split::horizontal)));
  type_section.gui.merge_with_section = section_on;
  auto& type = result.params.emplace_back(make_param(
    make_topo_info_basic("{E6025B4A-495C-421F-9A9A-8D2A247F94E7}", "Type", param_type, 1),
    make_param_dsp_voice(param_automate::automate), make_domain_item(type_items(), ""),
    make_param_gui_single(section_type, gui_edit_type::list, { 0, 0 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  type.gui.bindings.enabled.bind_params({ param_on }, [](auto const& vs) { return vs[0] != 0; });
  type.info.description = std::string("Selects envelope type.<br/>") +
    "Sustain - regular sustain type.<br/>" +
    "Follow - exactly follows the envelope ignoring note-off.<br/>" +
    "Release - follows the envelope (does not sustain) but respects note-off.";
  auto& mode = result.params.emplace_back(make_param(
    make_topo_info_basic("{C984B22A-68FB-44E4-8811-163D05CEC58B}", "Mode", param_mode, 1),
    make_param_dsp_voice(param_automate::automate), make_domain_item(mode_items(), ""),
    make_param_gui_single(section_type, gui_edit_type::list, { 1, 0 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  mode.gui.bindings.enabled.bind_params({ param_on }, [](auto const& vs) { return vs[0] != 0; });
  mode.info.description = std::string("Selects envelope slope mode.<br/>") +
    "Linear - linear slope, most cpu efficient.<br/>" +
    "Exponential unipolar - regular exponential slope.<br/>" +
    "Exponential bipolar - vertically splits section in 2 exponential parts.<br/>" +
    "Exponential split - horizontally and vertically splits section in 2 exponential parts to generate smooth curves.";

  auto& sync_section = result.sections.emplace_back(make_param_section(section_sync,
    make_topo_tag_basic("{B9A937AF-6807-438F-8F79-506C47F621BD}", "Sync"),
    make_param_section_gui({ 0, 2, 2, 1 }, { { 1, 1 }, { gui_dimension::auto_size_all, 1 } }, gui_label_edit_cell_split::horizontal)));
  sync_section.gui.merge_with_section = section_trigger;
  auto& sync = result.params.emplace_back(make_param(
    make_topo_info("{4E2B3213-8BCF-4F93-92C7-FA59A88D5B3C}", true, "Tempo Sync", "Snc", "Snc", param_sync, 1),
    make_param_dsp_voice(param_automate::automate), make_domain_toggle(false),
    make_param_gui_single(section_sync, gui_edit_type::toggle, { 0, 0 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  sync.gui.bindings.enabled.bind_params({ param_on }, [](auto const& vs) { return vs[0] != 0; });
  sync.info.description = "Toggles time or tempo-synced mode.";
  auto& filter = result.params.emplace_back(make_param(
    make_topo_info_basic("{C4D23A93-4376-4F9C-A1FA-AF556650EF6E}", "Smt", param_filter, 1),
    make_param_dsp_voice(param_automate::automate), make_domain_linear(0, max_filter_time_ms, 0, 0, "Ms"),
    make_param_gui_single(section_sync, gui_edit_type::knob, { 1, 0 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  filter.gui.bindings.enabled.bind_params({ param_on }, [](auto const& vs) { return vs[0] != 0; });
  filter.info.description = "Lowpass filter to smooth out rough edges.";

  auto& trig_section = result.sections.emplace_back(make_param_section(section_trigger,
    make_topo_tag_basic("{2764871C-8E30-4780-B804-9E0FDE1A63EE}", "Trigger"),
    make_param_section_gui({ 0, 3, 2, 1 }, { { 1, 1 }, { gui_dimension::auto_size_all, 1 } }, gui_label_edit_cell_split::horizontal)));
  trig_section.gui.merge_with_section = section_sync;
  auto& trigger = result.params.emplace_back(make_param(
    make_topo_info_basic("{84B6DC4D-D2FF-42B0-992D-49B561C46013}", "Trigger", param_trigger, 1),
    make_param_dsp_voice(param_automate::automate), make_domain_item(trigger_items(), ""),
    make_param_gui_single(section_trigger, gui_edit_type::list, { 0, 0 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  trigger.gui.bindings.enabled.bind_params({ param_on }, [](auto const& vs) { return vs[0] != 0; });
  trigger.gui.bindings.global_enabled.bind_param(module_voice_in, voice_in_param_mode, [](int v) { return v != engine_voice_mode_poly; });
  trigger.info.description = std::string("Selects trigger mode for monophonic mode.<br/>") +
    "Legato - envelope will not reset.<br/>" + 
    "Retrig - upon note-on event, envelope will start over from zero, may cause clicks.<br/>" +
    "Multi - upon note-on event, envelope will start over from the current level.<br/>" + 
    "To avoid clicks it is best to use release-monophonic mode with multi-triggered envelopes.";
  auto& sustain = result.params.emplace_back(make_param(
    make_topo_info("{E5AB2431-1953-40E4-AFD3-735DB31A4A06}", true, "Sustain", "Sustain", "Sustain", param_sustain, 1),
    make_param_dsp_accurate(param_automate::modulate), make_domain_percentage_identity(0.5, 0, true),
    make_param_gui_single(section_trigger, gui_edit_type::hslider, { 1, 0 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  sustain.gui.bindings.enabled.bind_params({ param_on }, [](auto const& vs) { return vs[0] != 0; });
  sustain.info.description = "Sustain level. Modulation takes place only at voice start.";

  auto& dahdr_section = result.sections.emplace_back(make_param_section(section_dahdr,
    make_topo_tag_basic("{96BDC7C2-7DF4-4CC5-88F9-2256975D70AC}", "DAHDR"),
    make_param_section_gui({ 0, 4, 2, 1 }, { { 1, 1 }, { 
      gui_dimension::auto_size_all, 1, gui_dimension::auto_size_all, 1, 
      gui_dimension::auto_size_all, 1, gui_dimension::auto_size_all, 1 } }, gui_label_edit_cell_split::horizontal)));
  dahdr_section.gui.bindings.visible.bind_params({ param_mode }, [](auto const& vs) { return vs[0] != mode_mseg; });
  auto& delay_time = result.params.emplace_back(make_param(
    make_topo_info("{E9EF839C-235D-4248-A4E1-FAD62089CC78}", true, "Dly Time", "Dly", "Dly Time", param_delay_time, 1),
    make_param_dsp_accurate(param_automate::modulate), make_domain_log(0, 10, 0, 1, 3, "Sec"),
    make_param_gui_single(section_dahdr, gui_edit_type::knob, { 0, 0 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  delay_time.gui.bindings.enabled.bind_params({ param_on, param_sync }, [](auto const& vs) { return vs[0] != 0 && vs[1] == 0; });
  delay_time.gui.bindings.visible.bind_params({ param_sync }, [](auto const& vs) { return vs[0] == 0; });
  delay_time.info.description = "Delay section length in seconds. Modulation takes place only at voice start.";
  auto& delay_tempo = result.params.emplace_back(make_param(
    make_topo_info("{A016A3B5-8BFC-4DCD-B41F-F69F3A239AFA}", true, "Dly Tempo", "Dly", "Dly Tempo", param_delay_tempo, 1),
    make_param_dsp_voice(param_automate::automate), make_domain_timesig_default(true, { 4, 1 }, { 0, 1 }),
    make_param_gui_single(section_dahdr, gui_edit_type::list, { 0, 0 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  delay_tempo.gui.submenu = make_timesig_submenu(delay_tempo.domain.timesigs);
  delay_tempo.gui.bindings.enabled.bind_params({ param_on, param_sync }, [](auto const& vs) { return vs[0] != 0 && vs[1] != 0; });
  delay_tempo.gui.bindings.visible.bind_params({ param_on, param_sync }, [](auto const& vs) { return vs[0] != 0 && vs[1] != 0; });
  delay_tempo.info.description = "Delay section length in bars.";
  auto& hold_time = result.params.emplace_back(make_param(
    make_topo_info("{66F6036E-E64A-422A-87E1-34E59BC93650}", true, "Hld Time", "Hld", "Hld Time", param_hold_time, 1),
    make_param_dsp_accurate(param_automate::modulate), make_domain_log(0, 10, 0, 1, 3, "Sec"),
    make_param_gui_single(section_dahdr, gui_edit_type::knob, { 1, 0 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  hold_time.gui.bindings.enabled.bind_params({ param_on, param_sync }, [](auto const& vs) { return vs[0] != 0 && vs[1] == 0; });
  hold_time.gui.bindings.visible.bind_params({ param_sync }, [](auto const& vs) { return vs[0] == 0; });
  hold_time.info.description = "Hold section length in seconds. Modulation takes place only at voice start.";
  auto& hold_tempo = result.params.emplace_back(make_param(
    make_topo_info("{97846CDB-7349-4DE9-8BDF-14EAD0586B28}", true, "Hld Tempo", "Hld", "Hld Tempo", param_hold_tempo, 1),
    make_param_dsp_voice(param_automate::automate), make_domain_timesig_default(true, { 4, 1 }, { 0, 1 }),
    make_param_gui_single(section_dahdr, gui_edit_type::list, { 1, 0 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  hold_tempo.gui.submenu = make_timesig_submenu(hold_tempo.domain.timesigs);
  hold_tempo.gui.bindings.enabled.bind_params({ param_on, param_sync }, [](auto const& vs) { return vs[0] != 0 && vs[1] != 0; });
  hold_tempo.gui.bindings.visible.bind_params({ param_on, param_sync }, [](auto const& vs) { return vs[0] != 0 && vs[1] != 0; });
  hold_tempo.info.description = "Hold section length in bars.";

  auto& attack_time = result.params.emplace_back(make_param(
    make_topo_info("{B1E6C162-07B6-4EE2-8EE1-EF5672FA86B4}", true, "Att Time", "Att", "Att Time", param_attack_time, 1),
    make_param_dsp_accurate(param_automate::modulate), make_domain_log(0, 10, 0.03, 1, 3, "Sec"),
    make_param_gui_single(section_dahdr, gui_edit_type::knob, { 0, 2 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  attack_time.gui.bindings.enabled.bind_params({ param_on, param_sync }, [](auto const& vs) { return vs[0] != 0 && vs[1] == 0; });
  attack_time.gui.bindings.visible.bind_params({ param_sync }, [](auto const& vs) { return vs[0] == 0; });
  attack_time.info.description = "Attack section length in seconds. Modulation takes place only at voice start.";
  auto& attack_tempo = result.params.emplace_back(make_param(
    make_topo_info("{3130A19C-AA2C-40C8-B586-F3A1E96ED8C6}", true, "Att Tempo", "Att", "Att Tempo", param_attack_tempo, 1),
    make_param_dsp_voice(param_automate::automate), make_domain_timesig_default(true, { 4, 1 }, { 1, 64 }),
    make_param_gui_single(section_dahdr, gui_edit_type::list, { 0, 2 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  attack_tempo.gui.submenu = make_timesig_submenu(attack_tempo.domain.timesigs);
  attack_tempo.gui.bindings.enabled.bind_params({ param_on, param_sync }, [](auto const& vs) { return vs[0] != 0 && vs[1] != 0; });
  attack_tempo.gui.bindings.visible.bind_params({ param_on, param_sync }, [](auto const& vs) { return vs[0] != 0 && vs[1] != 0; });
  attack_tempo.info.description = "Attack section length in bars.";
  auto& attack_slope = result.params.emplace_back(make_param(
    make_topo_info("{7C2DBB68-164D-45A7-9940-AB96F05D1777}", true, "Att Slope", "Slp", "Att Slope", param_attack_slope, 1),
    make_param_dsp_accurate(param_automate::modulate), make_domain_percentage_identity(0.5, 0, true),
    make_param_gui_single(section_dahdr, gui_edit_type::knob, { 1, 2 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  attack_slope.gui.bindings.enabled.bind_params({ param_on, param_mode }, [](auto const& vs) { return vs[0] != 0 && is_exp_slope(vs[1]); });
  attack_slope.info.description = "Controls attack slope for exponential types. Modulation takes place only at voice start.";

  auto& decay_time = result.params.emplace_back(make_param(
    make_topo_info("{45E37229-839F-4735-A31D-07DE9873DF04}", true, "Dcy Time", "Dcy", "Dcy Time", param_decay_time, 1),
    make_param_dsp_accurate(param_automate::modulate), make_domain_log(0, 10, 0.1, 1, 3, "Sec"),
    make_param_gui_single(section_dahdr, gui_edit_type::knob, { 0, 4 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  decay_time.gui.bindings.enabled.bind_params({ param_on, param_sync }, [](auto const& vs) { return vs[0] != 0 && vs[1] == 0; });
  decay_time.gui.bindings.visible.bind_params({ param_sync }, [](auto const& vs) { return vs[0] == 0; });
  decay_time.info.description = "Decay section length in seconds. Modulation takes place only at voice start.";
  auto& decay_tempo = result.params.emplace_back(make_param(
    make_topo_info("{47253C57-FBCA-4A49-AF88-88AC9F4781D7}", true, "Dcy Tempo", "Dcy", "Dcy Tempo", param_decay_tempo, 1),
    make_param_dsp_voice(param_automate::automate), make_domain_timesig_default(true, { 4, 1 }, { 1, 32 }),
    make_param_gui_single(section_dahdr, gui_edit_type::list, { 0, 4 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  decay_tempo.gui.submenu = make_timesig_submenu(decay_tempo.domain.timesigs);
  decay_tempo.gui.bindings.enabled.bind_params({ param_on, param_sync }, [](auto const& vs) { return vs[0] != 0 && vs[1] != 0; });
  decay_tempo.gui.bindings.visible.bind_params({ param_on, param_sync }, [](auto const& vs) { return vs[0] != 0 && vs[1] != 0; });
  decay_tempo.info.description = "Decay section length in bars.";
  auto& decay_slope = result.params.emplace_back(make_param(
    make_topo_info("{416C46E4-53E6-445E-8D21-1BA714E44EB9}", true, "Dcy Slope", "Slp", "Dcy Slope", param_decay_slope, 1),
    make_param_dsp_accurate(param_automate::modulate), make_domain_percentage_identity(0.5, 0, true),
    make_param_gui_single(section_dahdr, gui_edit_type::knob, { 1, 4 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  decay_slope.gui.bindings.enabled.bind_params({ param_on, param_mode }, [](auto const& vs) { return vs[0] != 0 && is_exp_slope(vs[1]); });
  decay_slope.info.description = "Controls decay slope for exponential types. Modulation takes place only at voice start.";

  auto& release_time = result.params.emplace_back(make_param(
    make_topo_info("{FFC3002C-C3C8-4C10-A86B-47416DF9B8B6}", true, "Rls Time", "Rls", "Rls Time", param_release_time, 1),
    make_param_dsp_accurate(param_automate::modulate), make_domain_log(0, 10, 0.2, 1, 3, "Sec"),
    make_param_gui_single(section_dahdr, gui_edit_type::knob, { 0, 6 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  release_time.gui.bindings.enabled.bind_params({ param_on, param_sync }, [](auto const& vs) { return vs[0] != 0 && vs[1] == 0; });
  release_time.gui.bindings.visible.bind_params({ param_sync }, [](auto const& vs) { return vs[0] == 0; });
  release_time.info.description = "Release section length in seconds. Modulation takes place only at voice start.";
  auto& release_tempo = result.params.emplace_back(make_param(
    make_topo_info("{FDC00AA5-8648-4064-BE77-1A9CDB6B53EE}", true, "Rls Tempo", "Rls", "Rls Tempo", param_release_tempo, 1),
    make_param_dsp_voice(param_automate::automate), make_domain_timesig_default(true, { 4, 1 }, {1, 16 }),
    make_param_gui_single(section_dahdr, gui_edit_type::list, { 0, 6 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  release_tempo.gui.submenu = make_timesig_submenu(release_tempo.domain.timesigs);
  release_tempo.gui.bindings.enabled.bind_params({ param_on, param_sync }, [](auto const& vs) { return vs[0] != 0 && vs[1] != 0; });
  release_tempo.gui.bindings.visible.bind_params({ param_on, param_sync }, [](auto const& vs) { return vs[0] != 0 && vs[1] != 0; });
  release_tempo.info.description = "Release section length in bars.";
  auto& release_slope = result.params.emplace_back(make_param(
    make_topo_info("{11113DB9-583A-48EE-A99F-6C7ABB693951}", true, "Rls Slope", "Slp", "Rls Slope", param_release_slope, 1),
    make_param_dsp_accurate(param_automate::modulate), make_domain_percentage_identity(0.5, 0, true),
    make_param_gui_single(section_dahdr, gui_edit_type::knob, { 1, 6 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  release_slope.gui.bindings.enabled.bind_params({ param_on, param_mode }, [](auto const& vs) { return vs[0] != 0 && is_exp_slope(vs[1]); });
  release_slope.info.description = "Controls release slope for exponential types. Modulation takes place only at voice start.";

  // todo disable the other controls for dahdsr
  // todo what can be automated/modulated
  auto& mseg_section = result.sections.emplace_back(make_param_section(section_mseg,
    make_topo_tag_basic("{62ECE061-2CF7-42BD-A7FE-9069306DB83F}", "MSEG"),
    make_param_section_gui({ 0, 4, 2, 1 }, { 1, 1 })));
  mseg_section.gui.bindings.visible.bind_params({ param_mode }, [](auto const& vs) { return vs[0] == mode_mseg; });
  mseg_section.gui.custom_gui_factory = [](plugin_gui* gui, lnf* lnf, int module_slot, component_store store) {
    return &store_component<mseg_editor>(store, gui, module_env, module_slot, param_mseg_start_y, param_mseg_end_y, param_mseg_on, param_mseg_x, param_mseg_y); };
  auto& mseg_start_y = result.params.emplace_back(make_param(
    make_topo_info("{BB1A9691-DA7D-460D-BDF3-7D99F272CD05}", true, "MSEG Start Y", "Start Y", "Start Y", param_mseg_start_y, 1),
    make_param_dsp_voice(param_automate::none), make_domain_linear(0.0, 1.0, 0.0, 2, ""),
    make_param_gui_none(section_mseg)));
  mseg_start_y.gui.bindings.enabled.bind_params({ param_on, param_mode }, [](auto const& vs) { return vs[0] != 0 && vs[1] == mode_mseg; });
  mseg_start_y.info.description = "TODO";
  auto& mseg_end_y = result.params.emplace_back(make_param(
    make_topo_info("{D4114CFC-B2E6-4927-9011-E49BF68995C4}", true, "MSEG End Y", "End Y", "End Y", param_mseg_end_y, 1),
    make_param_dsp_voice(param_automate::none), make_domain_linear(0.0, 1.0, 0.0, 2, ""),
    make_param_gui_none(section_mseg)));
  mseg_end_y.gui.bindings.enabled.bind_params({ param_on, param_mode }, [](auto const& vs) { return vs[0] != 0 && vs[1] == mode_mseg; });
  mseg_end_y.info.description = "TODO";
  auto& mseg_on = result.params.emplace_back(make_param(
    make_topo_info("{4E43B99B-D8A2-43A8-99FF-9A8E204CD99B}", true, "MSEG On", "On", "On", param_mseg_on, mseg_max_point_count),
    make_param_dsp_voice(param_automate::none), make_domain_toggle(false),
    make_param_gui_none(section_mseg)));
  mseg_on.gui.bindings.enabled.bind_params({ param_on, param_mode }, [](auto const& vs) { return vs[0] != 0 && vs[1] == mode_mseg; });
  mseg_on.info.description = "TODO";
  mseg_on.domain.default_selector_ = [](int, int s) {
    return s <= 1 ? "On" : "Off";
  };
  auto& mseg_x = result.params.emplace_back(make_param(
    make_topo_info("{CFF71CB5-C93F-44BE-AC42-1814D96B291A}", true, "MSEG X", "X", "X", param_mseg_x, mseg_max_point_count),
    make_param_dsp_voice(param_automate::none), make_domain_linear(0.0, 1.0, 0.0, 2, ""),
    make_param_gui_none(section_mseg)));
  mseg_x.gui.bindings.enabled.bind_params({ param_on, param_mode }, [](auto const& vs) { return vs[0] != 0 && vs[1] == mode_mseg; });
  mseg_x.info.description = "TODO";
  mseg_x.domain.default_selector_ = [](int, int s) {
    if (s == 0) return "0.25";
    if (s == 1) return "0.5";
    return "0.0";
  };
  auto& mseg_y = result.params.emplace_back(make_param(
    make_topo_info("{60F4F483-F5C9-486D-8D6B-E4098B08FDC4}", true, "MSEG Y", "Y", "Y", param_mseg_y, mseg_max_point_count),
    make_param_dsp_voice(param_automate::none), make_domain_linear(0.0, 1.0, 0.0, 2, ""),
    make_param_gui_none(section_mseg)));
  mseg_y.gui.bindings.enabled.bind_params({ param_on, param_mode }, [](auto const& vs) { return vs[0] != 0 && vs[1] == mode_mseg; });
  mseg_y.info.description = "TODO";
  mseg_y.domain.default_selector_ = [](int, int s) {
    if (s == 0) return "1.0";
    if (s == 1) return "0.5";
    return "0.0";
  };
  return result;
}

static inline double
calc_slope_lin(double slope_pos, double splt_bnd, double exp) 
{ return slope_pos; }
static inline double
calc_slope_exp_uni(double slope_pos, double splt_bnd, double exp)
{ return std::pow(slope_pos, exp); }
static inline double
calc_slope_exp_bi(double slope_pos, double splt_bnd, double exp)
{ return wave_skew_uni_xpb(slope_pos, exp); }

static inline double
calc_slope_exp_splt(double slope_pos, double splt_bnd, double exp)
{
  if (slope_pos < splt_bnd)
    return std::pow(slope_pos / splt_bnd, exp) * splt_bnd;
  return 1 - std::pow(1 - (slope_pos - splt_bnd) / (1 - splt_bnd), exp) * (1 - splt_bnd);
}

void
env_engine::reset_audio(
  plugin_block const* block,
  std::vector<note_event> const* in_notes,
  std::vector<note_event>* out_notes)
{
  _stage_pos = 0;
  _total_pos = 0;
  _current_level = 0;
  _multitrig_level = 0;
  _stage = env_stage::delay;
  _voice_initialized = false;

  auto const& block_auto = block->state.own_block_automation;
  float filter = block_auto[param_filter][0].real();
  _filter.init(block->sample_rate, filter / 1000.0f);
}

void 
env_engine::reset_graph(
  plugin_block const* block,
  std::vector<note_event> const* in_notes,
  std::vector<note_event>* out_notes,
  std::vector<mod_out_custom_state> const& custom_outputs, 
  void* context)
{
  reset_audio(block, nullptr, nullptr);

  _ffwd_stopped_at_samples = -1;
  _ffwd_target_total_pos = 0.0f;
  _is_ffwd_run_for_graph = false;

  bool new_ffwd_run_for_graph = false;
  float new_ffwd_target_total_pos = 0;

  bool seen_multitrig = false;
  bool is_render_for_cv_graph = false;
  bool seen_render_for_cv_graph = false;

  // see how much to forward, this seems too complicated to derive in any other way
  // backwards loop, outputs are sorted, latest-in-time are at the end
  if (custom_outputs.size())
    for (int i = (int)custom_outputs.size() - 1; i >= 0; i--)
    {
      if (custom_outputs[i].module_global == block->module_desc_.info.global)
      {
        if (!_is_ffwd_run_for_graph && custom_outputs[i].tag_custom == custom_tag_total_pos)
        {
          // microseconds, see below
          new_ffwd_run_for_graph = true;
          new_ffwd_target_total_pos = custom_outputs[i].value_custom_int() / 1000000.0f;
          break;
        }
        if (!seen_multitrig && custom_outputs[i].tag_custom == custom_tag_multitrig_level)
        {
          seen_multitrig = true;
          float trig_level = custom_outputs[i].value_custom_float();
          _current_level = trig_level;
          _multitrig_level = trig_level;
        }
      }
      if (!seen_render_for_cv_graph && custom_outputs[i].tag_custom == custom_out_shared_render_for_cv_graph)
      {
        is_render_for_cv_graph = true;
        seen_render_for_cv_graph = true;
      }
      if (_is_ffwd_run_for_graph && seen_multitrig && seen_render_for_cv_graph)
        break;
    }

  // dont want to do the moving image for the regular env plot, that one's got the moving mod indicator
  if (is_render_for_cv_graph)
  {
    _is_ffwd_run_for_graph = new_ffwd_run_for_graph;
    _ffwd_target_total_pos = new_ffwd_target_total_pos;
  }
}

void 
env_engine::process_graph(
  plugin_block& block,
  std::vector<note_event> const* in_notes,
  std::vector<note_event>* out_notes,
  std::vector<mod_out_custom_state> const& custom_outputs, 
  void* context)
{
  process_internal(block, static_cast<cv_cv_matrix_mixdown const*>(context));
  if (!_is_ffwd_run_for_graph || _ffwd_stopped_at_samples < 0) return;

  // shift graph image to the left
  _ffwd_stopped_at_samples = std::min(_ffwd_stopped_at_samples, block.end_frame - block.start_frame);
  for (int f = block.start_frame; f < block.end_frame - _ffwd_stopped_at_samples; f++)
    block.state.own_cv[0][0][f] = block.state.own_cv[0][0][f + _ffwd_stopped_at_samples];
  for (int f = block.end_frame - _ffwd_stopped_at_samples; f < block.end_frame; f++)
    block.state.own_cv[0][0][f] = 0.0f;
}

void
env_engine::init_slope_exp(double slope, double& exp)
{
  double slope_bounded = slope_min + slope_range * slope;
  exp = std::log(slope_bounded) / log_half;
}

void 
env_engine::init_slope_exp_splt(double slope, double split_pos, double& exp, double& splt_bnd)
{
  splt_bnd = slope_min + slope_range * split_pos;
  double slope_bounded = slope_min + slope_range * slope;
  if (slope_bounded < 0.5f) exp = std::log(slope_bounded) / log_half;
  else exp = std::log(1.0f - slope_bounded) / log_half;
}

void
env_engine::process_internal(plugin_block& block, cv_cv_matrix_mixdown const* modulation)
{
  // allow custom data for graphs
  if (modulation == nullptr)
  {
    cv_cv_matrix_mixer& mixer = get_cv_cv_matrix_mixer(block, false);
    modulation = &mixer.mix(block, module_env, block.module_slot);
  }

  auto const& block_auto = block.state.own_block_automation;
  bool on = block_auto[param_on][0].step() != 0;
  if (_stage == env_stage::end || (!on && block.module_slot != 0))
  {
    block.state.own_cv[0][0].fill(block.start_frame, block.end_frame, 0.0f);
    // CAUTION: microseconds
    if(_stage == env_stage::end && !block.graph)
      block.push_modulation_output(modulation_output::make_mod_output_custom_state_int(
        block.voice->state.slot,
        block.module_desc_.info.global,
        custom_tag_total_pos,
        (int)(_total_pos * 1000000)));
    return;
  }

  if(block.state.all_block_automation[module_voice_in][0][voice_in_param_mode][0].step() == engine_voice_mode_poly)
    process_mono<false>(block, modulation);
  else
    process_mono<true>(block, modulation);

  // only want the mod outputs for the actual audio engine
  if (block.graph) return;

  // need to push the total pos for graph (for the moving cv matrix signal)
  // since sample rates are different for audio and GUI, need a value in time not frames
  // reset_graph+render_graph then need to derive the env state by just truncating the entire thing to that position
  // CAUTION: microseconds. cannot go with maxint because total_pos is not in [0, 1] (env can be > 1 second)
  block.push_modulation_output(modulation_output::make_mod_output_custom_state_int(
    block.voice->state.slot,
    block.module_desc_.info.global,
    custom_tag_total_pos,
    (int)(_total_pos * 1000000)));
  if(block_auto[param_trigger][0].step() == trigger_multi)
    block.push_modulation_output(modulation_output::make_mod_output_custom_state_float(
      block.voice->state.slot,
      block.module_desc_.info.global,
      custom_tag_multitrig_level,
      _multitrig_level));

  // drop the mod indicators when we ended
  if (_stage == env_stage::end) return;
  float flt = block.state.own_block_automation[param_filter][0].real() / 1000.0f;
  float normalized_pos = std::clamp(_total_pos / (_dly + _att + _hld + _dcy + _rls + flt), 0.0, 1.0);
  block.push_modulation_output(modulation_output::make_mod_output_cv_state(
    block.voice->state.slot,
    block.module_desc_.info.global,
    normalized_pos));
}

template <bool Monophonic>
void env_engine::process_mono(plugin_block& block, cv_cv_matrix_mixdown const* modulation)
{
  switch (block.state.own_block_automation[param_type][0].step())
  {
  case type_sustain: process_mono_type<Monophonic, type_sustain>(block, modulation); break;
  case type_follow: process_mono_type<Monophonic, type_follow>(block, modulation); break;
  case type_release: process_mono_type<Monophonic, type_release>(block, modulation); break;
  default: assert(false); break;
  }
}

template <bool Monophonic, int Type>
void env_engine::process_mono_type(plugin_block& block, cv_cv_matrix_mixdown const* modulation)
{
  bool sync = block.state.own_block_automation[param_sync][0].step() != 0;
  if (sync) process_mono_type_sync<Monophonic, Type, true>(block, modulation);
  else process_mono_type_sync<Monophonic, Type, false>(block, modulation);
}

template <bool Monophonic, int Type, bool Sync>
void env_engine::process_mono_type_sync(plugin_block& block, cv_cv_matrix_mixdown const* modulation)
{
  int trigger = block.state.own_block_automation[param_trigger][0].step();
  if constexpr(!Monophonic) trigger = trigger_legato;
  switch (trigger)
  {
  case trigger_legato: process_mono_type_sync_trigger<Monophonic, Type, Sync, trigger_legato>(block, modulation); break;
  case trigger_retrig: process_mono_type_sync_trigger<Monophonic, Type, Sync, trigger_retrig>(block, modulation); break;
  case trigger_multi: process_mono_type_sync_trigger<Monophonic, Type, Sync, trigger_multi>(block, modulation); break;
  default: assert(false); break;
  }
}

template <bool Monophonic, int Type, bool Sync, int Trigger>
void env_engine::process_mono_type_sync_trigger(plugin_block& block, cv_cv_matrix_mixdown const* modulation)
{
  switch (block.state.own_block_automation[param_mode][0].step())
  {
  case mode_linear: process_mono_type_sync_trigger_mode<Monophonic, Type, Sync, Trigger, mode_linear>(block, modulation, calc_slope_lin); break;
  case mode_exp_bi: process_mono_type_sync_trigger_mode<Monophonic, Type, Sync, Trigger, mode_exp_bi>(block, modulation, calc_slope_exp_bi); break;
  case mode_exp_uni: process_mono_type_sync_trigger_mode<Monophonic, Type, Sync, Trigger, mode_exp_uni>(block, modulation, calc_slope_exp_uni); break;
  case mode_exp_split: process_mono_type_sync_trigger_mode<Monophonic, Type, Sync, Trigger, mode_exp_split>(block, modulation, calc_slope_exp_splt); break;
  case mode_mseg: break; // TODO
  default: assert(false); break;
  }
}

template <bool Monophonic, int Type, bool Sync, int Trigger, int Mode, class CalcSlope>
void env_engine::process_mono_type_sync_trigger_mode(plugin_block& block, cv_cv_matrix_mixdown const* modulation, CalcSlope calc_slope)
{
  auto const& block_auto = block.state.own_block_automation;

  // we cannot pick up the voice start values on ::reset
  // because we need access to modulation
  if(!_voice_initialized)
  {
    _voice_initialized = true;

    // These are not really continuous (we only pick them up at voice start)
    // but we fake it this way so they can participate in modulation.
    _stn = (*(*modulation)[param_sustain][0])[block.start_frame];
    _hld = block.normalized_to_raw_fast<domain_type::log>(module_env, param_hold_time, (*(*modulation)[param_hold_time][0])[block.start_frame]);
    _dcy = block.normalized_to_raw_fast<domain_type::log>(module_env, param_decay_time, (*(*modulation)[param_decay_time][0])[block.start_frame]);
    _dly = block.normalized_to_raw_fast<domain_type::log>(module_env, param_delay_time, (*(*modulation)[param_delay_time][0])[block.start_frame]);
    _att = block.normalized_to_raw_fast<domain_type::log>(module_env, param_attack_time, (*(*modulation)[param_attack_time][0])[block.start_frame]);
    _rls = block.normalized_to_raw_fast<domain_type::log>(module_env, param_release_time, (*(*modulation)[param_release_time][0])[block.start_frame]);

    if constexpr (Sync)
    {
      auto const& params = block.plugin_desc_.plugin->modules[module_env].params;
      _hld = timesig_to_time(block.host.bpm, params[param_hold_tempo].domain.timesigs[block_auto[param_hold_tempo][0].step()]);
      _dcy = timesig_to_time(block.host.bpm, params[param_decay_tempo].domain.timesigs[block_auto[param_decay_tempo][0].step()]);
      _dly = timesig_to_time(block.host.bpm, params[param_delay_tempo].domain.timesigs[block_auto[param_delay_tempo][0].step()]);
      _att = timesig_to_time(block.host.bpm, params[param_attack_tempo].domain.timesigs[block_auto[param_attack_tempo][0].step()]);
      _rls = timesig_to_time(block.host.bpm, params[param_release_tempo].domain.timesigs[block_auto[param_release_tempo][0].step()]);
    }

    // global unison - be sure not to scale envelope length to 0%
    if (block.voice->state.sub_voice_count > 1)
    {
      float const max_scale = 0.95f;
      float glob_uni_env_dtn = block.state.all_block_automation[module_voice_in][0][voice_in_param_uni_env_dtn][0].real();
      float voice_pos = unipolar_to_bipolar((float)block.voice->state.sub_voice_index / (block.voice->state.sub_voice_count - 1.0f));
      float scale_length = 1 + (voice_pos * glob_uni_env_dtn * max_scale);
      _hld *= scale_length;
      _dcy *= scale_length;
      _dly *= scale_length;
      _att *= scale_length;
      _rls *= scale_length;
    }

    if constexpr (is_exp_slope(Mode))
    {
      // These are also not really continuous (we only pick them up at voice start)
      // but we fake it this way so they can participate in modulation.
      float ds = (*(*modulation)[param_decay_slope][0])[block.start_frame];
      float as = (*(*modulation)[param_attack_slope][0])[block.start_frame];
      float rs = (*(*modulation)[param_release_slope][0])[block.start_frame];

      if constexpr(Mode == mode_exp_uni || Mode == mode_exp_bi)
      {
        init_slope_exp(ds, _slp_dcy_exp);
        init_slope_exp(rs, _slp_rls_exp);
        init_slope_exp(as, _slp_att_exp);
      }
      else if constexpr (Mode == mode_exp_split)
      {
        init_slope_exp_splt(ds, ds, _slp_dcy_exp, _slp_dcy_splt_bnd);
        init_slope_exp_splt(rs, rs, _slp_rls_exp, _slp_rls_splt_bnd);
        init_slope_exp_splt(as, 1 - as, _slp_att_exp, _slp_att_splt_bnd);
      }
      else 
        assert(false);
    }
  }

  for (int f = block.start_frame; f < block.end_frame; f++)
  {
    if (_stage == env_stage::end)
    {
      _current_level = 0;
      _multitrig_level = 0;
      block.state.own_cv[0][0][f] = 0;
      continue;
    }

    // need some time for the filter to fade out otherwise we'll pop on fast releases
    if (_stage == env_stage::filter)
    {
      _current_level = 0;
      _multitrig_level = 0;
      _stage_pos += 1.0 / block.sample_rate;
      
      _total_pos += 1.0 / block.sample_rate;
      if (_is_ffwd_run_for_graph)
        if (_total_pos >= _ffwd_target_total_pos && _ffwd_stopped_at_samples == -1)
          _ffwd_stopped_at_samples = _total_pos * block.sample_rate;

      float level = _filter.next(0);
      block.state.own_cv[0][0][f] = level;
      if (level < 1e-5)
      {
        _stage = env_stage::end;
        block.voice->finished |= block.module_slot == 0;
      }
      continue;
    }

    // see if we need to re/multitrigger
    if constexpr (Monophonic)
    {
      // be sure not to revive a released voice
      // otherwise we'll just keep building up voices in mono mode
      // plugin_base makes sure to only send the release signal after
      // the last note in a monophonic section
      if constexpr (Trigger != trigger_legato)
      {
        if(block.state.mono_note_stream[f].event_type == mono_note_stream_event::on)
        {
          if(_stage < env_stage::release)
          {
            _stage_pos = 0;
            _total_pos = 0;
            _stage = env_stage::delay;
            if constexpr (Trigger == trigger_retrig)
            {
              _current_level = 0;
              _multitrig_level = 0;
            }
            else
            {
              // multitrigger
              _current_level = _multitrig_level;
            }
          }
        }
      }
    }

    if (block.voice->state.release_frame == f && 
      (Type == type_sustain || (Type == type_release && _stage != env_stage::release)))
    {
      _stage_pos = 0;
      _stage = env_stage::release;
    }

    if (_stage == env_stage::sustain && Type == type_sustain)
    {
      _current_level = _stn;
      _multitrig_level = _stn;
      block.state.own_cv[0][0][f] = _filter.next(_stn);
      continue;
    }

    double stage_seconds = 0;
    switch (_stage)
    {
    case env_stage::hold: stage_seconds = _hld; break;
    case env_stage::decay: stage_seconds = _dcy; break;
    case env_stage::delay: stage_seconds = _dly; break;
    case env_stage::attack: stage_seconds = _att; break;
    case env_stage::release: stage_seconds = _rls; break;
    default: assert(false); break;
    }

    float out = 0;
    _stage_pos = std::min(_stage_pos, stage_seconds);
    double slope_pos = _stage_pos / stage_seconds;
    if (stage_seconds == 0) out = _current_level;
    else switch (_stage)
    {
    case env_stage::delay: 
      if constexpr(Trigger == trigger_multi)
        out = _current_level = _multitrig_level;
      else
        _current_level = _multitrig_level = out = 0;
      break;
    case env_stage::attack: 
      if constexpr (Trigger == trigger_multi)
        out = _current_level = _multitrig_level + (1 - _multitrig_level) * calc_slope(slope_pos, _slp_att_splt_bnd, _slp_att_exp);
      else
        _current_level = _multitrig_level = out = calc_slope(slope_pos, _slp_att_splt_bnd, _slp_att_exp); 
      break;
    case env_stage::hold: _current_level = _multitrig_level = out = 1; break;
    case env_stage::release: out = _current_level * (1 - calc_slope(slope_pos, _slp_rls_splt_bnd, _slp_rls_exp)); break;
    case env_stage::decay: _current_level = _multitrig_level = out = _stn + (1 - _stn) * (1 - calc_slope(slope_pos, _slp_dcy_splt_bnd, _slp_dcy_exp)); break;
    default: assert(false); stage_seconds = 0; break;
    }

    check_unipolar(out);
    check_unipolar(_current_level);
    check_unipolar(_multitrig_level);
    block.state.own_cv[0][0][f] = _filter.next(out);
    _stage_pos += 1.0 / block.sample_rate;
    
    _total_pos += 1.0 / block.sample_rate;
    if (_is_ffwd_run_for_graph)
      if (_total_pos >= _ffwd_target_total_pos && _ffwd_stopped_at_samples == -1)
        _ffwd_stopped_at_samples = _total_pos * block.sample_rate;

    if (_stage_pos < stage_seconds) continue;

    _stage_pos = 0;
    switch (_stage)
    {
    case env_stage::hold: _stage = env_stage::decay; break;
    case env_stage::attack: _stage = env_stage::hold; break;
    case env_stage::delay: _stage = env_stage::attack; break;
    case env_stage::release: _stage = env_stage::filter; break;
    case env_stage::decay: _stage = Type == type_sustain ? env_stage::sustain : env_stage::release; break;
    default: assert(false); break;
    }
  }
}

}
