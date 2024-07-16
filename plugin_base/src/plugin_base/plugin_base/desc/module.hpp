#pragma once

#include <plugin_base/desc/midi.hpp>
#include <plugin_base/desc/output.hpp>
#include <plugin_base/desc/param.hpp>
#include <plugin_base/desc/shared.hpp>
#include <plugin_base/topo/module.hpp>
#include <plugin_base/shared/utility.hpp>

#include <vector>
#include <string>

namespace plugin_base {

struct plugin_desc;

// runtime module descriptor
struct module_desc final {
  topo_desc_info info = {};
  module_topo const* module = {};
  std::vector<param_desc> params = {};
  std::vector<midi_desc> midi_sources = {};
  std::vector<output_desc> output_sources = {};

  PB_PREVENT_ACCIDENTAL_COPY_DEFAULT_CTOR(module_desc);
  void validate(plugin_desc const& plugin, int index) const;
  module_desc(
    module_topo const& module_, int topo, int slot, int global, 
    int param_global_start, int midi_source_global_start, int output_source_global_start);
};

}
