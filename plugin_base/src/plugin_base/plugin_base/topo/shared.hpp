#pragma once

#include <plugin_base/shared/utility.hpp>
#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>
#include <string>
#include <algorithm>
#include <functional>

namespace plugin_base {

struct module_topo;
struct plugin_topo;
struct gui_dimension;

// just a guess for validation, increase if needed
inline int constexpr topo_max = 1024;

enum class gui_scroll_mode { none, vertical };
enum class gui_label_justify { near, far, center };
enum class gui_label_align { top, bottom, left, right };
enum class gui_label_contents { none, name, value, drag, no_drag }; // no drag = none, but be explicit about it
enum class gui_label_edit_cell_split { no_split, horizontal, vertical };
enum class gui_edit_type { 
  none, toggle, list, autofit_list, knob, hslider, vslider, output_label_left, 
  output_label_center, output_toggle, output_module_name, output_meter };

typedef std::function<bool(int module_slot)>
gui_slot_binding_selector;
typedef std::function<bool(std::vector<int> const& vs)>
gui_param_binding_selector;
typedef std::function<bool(int)>
gui_global_param_binding_selector;

// for custom module/param context menus
struct custom_menu_entry { int action; std::string title; };
struct custom_menu { int menu_id; std::string name; std::vector<custom_menu_entry> entries; };

// plugin version
struct plugin_version final {
  int major;
  int minor;
  int patch;
};

inline bool 
operator<(plugin_version const& l, plugin_version const& r)
{
  if(l.major < r.major) return true;
  if(l.major > r.major) return false;
  if (l.minor < r.minor) return true;
  if (l.minor > r.minor) return false;
  return l.patch < r.patch;
}

// plugin and section metadata
struct topo_tag final {
  std::string id = {};
  std::string full_name = {};
  std::string display_name = {};
  std::string menu_display_name = {};
  std::string tabular_display_name = {};
  bool name_one_based = true;

  void validate() const;
  PB_PREVENT_ACCIDENTAL_COPY_DEFAULT_CTOR(topo_tag);
};

// module and parameter metadata
struct topo_info final {
  int index;
  int slot_count;
  topo_tag tag;

  // if this is set, value doesn't get copied into the plug state on patch load
  bool is_per_instance = false;

  // for reference generator
  std::string description;

  void validate() const;
  PB_PREVENT_ACCIDENTAL_COPY_DEFAULT_CTOR(topo_info);
};

// label positioning
struct gui_label final {
  gui_label_align align;
  gui_label_justify justify;
  gui_label_contents contents;
  PB_PREVENT_ACCIDENTAL_COPY_DEFAULT_CTOR(gui_label);
};

// submenu with header and indices into main list
struct gui_submenu final {
  std::string name;
  bool is_subheader;
  std::vector<int> indices;
  std::vector<std::shared_ptr<gui_submenu>> children;
  
  void validate() const;
  void add_subheader(std::string const& name);
  std::shared_ptr<gui_submenu> add_submenu(std::string const& name);
  void add_submenu(std::string const& name, std::vector<int> const& indices);
  PB_PREVENT_ACCIDENTAL_COPY_DEFAULT_CTOR(gui_submenu);
};

// position in parent grid
struct gui_position final {
  int row = -1;
  int column = -1;
  int row_span = 1;
  int column_span = 1;

  void validate(gui_dimension const& parent_dimension) const;
  gui_position move(int dy, int dx) const { return { row + dy, column + dx, row_span, column_span }; }
};

// binding to enabled/visible
struct gui_binding final {
  std::vector<int> params = {};
  gui_slot_binding_selector slot_selector = {};
  gui_param_binding_selector param_selector = {};

  PB_PREVENT_ACCIDENTAL_COPY_DEFAULT_CTOR(gui_binding);
  void validate(module_topo const& module, int slot_count) const;
  
  void bind_slot(gui_slot_binding_selector selector_);
  void bind_params(std::vector<int> const& params_, gui_param_binding_selector selector_);
  bool is_bound() const { return slot_selector != nullptr || param_selector != nullptr; }
};

// binding another module to enabled/visible
// module/param is assumed to be single slot count
struct gui_global_binding final {
  int param = -1;
  int module = -1;
  gui_global_param_binding_selector selector = {};

