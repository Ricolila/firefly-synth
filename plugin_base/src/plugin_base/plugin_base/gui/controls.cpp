#include <plugin_base/gui/lnf.hpp>
#include <plugin_base/gui/controls.hpp>
#include <plugin_base/shared/io_user.hpp>

#include <limits>
#include <algorithm>

using namespace juce;

namespace plugin_base {

static bool
is_enabled_mod_source(
  Component const& component, module_desc const* module, 
  param_desc const* param, param_desc const* alternate_drag_param)
{
  param_desc const* drag_param = alternate_drag_param != nullptr ? alternate_drag_param : param;
  if (!component.isEnabled()) return false;
  if (!drag_param->param->dsp.can_modulate(module->info.slot)) return false;
  return true;
}

static MouseCursor
drag_source_cursor(
  Component const& component, module_desc const* module, 
  param_desc const* param, param_desc const* alternate_drag_param, MouseCursor base_cursor)
{
  if (!is_enabled_mod_source(component, module, param, alternate_drag_param)) return base_cursor;
  return MouseCursor::DraggingHandCursor;
}

static void
drag_source_start_drag(
  Component& component, Font const& font, Colour color, module_desc const* module, 
  param_desc const* param, param_desc const* alternate_drag_param)
{
  param_desc const* drag_param = alternate_drag_param != nullptr ? alternate_drag_param : param;
  if (!is_enabled_mod_source(component, module, param, alternate_drag_param)) return;
  auto* container = DragAndDropContainer::findParentDragContainerFor(&component);
  if (container == nullptr) return;
  if (container->isDragAndDropActive()) return;
  ScaledImage drag_image = make_drag_source_image(font, drag_param->info.name, color);
  Point<int> offset(drag_image.getImage().getWidth() / 2 + 10, drag_image.getImage().getHeight() / 2 + 10);
  container->startDragging(juce::String(drag_param->info.id), &component, drag_image, false, &offset);
}

// either builds up a popupmenu or figures out the index of the domain item in the combobox (yeah its ugly)
static void
fill_popup_menu(
  param_domain const& domain, PopupMenu* menu, gui_submenu const* data, Colour const& subheader_color, 
  int* current_domain_index, int* current_combo_index, std::map<int, int>* domain_index_to_combo_index)
{
  assert((menu != nullptr) == (current_combo_index == nullptr));
  assert((menu != nullptr) == (current_domain_index == nullptr));
  assert((menu != nullptr) == (domain_index_to_combo_index == nullptr));

  if(menu) menu->clear();
  for (int i = 0; i < data->indices.size(); i++)
  {
    if(menu) 
      menu->addItem(data->indices[i] + 1, domain.raw_to_text(false, data->indices[i]));
    else
    {
      (*domain_index_to_combo_index)[*current_domain_index] = *current_combo_index;
      (*current_combo_index)++;
      (*current_domain_index)++;
    }
  }
  for(int i = 0; i < data->children.size(); i++)
  {
    if (data->children[i]->is_subheader)
    {
      if(menu)
        menu->addColouredItem(-1, data->children[i]->name, subheader_color, false, false, nullptr);
      else
        (*current_combo_index)++;
    }
    else
    {
      if (menu)
      {
        PopupMenu child;
        fill_popup_menu(domain, &child, data->children[i].get(), subheader_color, current_domain_index, current_combo_index, domain_index_to_combo_index);
        menu->addSubMenu(data->children[i]->name, child);
      } else
        fill_popup_menu(domain, nullptr, data->children[i].get(), subheader_color, current_domain_index, current_combo_index, domain_index_to_combo_index);
    }
  }
}

static void
fill_host_menu(PopupMenu& menu, std::vector<std::shared_ptr<host_menu_item>> const& children)
{
  for(int i = 0; i < children.size(); i++)
  {
    auto const& child = *children[i].get();
    if(child.flags & host_menu_flags_separator)
      menu.addSeparator();
    else if(child.children.empty())
      menu.addItem(child.tag + 1, child.name, child.flags & host_menu_flags_enabled, child.flags & host_menu_flags_checked);
    else
    {
      PopupMenu submenu;
      fill_host_menu(submenu, child.children);
      menu.addSubMenu(child.name, submenu, child.flags & host_menu_flags_enabled);
    }
  }
}

void 
text_button::mouseUp(MouseEvent const& e)
{
  if(!e.mods.isRightButtonDown())
    TextButton::mouseUp(e);
}

void
toggle_button::mouseUp(MouseEvent const& e)
{
  if (!e.mods.isRightButtonDown())
    ToggleButton::mouseUp(e);
}

void
menu_button::clicked()
{
  PopupMenu menu;
  PopupMenu sub_menu;
  PopupMenu* current_menu = &menu;
  std::string current_group = "";
  menu.setLookAndFeel(&getLookAndFeel());
  for (int i = 0; i < _items.size(); i++)
  {
    if (_items[i].group != current_group)
    {
      sub_menu = PopupMenu();
      current_menu = &sub_menu;
      current_group = _items[i].group;
    }
    current_menu->addItem(i + 1, _items[i].name, true, i == _selected_index);
    if(!_items[i].group.empty() && (i == _items.size() - 1 || _items[i].group != _items[i + 1].group))
      menu.addSubMenu(_items[i].group, sub_menu, true);
  }
  PopupMenu::Options options;
  options = options.withTargetComponent(this);
  menu.showMenuAsync(options, [this](int id) {
    int index = id - 1;
    if (index == _selected_index) return;
    _selected_index = index;
    if (_selected_index_changed != nullptr)
      _selected_index_changed(index);
  });
}

static std::string
param_slot_name(param_desc const* param)
{
  if(param->param->gui.display_formatter != nullptr)
    return param->param->gui.display_formatter(*param);
  std::string result = param->param->info.tag.display_name;
  if (param->param->info.slot_count > 1)
    result += " " + std::to_string(param->info.slot + 1);
  return result;
}

param_drag_label::
param_drag_label(plugin_gui* gui, module_desc const* module, param_desc const* param, lnf* lnf) :
binding_component(gui, module, &param->param->gui.bindings, param->info.slot),
_lnf(lnf), _param(param)
{ init(); }

MouseCursor 
param_drag_label::getMouseCursor()
{ return drag_source_cursor(*this, _module, _param, nullptr, Component::getMouseCursor()); }

void 
param_drag_label::mouseDrag(juce::MouseEvent const& e)
{ drag_source_start_drag(*this, _lnf->font(), _lnf->colors().bubble_outline, _module, _param, nullptr); }

void
param_drag_label::paint(Graphics& g)
{
  float w = getWidth();
  float h = getHeight();
  float x = (w - _size) / 2.0f;
  float y = (h - _size) / 2.0f;
  if(isEnabled())
    g.setColour(_lnf->colors().control_text);
  else
    g.setColour(_lnf->colors().control_text.darker(0.67f));
  g.fillEllipse(x, y, _size, _size);
}

std::string 
param_name_label::label_ref_text(param_desc const* param)
{
  auto const& ref_text = param->param->gui.label_reference_text;
  return ref_text.size()? ref_text: param_slot_name(param);
}

param_name_label::
param_name_label(plugin_gui* gui, module_desc const* module, param_desc const* param, param_desc const* alternate_drag_param, lnf* lnf):
binding_component(gui, module, &param->param->gui.bindings, param->info.slot),
autofit_label(lnf, label_ref_text(param)),
_param(param), _alternate_drag_param(alternate_drag_param)
{
  std::string name = param_slot_name(param);
  setText(name, juce::dontSendNotification); 
  init();
}

MouseCursor 
param_name_label::getMouseCursor()
{ return drag_source_cursor(*this, _module, _param, _alternate_drag_param, Component::getMouseCursor()); }

void 
param_name_label::mouseDrag(juce::MouseEvent const& e)
{ 
  auto& lnf_ = dynamic_cast<plugin_base::lnf&>(getLookAndFeel());
  drag_source_start_drag(*this, lnf_.font(), lnf_.colors().bubble_outline, _module, _param, _alternate_drag_param);
}

std::string
param_value_label::value_ref_text(plugin_gui* gui, param_desc const* param)
{
  auto const& ref_text = param->param->gui.value_reference_text;
  if (ref_text.size()) return ref_text;
  auto plain = param->param->domain.raw_to_plain(param->param->domain.max);
  return gui->gui_state()->plain_to_text_at_index(false, param->info.global, plain);
}

// Just guess max value is representative of the longest text.
param_value_label::
param_value_label(plugin_gui* gui, module_desc const* module, param_desc const* param, lnf* lnf) :
param_component(gui, module, param), 
autofit_label(lnf, value_ref_text(gui, param))
{ init(); }

void
param_value_label::own_param_changed(plain_value plain)
{ 
  std::string text = _gui->gui_state()->plain_to_text_at_index(false, _param->info.global, plain);
  setText(text, dontSendNotification); 
  setTooltip(_param->info.name + ": " + text);
}

MouseCursor 
param_value_label::getMouseCursor()
{ return drag_source_cursor(*this, _module, _param, nullptr, Component::getMouseCursor()); }

void 
param_value_label::mouseDrag(juce::MouseEvent const& e)
{ 
  auto& lnf_ = dynamic_cast<plugin_base::lnf&>(getLookAndFeel());
  drag_source_start_drag(*this, lnf_.font(), lnf_.colors().bubble_outline, _module, _param, nullptr);
}

last_tweaked_label::
last_tweaked_label(plugin_state const* state):
_state(state)
{
  state->add_any_listener(this);
  any_state_changed(0, state->get_plain_at_index(0));
}

void 
last_tweaked_label::any_state_changed(int index, plain_value plain)
{
  if(_state->desc().params[index]->param->dsp.direction == param_direction::output) return;
  setText(_state->desc().params[index]->full_name, dontSendNotification);
}

last_tweaked_editor::
last_tweaked_editor(plugin_state* state, lnf* lnf) :
_state(state)
{
  addListener(this);
  setFont(lnf->font());
  state->add_any_listener(this);
  setJustification(Justification::centredRight);
  any_state_changed(0, state->get_plain_at_index(0));
  setColour(TextEditor::ColourIds::textColourId, lnf->colors().control_text);
  setColour(TextEditor::ColourIds::highlightedTextColourId, lnf->colors().control_text);
}

last_tweaked_editor::
~last_tweaked_editor()
{
  _state->remove_any_listener(this);
  removeListener(this);
}

void
last_tweaked_editor::any_state_changed(int index, plain_value plain)
{
  if(_updating) return;
  if (_state->desc().params[index]->param->dsp.direction == param_direction::output) return;
  _last_tweaked = index;
  setText(String(_state->desc().params[index]->param->domain.plain_to_text(false, plain)), dontSendNotification);
}

void 
last_tweaked_editor::textEditorTextChanged(TextEditor& te)
{
  plain_value plain;
  if(_last_tweaked == -1) return;
  std::string value = te.getText().toStdString();
  if(!_state->desc().params[_last_tweaked]->param->domain.text_to_plain(false, value, plain)) return;
  _updating = true;
  _state->set_plain_at_index(_last_tweaked, plain);
  _updating = false;
}

preset_button::
preset_button(plugin_gui* gui) :
_gui(gui), _presets(gui->gui_state()->desc().presets())
{ 
  set_items(vector_map(_presets, [](auto const& p) { return menu_button_item { p.name, p.group }; }));
  extra_state_changed();
  setButtonText("Preset");
  _gui->extra_state_()->add_listener(factory_preset_key, this);
  _selected_index_changed = [this](int index) {
    index = std::clamp(index, 0, (int)get_items().size());
    _gui->extra_state_()->set_text(factory_preset_key, get_items()[index].name);
    _gui->load_patch(_presets[index].path, true);
  };
}

void 
preset_button::extra_state_changed()
{
  std::string selected_preset = _gui->extra_state_()->get_text(factory_preset_key, "");
  for(int i = 0; i < get_items().size(); i++)
    if(get_items()[i].name == selected_preset)
    {
      set_selected_index(i);
      break;
    }
}

theme_button::
theme_button(plugin_gui* gui) :
_gui(gui), _themes(gui->gui_state()->desc().themes())
{ 
  auto const& topo = *gui->gui_state()->desc().plugin;
  std::string default_theme = topo.gui.default_theme;
  std::string theme = user_io_load_list(topo, user_io::base, user_state_theme_key, default_theme, _themes);
  set_items(vector_map(_themes, [](auto const& t) { return menu_button_item { t, ""}; }));
  setButtonText("Theme");
  for(int i = 0; i < _themes.size(); i++)
    if(_themes[i] == theme)
      set_selected_index(i);
  _selected_index_changed = [this](int index) {
    index = std::clamp(index, 0, (int)get_items().size());
    // DONT run synchronously because theme_changed will destroy [this]!
    user_io_save_list(*_gui->gui_state()->desc().plugin, user_io::base, user_state_theme_key, _themes[index]);
    MessageManager::callAsync([gui = _gui, theme_name = _themes[index]]() { gui->theme_changed(theme_name); });
  };
}

image_component::
image_component(
  format_config const* config, 
  std::string const& theme,
  std::string const& file_name, 
  RectanglePlacement placement)
{
  String path((get_resource_location(config) / resource_folder_themes / theme / file_name).string());
  setImage(ImageCache::getFromFile(path), placement);
}

autofit_label::
autofit_label(lnf* lnf, std::string const& reference_text, bool bold, int height, bool tabular):
_bold(bold), _tabular(tabular), _font_height(height)
{
  setBorderSize({ 1, 2, 1, 4 });
  auto label_font = lnf->getLabelFont(*this);
  if(bold) label_font = label_font.boldened();
  if(height != -1) label_font = label_font.withHeight(height);
  float th = label_font.getHeight();
  float tw = label_font.getStringWidthFloat(reference_text);
  float nw = std::ceil(tw) + getBorderSize().getLeftAndRight();
  setSize(nw, std::ceil(th) + getBorderSize().getTopAndBottom());
  setText(reference_text, dontSendNotification);
}

autofit_button::
autofit_button(lnf* lnf, std::string const& text)
{
  float vmargin = 6.0f;
  float hmargin = 16.0f;
  setButtonText(text);
  auto const& button_font = lnf->getTextButtonFont(*this, getHeight());
  float th = button_font.getHeight();
  float tw = button_font.getStringWidthFloat(getButtonText());
  setSize(std::ceil(tw) + hmargin, std::ceil(th) + vmargin);
}

float 
autofit_combobox::max_text_width(PopupMenu const& menu)
{
  float result = 0;
  PopupMenu::MenuItemIterator iter(menu);
  auto const& font = _lnf->getComboBoxFont(*this);
  while(iter.next())
  {
    auto text = iter.getItem().text;
    auto text_width = font.getStringWidthFloat(text);
    if(iter.getItem().subMenu)
      result = std::max(result, max_text_width(*iter.getItem().subMenu));
    else
      result = std::max(result, text_width);
  }
  return result;
}

void
autofit_combobox::autofit()
{
  if(!_autofit) return;

  int const hpadding = 19;
  auto const& font = _lnf->getComboBoxFont(*this);
  float text_height = font.getHeight();
  float max_width = max_text_width(*getRootMenu());
  setSize(std::ceil(max_width + hpadding), std::ceil(text_height));
}

param_component::
param_component(plugin_gui* gui, module_desc const* module, param_desc const* param) :
binding_component(gui, module, &param->param->gui.bindings, param->info.slot), _param(param)
{ _gui->gui_state()->add_listener(_param->info.global, this); }

void
param_component::state_changed(int index, plain_value plain)
{
  if (index == _param->info.global)
    own_param_changed(plain);
  else
    binding_component::state_changed(index, plain);
}

void
param_component::init()
{
  // Must be called by subclass constructor as we dynamic_cast to Component inside.
  state_changed(_param->info.global, _gui->gui_state()->get_plain_at_index(_param->info.global));

  binding_component::init();

  // note that we never deregister since we cannot dynamic_cast in the destructor
  // should not be a problem as we are our own listener
  dynamic_cast<Component&>(*this).addMouseListener(this, true);
}

void 
param_component::mouseUp(MouseEvent const& evt)
{
  if(!evt.mods.isRightButtonDown()) return;
  auto& self = dynamic_cast<Component&>(*this);
  if(!self.isEnabled()) return;
  if(_param->param->dsp.direction == param_direction::output) return;

  PopupMenu menu;
  PopupMenu::Options options;
  options = options.withTargetComponent(self);
  menu.setLookAndFeel(&self.getLookAndFeel());

  bool have_menu = false;
  auto lnf = dynamic_cast<plugin_base::lnf*>(&self.getLookAndFeel());
  auto colors = lnf->module_gui_colors(_module->module->info.tag.full_name);

  std::unique_ptr<param_menu_handler> plugin_handler = {};
  param_menu_handler_factory plugin_handler_factory = _param->param->gui.menu_handler_factory;
  if(plugin_handler_factory)
    plugin_handler = plugin_handler_factory(_gui->gui_state());
  if (plugin_handler)
  {
    auto plugin_menus = plugin_handler->menus();
    for (int m = 0; m < plugin_menus.size(); m++)
    {
      have_menu = true;
      if (!plugin_menus[m].name.empty())
        menu.addColouredItem(-1, plugin_menus[m].name, colors.tab_text, false, false, nullptr);
      for (int e = 0; e < plugin_menus[m].entries.size(); e++)
        menu.addItem(10000 + m * 1000 + e * 100, plugin_menus[m].entries[e].title);
    }
  }

  auto host_menu = _gui->gui_state()->desc().config->context_menu(_param->info.id_hash);
  if (host_menu && host_menu->root.children.size())
  {
    have_menu = true;
    menu.addColouredItem(-1, "Host", colors.tab_text, false, false, nullptr);
    fill_host_menu(menu, host_menu->root.children);
  }

  if(!have_menu) return;

  menu.showMenuAsync(options, [this, host_menu = host_menu.release(), plugin_handler = plugin_handler.release()](int id) {
    if(1 <= id && id < 10000) host_menu->clicked(id - 1);
    else if(10000 <= id)
    {
      auto plugin_menus = plugin_handler->menus();
      auto const& menu = plugin_menus[(id - 10000) / 1000];
      auto const& action_entry = menu.entries[((id - 10000) % 1000) / 100];
      _gui->gui_state()->begin_undo_region();
      std::string item = plugin_handler->execute(menu.menu_id, action_entry.action, _module->info.topo, _module->info.slot, _param->info.topo, _param->info.slot);      
      _gui->gui_state()->end_undo_region(action_entry.title, item);
    }
    delete host_menu;
    delete plugin_handler;
  });
}

module_name_label::
module_name_label(plugin_gui* gui, module_desc const* module, param_desc const* param, lnf* lnf) :
param_component(gui, module, param), 
autofit_label(lnf, param->param->gui.label_reference_text)
{ init(); }

void
module_name_label::own_param_changed(plain_value plain)
{ 
  auto const& desc = _gui->gui_state()->desc().modules[plain.step()];
  if (!desc.module->gui.visible)
  {
    setTooltip("");
    setText("", dontSendNotification);
    return;
  }
  std::string full_name = desc.module->info.tag.full_name;
  std::string display_name = desc.module->info.tag.display_name;
  if(desc.module->info.slot_count > 1)
  {
    std::string slot = std::to_string(desc.info.slot + 1);
    full_name += " " + slot;
    display_name += " " + slot;
  }
  setTooltip(full_name);
  setText(display_name, dontSendNotification);
}

void 
param_toggle_button::own_param_changed(plain_value plain)
{
  _checked = plain.step() != 0;
  setToggleState(plain.step() != 0, dontSendNotification);
  setTooltip(_param->info.name + ": " + _param->param->domain.plain_to_text(false, plain));
}

void 
param_toggle_button::buttonStateChanged(Button*)
{ 
  if(_checked == getToggleState()) return;
  plain_value plain = _param->param->domain.raw_to_plain(getToggleState() ? 1 : 0);
  _checked = getToggleState();
  _gui->param_changed(_param->info.global, plain);
}

param_toggle_button::
param_toggle_button(plugin_gui* gui, module_desc const* module, param_desc const* param, lnf* lnf):
param_component(gui, module, param), autofit_togglebutton(lnf, param->param->gui.tabular)
{
  auto value = param->param->domain.default_plain(module->info.slot, param->info.slot);
  setTooltip(_param->info.name + ": " + _param->param->domain.plain_to_text(false, value));
  _checked = value.step() != 0;
  addListener(this);
  init();
}

param_slider::
param_slider(plugin_gui* gui, module_desc const* module, param_desc const* param) :
param_component(gui, module, param), Slider()
{
  setPopupDisplayEnabled(true, true, nullptr);  
  switch (param->param->gui.edit_type)
  {
  case gui_edit_type::knob: setSliderStyle(Slider::RotaryVerticalDrag); break;
  case gui_edit_type::vslider: setSliderStyle(Slider::LinearVertical); break;
  case gui_edit_type::hslider: setSliderStyle(Slider::LinearHorizontal); break;
  default: assert(false); break;
  }

  setTextBoxStyle(Slider::NoTextBox, true, 0, 0);
  if(param->param->domain.unit.size())
    setTextValueSuffix(" " + param->param->domain.unit);
  setNumDecimalPlacesToDisplay(param->param->domain.precision);
  if (!param->param->domain.is_real()) setRange(param->param->domain.min, param->param->domain.max, 1);
  else setNormalisableRange(
    NormalisableRange<double>(param->param->domain.min, param->param->domain.max,
    [this](double s, double e, double v) { return _param->param->domain.normalized_to_raw(normalized_value(v)); },
    [this](double s, double e, double v) { return _param->param->domain.raw_to_normalized(v).value(); }));
  setDoubleClickReturnValue(true, param->param->domain.default_raw(_module->info.slot, _param->info.slot), ModifierKeys::noModifiers);
  param_component::init();
}

int 
param_slider::fixed_width(int parent_w, int parent_h) const
{
  if(_param->param->gui.edit_type != gui_edit_type::knob) return -1;
  return parent_h;
}

param_combobox::
param_combobox(plugin_gui* gui, module_desc const* module, param_desc const* param, lnf* lnf) :
param_component(gui, module, param), 
autofit_combobox(lnf, param->param->gui.edit_type == gui_edit_type::autofit_list, param->param->gui.tabular)
{
  auto const& domain = param->param->domain;
  auto const& param_gui = param->param->gui;
  auto colors = lnf->module_gui_colors(module->module->info.tag.full_name);
  if(!param_gui.submenu)
  {
    int index = 0;
    for(int i = domain.min; i <= domain.max; i++, index++)
      addItem(domain.raw_to_text(false, i), index + 1);
  }
  else
  {
    auto const& color = colors.tab_text;
    fill_popup_menu(domain, getRootMenu(), param_gui.submenu.get(), color, nullptr, nullptr, nullptr);
  }
  autofit();
  addListener(this);
  setEditableText(false);
  init();
}

void 
param_combobox::own_param_changed(plain_value plain)
{
  setSelectedId(plain.step() + 1 - _param->param->domain.min, dontSendNotification);
  std::string value = _param->param->domain.plain_to_text(false, plain);
  setTooltip(_param->info.name + ": " + value);
}

void 
param_combobox::showPopup()
{
  auto const& items = _param->param->domain.items;
  for (int i = 0; i < items.size(); i++)
    setItemEnabled(i + 1, true);

  if(_param->param->gui.item_enabled.is_param_bound())
  {
    auto m = _param->param->gui.item_enabled.param;
    if(m.param_slot == gui_item_binding::match_param_slot) m.param_slot = _param->info.slot;
    auto other = _gui->gui_state()->get_plain_at(m.module_index, m.module_slot, m.param_index, m.param_slot);
    for(int i = 0; i < items.size(); i++)
      setItemEnabled(i + 1, _param->param->gui.item_enabled.selector(other.step(), _param->param->domain.min + i));
  }
  if (_param->param->gui.item_enabled.auto_bind)
  {
    auto const& topo = *_gui->gui_state()->desc().plugin;
    for (int i = 0; i < items.size(); i++)
    {
      bool enabled = true;
      auto const& that_param = items[i].param_topo;
      auto const& that_topo = topo.modules[that_param.module_index].params[that_param.param_index];
      if(that_topo.gui.bindings.enabled.slot_selector != nullptr)
        enabled &= that_topo.gui.bindings.enabled.slot_selector(that_param.module_index);
      if (that_topo.gui.bindings.visible.slot_selector != nullptr)
        enabled &= that_topo.gui.bindings.visible.slot_selector(that_param.module_index);
      if (that_topo.gui.bindings.enabled.param_selector != nullptr)
      {
        std::vector<int> that_values;
        for(int j = 0; j < that_topo.gui.bindings.enabled.params.size(); j++)
        {
          param_topo_mapping m = that_param;
          m.param_index = that_topo.gui.bindings.enabled.params[j];
          that_values.push_back(_gui->gui_state()->get_plain_at(m).step());
        }
        enabled &= that_topo.gui.bindings.enabled.param_selector(that_values);
      }
      if (that_topo.gui.bindings.visible.param_selector != nullptr)
      {
        std::vector<int> that_values;
        for (int j = 0; j < that_topo.gui.bindings.visible.params.size(); j++)
        {
          param_topo_mapping m = that_param;
          m.param_index = that_topo.gui.bindings.visible.params[j];
          that_values.push_back(_gui->gui_state()->get_plain_at(m).step());
        }
        enabled &= that_topo.gui.bindings.visible.param_selector(that_values);
      }

      // note that is is possible to combine auto_bind
      // with a custom enabled selector so we need to check both
      setItemEnabled(i + 1, isItemEnabled(i + 1) && enabled);
    }
  }
  ComboBox::showPopup();
}

int 
param_combobox::get_item_index(std::string const& item_id) const
{
  int result = -1;
  assert(_param->param->gui.enable_dropdown_drop_target);
  for (int i = 0; i < _param->param->domain.items.size(); i++)
    if (item_id == _param->param->domain.items[i].id)
    {
      result = i;
      break;
    }

  if (result == -1) return result;
  
  // need to take subheaders into account
  if (_param->param->gui.submenu)
  {
    int current_combo_index = 0;
    int current_domain_index = 0;
    std::map<int, int> domain_index_to_combo_index = {};
    fill_popup_menu(_param->param->domain, nullptr, _param->param->gui.submenu.get(), Colour(), 
      &current_domain_index, &current_combo_index, &domain_index_to_combo_index);
    result = domain_index_to_combo_index.at(result);
  }

  return result;
}

void
param_combobox::itemDragExit(DragAndDropTarget::SourceDetails const& details)
{
  _drop_target_action = drop_target_action::none;
  resized(); // needed to trigger positionComboBoxText
  repaint();
}

void
param_combobox::itemDragEnter(DragAndDropTarget::SourceDetails const& details)
{
  std::string source_id = details.description.toString().toStdString();
  _drop_target_action = get_item_index(source_id) != -1 ? drop_target_action::allow : drop_target_action::deny;
  resized(); // needed to trigger positionComboBoxText
  repaint();
}

void 
param_combobox::itemDropped(DragAndDropTarget::SourceDetails const& details)
{
  int index = get_item_index(details.description.toString().toStdString());
  if (index != -1) setSelectedItemIndex(index);
  itemDragExit(details);
}

bool 
param_combobox::isInterestedInDragSource(DragAndDropTarget::SourceDetails const& details)
{
  return _param->param->gui.enable_dropdown_drop_target;
}

}