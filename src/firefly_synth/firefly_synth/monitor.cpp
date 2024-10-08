#include <plugin_base/dsp/engine.hpp>
#include <plugin_base/topo/plugin.hpp>
#include <plugin_base/topo/support.hpp>

#include <firefly_synth/synth.hpp>
#include <array>
#include <algorithm>

using namespace plugin_base;

namespace firefly_synth {

enum { section_main };
enum { 
  param_mts_status, param_drain, param_threads, param_voices,
  param_hi_mod_cpu, param_hi_mod, param_gain, param_cpu };

class monitor_engine: 
public module_engine {
public:
  monitor_engine() = default;
  void process(plugin_block& block) override;
  void reset(plugin_block const*) override {}
  PB_PREVENT_ACCIDENTAL_COPY(monitor_engine);
};

module_topo
monitor_topo(int section, gui_position const& pos, int polyphony, bool is_fx)
{
  module_topo result(make_module(
    make_topo_info_basic("{C20F2D2C-23C6-41BE-BFB3-DE9EDFB051EC}", "Monitor", module_monitor, 1),
    make_module_dsp(module_stage::output, module_output::none, 0, {}),
    make_module_gui(section, pos, { { 1 } , { 1 } })));
  result.gui.show_tab_header = false;
  result.info.description = "Monitor module with active voice count, CLAP threadpool thread count, global output gain, overall CPU usage and highest-module CPU usage.";
  
  result.gui.enable_tab_menu = false;
  result.engine_factory = [](auto const&, int, int) { return std::make_unique<monitor_engine>(); };

  gui_dimension dimension = { { 1, 1 } , { 
    gui_dimension::auto_size_all, gui_dimension::auto_size_all, 
    gui_dimension::auto_size_all, gui_dimension::auto_size_all,
    gui_dimension::auto_size_all, gui_dimension::auto_size_all,
    gui_dimension::auto_size_all, 1 } };
  result.sections.emplace_back(make_param_section(section_main,
    make_topo_tag_basic("{988E6A84-A012-413C-B33B-80B8B135D203}", "Main"),
    make_param_section_gui({ 0, 0 }, dimension, gui_label_edit_cell_split::horizontal)));
  auto& mts_status = result.params.emplace_back(make_param(
    make_topo_info_basic("{4388D544-4208-4839-A73C-2C641D915BD7}", "MTS-ESP", param_mts_status, 1),
    make_param_dsp_output(), make_domain_toggle(false),
    make_param_gui_single(section_main, gui_edit_type::output_toggle, { 0, 0 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  mts_status.info.description = "MTS-ESP master status.";
  auto& drained = result.params.emplace_back(make_param(
    make_topo_info("{C10AEC7B-A79D-4236-B65E-BBC444FAC854}", true, "Voices Drained", "Drained", "Drained", param_drain, 1),
    make_param_dsp_output(), make_domain_toggle(false),
    make_param_gui_single(section_main, gui_edit_type::output_toggle, { 1, 0 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  drained.info.description = "Voiced drained indicator.";

  auto& thrs = result.params.emplace_back(make_param(
    make_topo_info_basic("{FD7E410D-D4A6-4AA2-BDA0-5B5E6EC3E13A}", "Threads", param_threads, 1),
    make_param_dsp_output(), make_domain_step(0, polyphony, 0, 0),
    make_param_gui_single(section_main, gui_edit_type::output_label_center, { 0, 2 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  thrs.info.description = "Number of CLAP threadpool threads used to process voices in the last block. For VST3, this will always be 0 or 1.";
  auto& voices = result.params.emplace_back(make_param(
    make_topo_info_basic("{2827FB67-CF08-4785-ACB2-F9200D6B03FA}", "Voices", param_voices, 1),
    make_param_dsp_output(), make_domain_step(0, polyphony, 0, 0),
    make_param_gui_single(section_main, gui_edit_type::output_label_center, { 1, 2 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  voices.info.description = "Active voice count. Max 32, after that, recycling will occur.";

  auto& hi_cpu = result.params.emplace_back(make_param(
    make_topo_info_basic("{2B13D43C-FFB5-4A66-9532-39B0F8258161}", "Hi CPU", param_hi_mod_cpu, 1),
    make_param_dsp_output(), make_domain_percentage(0, 0.99, 0, 0, false),
    make_param_gui_single(section_main, gui_edit_type::output_label_left, { 0, 4 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  hi_cpu.info.description = "CPU usage of the most expensive module, relative to total CPU usage.";
  auto& hi_module = result.params.emplace_back(make_param(
    make_topo_info_basic("{BE8AF913-E888-4A0E-B674-8151AF1B7D65}", "Hi Module", param_hi_mod, 1),
    make_param_dsp_output(), make_domain_step(0, 999, 0, 0),
    make_param_gui_single(section_main, gui_edit_type::output_module_name, { 1, 4 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  hi_module.info.description = "Module that used the most CPU relative to total usage.";

  auto& gain = result.params.emplace_back(make_param(
    make_topo_info_basic("{6AB939E0-62D0-4BA3-8692-7FD7B740ED74}", "Gain", param_gain, 1),
    make_param_dsp_output(), make_domain_percentage(0, 9.99, 0, 0, false),
    make_param_gui_single(section_main, gui_edit_type::output_meter, { 0, 6 },
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  gain.info.description = "Global output gain. Nothing is clipped, so this may well exceed 100%.";
  auto& cpu = result.params.emplace_back(make_param(
    make_topo_info_basic("{55919A34-BF81-4EDF-8222-F0F0BE52DB8E}", "CPU", param_cpu, 1),
    make_param_dsp_output(), make_domain_percentage(0, 9.99, 0, 0, false),
    make_param_gui_single(section_main, gui_edit_type::output_meter, { 1, 6 },    
      make_label(gui_label_contents::name, gui_label_align::left, gui_label_justify::near))));
  cpu.info.description = std::string("CPU usage relative to last processing block length. ") +
    "For example, if it took 1 ms to render a 5 ms block, this will be 20%.";
  return result;
}

void
monitor_engine::process(plugin_block& block)
{
  float max_out = 0.0f;
  for (int c = 0; c < 2; c++)  
    for(int f = block.start_frame; f < block.end_frame; f++)
      max_out = std::max(max_out, block.out->host_audio[c][f]);

  auto const& params = block.plugin_desc_.plugin->modules[module_monitor].params;
  block.set_out_param(param_voices, 0, block.out->voice_count);
  block.set_out_param(param_threads, 0, block.out->thread_count);
  block.set_out_param(param_drain, 0, block.out->voices_drained);
  block.set_out_param(param_mts_status, 0, block.out->mts_esp_status);
  block.set_out_param(param_hi_mod, 0, block.out->high_cpu_module);
  block.set_out_param(param_gain, 0, std::clamp((double)max_out, 0.0, params[param_gain].domain.max));
  block.set_out_param(param_cpu, 0, std::clamp(block.out->cpu_usage, 0.0, params[param_cpu].domain.max));
  block.set_out_param(param_hi_mod_cpu, 0, std::clamp(block.out->high_cpu_module_usage, 0.0, params[param_hi_mod_cpu].domain.max));
}

}