  PB_PREVENT_ACCIDENTAL_COPY_DEFAULT_CTOR(gui_global_binding);
  bool is_bound() const { return selector != nullptr; }
  void validate(plugin_topo const& plugin) const;
  void bind_param(int module, int param, gui_global_param_binding_selector selector_);
};

// binding to enabled/visible
struct gui_bindings final {
  gui_binding enabled;
  gui_binding visible;
  gui_global_binding global_enabled;
  gui_global_binding global_visible;

  PB_PREVENT_ACCIDENTAL_COPY_DEFAULT_CTOR(gui_bindings);
  void validate(plugin_topo const& plugin, module_topo const& module, int slot_count) const;
};

// dimensions of own grid (relative distribution)
// positive is relative, negative is absolute, 0 is auto
struct gui_dimension final {
  static inline int constexpr auto_size = std::numeric_limits<int>::max();
  static inline int constexpr auto_size_all = std::numeric_limits<int>::max() - 1;
  std::vector<int> row_sizes = { 1 };
  std::vector<int> column_sizes = { 1 };

  void validate(
    gui_label_edit_cell_split cell_split,
    std::vector<gui_position> const& children,
    std::vector<gui_label_contents> label_contents,
    std::function<bool(int)> include,
    std::function<bool(int)> always_visible) const;

  gui_dimension() = default;
  gui_dimension(gui_dimension const&) = default;
  gui_dimension(int row_count, int column_count);
  gui_dimension(std::vector<int> const& row_sizes, std::vector<int> const& column_sizes);
};

// color scheme
struct gui_colors final {
  juce::Colour tab_text = juce::Colour(0xFF00FF00);
  juce::Colour tab_text_inactive = juce::Colour(0xFF00FF00);
  juce::Colour tab_button = juce::Colour(0xFF00FF00);
  juce::Colour tab_header = juce::Colour(0xFF00FF00);
  juce::Colour graph_grid = juce::Colour(0xFF00FF00);
  juce::Colour graph_text = juce::Colour(0xFF00FF00);
  juce::Colour graph_background = juce::Colour(0xFF00FF00);
  juce::Colour graph_area = juce::Colour(0xFF00FF00);
  juce::Colour graph_line = juce::Colour(0xFF00FF00);
  juce::Colour graph_mod_indicator = juce::Colour(0xFF00FF00);
  juce::Colour bubble_outline = juce::Colour(0xFF00FF00);
  juce::Colour slider_background = juce::Colour(0xFF00FF00);
  juce::Colour slider_highlight = juce::Colour(0xFF00FF00);
  juce::Colour slider_shadow = juce::Colour(0xFF00FF00);
  juce::Colour slider_automation = juce::Colour(0xFF00FF00);
  juce::Colour slider_can_modulate = juce::Colour(0xFF00FF00);
  juce::Colour section_outline = juce::Colour(0xFF00FF00);
  juce::Colour section_background = juce::Colour(0xFF00FF00);
  juce::Colour edit_text = juce::Colour(0xFF00FF00);
  juce::Colour table_header = juce::Colour(0xFF00FF00);
  juce::Colour label_text = juce::Colour(0xFF00FF00);
  juce::Colour control_tick = juce::Colour(0xFF00FF00);
  juce::Colour control_text = juce::Colour(0xFF00FF00);
  juce::Colour control_outline = juce::Colour(0xFF00FF00);
  juce::Colour control_background = juce::Colour(0xFF00FF00);
  juce::Colour scrollbar_thumb = juce::Colour(0xFF00FF00);
  juce::Colour scrollbar_background = juce::Colour(0xFF00FF00);
  juce::Colour popup_text = juce::Colour(0xFF00FF00);
  PB_PREVENT_ACCIDENTAL_COPY_DEFAULT_CTOR(gui_colors);
};

inline gui_dimension::
gui_dimension(int row_count, int column_count) : 
row_sizes(row_count, 1), column_sizes(column_count, 1) {}

inline gui_dimension::
gui_dimension(std::vector<int> const& row_sizes, std::vector<int> const& column_sizes) : 
row_sizes(row_sizes), column_sizes(column_sizes) {}

}
