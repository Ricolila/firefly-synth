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
public juce::Component 
{
  juce::Component* const _child;
  juce::BorderSize<int> const _margin;
public:
  void resized() override;
  margin_component(juce::Component* child, juce::BorderSize<int> const& margin):
  _child(child), _margin(margin) { add_and_make_visible(*this, *child); }
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
  void currentTabChanged(int index, juce::String const& name) 
  { if (tab_changed != nullptr) tab_changed(index); }
};

// rounded rectangle container
class rounded_container:
public juce::Component,
public autofit_component
{
  bool const _fill;
  int const _radius;
  bool const _vertical;
  juce::Component* _child;
  juce::Colour const _color1;
  juce::Colour const _color2;
public:
  int fixed_width() const override;
  int fixed_height() const override;

  void resized() override;
  void paint(juce::Graphics& g) override;

  rounded_container(
    juce::Component* child, int radius, bool fill, bool vertical,
    juce::Colour const& color1, juce::Colour const& color2):
  _fill(fill), _radius(radius), _vertical(vertical), _child(child), _color1(color1), _color2(color2)
  { add_and_make_visible(*this, *child); }
};

// grid component as opposed to grid layout
// resizes children on resize
class grid_component:
public juce::Component,
public autofit_component
{
  float const _gap_size;
  int const _autofit_row;
  int const _autofit_column;
  gui_dimension const _dimension;
  std::vector<gui_position> _positions = {};

public:
  void resized() override;
  int fixed_width() const override;
  int fixed_height() const override;

  void add(Component& child, gui_position const& position);
  void add(Component& child, bool vertical, int position) 
  { add(child, gui_position { vertical? position: 0, vertical? 0: position }); }

  // Can't intercept mouse as we may be invisible on top of 
  // another grid in case of param or section dependent visibility.
  grid_component(gui_dimension const& dimension, float gap_size, int autofit_row = 0, int autofit_column = 0) :
  _gap_size(gap_size), _dimension(dimension), _autofit_row(autofit_row), _autofit_column(autofit_column)
  { setInterceptsMouseClicks(false, true); }
  grid_component(bool vertical, int count, float gap_size, int autofit_row = 0, int autofit_column = 0) :
  grid_component(gui_dimension { vertical ? count : 1, vertical ? 1 : count }, gap_size, autofit_row, autofit_column) {}
};

// binding_component that hosts a number of plugin parameters
class param_section_grid :
public binding_component,
public grid_component
{
public:
  param_section_grid(plugin_gui* gui, module_desc const* module, param_section const* section, float gap_size):
  binding_component(gui, module, &section->gui.bindings, 0), grid_component(section->gui.dimension, gap_size) { init(); }
};

}