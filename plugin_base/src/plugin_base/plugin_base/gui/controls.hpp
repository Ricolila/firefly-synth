#pragma once

#include <plugin_base/desc/plugin.hpp>
#include <plugin_base/topo/plugin.hpp>
#include <plugin_base/gui/lnf.hpp>
#include <plugin_base/gui/gui.hpp>
#include <plugin_base/gui/components.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

namespace plugin_base {

// same as juce version but does not react to right-click
class text_button:
public juce::TextButton
{
public:
  void mouseUp(juce::MouseEvent const& e) override;
};

// same as juce version but does not react to right-click
class toggle_button :
public juce::ToggleButton
{
public:
  void mouseUp(juce::MouseEvent const& e) override;
};

// same as juce version but loads from resources folder
class image_component :
public juce::ImageComponent
{
public:
  image_component(
  format_config const* config, 
    std::string const& theme,
    std::string const& file_name, 
    juce::RectanglePlacement placement);
};

// button that resizes to text content
class autofit_button :
public text_button,
public autofit_component
{
public:
  autofit_button(lnf* lnf, std::string const& text);
  int fixed_width(int parent_w, int parent_h) const override { return getWidth(); }
  int fixed_height(int parent_w, int parent_h) const override { return getHeight(); }
};

// fixed size checkbox
class autofit_togglebutton :
public toggle_button,
public autofit_component
{
  lnf* const _lnf;
  bool const _tabular;
public:
  int fixed_width(int parent_w, int parent_h) const override { return _lnf->toggle_height(_tabular); }
  int fixed_height(int parent_w, int parent_h) const override { return _lnf->toggle_height(_tabular); }
  autofit_togglebutton(lnf* lnf, bool tabular): _lnf(lnf), _tabular(tabular) { setSize(lnf->toggle_height(_tabular), lnf->toggle_height(_tabular)); }
};

// label that resizes to text content
class autofit_label :
  public juce::Label,
  public autofit_component
{
  bool const _bold;
  bool const _tabular;
  int const _font_height;
public:
  bool bold() const { return _bold; }
  bool tabular() const { return _tabular; }
  int font_height() const { return _font_height; }
  int fixed_width(int parent_w, int parent_h) const override { return getWidth(); }
  int fixed_height(int parent_w, int parent_h) const override { return getHeight(); }
  autofit_label(lnf* lnf, std::string const& reference_text, bool bold = false, int font_height = -1, bool tabular = false);
};

// dropdown that resizes to largest item
class autofit_combobox :
public juce::ComboBox,
public autofit_component
{
  lnf* const _lnf;
  bool const _autofit;
  bool const _tabular;
  float max_text_width(juce::PopupMenu const& menu);
public:
  void autofit();
  autofit_combobox(lnf* lnf, bool autofit, bool tabular) : _lnf(lnf), _autofit(autofit), _tabular(tabular) {}
  int fixed_width(int parent_w, int parent_h) const override { return getWidth(); }
  int fixed_height(int parent_w, int parent_h) const override { return _lnf->combo_height(_tabular); }
};

// button that opens a popupmenu
// basically a combobox that shows a fixed button text
struct menu_button_item { std::string name; std::string group; };
class menu_button :
public text_button
{
  int _selected_index = -1;
  // popup filled by processing these in order
  // should be sorted by group, empty group means no submenu
  std::vector<menu_button_item> _items;
protected:
  void clicked() override;
  std::function<void(int)> _selected_index_changed;
public:  
  std::vector<menu_button_item> const& get_items() const { return _items; }
  void set_items(std::vector<menu_button_item> const& items) { _items = items; }
  void set_selected_index(int index) { _selected_index = std::clamp(index, 0, (int)_items.size() - 1); }
};

// tracks last parameter change
class last_tweaked_label :
public juce::Label,
public any_state_listener
{
  plugin_gui* const _gui;
public:
  last_tweaked_label(plugin_gui* gui, lnf* lnf);
  ~last_tweaked_label() { _gui->gui_state()->remove_any_listener(this); }
  void any_state_changed(int index, plain_value plain) override;
};

// tracks last parameter change
class last_tweaked_editor :
public juce::TextEditor,
public juce::TextEditor::Listener,
public any_state_listener
{
  bool _updating = false;
  int _last_tweaked = -1;
  plugin_state* const _state;
public:
  ~last_tweaked_editor();
  last_tweaked_editor(plugin_state* state, lnf* lnf);

  void textEditorTextChanged(juce::TextEditor&) override;
  void any_state_changed(int index, plain_value plain) override;
};

// TODO remove
// ALSO todo check all of this which is need all kinds of special buttons
// load/save/init/clear patch
class patch_menu :
public text_button
{
  plugin_gui* const _gui;
public:
  void clicked() override;
  patch_menu(plugin_gui* gui);
};

// TODO remove
// binds factory preset to extra_state
class preset_button:
public menu_button,
public extra_state_listener
{
  plugin_gui* const _gui;
  std::vector<resource_item> _presets = {};
public:
  preset_button(plugin_gui* gui);
  void extra_state_changed() override;
  ~preset_button() { _gui->extra_state_()->remove_listener(extra_state_factory_preset_key, this); }
};

// binds theme preset to user config
class theme_button:
public menu_button
{
  plugin_gui* const _gui;
  std::vector<std::string> _themes = {};
public:
  theme_button(plugin_gui* gui);
};

// binding_component that is additionally bound to a single parameter value
// i.e., edit control or a label that displays a plugin parameter value
// also provides host context menu
class param_component:
public binding_component,
public juce::MouseListener
{
  void own_param_changed(plain_value plain);

protected:
  param_desc const* const _param;

public:
  param_desc const* param() const { return _param; }
  void mouseUp(juce::MouseEvent const& e) override;
  void state_changed(int index, plain_value plain) override;
  virtual ~param_component() { _gui->gui_state()->remove_listener(_param->info.global, this); }

protected:
  void init() override;
  virtual void own_param_changed_core(plain_value plain) = 0;
  param_component(plugin_gui* gui, module_desc const* module, param_desc const* param);
};

// just a drag handle for d&d support
class param_drag_label:
public binding_component,
public autofit_component,
public juce::Component
{
  static int const _size = 7;
  lnf* const _lnf;
  param_desc const* const _param;

protected:
  void enablementChanged() { repaint(); }

public:
  void paint(juce::Graphics& g) override;
  juce::MouseCursor getMouseCursor() override;
  void mouseDrag(juce::MouseEvent const& e) override;
  int fixed_width(int parent_w, int parent_h) const override { return _size; }
  int fixed_height(int parent_w, int parent_h) const override { return _size; }
  param_drag_label(plugin_gui* gui, module_desc const* module, param_desc const* param, lnf* lnf);
};

// static parameter name display + d&d support
class param_name_label:
public binding_component,
public autofit_label
{
  param_desc const* const _param;
  param_desc const* const _alternate_drag_param;
  output_desc const* const _alternate_drag_output;
  static std::string label_ref_text(param_desc const* param);
public:
  juce::MouseCursor getMouseCursor() override;
  void mouseDrag(juce::MouseEvent const& e) override;
  param_name_label(
    plugin_gui* gui, module_desc const* module, param_desc const* param, 
    param_desc const* alternate_drag_param, output_desc const* alternate_drag_output, lnf* lnf);
};

// dynamic parameter value display + d&d support
class param_value_label:
public param_component, 
public autofit_label
{
  static std::string value_ref_text(plugin_gui* gui, param_desc const* param);
protected:
  void own_param_changed_core(plain_value plain) override final;
public:
  juce::MouseCursor getMouseCursor() override;
  void mouseDrag(juce::MouseEvent const& e) override;
  param_value_label(plugin_gui* gui, module_desc const* module, param_desc const* param, lnf* lnf);
};

// dynamic module name display
class module_name_label :
public param_component,
public autofit_label
{
protected:
  void own_param_changed_core(plain_value plain) override final;
public:
  module_name_label(plugin_gui* gui, module_desc const* module, param_desc const* param, lnf* lnf);
};

// checkbox bound to single parameter
class param_toggle_button :
public param_component,
public autofit_togglebutton,
public juce::Button::Listener
{
  bool _checked = false;
protected:
  void own_param_changed_core(plain_value plain) override final;

public:
  void buttonClicked(Button*) override {}
  void buttonStateChanged(Button*) override;
  ~param_toggle_button() { removeListener(this); }
  param_toggle_button(plugin_gui* gui, module_desc const* module, param_desc const* param, lnf* lnf);
};

// slider bound to single parameter
// only horizontally autofits knobs not sliders!
class param_slider:
public param_component,
public juce::Slider,
public autofit_component,
public mod_indicator_state_listener
{
  float _min_mod_indicator = -1.0f;
  float _max_mod_indicator = -1.0f;
  double _mod_indicator_activated_time_seconds = {};

protected:
  void own_param_changed_core(plain_value plain) override final
  { setValue(_param->param->domain.plain_to_raw(plain), juce::dontSendNotification); }

public: 
  ~param_slider();
  param_slider(plugin_gui* gui, module_desc const* module, param_desc const* param);

  // param modulation indicators
  void mod_indicator_state_changed(std::vector<mod_indicator_state> const& states) override;
  float min_mod_indicator() const { return _min_mod_indicator; }
  float max_mod_indicator() const { return _max_mod_indicator; }
  double mod_indicator_activated_time_seconds() const { return _mod_indicator_activated_time_seconds; }

  int fixed_width(int parent_w, int parent_h) const override;
  int fixed_height(int parent_w, int parent_h) const override { return -1; }

  void stoppedDragging() override { _gui->param_end_changes(_param->info.global); }
  void startedDragging() override { _gui->param_begin_changes(_param->info.global); }
  void valueChanged() override { _gui->param_changing(_param->info.global, _param->param->domain.raw_to_plain(getValue())); }

  juce::String getTextFromValue(double value) override 
  { return juce::String(_param->info.name + ": ") + juce::Slider::getTextFromValue(value * (_param->param->domain.display == domain_display::percentage ? 100 : 1)); }
};

enum class drop_target_action { none, never, not_now, ok };

// dropdown bound to single parameter
// NOTE: we only support drag-drop onto combos for now
class param_combobox :
public param_component,
public autofit_combobox, 
public juce::DragAndDropTarget,
public juce::ComboBox::Listener
{
  drop_target_action _drop_target_action = drop_target_action::none;
  int get_item_tag(std::string const& item_id) const;

  void update_all_items_enabled_state();

protected:
  void own_param_changed_core(plain_value plain) override final;

public:
  ~param_combobox() { removeListener(this); }
  param_combobox(plugin_gui* gui, module_desc const* module, param_desc const* param, lnf* lnf);

  // d&d support
  drop_target_action get_drop_target_action() const { return _drop_target_action; }
  void itemDropped(juce::DragAndDropTarget::SourceDetails const& details) override;
  void itemDragExit(juce::DragAndDropTarget::SourceDetails const& details) override;
  void itemDragEnter(juce::DragAndDropTarget::SourceDetails const& details) override;
  bool isInterestedInDragSource(juce::DragAndDropTarget::SourceDetails const& details) override;

  void showPopup() override;
  void comboBoxChanged(ComboBox*) override final
  { _gui->param_changed(_param->info.global, _param->param->domain.raw_to_plain(getSelectedId() - 1 + _param->param->domain.min)); }
};

}