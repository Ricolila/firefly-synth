#pragma once

#include <plugin_base/gui/gui.hpp>
#include <plugin_base/shared/state.hpp>
#include <plugin_base/shared/utility.hpp>
#include <plugin_base.vst3/utility.hpp>

#include <pluginterfaces/gui/iplugview.h>
#include <public.sdk/source/vst/vsteditcontroller.h>

#include <map>
#include <utility>

namespace plugin_base::vst3 {

class pb_editor;

class pb_controller final:
public format_config,
public any_state_listener,
public gui_param_listener,
public Steinberg::Vst::IMidiMapping,
public Steinberg::Vst::EditControllerEx1
{
  // needs to be first, everyone else needs it
  std::unique_ptr<plugin_desc> _desc;
  pb_editor* _editor = {};
  plugin_state _gui_state = {};
  extra_state _extra_state;
  std::map<int, int> _midi_id_to_param = {};

  // module modulation indicator states
  int _mod_indicator_count = 0;
  int _mod_indicator_count_param_tag = -1;
  std::map<int, int> _tag_to_mod_indicator_index = {};
  std::array<bool, mod_indicator_output_param_count> _mod_indicator_param_set = {};
  std::array<int, mod_indicator_output_param_count> _mod_indicator_param_tags = {};
  std::vector<mod_indicator_state> _mod_indicator_states_to_gui = {};
  std::array<mod_indicator_state, mod_indicator_output_param_count> _mod_indicator_states_from_audio = {};

  // see param_state_changed and setParamNormalized
  // when host comes at us with an automation value, that is
  // reported through setParamNormalized, which through a
  // bunch of gui event handlers comes back to us at param_state_changed
  // which then AGAIN calls setParamNormalized and (even worse) performEdit
  // could maybe be fixed by differentiating between host-initiated and
  // user-initiated gui changes, but it's a lot of work, so just go with
  // a reentrancy flag
  bool _inside_set_param_normalized = false;

  void param_state_changed(int index, plain_value plain);

public: 
  pb_controller(plugin_topo const* topo);
  ~pb_controller() { _gui_state.remove_any_listener(this); }

  OBJ_METHODS(pb_controller, EditControllerEx1)
  DEFINE_INTERFACES
    DEF_INTERFACE(IMidiMapping)
  END_DEFINE_INTERFACES(EditControllerEx1)
  REFCOUNT_METHODS(EditControllerEx1)
  PB_PREVENT_ACCIDENTAL_COPY(pb_controller);

  plugin_state& gui_state() { return _gui_state; }
  extra_state& extra_state_() { return _extra_state; }
  void editorDestroyed(Steinberg::Vst::EditorView*) override { _editor = nullptr; }

  void gui_param_end_changes(int index) override;
  void gui_param_begin_changes(int index) override;
  void any_state_changed(int index, plain_value plain) override { param_state_changed(index, plain); }
  void gui_param_changing(int index, plain_value plain) override { param_state_changed(index, plain); }

  std::string format_name() const override { return "VST3"; }
  std::unique_ptr<host_menu> context_menu(int param_id) const override;
  std::filesystem::path resources_folder(std::filesystem::path const& binary_path) const override
  { return binary_path.parent_path().parent_path() / "Resources"; }

  Steinberg::tresult PLUGIN_API getMidiControllerAssignment(
    Steinberg::int32 bus, Steinberg::int16 channel,
    Steinberg::Vst::CtrlNumber number, Steinberg::Vst::ParamID& id) override;

  Steinberg::IPlugView* PLUGIN_API createView(char const* name) override;
  Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) override;
  Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) override;
  Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) override;
  Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream* state) override;
  Steinberg::tresult PLUGIN_API setParamNormalized(Steinberg::Vst::ParamID tag, Steinberg::Vst::ParamValue value) override;
};

}
