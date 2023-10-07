#include <plugin_base/gui/controls.hpp>

using namespace juce;

namespace plugin_base {

param_component::
param_component(plugin_gui* gui, module_desc const* module, param_desc const* param) :
binding_component(gui, module, &param->param->gui.bindings, param->slot), _param(param)
{ _gui->add_plugin_listener(_param->global, this); }

void
param_component::plugin_changed(int index, plain_value plain)
{
  if (index == _param->global)
    own_param_changed(plain);
  else
    binding_component::plugin_changed(index, plain);
}

void
param_component::init()
{
  // Must be called by subclass constructor as we dynamic_cast to Component inside.
  auto const& own_mapping = _gui->desc()->mappings[_param->global];
  plugin_changed(_param->global, own_mapping.value_at(_gui->gui_state()));
  binding_component::init();
}

void
param_textbox::textEditorTextChanged(TextEditor&)
{
  plain_value plain;
  std::string text(getText().toStdString());
  if (!_param->param->domain.text_to_plain(text, plain)) return;
  _last_parsed = text;
  _gui->gui_changed(_param->global, plain);
}

void
param_value_label::own_param_changed(plain_value plain)
{ 
  std::string text = _param->param->domain.plain_to_text(plain);
  if(_both) text = _param->name + " " + text;
  setText(text, dontSendNotification); 
}

void 
param_toggle_button::own_param_changed(plain_value plain)
{
  _checked = plain.step() != 0;
  setToggleState(plain.step() != 0, dontSendNotification);
}

void 
param_toggle_button::buttonStateChanged(Button*)
{ 
  if(_checked == getToggleState()) return;
  plain_value plain = _param->param->domain.raw_to_plain(getToggleState() ? 1 : 0);
  _checked = getToggleState();
  _gui->gui_changed(_param->global, plain);
}

param_textbox::
param_textbox(plugin_gui* gui, module_desc const* module, param_desc const* param) :
param_component(gui, module, param), TextEditor()
{
  addListener(this);
  init();
}

param_toggle_button::
param_toggle_button(plugin_gui* gui, module_desc const* module, param_desc const* param):
param_component(gui, module, param), ToggleButton()
{ 
  auto value = param->param->domain.default_plain();
  _checked = value.step() != 0;
  addListener(this);
  init();
}

param_combobox::
param_combobox(plugin_gui* gui, module_desc const* module, param_desc const* param) :
param_component(gui, module, param), ComboBox()
{
  switch (param->param->domain.type)
  {
  case domain_type::name:
    for (int i = 0; i < param->param->domain.names.size(); i++)
      addItem(param->param->domain.names[i], i + 1);
    break;
  case domain_type::item:
    for (int i = 0; i < param->param->domain.items.size(); i++)
      addItem(param->param->domain.items[i].name, i + 1);
    break;
  case domain_type::step:
  case domain_type::toggle:
    for (int i = param->param->domain.min; i <= param->param->domain.max; i++)
      addItem(std::to_string(i), param->param->domain.min + i + 1);
    break;
  default:
    assert(false);
    break;
  }
  addListener(this);
  setEditableText(false);
  init();
}

param_slider::
param_slider(plugin_gui* gui, module_desc const* module, param_desc const* param) :
param_component(gui, module, param), Slider()
{
  switch (param->param->gui.edit_type)
  {
  case gui_edit_type::knob: setSliderStyle(Slider::RotaryVerticalDrag); break;
  case gui_edit_type::vslider: setSliderStyle(Slider::LinearVertical); break;
  case gui_edit_type::hslider: setSliderStyle(Slider::LinearHorizontal); break;
  default: assert(false); break;
  }
  setTextBoxStyle(Slider::NoTextBox, true, 0, 0);
  if (!param->param->domain.is_real()) setRange(param->param->domain.min, param->param->domain.max, 1);
  else setNormalisableRange(
    NormalisableRange<double>(param->param->domain.min, param->param->domain.max,
    [this](double s, double e, double v) { return _param->param->domain.normalized_to_raw(normalized_value(v)); },
    [this](double s, double e, double v) { return _param->param->domain.raw_to_normalized(v).value(); }));
  setDoubleClickReturnValue(true, param->param->domain.default_raw(), ModifierKeys::noModifiers);
  param_component::init();
}

}