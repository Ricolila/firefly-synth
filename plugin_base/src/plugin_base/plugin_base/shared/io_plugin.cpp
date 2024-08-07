#include <plugin_base/topo/plugin.hpp>
#include <plugin_base/shared/io_plugin.hpp>
#include <plugin_base/shared/logger.hpp>

#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>

#include <memory>
#include <fstream>

using namespace juce;

namespace plugin_base {

// file format
static int const file_version = 1;
static std::string const file_magic = "{296BBDE2-6411-4A85-BFAF-A9A7B9703DF0}";

static std::unique_ptr<DynamicObject> save_extra_internal(extra_state const& state);
static std::unique_ptr<DynamicObject> save_state_internal(plugin_state const& state);
static load_result load_extra_internal(var const& json, extra_state& state);
static load_result load_state_internal(var const& json, plugin_version const& old_version, plugin_state& state);

load_handler::
load_handler(juce::var const* json, plugin_version const& old_version):
_json(json), _old_version(old_version) {}

bool 
load_handler::old_param_value(
  std::string const& old_module_id, int old_module_slot,
  std::string const& old_param_id, int old_param_slot,
  std::string& old_value) const
{
  for (int m = 0; m < (*_json)["state"].size(); m++)
    if((*_json)["modules"][m]["id"].toString().toStdString() == old_module_id)
    {
      var module_slots = (*_json)["state"][m]["slots"];
      if(module_slots.size() > old_module_slot)
      {
        var old_params = module_slots[old_module_slot]["params"];
        for(int p = 0; p < old_params.size(); p++)
        {
          auto old_json_param_id = (*_json)["modules"][m]["params"][p]["id"].toString().toStdString();
          if(old_json_param_id == old_param_id)
          {
            var param_slots = old_params[p]["slots"];
            if(param_slots.size() > old_param_slot)
            {
              old_value = param_slots[old_param_slot].toString().toStdString();
              return true;
            }
          }
        }
      }
    }
  return false;
}

static std::vector<char>
release_json_to_buffer(std::unique_ptr<DynamicObject>&& json)
{
  std::string text = JSON::toString(var(json.release())).toStdString();
  return std::vector<char>(text.begin(), text.end());
}

static load_result
load_json_from_buffer(std::vector<char> const& buffer, var& json)
{
  std::string text(buffer.size(), '\0');
  std::copy(buffer.begin(), buffer.end(), text.begin());
  auto parse_result = JSON::parse(String(text), json);
  if (!parse_result.wasOk())
    return load_result("Invalid json.");
  return load_result();
}

static std::unique_ptr<DynamicObject>
wrap_json_with_meta(plugin_topo const& topo, var const& json)
{
  auto meta = std::make_unique<DynamicObject>();
  meta->setProperty("file_magic", var(file_magic));
  meta->setProperty("file_version", var(file_version));
  meta->setProperty("plugin_id", String(topo.tag.id));
  meta->setProperty("plugin_name", String(topo.tag.full_name));
  meta->setProperty("plugin_version_major", topo.version.major);
  meta->setProperty("plugin_version_minor", topo.version.minor);  
  meta->setProperty("plugin_version_patch", topo.version.patch);

  auto checked = std::make_unique<DynamicObject>();
  checked->setProperty("meta", var(meta.release()));
  checked->setProperty("content", json);

  // checksum so on load we can assume integrity of the block 
  // instead of doing lots of structural json validation
  var checked_var(checked.release());
  String checked_text(JSON::toString(checked_var));
  auto result = std::make_unique<DynamicObject>();
  result->setProperty("checksum", var(MD5(checked_text.toUTF8()).toHexString()));
  result->setProperty("checked", checked_var);
  return result;
}

static load_result
unwrap_json_from_meta(
  plugin_topo const& topo, var const& json, 
  var& result, plugin_version& old_version)
{
  old_version.major = 0;
  old_version.minor = 0;
  old_version.patch = 0;

  if(!json.hasProperty("checksum"))
    return load_result("Invalid checksum.");
  if (!json.hasProperty("checked"))
    return load_result("Invalid checked.");
  if (json["checksum"] != MD5(JSON::toString(json["checked"]).toUTF8()).toHexString())
    return load_result("Invalid checksum.");

  auto const& checked = json["checked"];
  if(!checked.hasProperty("content"))
    return load_result("Invalid content.");
  if(!checked.hasProperty("meta"))
    return load_result("Invalid meta data.");

  auto const& meta = checked["meta"];
  if (!meta.hasProperty("file_magic") || meta["file_magic"] != file_magic)
    return load_result("Invalid file magic.");
  if (!meta.hasProperty("file_version") || (int)meta["file_version"] > file_version)
    return load_result("Invalid file version.");
  if (meta["plugin_id"] != topo.tag.id)
    return load_result("Invalid plugin id.");

  old_version.major = (int)meta["plugin_version_major"];
  old_version.minor = (int)meta["plugin_version_minor"];
  if(meta.hasProperty("plugin_version_patch"))
    old_version.patch = (int)meta["plugin_version_patch"];

  if (old_version.major > topo.version.major)
    return load_result("Invalid plugin version.");
  if (old_version.major == topo.version.major)
    if (old_version.minor > topo.version.minor)
      return load_result("Invalid plugin version.");
  if (old_version.major == topo.version.major && old_version.minor == topo.version.minor)
    if (old_version.patch > topo.version.patch)
      return load_result("Invalid plugin version.");

  result = checked["content"];
  return load_result();
}

std::vector<char>
plugin_io_save_extra(plugin_topo const& topo, extra_state const& state)
{
  PB_LOG_FUNC_ENTRY_EXIT();
  auto json = wrap_json_with_meta(topo, var(save_extra_internal(state).release()));
  return release_json_to_buffer(std::move(json));
}

std::vector<char>
plugin_io_save_state(plugin_state const& state)
{
  PB_LOG_FUNC_ENTRY_EXIT();
  auto json = wrap_json_with_meta(*state.desc().plugin, var(save_state_internal(state).release()));
  return release_json_to_buffer(std::move(json));
}

load_result
plugin_io_load_state(std::vector<char> const& data, plugin_state& state)
{
  var json;
  var content;
  plugin_version old_version;

  PB_LOG_FUNC_ENTRY_EXIT();
  auto result = load_json_from_buffer(data, json);
  if (!result.ok()) return result;
  result = unwrap_json_from_meta(*state.desc().plugin, json, content, old_version);
  return load_state_internal(content, old_version, state);
}

load_result
plugin_io_load_extra(plugin_topo const& topo, std::vector<char> const& data, extra_state& state)
{
  var json;
  var content;
  plugin_version old_version;

  PB_LOG_FUNC_ENTRY_EXIT();
  auto result = load_json_from_buffer(data, json);
  if (!result.ok()) return result;
  result = unwrap_json_from_meta(topo, json, content, old_version);
  return load_extra_internal(content, state);
}

load_result
plugin_io_load_file_all(
  std::filesystem::path const& path, plugin_state& plugin, extra_state& extra)
{
  PB_LOG_FUNC_ENTRY_EXIT();
  load_result failed("Could not read file.");
  std::vector<char> data = file_load(path);
  if(data.size() == 0) return failed;
  return plugin_io_load_all(data, plugin, extra);
};

bool
plugin_io_save_file_all(
  std::filesystem::path const& path, plugin_state const& plugin, extra_state const& extra)
{
  PB_LOG_FUNC_ENTRY_EXIT();
  std::ofstream stream(path, std::ios::out | std::ios::binary);
  if (stream.bad()) return false;
  auto data(plugin_io_save_all(plugin, extra));
  stream.write(data.data(), data.size());
  return !stream.bad();
}

std::vector<char> 
plugin_io_save_all(plugin_state const& plugin, extra_state const& extra)
{
  PB_LOG_FUNC_ENTRY_EXIT();
  auto const& topo = *plugin.desc().plugin;
  auto root = std::make_unique<DynamicObject>();
  root->setProperty("extra", var(wrap_json_with_meta(topo, var(save_extra_internal(extra).release())).release()));
  root->setProperty("plugin", var(wrap_json_with_meta(topo, var(save_state_internal(plugin).release())).release()));
  return release_json_to_buffer(wrap_json_with_meta(topo, var(root.release())));
}

load_result 
plugin_io_load_all(std::vector<char> const& data, plugin_state& plugin, extra_state& extra)
{
  var json;
  var content;
  plugin_version old_version;

  PB_LOG_FUNC_ENTRY_EXIT();
  auto result = load_json_from_buffer(data, json);
  if(!result.ok()) return result;
  result = unwrap_json_from_meta(*plugin.desc().plugin, json, content, old_version);
  if (!result.ok()) return result;

  // can't produce warnings, only errors
  var extra_content;
  result = unwrap_json_from_meta(*plugin.desc().plugin, content["extra"], extra_content, old_version);
  if (!result.ok()) return result;
  auto extra_state_load = extra_state(extra.keyset());
  result = load_extra_internal(extra_content, extra_state_load);
  if (!result.ok()) return result;

  var plugin_content;
  result = unwrap_json_from_meta(*plugin.desc().plugin, content["plugin"], plugin_content, old_version);
  if (!result.ok()) return result;
  result = load_state_internal(plugin_content, old_version, plugin);
  if(!result.ok()) return result;
  for(auto k: extra_state_load.keyset())
    if(extra_state_load.contains_key(k))
      extra.set_var(k, extra_state_load.get_var(k));
  return result;
}

std::unique_ptr<DynamicObject>
save_extra_internal(extra_state const& state)
{
  auto root = std::make_unique<DynamicObject>();
  for(auto k: state.keyset())
    if(state.contains_key(k))
      root->setProperty(String(k), var(state.get_var(k)));
  return root;
}

load_result 
load_extra_internal(var const& json, extra_state& state)
{
  state.clear();
  for(auto k: state.keyset())
    if(json.hasProperty(String(k)))
      state.set_var(k, json[Identifier(k)]);
  return load_result();
}

std::unique_ptr<DynamicObject>
save_state_internal(plugin_state const& state)
{
  var modules;
  auto plugin = std::make_unique<DynamicObject>();
  
  // store some topo info so we can provide meaningful warnings
  for (int m = 0; m < state.desc().plugin->modules.size(); m++)
  {
    var params;
    auto const& module_topo = state.desc().plugin->modules[m];
    auto module = std::make_unique<DynamicObject>();
    module->setProperty("id", String(module_topo.info.tag.id));
    module->setProperty("slot_count", module_topo.info.slot_count);
    module->setProperty("name", String(module_topo.info.tag.full_name));
    for (int p = 0; p < module_topo.params.size(); p++)
    {
      auto const& param_topo = module_topo.params[p];
      if(param_topo.dsp.direction == param_direction::output) continue;
      auto param = std::make_unique<DynamicObject>();
      param->setProperty("id", String(param_topo.info.tag.id));
      param->setProperty("slot_count", param_topo.info.slot_count);
      param->setProperty("name", String(param_topo.info.tag.full_name));
      params.append(var(param.release()));
    }
    module->setProperty("params", params);
    modules.append(var(module.release()));
  }  
  plugin->setProperty("modules", modules);

  // dump the textual values in 4d format
  var module_states;
  for (int m = 0; m < state.desc().plugin->modules.size(); m++)
  {
    var module_slot_states;
    auto const& module_topo = state.desc().plugin->modules[m];
    auto module_state = std::make_unique<DynamicObject>();
    for (int mi = 0; mi < module_topo.info.slot_count; mi++)
    {
      var param_states;
      auto module_slot_state = std::make_unique<DynamicObject>();
      for (int p = 0; p < module_topo.params.size(); p++)
      {
        var param_slot_states;
        auto const& param_topo = module_topo.params[p];
        if(param_topo.dsp.direction == param_direction::output) continue;
        auto param_state = std::make_unique<DynamicObject>();
        for (int pi = 0; pi < param_topo.info.slot_count; pi++)
        {
          int index = state.desc().param_mappings.topo_to_index[m][mi][p][pi];
          param_slot_states.append(var(String(state.plain_to_text_at_index(true, index, state.get_plain_at(m, mi, p, pi)))));
        }
        param_state->setProperty("slots", param_slot_states);
        param_states.append(var(param_state.release()));
      }
      module_slot_state->setProperty("params", param_states);
      module_slot_states.append(var(module_slot_state.release()));
    }
    module_state->setProperty("slots", module_slot_states);
    module_states.append(var(module_state.release()));
  }  
  plugin->setProperty("state", module_states);
  return plugin;
}

load_result
load_state_internal(
  var const& json, plugin_version const& old_version, plugin_state& state)
{
  if(!json.hasProperty("state"))
    return load_result("Invalid plugin.");
  if (!json.hasProperty("modules"))
    return load_result("Invalid plugin.");
  
  // good to go - only warnings from now on
  // set up handler for loading old state
  // in case plugin wants to do conversion
  load_result result;
  state.init(state_init_type::empty);
  load_handler handler(&json, old_version);

  for(int m = 0; m < json["modules"].size(); m++)
  {
    // check for old module not found
    auto module_id = json["modules"][m]["id"].toString().toStdString();
    auto module_name = json["modules"][m]["name"].toString().toStdString();
    auto module_iter = state.desc().module_id_to_index.find(module_id);
    if (module_iter == state.desc().module_id_to_index.end())
    {
#ifndef NDEBUG
      result.warnings.push_back("Module '" + module_name + "' was deleted.");
#endif
      continue;
    }

    // check for changed module slot count
    var module_slot_count = json["modules"][m]["slot_count"];
    auto const& new_module = state.desc().plugin->modules[module_iter->second];
    if ((int)module_slot_count > new_module.info.slot_count)
    {
#ifndef NDEBUG
      result.warnings.push_back("Module '" + new_module.info.tag.full_name + "' decreased slot count.");
#endif
    }

    for (int p = 0; p < json["modules"][m]["params"].size(); p++)
    {
      // check for old param not found
      auto param_id = json["modules"][m]["params"][p]["id"].toString().toStdString();
      auto param_name = json["modules"][m]["params"][p]["name"].toString().toStdString();
      auto param_iter = state.desc().param_mappings.id_to_index.at(module_id).find(param_id);
      if (param_iter == state.desc().param_mappings.id_to_index.at(module_id).end())
      {
#ifndef NDEBUG
        result.warnings.push_back("Param '" + module_name + " " + param_name + "' was deleted.");
#endif
        continue;
      }

      // check for changed param slot count
      var param_slot_count = json["modules"][m]["params"][p]["slot_count"];
      auto const& new_param = state.desc().plugin->modules[module_iter->second].params[param_iter->second];
      if ((int)param_slot_count > new_param.info.slot_count)
      {
#ifndef NDEBUG
        result.warnings.push_back("Param '" + new_module.info.tag.full_name + " " + new_param.info.tag.full_name + "' decreased slot count.");
#endif
      }
    }
  }

  // copy over old state, push parse warnings as we go
  for (int m = 0; m < json["state"].size(); m++)
  {
    auto module_id = json["modules"][m]["id"].toString().toStdString();
    auto module_iter = state.desc().module_id_to_index.find(module_id);
    if(module_iter == state.desc().module_id_to_index.end()) continue;
    var module_slots = json["state"][m]["slots"];

    // set up the topo specific converter, if any
    auto const& new_module = state.desc().plugin->modules[module_iter->second];
    std::unique_ptr<state_converter> converter = {};
    if (new_module.state_converter_factory != nullptr)
      converter = new_module.state_converter_factory(&state.desc());

    for(int mi = 0; mi < module_slots.size() && mi < new_module.info.slot_count; mi++)
    {
      for (int p = 0; p < module_slots[mi]["params"].size(); p++)
      {
        auto param_id = json["modules"][m]["params"][p]["id"].toString().toStdString();
        auto param_iter = state.desc().param_mappings.id_to_index.at(module_id).find(param_id);
        if (param_iter == state.desc().param_mappings.id_to_index.at(module_id).end()) continue;
        var param_slots = json["state"][m]["slots"][mi]["params"][p]["slots"];
        auto const& new_param = state.desc().plugin->modules[module_iter->second].params[param_iter->second];

        // readonly support for per-instance microtuning (outside of the patch)
        if (new_param.info.is_readonly) 
          continue;

        for (int pi = 0; pi < param_slots.size() && pi < new_param.info.slot_count; pi++)
        {
          plain_value plain;
          int index = state.desc().param_mappings.topo_to_index[new_module.info.index][mi][new_param.info.index][pi];
          std::string text = json["state"][m]["slots"][mi]["params"][p]["slots"][pi].toString().toStdString();
          if(state.text_to_plain_at_index(true, index, text, plain))
            state.set_plain_at(new_module.info.index, mi, new_param.info.index, pi, plain);
          else
          {
            // give plugin a chance to recover
            plain_value new_value;
            if(converter && converter->handle_invalid_param_value(new_module.info.tag.id, mi, new_param.info.tag.id, pi, text, handler, new_value))
              state.set_plain_at(new_module.info.index, mi, new_param.info.index, pi, new_value);
            else
            {
#ifndef NDEBUG
              result.warnings.push_back("Param '" + new_module.info.tag.full_name + " " + new_param.info.tag.full_name + "': invalid value '" + text + "'.");
#endif
            }
          }
        }
      }

      // all params are set, do optional post-process conversion
      if(converter) 
        converter->post_process_existing(handler, state);
    }
  }

  // run unconditional postprocessors
  // this is mainly meant to allow completely new stuff to copy over old stuff
  for (int m = 0; m < state.desc().plugin->modules.size(); m++)
  {
    std::unique_ptr<state_converter> converter = {};
    auto const& mod_topo = state.desc().plugin->modules[m];
    if (mod_topo.state_converter_factory != nullptr)
    {
      auto always_converter = mod_topo.state_converter_factory(&state.desc());
      if (always_converter) always_converter->post_process_always(handler, state);
    }
  }

  return result;
}

}