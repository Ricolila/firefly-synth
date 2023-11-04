#pragma once

#include <plugin_base/gui/lnf.hpp>
#include <plugin_base/topo/plugin.hpp>
#include <plugin_base/shared/jarray.hpp>
#include <plugin_base/dsp/block/plugin.hpp>

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace infernal_synth {

typedef plugin_base::jarray<
  plugin_base::jarray<float, 1> const*, 4> 
cv_matrix_output;

struct module_mapping { int topo; int slot; };

struct matrix_modules
{
  std::vector<module_mapping> mappings;
  std::vector<plugin_base::list_item> items;
  std::shared_ptr<plugin_base::gui_submenu> submenu;
};

enum { 
  module_glfo, module_vlfo, module_env, module_cv_matrix, 
  module_audio_matrix, module_osc, module_fx, module_delay,
  module_monitor, module_count };

inline cv_matrix_output const&
get_cv_matrix_output(plugin_base::plugin_block& block)
{
  void* cv_matrix_context = block.voice->all_context[module_cv_matrix][0];
  return *static_cast<cv_matrix_output const*>(cv_matrix_context);
}

matrix_modules
make_matrix_modules(std::vector<plugin_base::module_topo const*> const& sources);

std::unique_ptr<plugin_base::plugin_topo> synth_topo();
plugin_base::module_topo env_topo(int section, plugin_base::gui_colors const& colors, plugin_base::gui_position const& pos);
plugin_base::module_topo osc_topo(int section, plugin_base::gui_colors const& colors, plugin_base::gui_position const& pos);
plugin_base::module_topo delay_topo(int section, plugin_base::gui_colors const& colors, plugin_base::gui_position const& pos);
plugin_base::module_topo lfo_topo(int section, plugin_base::gui_colors const& colors, plugin_base::gui_position const& pos, bool global);
plugin_base::module_topo fx_topo(int section, plugin_base::gui_colors const& colors, plugin_base::gui_position const& pos, int osc_slot_count);
plugin_base::module_topo monitor_topo(int section, plugin_base::gui_colors const& colors, plugin_base::gui_position const& pos, int polyphony);
plugin_base::module_topo cv_matrix_topo(int section, plugin_base::gui_colors const& colors, plugin_base::gui_position const& pos,
  std::vector<plugin_base::module_topo const*> const& sources, std::vector<plugin_base::module_topo const*> const& targets);
plugin_base::module_topo audio_matrix_topo(int section, plugin_base::gui_colors const& colors, plugin_base::gui_position const& pos,
  std::vector<plugin_base::module_topo const*> const& sources, std::vector<plugin_base::module_topo const*> const& targets);

}
