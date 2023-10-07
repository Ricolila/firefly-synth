#include <plugin_base/topo/module.hpp>
#include <plugin_base/topo/section.hpp>

namespace plugin_base {

void 
section_topo::validate(module_topo const& module, int index_) const
{
  tag.validate();
  gui.bindings.validate(module, 1);
  gui.position.validate(module.gui.dimension);

  assert(this->index == index_);
  auto include = [this, &module](int p) { return module.params[p].gui.section == this->index; };
  auto always_visible = [&module](int p) { return module.params[p].gui.bindings.visible.selector == nullptr; };
  gui.dimension.validate(module.params, include, always_visible);
}

}