#pragma once

#include <plugin_base/shared/value.hpp>
#include <juce_gui_basics/juce_gui_basics.h>

namespace plugin_base {

void gui_init();
void gui_terminate();

void add_and_make_visible(
  juce::Component& parent, 
  juce::Component& child);

class gui_listener
{
public:
  void gui_changed(int index, plain_value plain);
  virtual void gui_end_changes(int index) = 0;
  virtual void gui_begin_changes(int index) = 0;
  virtual void gui_changing(int index, plain_value plain) = 0;
};

}