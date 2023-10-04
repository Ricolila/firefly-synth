#pragma once

#include <vector>

namespace plugin_base {

enum class gui_label_justify { near, center, far };
enum class gui_label_align { top, bottom, left, right };
enum class gui_label_contents { none, name, value, both };

enum class gui_layout { single, horizontal, vertical, tabbed };
enum class gui_edit_type { toggle, list, text, knob, hslider, vslider };

typedef bool(*
gui_binding_selector)(
  std::vector<int> const& values, 
  std::vector<int> const& context);

// position in parent grid
struct gui_position final {
  int row = -1;
  int column = -1;
  int row_span = 1;
  int column_span = 1;
};

// binding to enabled/visible
struct gui_binding final {
  std::vector<int> params = {};
  std::vector<int> context = {};
  gui_binding_selector selector = {};
  INF_DECLARE_MOVE_ONLY_DEFAULT_CTOR(gui_binding);
};

// binding to enabled/visible
struct gui_bindings final {
  gui_binding enabled;
  gui_binding visible;
  INF_DECLARE_MOVE_ONLY_DEFAULT_CTOR(gui_bindings);
};

// dimensions of own grid (relative distribution)
struct gui_dimension final {
  std::vector<int> row_sizes = { 1 };
  std::vector<int> column_sizes = { 1 };

  gui_dimension() = default;
  gui_dimension(gui_dimension const&) = default;
  gui_dimension(int row_count, int column_count):
  row_sizes(row_count, 1), column_sizes(column_count, 1) {}
  gui_dimension(std::vector<int> const& row_sizes, std::vector<int> const& column_sizes):
  row_sizes(row_sizes), column_sizes(column_sizes) {}
};

}
