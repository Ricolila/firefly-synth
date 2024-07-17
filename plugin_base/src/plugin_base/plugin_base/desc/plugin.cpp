#include <plugin_base/desc/plugin.hpp>
#include <plugin_base/shared/utility.hpp> 
#include <plugin_base/shared/logger.hpp>

#include <set>

namespace plugin_base {

plugin_desc::
plugin_desc(plugin_topo const* plugin, format_config const* config):
plugin(plugin), config(config)
{
  PB_LOG_FUNC_ENTRY_EXIT();
    
  assert(plugin);

  int param_global = 0;
  int module_global = 0;
  int midi_source_global = 0;
  int output_source_global = 0;

  for(int m = 0; m < plugin->modules.size(); m++)
  {
    auto const& module = plugin->modules[m];
    midi_mappings.topo_to_index.emplace_back();
    param_mappings.topo_to_index.emplace_back();
    output_mappings.topo_to_index.emplace_back();
    if(module.dsp.stage == module_stage::input) module_voice_start++;
    if(module.dsp.stage == module_stage::input) module_output_start++;
    if(module.dsp.stage == module_stage::voice) module_output_start++;
    module_id_to_index[module.info.tag.id] = m;
    auto& param_id_mapping = param_mappings.id_to_index[module.info.tag.id];
    for(int p = 0; p < module.params.size(); p++)
      param_id_mapping[module.params[p].info.tag.id] = p;
    for(int mi = 0; mi < module.info.slot_count; mi++)
    {
      midi_mappings.topo_to_index[m].emplace_back();
      param_mappings.topo_to_index[m].emplace_back();
      output_mappings.topo_to_index[m].emplace_back();
      modules.emplace_back(module_desc(module, m, mi, module_global++, param_global, midi_source_global, output_source_global));
      for (int ms = 0; ms < module.midi_sources.size(); ms++)
      {
        auto const& source = module.midi_sources[ms];
        PB_ASSERT_EXEC(midi_mappings.id_to_index.insert(std::pair(source.id, midi_source_global)).second);
        midi_mappings.topo_to_index[m][mi].push_back(midi_source_global++);
      }
      for (int os = 0; os < module.dsp.outputs.size(); os++)
        output_mappings.topo_to_index[m][mi].push_back(output_source_global++);
      for(int p = 0; p < module.params.size(); p++)
      {
        auto const& param = module.params[p];
        param_mappings.topo_to_index[m][mi].emplace_back();
        for(int pi = 0; pi < param.info.slot_count; pi++)
          param_mappings.topo_to_index[m][mi][p].push_back(param_global++);
      }
    }
  }

  param_global = 0;
  midi_source_global = 0;
  output_source_global = 0;
  for(int m = 0; m < modules.size(); m++)
  {
    auto const& module = modules[m];
    if(module.info.slot == 0)
      module_topo_to_index[module.module->info.index] = m;

    for (int p = 0; p < module.params.size(); p++)
    {
      auto const& param = module.params[p];
      param_mapping mapping;
      mapping.param_local = p;
      mapping.module_global = m;
      mapping.topo.param_slot = param.info.slot;
      mapping.topo.param_index = param.info.topo;
      mapping.topo.module_slot = module.info.slot;
      mapping.topo.module_index = module.info.topo;
      mapping.param_global = param_global++;
      param_mappings.index_to_tag.push_back(param.info.id_hash);
      param_mappings.tag_to_index[param.info.id_hash] = param_mappings.params.size();
      param_mappings.params.push_back(std::move(mapping));
      params.push_back(&module.params[p]);
    }

    for (int ms = 0; ms < module.midi_sources.size(); ms++)
    {
      auto const& source = module.midi_sources[ms];
      midi_mapping mapping;
      mapping.midi_local = ms;
      mapping.module_global = m;
      mapping.topo.midi_index = source.info.topo;
      mapping.topo.module_slot = module.info.slot;
      mapping.topo.module_index = module.info.topo;
      mapping.midi_global = midi_source_global++;
      midi_mappings.index_to_tag.push_back(source.info.id_hash);
      midi_mappings.tag_to_index[source.info.id_hash] = midi_mappings.midi_sources.size();
      midi_mappings.midi_sources.push_back(std::move(mapping));
      midi_sources.push_back(&module.midi_sources[ms]);
    }

    for (int os = 0; os < module.output_sources.size(); os++)
    {
      auto const& source = module.output_sources[os];
      output_mapping mapping;
      mapping.output_local = os;
      mapping.module_global = m;
      mapping.topo.output_index = source.info.topo;
      mapping.topo.module_slot = module.info.slot;
      mapping.topo.module_index = module.info.topo;
      mapping.output_global = output_source_global++;
      output_mappings.index_to_tag.push_back(source.info.id_hash);
      output_mappings.tag_to_index[source.info.id_hash] = output_mappings.output_sources.size();
      output_mappings.output_sources.push_back(std::move(mapping));
      output_sources.push_back(&module.output_sources[os]);
    }
  }

  // link midi sources <-> regular params
  param_global = 0;
  for (int m = 0; m < modules.size(); m++)
  {
    auto const& module = modules[m];
    for (int p = 0; p < module.params.size(); p++)
    {
      auto const& param = module.params[p];
      if (param.param->dsp.is_midi(module.info.slot))
      {
        auto const& source = param.param->dsp.midi_source;
        int midi_source = midi_mappings.topo_to_index[source.module_index][source.module_slot][source.midi_index];
        param_mappings.params[param_global].midi_source_global = midi_source;
        auto& linked_params = midi_mappings.midi_sources[midi_source].linked_params;
        assert(std::find(linked_params.begin(), linked_params.end(), param_global) == linked_params.end());
        linked_params.push_back(param_global);
      }
      param_global++;
    }
  }

  param_count = param_global;
  midi_count = midi_source_global;
  output_count = output_source_global;
  module_count = modules.size();
}

std::vector<std::string>
plugin_desc::themes() const
{
  // expect theme folders directly in the "themes" folder
  std::vector<std::string> result;
  auto themes_folder = get_resource_location(config) / resource_folder_themes;
  if(!std::filesystem::exists(themes_folder)) return {};
  for (auto const& entry : std::filesystem::directory_iterator{ themes_folder })
    if (entry.is_directory())
      result.push_back(entry.path().filename().string());
  std::sort(result.begin(), result.end(), [](auto const& l, auto const& r) { return l < r; });
  return result;
}

std::vector<resource_item>
plugin_desc::presets() const
{
  // expect preset files nested 1 level deep, subfolders act as grouping
  std::vector<resource_item> result;
  auto preset_folder = get_resource_location(config) / resource_folder_presets;
  if (!std::filesystem::exists(preset_folder)) return {};
  for (auto const& group_entry : std::filesystem::directory_iterator{ preset_folder })
    if(group_entry.is_directory())
      for (auto const& entry : std::filesystem::directory_iterator{ group_entry.path()})
        if (entry.is_regular_file() && entry.path().extension().string() == std::string(".") + plugin->extension)
          result.push_back({ entry.path().stem().string(), entry.path().string(), group_entry.path().filename().string() });
  std::sort(result.begin(), result.end(), [](auto const& l, auto const& r) { 
    if(l.group < r.group) return true;
    if(r.group < l.group) return false;
    return l.name < r.name; 
  });
  return result;
}

void
plugin_desc::validate() const
{
  int param_global = 0;
  int midi_source_global = 0;
  (void)param_global;
  (void)midi_source_global;

  std::set<int> all_hashes;
  std::set<std::string> all_ids;

  plugin->validate();
  midi_mappings.validate(*this);
  param_mappings.validate(*this);
  output_mappings.validate(*this);

  assert(param_count > 0);
  assert(module_count > 0);
  assert(module_voice_start >= 0);
  assert(params.size() == param_count);
  assert(modules.size() == module_count);
  assert(module_voice_start <= module_output_start);
  assert(module_output_start < plugin->modules.size());

  for (int ms = 0; ms < plugin->gui.module_sections.size(); ms++)
    PB_ASSERT_EXEC(all_ids.insert(plugin->gui.module_sections[ms].id).second);

  for (int m = 0; m < modules.size(); m++)
  {
    auto const& module = modules[m];
    module.validate(*this, m);
    PB_ASSERT_EXEC(all_ids.insert(module.info.id).second);
    PB_ASSERT_EXEC(all_hashes.insert(module.info.id_hash).second);
    for (int ms = 0; ms < module.midi_sources.size(); ms++)
    {
      auto const& source = module.midi_sources[ms];
      assert(source.info.global == midi_source_global++);
      PB_ASSERT_EXEC(all_ids.insert(source.info.id).second);
      PB_ASSERT_EXEC(all_hashes.insert(source.info.id_hash).second);
    }
    for (int p = 0; p < module.params.size(); p++)
    {
      auto const& param = module.params[p];
      assert(param.info.global == param_global++);
      PB_ASSERT_EXEC(all_ids.insert(param.info.id).second);
      PB_ASSERT_EXEC(all_hashes.insert(param.info.id_hash).second);
    }
  }
}

void
plugin_output_mappings::validate(plugin_desc const& plugin) const
{
  assert(tag_to_index.size() == plugin.output_count);
  assert(index_to_tag.size() == plugin.output_count);
  assert(output_sources.size() == plugin.output_count);
  assert(topo_to_index.size() == plugin.plugin->modules.size());

  int output_source_global = 0;
  (void)output_source_global;
  for (int m = 0; m < plugin.plugin->modules.size(); m++)
  {
    auto const& module = plugin.plugin->modules[m];
    (void)module;
    assert(topo_to_index[m].size() == module.info.slot_count);
    for (int mi = 0; mi < module.info.slot_count; mi++)
    {
      assert(topo_to_index[m][mi].size() == module.dsp.outputs.size());
      for (int os = 0; os < module.dsp.outputs.size(); os++)
        assert(topo_to_index[m][mi][os] == output_source_global++);
    }
  }
}

void
plugin_midi_mappings::validate(plugin_desc const& plugin) const
{
  assert(midi_sources.size() == plugin.midi_count);
  assert(tag_to_index.size() == plugin.midi_count);
  assert(index_to_tag.size() == plugin.midi_count);
  assert(topo_to_index.size() == plugin.plugin->modules.size());

  int midi_source_global = 0;
  (void)midi_source_global;
  for (int m = 0; m < plugin.plugin->modules.size(); m++)
  {
    auto const& module = plugin.plugin->modules[m];
    (void)module;
    assert(topo_to_index[m].size() == module.info.slot_count);
    for (int mi = 0; mi < module.info.slot_count; mi++)
    {
      assert(topo_to_index[m][mi].size() == module.midi_sources.size());
      for (int ms = 0; ms < module.midi_sources.size(); ms++)
        assert(topo_to_index[m][mi][ms] == midi_source_global++);
    }
  }

  for (int ms = 0; ms < midi_sources.size(); ms++)
  {
    auto const& source = midi_sources[ms];
    for (int p = 0; p < source.linked_params.size(); p++)
    {
      auto const& dsp = plugin.params[source.linked_params[p]]->param->dsp;
      auto const& mapping = plugin.param_mappings.params[source.linked_params[p]];
      (void)dsp;
      (void)mapping;
      assert(dsp.midi_source == source.topo);
      assert(dsp.is_midi(mapping.topo.module_slot));
    }
  }
}

void
plugin_param_mappings::validate(plugin_desc const& plugin) const
{
  assert(params.size() == plugin.param_count);
  assert(tag_to_index.size() == plugin.param_count);
  assert(index_to_tag.size() == plugin.param_count);
  assert(id_to_index.size() == plugin.plugin->modules.size());
  assert(id_to_index.size() == plugin.plugin->modules.size());
  assert(topo_to_index.size() == plugin.plugin->modules.size());

  int param_global = 0;
  (void)param_global;
  for (int m = 0; m < plugin.plugin->modules.size(); m++)
  {
    auto const& module = plugin.plugin->modules[m];
    (void)module;
    assert(topo_to_index[m].size() == module.info.slot_count);
    assert(id_to_index.at(module.info.tag.id).size() == module.params.size());
    for (int mi = 0; mi < module.info.slot_count; mi++)
    {
      assert(topo_to_index[m][mi].size() == module.params.size());
      for (int p = 0; p < module.params.size(); p++)
      {
        auto const& param = module.params[p];
        assert(topo_to_index[m][mi][p].size() == param.info.slot_count);
        for (int pi = 0; pi < param.info.slot_count; pi++)
          assert(topo_to_index[m][mi][p][pi] == param_global++);
      }
    }
  }

  for (int p = 0; p < plugin.params.size(); p++)
  {
    auto const& pm = params[p];
    auto const& param = *plugin.params[p];
    if (param.param->dsp.is_midi(pm.topo.module_slot))
    {
      assert(0 <= pm.midi_source_global && pm.midi_source_global < plugin.midi_count);
      assert(param.param->dsp.midi_source == plugin.midi_mappings.midi_sources[pm.midi_source_global].topo);
      auto const& linked_params = plugin.midi_mappings.midi_sources[pm.midi_source_global].linked_params;
      (void)linked_params;
      assert(std::find(linked_params.begin(), linked_params.end(), p) != linked_params.end());
    }
  }
}

}
