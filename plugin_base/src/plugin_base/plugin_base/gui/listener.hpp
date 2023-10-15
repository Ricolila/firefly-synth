#pragma once

#include <plugin_base/shared/value.hpp>

namespace plugin_base {

class gui_listener
{
public:
  void gui_changed(int index, plain_value plain);
  virtual void gui_end_changes(int index) = 0;
  virtual void gui_begin_changes(int index) = 0;
  virtual void gui_changing(int index, plain_value plain) = 0;
};

}