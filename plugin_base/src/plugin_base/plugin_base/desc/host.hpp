#pragma once

#include <plugin_base/shared/utility.hpp>

#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace plugin_base {

// from resources folder
struct factory_preset
{
  std::string name;
  std::string path;
};

enum { 
  host_menu_flags_none = 0x0, 
  host_menu_flags_checked = 0x1,
  host_menu_flags_enabled = 0x2, 
  host_menu_flags_separator = 0x4 };

struct host_menu_item
{
  int flags;
  std::string name;
  std::function<void()> clicked;
  std::vector<std::unique_ptr<host_menu_item>> children;
};

// per-param host context menu
class host_context_menu {
public:
  virtual std::unique_ptr<host_menu_item> root() = 0;
};

// differences between plugin formats
struct format_config {
  PB_PREVENT_ACCIDENTAL_COPY_DEFAULT_CTOR(format_config);

  virtual std::unique_ptr<host_context_menu>
  context_menu(int param_id) const = 0;
  virtual std::filesystem::path 
  resources_folder(std::filesystem::path const& binary_path) const = 0;
};

}