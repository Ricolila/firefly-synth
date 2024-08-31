#include <firefly_synth/synth.hpp>
#include <firefly_synth/plugin.hpp>

#include <plugin_base/dsp/engine.hpp>
#include <plugin_base/topo/support.hpp>
#include <plugin_base/gui/graph.hpp>
#include <plugin_base/gui/controls.hpp>
#include <plugin_base/gui/containers.hpp>
#include <plugin_base/shared/logger.hpp>

using namespace juce;
using namespace plugin_base;

namespace firefly_synth {

static int const synth_first_column_size = 42;
static int const synth_second_column_size = 17;
  
static std::string const main_graph_name = "Main Graph";
static std::string const vfx_graph_name = "Voice FX Graph";
static std::string const env_graph_name = "Envelope Graph";
static std::string const gfx_graph_name = "Global FX Graph";
static std::string const osc_graph_name = "Oscillator Graph";
static std::string const vlfo_graph_name = "Voice LFO Graph";
static std::string const glfo_graph_name = "Global LFO Graph"; 
static std::string const voice_matrix_graphs_name = "Voice Matrix Graphs";
static std::string const global_matrix_graphs_name = "Global Matrix Graphs";

enum { 
  custom_section_title,
  custom_section_main_graph,
  custom_section_patch_controls,
  custom_section_edit_controls, 
  custom_section_theme_select,
  custom_section_gfx_graph,
  custom_section_glfo_graph,
  custom_section_global_matrix_graphs,
  custom_section_fx_count,
  custom_section_voice_matrix_graphs = custom_section_fx_count,
  custom_section_osc_graph,
  custom_section_vfx_graph,
  custom_section_vlfo_graph,
  custom_section_env_graph,
  custom_section_synth_count };

enum { 
  module_section_hidden, 
  module_section_master_settings,
  module_section_global_in,
  module_section_global_out,
  module_section_gfx, 
  module_section_glfo,
  module_section_monitor, 
  module_section_global_matrices,
  module_section_fx_count,
  module_section_voice_matrices = module_section_fx_count,
  module_section_voice_in,
  module_section_voice_out,
  module_section_osc, 
  module_section_vfx, 
  module_section_vlfo, 
  module_section_env, 
  module_section_synth_count };

static gui_dimension
make_plugin_dimension(bool is_fx, plugin_topo_gui_theme_settings const& settings)
{    
  gui_dimension result;  
  result.column_sizes = { synth_first_column_size, synth_second_column_size, 127, 34, 64, 64 };
  int height = settings.get_default_width(is_fx) * settings.get_aspect_ratio_height(is_fx) / settings.get_aspect_ratio_width(is_fx);
  std::vector<gui_vertical_section_size> section_vsizes = { { true, 2 }, { true, 0.75f }, { true, 2.25f }, { true, 4 }, { true, 4 } };
  if (!is_fx) section_vsizes.insert(section_vsizes.end(), { { true, 4 }, { true, 4 }, { true, 4 }, { true, 4 }, { true, 4 } });
  result.row_sizes = gui_vertical_distribution(height, settings.get_font_height(), section_vsizes);
  return result;
}

static module_graph_params
make_module_graph_params(int module, bool render_on_module_mouse_enter, 
  bool render_on_param_mouse_enter, bool render_on_modulation_output_change,
  std::vector<int> const& dependent_module_indices)
{
  module_graph_params result;
  result.fps = 10;
  result.module_index = module;
  result.render_on_tweak = true;
  result.render_on_tab_change = true;
  result.dependent_module_indices = dependent_module_indices;
  result.render_on_module_mouse_enter = render_on_module_mouse_enter;
  result.render_on_modulation_output_change = render_on_modulation_output_change;
  if(render_on_param_mouse_enter)
    result.render_on_param_mouse_enter_modules = { -1 };
  return result;
}

static Component&
make_module_graph_section(
  plugin_gui* gui, lnf* lnf, component_store store, 
  int module, std::string const& name_in_theme, 
  bool render_on_module_mouse_enter, bool render_on_param_mouse_enter, bool render_on_modulation_output_change,
  std::vector<int> const& dependent_module_indices, float partition_scale = 0.15f)
{
  graph_params params;
  params.name_in_theme = name_in_theme;
  params.partition_scale = partition_scale;
  params.scale_type = graph_params::scale_w;
  module_graph_params module_params = make_module_graph_params(module, 
    render_on_module_mouse_enter, render_on_param_mouse_enter, render_on_modulation_output_change, dependent_module_indices);
  return store_component<module_graph>(store, gui, lnf, params, module_params);
}

static Component&
make_main_graph_section(plugin_gui* gui, lnf* lnf, component_store store)
{
  graph_params params;
  params.partition_scale = 0.125f;
  params.name_in_theme = main_graph_name;
  params.scale_type = graph_params::scale_w;
  module_graph_params module_params;
  module_params.fps = 10;
  module_params.module_index = -1;
  module_params.render_on_tweak = true;
  module_params.render_on_tab_change = false;
  module_params.render_on_module_mouse_enter = true;
  module_params.render_on_modulation_output_change = true;
  module_params.render_on_param_mouse_enter_modules = {
    module_gcv_cv_matrix, module_global_in, module_master_settings, module_vcv_cv_matrix, module_voice_in, module_osc_osc_matrix,
    module_vaudio_audio_matrix, module_gaudio_audio_matrix, module_vcv_audio_matrix, module_gcv_audio_matrix };
  return store_component<module_graph>(store, gui, lnf, params, module_params);
}

static Component&
make_matrix_graphs_section(
  plugin_gui* gui, lnf* lnf, component_store store, 
  bool is_fx, int module_section, std::string const& name_in_theme)
{
  return store_component<tabbed_module_section_container>(store, gui, module_section,
    [gui, is_fx, lnf, module_section, name_in_theme](int module_index) -> std::unique_ptr<juce::Component> {
      graph_params params;
      params.name_in_theme = name_in_theme;
      params.scale_type = graph_params::scale_h;
      params.partition_scale = 0.33f;

      if (is_fx)
      {
        assert(module_section == module_section_global_matrices);
        return std::make_unique<module_graph>(gui, lnf, params, make_module_graph_params(module_index, false, true, true,
          { module_global_in, module_glfo, module_gfx, module_gaudio_audio_matrix, module_global_out, module_gcv_audio_matrix }));
      }

      switch (module_index)
      {
      case module_vaudio_audio_matrix: return std::make_unique<module_graph>(gui, lnf, params, make_module_graph_params(module_index, false, true, false, { }));
      case module_gaudio_audio_matrix: return std::make_unique<module_graph>(gui, lnf, params, make_module_graph_params(module_index, false, true, false, { }));
      case module_osc_osc_matrix: return std::make_unique<module_graph>(gui, lnf, params, make_module_graph_params(module_index, true, false, false,
        { module_osc, module_voice_in }));
      case module_vcv_audio_matrix: return std::make_unique<module_graph>(gui, lnf, params, make_module_graph_params(module_index, false, true, true,
        { module_global_in, module_glfo, module_vlfo, module_env, module_voice_in, module_voice_out,
        module_osc, module_osc_osc_matrix, module_vfx, module_vaudio_audio_matrix }));
      case module_gcv_audio_matrix: return std::make_unique<module_graph>(gui, lnf, params, make_module_graph_params(module_index, false, true, true,
        { module_global_in, module_glfo, module_gfx, module_gaudio_audio_matrix, module_global_out }));
      case module_vcv_cv_matrix: return std::make_unique<module_graph>(gui, lnf, params, make_module_graph_params(module_index, false, true, true,
        { module_global_in, module_glfo, module_gcv_audio_matrix, module_vlfo, module_env, module_vcv_audio_matrix }));
      case module_gcv_cv_matrix: return std::make_unique<module_graph>(gui, lnf, params, make_module_graph_params(module_index, false, true, true,
        { module_global_in, module_glfo, module_gcv_audio_matrix }));
      default: assert(false); return std::make_unique<Component>();
      }
    });
}

static Component&
make_patch_controls_section(plugin_gui* gui, lnf* lnf, component_store store)
{
  auto colors = lnf->section_gui_colors("Patch");
  auto& result = store_component<grid_component>(store, gui_dimension{ { 1, 1, }, { gui_dimension::auto_size_all, 1, 1 } }, 2, 2, 0, 0);
  auto& patch_label = store_component<autofit_label>(store, lnf, "Patch");
  patch_label.setText("Patch", juce::dontSendNotification);
  patch_label.setJustificationType(Justification::centredLeft);
  patch_label.setColour(Label::ColourIds::textColourId, colors.label_text);
  result.add(patch_label, { 0, 0, 2, 1 });
  result.add(gui->make_load_button(), { 0, 1 });
  result.add(gui->make_save_button(), { 0, 2 });
  result.add(gui->make_init_button(), { 1, 1 });
  result.add(gui->make_clear_button(), { 1, 2 });
  return result;
}

static Component&
make_theme_select_section(plugin_gui* gui, lnf* lnf, component_store store)
{
  auto colors = lnf->section_gui_colors("Theme");
  auto& result = store_component<grid_component>(store, gui_dimension{ { 1 }, { gui_dimension::auto_size, 1 } }, 2, 2, 0, 0);
  auto& theme_label = store_component<autofit_label>(store, lnf, "Theme");
  theme_label.setText("Theme", juce::dontSendNotification);
  theme_label.setJustificationType(Justification::centredLeft);
  theme_label.setColour(Label::ColourIds::textColourId, colors.label_text);
  result.add(theme_label, { 0, 0 });
  result.add(store_component<theme_combo>(store, gui, lnf), { 0, 1, 1, 1 });
  return result;
}

static Component&
make_edit_controls_section(plugin_gui* gui, lnf* lnf, component_store store)
{
  auto colors = lnf->section_gui_colors("Tweak");
  auto& result = store_component<grid_component>(store, gui_dimension{ 2, 4 }, 2, 2, 0, 0);
  auto& tweak_name_label = store_component<juce::Label>(store);
  tweak_name_label.setText("Tweak", juce::dontSendNotification);
  tweak_name_label.setJustificationType(Justification::centredLeft);
  tweak_name_label.setColour(Label::ColourIds::textColourId, colors.control_text);
  result.add(tweak_name_label, { 0, 0 });
  auto& tweak_value_label = store_component<juce::Label>(store);
  tweak_value_label.setText("Value", juce::dontSendNotification);
  tweak_value_label.setJustificationType(Justification::centredLeft);
  tweak_value_label.setColour(Label::ColourIds::textColourId, colors.control_text);
  result.add(tweak_value_label, { 1, 0 });
  auto& tweak = store_component<last_tweaked_label>(store, gui, lnf);
  result.add(tweak, { 0, 1, 1, 3 });
  result.add(store_component<last_tweaked_editor>(store, gui->automation_state(), lnf), { 1, 1, 1, 3 });
  return result;
} 

static Component&
make_title_section(plugin_gui* gui, lnf* lnf, component_store store, bool is_fx)
{
  auto colors = lnf->section_gui_colors("Title Text");
  std::string name = is_fx? FF_SYNTH_FX_NAME: FF_SYNTH_INST_NAME;
  for(int i = 0; i < name.size(); i++) name[i] = std::toupper(name[i]); 
  auto& grid = store_component<grid_component>(store, gui_dimension({ { 2, 1 }, { synth_first_column_size, synth_second_column_size } }), 2, 2, 0, 1);
  auto& title_label = store_component<autofit_label>(store, lnf, name, true, 15);
  title_label.setColour(Label::ColourIds::textColourId, colors.control_text);
  title_label.setJustificationType(Justification::left);
  grid.add(title_label, { 0, 0, 1, 1 });
  std::string version_text = std::string(FF_SYNTH_VERSION_TEXT) + " " + gui->automation_state()->desc().plugin->config->format_name() + " ";
#ifdef __aarch64__
  version_text += "ARM";
#else
  version_text += "X64"; 
#endif
  auto& version_label = store_component<autofit_label>(store, lnf, version_text, false, 10);
  version_label.setJustificationType(Justification::right);
  version_label.setColour(Label::ColourIds::textColourId, colors.control_text);
  grid.add(version_label, { 1, 0, 1, 1 });
  grid.add(store_component<image_component>(
    store, gui->automation_state()->desc().plugin->config,
    lnf->theme(), "header.png", RectanglePlacement::xRight), { 0, 1, 2, 1 });
  return grid;
}

std::unique_ptr<module_tab_menu_handler>
make_audio_routing_menu_handler(plugin_state* state, bool global)
{
  auto cv_params = make_audio_routing_cv_params(state, global);
  auto audio_params = make_audio_routing_audio_params(state, global);
  return std::make_unique<audio_routing_menu_handler>(state, cv_params, std::vector({ audio_params }));
}

cv_audio_matrix_mixdown
make_static_cv_matrix_mixdown(plugin_block& block)
{
  cv_audio_matrix_mixdown result;
  auto const& topo = *block.plugin_desc_.plugin;
  plugin_dims dims(topo, topo.audio_polyphony);
  result.resize(dims.module_slot_param_slot);
  for (int m = 0; m < topo.modules.size(); m++)
  {
    auto const& module = topo.modules[m];
    for (int mi = 0; mi < module.info.slot_count; mi++)
      for (int p = 0; p < module.params.size(); p++)
      {
        auto const& param = module.params[p];
        if (param.dsp.can_modulate(mi))
          for (int pi = 0; pi < param.info.slot_count; pi++)
            result[m][mi][p][pi] = &block.state.all_accurate_automation[m][mi][p][pi];
      }
  }
  return result;
}

std::vector<module_topo const*>
make_audio_audio_matrix_sources(plugin_topo const* topo, bool global)
{
  bool is_fx = topo->type == plugin_type::fx;
  if (!global) return { &topo->modules[module_osc], &topo->modules[module_vfx] };
  if (!is_fx) return { &topo->modules[module_voice_mix], &topo->modules[module_gfx] };
  return { &topo->modules[module_external_audio], &topo->modules[module_gfx] };
}

std::vector<module_topo const*>
make_audio_audio_matrix_targets(plugin_topo const* topo, bool global)
{
  if (global) return { &topo->modules[module_gfx], &topo->modules[module_global_out] };
  else return { &topo->modules[module_vfx], &topo->modules[module_voice_out] };
}

std::vector<module_topo const*>
make_cv_cv_matrix_targets(plugin_topo const* topo, bool global)
{
  if (global)
    return {
      &topo->modules[module_glfo],
      &topo->modules[module_gcv_audio_matrix] };
  else
    return {
      &topo->modules[module_env],
      &topo->modules[module_vlfo],
      &topo->modules[module_vcv_audio_matrix] };
}

std::vector<module_topo const*>
make_cv_audio_matrix_targets(plugin_topo const* topo, bool global)
{
  if (global)
    return {
      &topo->modules[module_gfx], 
      &topo->modules[module_gaudio_audio_matrix],
      &topo->modules[module_global_out] };
  else
    return { 
      &topo->modules[module_voice_in],
      &topo->modules[module_osc], 
      &topo->modules[module_osc_osc_matrix],
      &topo->modules[module_vfx],
      &topo->modules[module_vaudio_audio_matrix],
      &topo->modules[module_voice_out] };
}

std::vector<cv_source_entry>
make_cv_matrix_sources(plugin_topo const* topo, bool global)
{
  if(global)
    return { 
      { "", &topo->modules[module_global_in]},
      { "", &topo->modules[module_glfo] },
      { "", &topo->modules[module_midi] } };
  else
    return { 
      { "Global", nullptr }, 
      { "", &topo->modules[module_global_in]},
      { "", &topo->modules[module_glfo] },
      { "", &topo->modules[module_midi] },
      { "Voice", nullptr }, 
      { "", &topo->modules[module_env] },
      { "", &topo->modules[module_vlfo] },
      { "", &topo->modules[module_voice_note] },
      { "", &topo->modules[module_voice_on_note] }};
}

std::unique_ptr<plugin_topo>
synth_topo(format_basic_config const* config, bool is_fx, std::string const& full_name)
{
  PB_LOG_FUNC_ENTRY_EXIT();   
  auto result = std::make_unique<plugin_topo>();

  result->config = config;
  result->extension = "ffpreset";
  result->vendor = "Sjoerd van Kreel";
  result->full_name = full_name;
  result->version.major = FF_SYNTH_VERSION_MAJOR;
  result->version.minor = FF_SYNTH_VERSION_MINOR;
  result->version.patch = FF_SYNTH_VERSION_PATCH;
  result->engine.voice_mode.module_index = module_voice_in;
  result->engine.voice_mode.param_index = voice_in_param_mode;
  result->engine.bpm_smoothing.module_index = module_master_settings;
  result->engine.bpm_smoothing.param_index = master_settings_param_tempo_smooth;
  result->engine.midi_smoothing.module_index = module_master_settings;
  result->engine.midi_smoothing.param_index = master_settings_param_midi_smooth;
  result->engine.automation_smoothing.module_index = module_master_settings;
  result->engine.automation_smoothing.param_index = master_settings_param_auto_smooth;
  result->engine.tuning_mode.module_index = is_fx? -1: module_master_settings;
  result->engine.tuning_mode.param_index = is_fx ? -1 : master_settings_param_tuning_mode;

  // this is INCLUDING global unison!
  result->audio_polyphony = 64;
  result->graph_polyphony = 1;
  result->engine.sub_voice_counter = [](bool graph, plugin_state const& state)
  {
    // Global unison needs some help from plugin_base as we treat 
    // those voices just like regular polyphonic voices.
    if (graph) return 1;
    if(state.get_plain_at(module_voice_in, 0, voice_in_param_mode, 0).step() != engine_voice_mode_poly) return 1;
    return state.get_plain_at(module_voice_in, 0, voice_in_param_uni_voices, 0).step();
  };

  if(is_fx)
  {
    result->type = plugin_type::fx;
    result->tag = make_topo_tag_basic(FF_SYNTH_FX_ID, FF_SYNTH_FX_NAME);
  }
  else
  {
    result->type = plugin_type::synth; 
    result->tag = make_topo_tag_basic(FF_SYNTH_INST_ID, FF_SYNTH_INST_NAME);
  }
              
  result->gui.default_theme = "Firefly Hot";                                   
  result->gui.custom_sections.resize(is_fx? custom_section_fx_count: custom_section_synth_count);
  result->gui.dimension_factory = [is_fx](auto const& settings) { return make_plugin_dimension(is_fx, settings); };
  auto make_title_section_ui = [is_fx](plugin_gui* gui, lnf* lnf, auto store) -> Component& {
    return make_title_section(gui, lnf, store, is_fx); };
  result->gui.custom_sections[custom_section_title] = make_custom_section_gui(
    custom_section_title, "Title", { 0, 0, 1, 2 }, make_title_section_ui);
  result->gui.custom_sections[custom_section_patch_controls] = make_custom_section_gui(
    custom_section_patch_controls, "Patch", { 0, 5, 1, 1 }, 
      [](auto gui, auto lnf, auto store) -> juce::Component& { return make_patch_controls_section(gui, lnf, store); });
  result->gui.custom_sections[custom_section_theme_select] = make_custom_section_gui(
    custom_section_theme_select, "Theme", { 1, 5, 1, 1 },
    [](auto gui, auto lnf, auto store) -> juce::Component& { return make_theme_select_section(gui, lnf, store); });
  result->gui.custom_sections[custom_section_edit_controls] = make_custom_section_gui(
    custom_section_edit_controls, "Tweak", { 0, 4, 1, 1 }, 
      [](auto gui, auto lnf, auto store) -> juce::Component& { return make_edit_controls_section(gui, lnf, store); });
  result->gui.custom_sections[custom_section_main_graph] = make_custom_section_gui(
    custom_section_main_graph, main_graph_name, { 0, 3, 1, 1 }, [](auto* gui, auto* lnf, auto store)
    -> Component& { return make_main_graph_section(gui, lnf, store); });
  result->gui.custom_sections[custom_section_gfx_graph] = make_custom_section_gui(
    custom_section_gfx_graph, gfx_graph_name, { 3, 3, 1, 1 }, [](auto* gui, auto* lnf, auto store)
    -> Component& { return make_module_graph_section(gui, lnf, store, module_gfx, gfx_graph_name, false, false, false, {}); });
  result->gui.custom_sections[custom_section_glfo_graph] = make_custom_section_gui(
    custom_section_glfo_graph, glfo_graph_name, { 4, 3, 1, 1 }, [](auto* gui, auto* lnf, auto store)
    -> Component& { return make_module_graph_section(gui, lnf, store, module_glfo, glfo_graph_name, false, false, true, {}); });
  if (is_fx)
  {
    result->gui.custom_sections[custom_section_global_matrix_graphs] = make_custom_section_gui(
      custom_section_global_matrix_graphs, global_matrix_graphs_name, { 4, 4, 1, 2 }, [](auto* gui, auto* lnf, auto store)
      -> Component& { return make_matrix_graphs_section(gui, lnf, store, true, module_section_global_matrices, global_matrix_graphs_name); });
  }
  else
  {
    result->gui.custom_sections[custom_section_osc_graph] = make_custom_section_gui(
      custom_section_osc_graph, osc_graph_name, { 6, 3, 1, 1 }, [](auto* gui, auto* lnf, auto store)
      -> Component& { return make_module_graph_section(gui, lnf, store, module_osc, osc_graph_name, false, false, false, { module_osc_osc_matrix, module_voice_in }); });
    result->gui.custom_sections[custom_section_vfx_graph] = make_custom_section_gui(
      custom_section_vfx_graph, vfx_graph_name, { 7, 3, 1, 1 }, [](auto* gui, auto* lnf, auto store)
      -> Component& { return make_module_graph_section(gui, lnf, store, module_vfx, vfx_graph_name, false, false, false, {}); });
    result->gui.custom_sections[custom_section_vlfo_graph] = make_custom_section_gui(
      custom_section_vlfo_graph, vlfo_graph_name, { 8, 3, 1, 1 }, [](auto* gui, auto* lnf, auto store)
      -> Component& { return make_module_graph_section(gui, lnf, store, module_vlfo, vlfo_graph_name, false, false, true, {}); });
    result->gui.custom_sections[custom_section_env_graph] = make_custom_section_gui(
      custom_section_env_graph, env_graph_name, { 9, 3, 1, 1 }, [](auto* gui, auto* lnf, auto store)
      -> Component& { return make_module_graph_section(gui, lnf, store, module_env, env_graph_name, false, false, true, {}); });
    result->gui.custom_sections[custom_section_global_matrix_graphs] = make_custom_section_gui(
      custom_section_global_matrix_graphs, global_matrix_graphs_name, { 4, 4, 1, 2 }, [](auto* gui, auto* lnf, auto store)
      -> Component& { return make_matrix_graphs_section(gui, lnf, store, false, module_section_global_matrices, global_matrix_graphs_name); });

    result->gui.custom_sections[custom_section_voice_matrix_graphs] = make_custom_section_gui(
      custom_section_voice_matrix_graphs, voice_matrix_graphs_name, { 9, 4, 1, 2 }, [](auto* gui, auto* lnf, auto store)
      -> Component& { return make_matrix_graphs_section(gui, lnf, store, false, module_section_voice_matrices, voice_matrix_graphs_name); });
  }

  result->gui.module_sections.resize(is_fx? module_section_fx_count: module_section_synth_count);
  result->gui.module_sections[module_section_hidden] = make_module_section_gui_none(
    "{F289D07F-0A00-4AB1-B87B-685CB4D8B2F8}", module_section_hidden);
  result->gui.module_sections[module_section_glfo] = make_module_section_gui(
    "{96C75EE5-577E-4508-A85A-E92FF9FD8A4D}", module_section_glfo, { 4, 0, 1, 3 }, { 1, 1 });
  result->gui.module_sections[module_section_gfx] = make_module_section_gui(
    "{654B206B-27AE-4DFD-B885-772A8AD0A4F3}", module_section_gfx, { 3, 0, 1, 3 }, { 1, 1 });
  result->gui.module_sections[module_section_global_in] = make_module_section_gui(
    "{F9578AAA-66A4-4B0C-A941-4719B5F0E998}", module_section_global_in, { 2, 0, 1, 3 }, { 1, 1 });
  result->gui.module_sections[module_section_master_settings] = make_module_section_gui(
    "{D7ECBB86-2257-43DE-80FA-A648F648F715}", module_section_master_settings, { 1, 0, 1, 5 }, { 1, 1 });
  result->gui.module_sections[module_section_global_out] = make_module_section_gui(
    "{F77335AC-B701-40DA-B4C2-1F55DBCC29A4}", module_section_global_out, { 2, 3, 1, 1 }, { { 1 }, { 1 } });
  result->gui.module_sections[module_section_monitor] = make_module_section_gui(
    "{8FDAEB21-8876-4A90-A8E1-95A96FB98FD8}", module_section_monitor, { 0, 2, 1, 1 }, { { 1 }, { 1 } });

  if(is_fx) 
  {
    std::vector<int> fx_only_matrix_modules = { 
      module_gaudio_audio_matrix, module_gcv_audio_matrix, module_gcv_cv_matrix };
    result->gui.module_sections[module_section_global_matrices] = make_module_section_gui_tabbed(
      "{D450B51E-468E-457E-B954-FF1B9645CADB}", module_section_global_matrices, { 2, 4, 2, 2 }, fx_only_matrix_modules);
  } else 
  {
    result->gui.module_sections[module_section_vlfo] = make_module_section_gui(
      "{0DA0E7C3-8DBB-440E-8830-3B6087F23B81}", module_section_vlfo, { 8, 0, 1, 3 }, { 1, 1 });
    result->gui.module_sections[module_section_vfx] = make_module_section_gui(
      "{5BD5F320-3CA9-490B-B69E-36A003F41CEC}", module_section_vfx, { 7, 0, 1, 3 }, { 1, 1 });
    result->gui.module_sections[module_section_env] = make_module_section_gui(
      "{AB26F56E-DC6D-4F0B-845D-C750728F8FA2}", module_section_env, { 9, 0, 1, 3 }, { 1, 1 });
    result->gui.module_sections[module_section_osc] = make_module_section_gui(
      "{7A457CCC-E719-4C07-98B1-017EA7DEFB1F}", module_section_osc, { 6, 0, 1, 3 }, { { 1 }, { 1 } });
    result->gui.module_sections[module_section_voice_in] = make_module_section_gui(
      "{FB435C64-8349-4F0F-84FC-FFC82002D69F}", module_section_voice_in, { 5, 0, 1, 3 }, { { 1 }, { 1 } });
    result->gui.module_sections[module_section_voice_out] = make_module_section_gui(
      "{2B764ECA-B745-4087-BB73-1B5952BC6B96}", module_section_voice_out, { 5, 3, 1, 1 }, { { 1 }, { 1 } });

    std::vector<int> global_matrix_modules = { module_gaudio_audio_matrix, module_gcv_audio_matrix, module_gcv_cv_matrix };
    std::vector<int> voice_matrix_modules = { module_osc_osc_matrix, module_vaudio_audio_matrix, module_vcv_audio_matrix, module_vcv_cv_matrix };
    result->gui.module_sections[module_section_global_matrices] = make_module_section_gui_tabbed(
      "{628D5FCB-0672-47A5-A23D-31257D4CCAF1}", module_section_global_matrices, { 2, 4, 2, 2 }, global_matrix_modules);
    result->gui.module_sections[module_section_voice_matrices] = make_module_section_gui_tabbed(
      "{9BEB7D58-47AB-4E2D-891B-349607170508}", module_section_voice_matrices, { 5, 4, 4, 2 }, voice_matrix_modules);
  }

  result->modules.resize(module_count);
  result->modules[module_midi] = midi_topo(module_section_hidden);
  result->modules[module_voice_note] = voice_note_topo(module_section_hidden);
  result->modules[module_voice_mix] = voice_mix_topo(module_section_hidden, is_fx);
  result->modules[module_external_audio] = external_audio_topo(module_section_hidden, is_fx);
  result->modules[module_env] = env_topo(is_fx? module_section_hidden: module_section_env, { 0, 0 });
  result->modules[module_gfx] = fx_topo(module_section_gfx, { 0, 0 }, true, is_fx);
  result->modules[module_vfx] = fx_topo(is_fx ? module_section_hidden : module_section_vfx, { 0, 0 }, false, is_fx);
  result->modules[module_glfo] = lfo_topo(module_section_glfo, { 0, 0 }, true, is_fx);
  result->modules[module_vlfo] = lfo_topo(is_fx ? module_section_hidden : module_section_vlfo, { 0, 0 }, false, is_fx);
  result->modules[module_osc] = osc_topo(is_fx ? module_section_hidden : module_section_osc, { 0, 0 });
  result->modules[module_global_in] = global_in_topo(module_section_global_in, is_fx, { 0, 0 });
  result->modules[module_master_settings] = master_settings_topo(module_section_master_settings, { 0, 0 }, is_fx, result.get());
  result->modules[module_voice_on_note] = voice_on_note_topo(result.get(), module_section_hidden); // must be after all global cv  
  result->modules[module_voice_in] = voice_in_topo(is_fx ? module_section_hidden : module_section_voice_in, { 0, 0 }); // must be after all cv
  result->modules[module_voice_out] = audio_out_topo(is_fx ? module_section_hidden : module_section_voice_out, { 0, 0 }, false);
  result->modules[module_global_out] = audio_out_topo(module_section_global_out, { 0, 0 }, true);
  result->modules[module_monitor] = monitor_topo(module_section_monitor, { 0, 0 }, result->audio_polyphony, is_fx);
  result->modules[module_osc_osc_matrix] = osc_osc_matrix_topo(is_fx ? module_section_hidden : module_section_voice_matrices, { 0, 0 }, result.get());
  result->modules[module_gaudio_audio_matrix] = audio_audio_matrix_topo(module_section_global_matrices, { 0, 0 }, true, is_fx,
    make_audio_audio_matrix_sources(result.get(), true), make_audio_audio_matrix_targets(result.get(), true));
  result->modules[module_gcv_audio_matrix] = cv_matrix_topo(module_section_global_matrices, { 0, 0 }, false, true, is_fx,
    make_cv_matrix_sources(result.get(), true), {}, make_cv_audio_matrix_targets(result.get(), true));
  result->modules[module_gcv_cv_matrix] = cv_matrix_topo(module_section_global_matrices, { 0, 0 }, true, true, is_fx,
    make_cv_matrix_sources(result.get(), true), {}, make_cv_cv_matrix_targets(result.get(), true));
  result->modules[module_vaudio_audio_matrix] = audio_audio_matrix_topo(is_fx ? module_section_hidden : module_section_voice_matrices, { 0, 0 }, false, is_fx,
    make_audio_audio_matrix_sources(result.get(), false), make_audio_audio_matrix_targets(result.get(), false));
  result->modules[module_vcv_audio_matrix] = cv_matrix_topo(is_fx ? module_section_hidden : module_section_voice_matrices, { 0, 0 }, false, false, is_fx,
    make_cv_matrix_sources(result.get(), false),
    make_cv_matrix_sources(result.get(), true),
    make_cv_audio_matrix_targets(result.get(), false));
  result->modules[module_vcv_cv_matrix] = cv_matrix_topo(is_fx ? module_section_hidden : module_section_voice_matrices, { 0, 0 }, true, false, is_fx,
    make_cv_matrix_sources(result.get(), false),
    make_cv_matrix_sources(result.get(), true),
    make_cv_cv_matrix_targets(result.get(), false));
  return result;
} 

}
