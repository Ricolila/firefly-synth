#pragma once

#include <plugin_base/desc/plugin.hpp>
#include <plugin_base/topo/plugin.hpp>
#include <plugin_base/gui/gui.hpp>
#include <plugin_base/gui/lnf.hpp>
#include <plugin_base/gui/components.hpp>

#include <juce_gui_basics/juce_gui_basics.h>

namespace plugin_base {

// needs child to be able to autofit in 1 direction
class autofit_viewport:
public juce::Viewport
{
  lnf* const _lnf;
public:
  void resized() override;
  autofit_viewport(lnf* lnf) : _lnf(lnf) {}
};

// adds some margin around another component
class margin_component :
public juce::Component,
public autofit_component
{
  juce::Component* const _child;
  juce::BorderSize<int> const _margin;
public:
  void resized() override;
  int fixed_width(int parent_w, int parent_h) const override;
  int fixed_height(int parent_w, int parent_h) const override;
  margin_component(juce::Component* child, juce::BorderSize<int> const& margin):
  _child(child), _margin(margin) { add_and_make_visible(*this, *child); }
};

// displays a child component based on extra state changes
class extra_state_container:
public juce::Component,
public extra_state_listener
{
  plugin_gui* const _gui = {};
  std::string const _state_key = {};
  std::unique_ptr<juce::Component> _child = {};
protected:
  plugin_gui* const gui() { return _gui; }
  std::string const& state_key() const { return _state_key; }
  virtual std::unique_ptr<juce::Component> create_child() = 0;
public:
  void extra_state_changed() override;
  extra_state_container(plugin_gui* gui, std::string const& state_key);
  void resized() override { if (_child) { _child->setBounds(getLocalBounds()); } }
  virtual ~extra_state_container() { _gui->extra_state_()->remove_listener(_state_key, this); }
};

// displays a child component based on module tab changes
class tabbed_module_section_container:
public extra_state_container
{
private:
  int const _section_index;
  std::function<std::unique_ptr<juce::Component>(int module_index)> _factory;
protected:
  std::unique_ptr<juce::Component> create_child() override;
public:
  tabbed_module_section_container(
    plugin_gui* gui, int section_index,
    std::function<std::unique_ptr<juce::Component>(int module_index)> factory);
};

// tab component with persistent selection and change listener
class tab_component :
public juce::TabbedComponent,
public extra_state_listener
{
  extra_state* const _state;
  std::string const _storage_id;
public:
  std::function<void(int)> tab_changed;
  ~tab_component();
  tab_component(extra_state* state, std::string const& storage_id, juce::TabbedButtonBar::Orientation orientation);

  void extra_state_changed() override
  { setCurrentTabIndex(std::clamp(_state->get_num(_storage_id, 0), 0, getNumTabs())); }
  void currentTabChanged(int index, juce::String const& name) override
  { if (tab_changed != nullptr) tab_changed(index); }
};

// rounded rectangle container
enum class rounded_container_mode { fill, stroke, both };
class rounded_container:
public juce::Component,
public autofit_component
{
  int const _radius;
  int const _vpadding;
  int const _margin_left;
  bool const _vertical;
  juce::Component* _child;
  juce::Colour const _background1;
  juce::Colour const _background2;
  juce::Colour const _outline1;
  juce::Colour const _outline2;
  rounded_container_mode const _mode;

  int radius_and_padding() const { return _radius + _vpadding; }

public:
  void resized() override;
  void paint(juce::Graphics& g) override;
  int fixed_width(int parent_w, int parent_h) const override;
  int fixed_height(int parent_w, int parent_h) const override;

  rounded_container(
    juce::Component* child, int radius, int vpadding, int margin_left, bool vertical, rounded_container_mode mode,
    juce::Colour const& background1, juce::Colour const& background2, juce::Colour const& outline1, juce::Colour const& outline2):
  _radius(radius), _vpadding(vpadding), _margin_left(margin_left), _vertical(vertical),
  _background1(background1), _background2(background2), _outline1(outline1), _outline2(outline2), _mode(mode)
  { add_and_make_visible(*this, *child); }
};

// hosts a number of section params
class param_section_container:
public binding_component,
public rounded_container
{
public:
  param_section_container(plugin_gui* gui, lnf* lnf, module_desc const* module, param_section const* section, juce::Component* child, int margin_left);
};

// grid component as opposed to grid layout
// resizes children on resize
class grid_component:
public juce::Component,
public autofit_component
{
  float const _vgap_size;
  float const _hgap_size;
  int const _autofit_row;
  int const _autofit_column;
  gui_dimension const _dimension;
  std::vector<gui_position> _positions = {};

public:
  void resized() override;
  int fixed_width(int parent_w, int parent_h) const override;
  int fixed_height(int parent_w, int parent_h) const override;

  void add(Component& child, gui_position const& position);
  void add(Component& child, bool vertical, int position) 
  { add(child, gui_position { vertical? position: 0, vertical? 0: position }); }

  // Can't intercept mouse as we may be invisible on top of 
  // another grid in case of param or section dependent visibility.
  grid_component(gui_dimension const& dimension, float vgap_size, float hgap_size, int autofit_row, int autofit_column) :
  _vgap_size(vgap_size), _hgap_size(hgap_size), _dimension(dimension), _autofit_row(autofit_row), _autofit_column(autofit_column)
  { setInterceptsMouseClicks(false, true); }
  grid_component(bool vertical, int count, float vgap_size, float hgap_size, int autofit_row, int autofit_column) :
  grid_component(gui_dimension { vertical ? count : 1, vertical ? 1 : count }, vgap_size, hgap_size, autofit_row, autofit_column) {}
};

}