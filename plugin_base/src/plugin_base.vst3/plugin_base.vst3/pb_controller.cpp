#include <plugin_base/desc/dims.hpp>
#include <plugin_base/dsp/utility.hpp>
#include <plugin_base/shared/value.hpp>
#include <plugin_base/shared/io_plugin.hpp>
#include <plugin_base/shared/logger.hpp>

#include <plugin_base.vst3/utility.hpp>
#include <plugin_base.vst3/pb_param.hpp>
#include <plugin_base.vst3/pb_editor.hpp>
#include <plugin_base.vst3/pb_controller.hpp>

#include <base/source/fstring.h>
#include <pluginterfaces/vst/ivstcontextmenu.h>

#include <juce_events/juce_events.h>
#include <stack>

using namespace juce;
using namespace Steinberg;
using namespace Steinberg::Vst;

namespace plugin_base::vst3 {

pb_basic_config const* 
pb_basic_config::instance()
{
  static pb_basic_config result = {};
  return &result;
}

pb_controller::
~pb_controller() 
{ _gui_state.remove_any_listener(this); }

pb_controller::
pb_controller(plugin_topo const* topo):
_desc(std::make_unique<plugin_desc>(topo, this)),
_gui_state(_desc.get(), true),
_extra_state(gui_extra_state_keyset(*_desc->plugin))
{ 
  PB_LOG_FUNC_ENTRY_EXIT();
  _gui_state.add_any_listener(this);  

  // fetch mod indicator param tags
  _mod_indicator_states_to_gui.resize(mod_indicator_output_param_count);
  _mod_indicator_count_param_tag = stable_hash(mod_indicator_count_param_guid);
  for (int i = 0; i < mod_indicator_output_param_count; i++)
  {
    _mod_indicator_param_tags[i] = stable_hash(mod_indicator_param_guids[i]);
    _tag_to_mod_indicator_index[_mod_indicator_param_tags[i]] = i;
  }
}

void 
pb_controller::gui_param_begin_changes(int index) 
{ 
  _gui_state.begin_undo_region();
  beginEdit(gui_state().desc().param_mappings.index_to_tag[index]); 
}

void
pb_controller::gui_param_end_changes(int index)
{
  endEdit(gui_state().desc().param_mappings.index_to_tag[index]);
  gui_state().end_undo_region("Change", gui_state().desc().params[index]->full_name);
}

IPlugView* PLUGIN_API
pb_controller::createView(char const* name)
{
  PB_LOG_FUNC_ENTRY_EXIT();
  if (ConstString(name) != ViewType::kEditor) return nullptr;
  return _editor = new pb_editor(this, &_mod_indicator_states_to_gui);
}

tresult PLUGIN_API
pb_controller::getState(IBStream* state)
{
  PB_LOG_FUNC_ENTRY_EXIT();
  std::vector<char> data(plugin_io_save_extra_state(*_gui_state.desc().plugin, _extra_state));
  return state->write(data.data(), data.size());
}

tresult PLUGIN_API 
pb_controller::setState(IBStream* state)
{
  PB_LOG_FUNC_ENTRY_EXIT();
  if (!plugin_io_load_extra_state(*_gui_state.desc().plugin, load_ibstream(state), _extra_state).ok())
    return kResultFalse;
  return kResultOk;
}

tresult PLUGIN_API
pb_controller::setComponentState(IBStream* state)
{
  PB_LOG_FUNC_ENTRY_EXIT();
  gui_state().begin_undo_region();
  if (!plugin_io_load_instance_state(load_ibstream(state), gui_state(), false).ok())
  {
    gui_state().discard_undo_region();
    return kResultFalse;
  }
  for (int p = 0; p < gui_state().desc().param_count; p++)
    gui_param_changed(p, gui_state().get_plain_at_index(p));
  gui_state().discard_undo_region();
  return kResultOk;
}

tresult PLUGIN_API 
pb_controller::setParamNormalized(ParamID tag, ParamValue value)
{
  _inside_set_param_normalized = true;
  if(EditControllerEx1::setParamNormalized(tag, value) != kResultTrue) 
  {
    _inside_set_param_normalized = false;
    return kResultFalse;
  }
  
  // fake midi params are not mapped
  auto mapping_iter = gui_state().desc().param_mappings.tag_to_index.find(tag);
  if(mapping_iter != gui_state().desc().param_mappings.tag_to_index.end())
    _gui_state.set_normalized_at_index(mapping_iter->second, normalized_value(value));

  // mod indicator support
  // this is a bit of a cop out but at least it should be working without resorting to messaging
  // upon receiving the "count" param, update the count
  // upon receiving any other param, set the fill bit
  // whenever the first N consecutive fill bits >= count, repaint and reset
  bool needs_mod_indicator_rescan = false;
  if (tag == _mod_indicator_count_param_tag)
  {
    _mod_indicator_count = *reinterpret_cast<std::size_t*>(&value);
    needs_mod_indicator_rescan = true;
  }
  auto mod_ind_iter = _tag_to_mod_indicator_index.find(tag);
  if (mod_ind_iter != _tag_to_mod_indicator_index.end())
  {
    _mod_indicator_param_set[mod_ind_iter->second] = true;
    _mod_indicator_states_from_audio[mod_ind_iter->second].packed = *reinterpret_cast<std::uint64_t*>(&value);
    needs_mod_indicator_rescan = true;
  }
  if (needs_mod_indicator_rescan)
  {
    bool filled_to_count = true;
    for (int i = 0; i < _mod_indicator_count; i++)
      if (!_mod_indicator_param_set[i])
      {
        filled_to_count = false;
        break;
      }
    if (filled_to_count)
    {
      _mod_indicator_param_set.fill(false);
      _mod_indicator_states_to_gui.clear();
      _mod_indicator_states_to_gui.insert(
        _mod_indicator_states_to_gui.begin(), 
        _mod_indicator_states_from_audio.begin(), 
        _mod_indicator_states_from_audio.begin() + _mod_indicator_count);
      _mod_indicator_count = 0;
      if (_editor) _editor->mod_indicator_states_changed();
    }
  }

  _inside_set_param_normalized = false;
  return kResultTrue;
}

tresult PLUGIN_API 
pb_controller::getMidiControllerAssignment(int32 bus, int16 channel, CtrlNumber number, ParamID& id)
{
  if(bus != 0) return kResultFalse;
  auto iter = _midi_id_to_param.find(number);
  if(iter == _midi_id_to_param.end()) return kResultFalse;
  id = iter->second;
  return kResultTrue;
}

void
pb_controller::param_state_changed(int index, plain_value plain)
{
  if(_inside_set_param_normalized) return;
  if (_gui_state.desc().params[index]->param->dsp.direction == param_direction::output) return;
  int tag = gui_state().desc().param_mappings.index_to_tag[index];
  auto normalized = gui_state().desc().plain_to_normalized_at_index(index, plain).value();

  // Per-the-spec we should not have to call setParamNormalized here but not all hosts agree.
  performEdit(tag, normalized);
  setParamNormalized(tag, normalized);
}

std::unique_ptr<host_menu>
pb_controller::context_menu(int param_id) const
{
  FUnknownPtr<IComponentHandler3> handler(componentHandler);
  if (handler == nullptr) return {};
  if (_editor == nullptr) return {};

  ParamID id = param_id;
  IContextMenuTarget* target;
  IPtr<IContextMenu> vst_menu(handler->createContextMenu(_editor, &id));
  if(!vst_menu) return {};
  auto result = std::make_unique<host_menu>();
  std::stack<host_menu_item*> menu_stack;
  menu_stack.push(&result->root);

  for (int i = 0; i < vst_menu->getItemCount(); i++)
  {
    IContextMenu::Item vst_item;
    if (vst_menu->getItem(i, vst_item, &target) != kResultOk) return {};
    
    auto item = menu_stack.top()->children.emplace_back(std::make_shared<host_menu_item>());
    item->tag = i;
    item->flags = host_menu_flags_enabled;
    item->name = to_8bit_string(vst_item.name);
    if((vst_item.flags & IContextMenuItem::kIsChecked) == IContextMenuItem::kIsChecked)
      item->flags |= host_menu_flags_checked;
    if ((vst_item.flags & IContextMenuItem::kIsSeparator) == IContextMenuItem::kIsSeparator)
      item->flags |= host_menu_flags_separator;
    
    if ((vst_item.flags & IContextMenuItem::kIsGroupStart) == IContextMenuItem::kIsGroupStart)
      menu_stack.push(item.get());
    else if ((vst_item.flags & IContextMenuItem::kIsGroupEnd) == IContextMenuItem::kIsGroupEnd)
      menu_stack.pop();
    else if ((vst_item.flags & IContextMenuItem::kIsDisabled) == IContextMenuItem::kIsDisabled)
      item->flags &= ~host_menu_flags_enabled;
  }
  assert(menu_stack.size() == 1);
  menu_stack.pop();

  result->clicked = [vst_menu](int tag) { 
    IContextMenu::Item item = {};
    IContextMenuTarget* target = nullptr;
    if(tag < 0) return; // can happen on close
    assert(tag < vst_menu->getItemCount());
    if(vst_menu->getItem(tag, item, &target) != kResultOk) return;
    if(target == nullptr) return;
    target->executeMenuItem(item.tag);
  };
  return result;
}

tresult PLUGIN_API 
pb_controller::initialize(FUnknown* context)
{
  PB_LOG_FUNC_ENTRY_EXIT();

  int unit_id = 1;
  if(EditController::initialize(context) != kResultTrue) 
    return kResultFalse;

  for(int m = 0; m < gui_state().desc().modules.size(); m++)
  {
    auto const& module = gui_state().desc().modules[m];
    UnitInfo unit_info;
    unit_info.id = unit_id++;
    unit_info.parentUnitId = kRootUnitId;
    unit_info.programListId = kNoProgramListId;
    from_8bit_string(unit_info.name, module.info.name.c_str());
    addUnit(new Unit(unit_info));

    for (int p = 0; p < module.params.size(); p++)
    {
      ParameterInfo param_info = {};
      auto const& param = module.params[p];
      param_info.unitId = unit_info.id;
      param_info.id = param.info.id_hash;
      from_8bit_string(param_info.title, param.full_name.c_str());
      from_8bit_string(param_info.shortTitle, param.full_name.c_str());
      from_8bit_string(param_info.units, param.param->domain.unit.c_str());
      param_info.defaultNormalizedValue = param.param->domain.default_normalized(module.info.slot, param.info.slot).value();

      param_info.flags = ParameterInfo::kNoFlags;
      if(param.param->dsp.can_automate(module.info.slot))
        param_info.flags |= ParameterInfo::kCanAutomate;
      else
        param_info.flags |= ParameterInfo::kIsReadOnly;
      if(param.param->gui.is_list())
        param_info.flags |= ParameterInfo::kIsList;
      param_info.stepCount = 0;
      if (!param.param->domain.is_real())
        param_info.stepCount = param.param->domain.max - param.param->domain.min;
      parameters.addParameter(new pb_param(&_gui_state, module.params[p].param, module.params[p].info.global, param_info));
    }
  }

  // be sure to append fake midi params *after* the real ones
  // to not mess up the tag to index mapping
  for (int m = 0; m < gui_state().desc().modules.size(); m++)
  {
    auto const& module = gui_state().desc().modules[m];
    for (int ms = 0; ms < module.midi_sources.size(); ms++)
    {
      ParameterInfo param_info = {};
      auto const& source = module.midi_sources[ms];
      param_info.stepCount = 0;
      param_info.unitId = kRootUnitId;
      param_info.id = source.info.id_hash;
      param_info.flags = ParameterInfo::kIsHidden | ParameterInfo::kIsHidden;
      parameters.addParameter(new Parameter(param_info));
      param_info.defaultNormalizedValue = source.source->default_;
      _midi_id_to_param[source.source->id] = param_info.id;
    }
  }

  // add fake mod indicator parameters
  ParameterInfo mod_indicator_count_param = {};
  mod_indicator_count_param.unitId = kRootUnitId;
  mod_indicator_count_param.defaultNormalizedValue = 0;
  mod_indicator_count_param.stepCount = mod_indicator_output_param_count + 1;
  mod_indicator_count_param.id = stable_hash(mod_indicator_count_param_guid);
  mod_indicator_count_param.flags = ParameterInfo::kIsReadOnly | ParameterInfo::kIsHidden;
  parameters.addParameter(new Parameter(mod_indicator_count_param));

  for (int i = 0; i < mod_indicator_output_param_count; i++)
  {
    ParameterInfo mod_indicator_param = {};
    mod_indicator_param.stepCount = 0;
    mod_indicator_param.unitId = kRootUnitId;
    mod_indicator_param.defaultNormalizedValue = 0;
    mod_indicator_param.id = stable_hash(mod_indicator_param_guids[i]);
    mod_indicator_param.flags = ParameterInfo::kIsReadOnly | ParameterInfo::kIsHidden;
    parameters.addParameter(new Parameter(mod_indicator_param));
  }

  // make sure no clashes
  std::set<int> hashes = {};
  (void)hashes;
  for (int i = 0; i < parameters.getParameterCount(); i++)
    assert(hashes.insert(parameters.getParameterByIndex(i)->getInfo().id).second);

  return kResultTrue;
}

}