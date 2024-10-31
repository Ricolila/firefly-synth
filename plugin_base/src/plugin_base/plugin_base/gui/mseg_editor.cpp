#include <plugin_base/gui/mseg_editor.hpp>

using namespace juce;

namespace plugin_base {

static float const seg_w_min = 1.0f;
static float const seg_w_max = 100.0f;

static char const* mseg_magic = "MSEG_MAGIC";
static float const point_size = 8.0f;
static float const line_thickness = 2.0f;
static float const padding = point_size * 0.5f + 2;
  
mseg_editor::
mseg_editor(
  plugin_gui* gui, lnf* lnf, int module_index, int module_slot, 
  int start_y_param, int count_param, int w_param, int y_param, 
  int slope_param, int grid_x_param, int grid_y_param, bool is_external):
_gui(gui), _lnf(lnf),
_module_index(module_index), _module_slot(module_slot),
_start_y_param(start_y_param), _count_param(count_param),
_w_param(w_param), _y_param(y_param), _slope_param(slope_param), 
_grid_x_param(grid_x_param), _grid_y_param(grid_y_param), _is_external(is_external)
{
  assert(gui != nullptr);
  assert(lnf != nullptr);
  auto const& topo = *gui->automation_state()->desc().plugin;
  auto const& param_list = topo.modules[module_index].params;
  (void)param_list;

  auto is_step_gte_0 = [](param_topo const& pt) {
    return pt.domain.type == domain_type::step && pt.domain.min == 0; };
  assert(is_step_gte_0(param_list[grid_x_param]));
  assert(is_step_gte_0(param_list[grid_y_param]));

  auto check_w_param = [](param_topo const& pt) {
    return pt.domain.type == domain_type::linear && pt.domain.min == seg_w_min && pt.domain.max == seg_w_max; };
  assert(check_w_param(param_list[w_param]));

  auto is_linear_unit = [](param_topo const& pt) {
    return pt.domain.type == domain_type::identity && pt.domain.min == 0 && pt.domain.max == 1; };
  assert(is_linear_unit(param_list[start_y_param]));
  assert(is_linear_unit(param_list[y_param]));
  assert(is_linear_unit(param_list[slope_param]));
  assert(param_list[start_y_param].info.slot_count == 1);
  assert(param_list[w_param].info.slot_count >= 1);
  assert(param_list[y_param].info.slot_count == param_list[w_param].info.slot_count);
  assert(param_list[slope_param].info.slot_count == param_list[w_param].info.slot_count); 

  _max_seg_count = param_list[w_param].info.slot_count;
  init_from_plug_state();

  _gui->automation_state()->add_listener(_module_index, _module_slot, _count_param, 0, this);
  _gui->automation_state()->add_listener(_module_index, _module_slot, _start_y_param, 0, this);
  for (int i = 0; i < _gui->automation_state()->desc().plugin->modules[_module_index].params[_w_param].info.slot_count; i++)
    _gui->automation_state()->add_listener(_module_index, _module_slot, _w_param, i, this);
  for (int i = 0; i < _gui->automation_state()->desc().plugin->modules[_module_index].params[_y_param].info.slot_count; i++)
    _gui->automation_state()->add_listener(_module_index, _module_slot, _y_param, i, this);
  for (int i = 0; i < _gui->automation_state()->desc().plugin->modules[_module_index].params[_slope_param].info.slot_count; i++)
    _gui->automation_state()->add_listener(_module_index, _module_slot, _slope_param, i, this); 
}

mseg_editor::
~mseg_editor()
{
  _gui->automation_state()->remove_listener(_module_index, _module_slot, _count_param, 0, this);
  _gui->automation_state()->remove_listener(_module_index, _module_slot, _start_y_param, 0, this);
  for (int i = 0; i < _gui->automation_state()->desc().plugin->modules[_module_index].params[_w_param].info.slot_count; i++)
    _gui->automation_state()->remove_listener(_module_index, _module_slot, _w_param, i, this);
  for (int i = 0; i < _gui->automation_state()->desc().plugin->modules[_module_index].params[_y_param].info.slot_count; i++)
    _gui->automation_state()->remove_listener(_module_index, _module_slot, _y_param, i, this);
  for (int i = 0; i < _gui->automation_state()->desc().plugin->modules[_module_index].params[_slope_param].info.slot_count; i++)
    _gui->automation_state()->remove_listener(_module_index, _module_slot, _slope_param, i, this);
}

void 
mseg_editor::init_from_plug_state()
{
  _gui_segs.clear();
  _gui_start_y = _gui->automation_state()->get_plain_at(_module_index, _module_slot, _start_y_param, 0).real();
  _current_seg_count = _gui->automation_state()->get_plain_at(_module_index, _module_slot, _count_param, 0).step();
  for (int i = 0; i < _current_seg_count; i++)
  {
    mseg_seg seg = {};
    seg.w = _gui->automation_state()->get_plain_at(_module_index, _module_slot, _w_param, i).real();
    seg.y = _gui->automation_state()->get_plain_at(_module_index, _module_slot, _y_param, i).real();
    seg.slope = _gui->automation_state()->get_plain_at(_module_index, _module_slot, _slope_param, i).real();
    _gui_segs.push_back(seg);
  }
}

void 
mseg_editor::state_changed(int index, plain_value plain)
{
  if (_drag_seg != -1 || _drag_start_y) return;
  _is_dirty = true;
  repaint();
}

float
mseg_editor::get_seg_total_x(int seg) const
{
  float result = 0.0f;
  for (int i = 0; i <= seg; i++)
    result += _gui_segs[i].w;
  return result;
}

float 
mseg_editor::sloped_y_pos(float pos, int seg) const
{
  float slope = _gui_segs[seg].slope;
  float y1 = seg == 0 ? _gui_start_y : _gui_segs[seg - 1].y;
  float y2 = _gui_segs[seg].y;
  double const slope_min = exp_slope_min;
  double const slope_max = (1.0 - exp_slope_min);
  double const slope_range = slope_max - slope_min;
  double const slope_bounded = exp_slope_min + slope_range * slope;
  double const exp = std::log(slope_bounded) / std::log(0.5);
  return y1 + std::pow(pos, exp) * (y2 - y1); 
}

void 
mseg_editor::make_slope_path(
  float x, float y, float w, float h,
  int seg, bool closed, juce::Path& path) const
{
  path = {};
  float x1_norm = seg == 0 ? 0.0f : get_seg_norm_x(seg - 1);
  float x2_norm = get_seg_norm_x(seg);
  float y1_norm = seg == 0? _gui_start_y: _gui_segs[seg - 1].y;

  int const pixel_count = (int)std::ceil((x2_norm - x1_norm) * w);
  if (closed)
  {
    path.startNewSubPath(x + w * x1_norm, y + h);
    path.lineTo(x + w * x1_norm, y + h - h * y1_norm);
  }
  else
    path.startNewSubPath(x + w * x1_norm, y + h - h * y1_norm);
  for (int j = 1; j < pixel_count; j++)
  {
    float pos = j / (pixel_count - 1.0f);
    float x_this_pos_norm = x1_norm + pos * (x2_norm - x1_norm);
    float y_this_pos_norm = sloped_y_pos(pos, seg);
    float x_this_pos = x + w * x_this_pos_norm;
    float y_this_pos = y + h - h * y_this_pos_norm;
    path.lineTo(x_this_pos, y_this_pos);

    if (closed && j == pixel_count - 1)
    {
      path.lineTo(x_this_pos, y + h);
      path.closeSubPath();
    }
  }
}

void
mseg_editor::mouseDoubleClick(MouseEvent const& event)
{
  int hit_seg;
  bool hit_start_y;
  bool hit_seg_slope;
  bool hit = hit_test(event, hit_start_y, hit_seg, hit_seg_slope);
  
  // cannot delete the first one
  if (hit && hit_start_y) return;
  
  // reset slope to default
  if (hit && hit_seg_slope)
  {
    _gui_segs[hit_seg].slope = 0.5f;
    _gui->param_changed(_module_index, _module_slot, _slope_param, hit_seg, 0.5f);
    repaint();
    return;
  }

  auto update_plug_all_params = [this](std::string const& action) {
    auto const& desc = _gui->automation_state()->desc();
    int undo_token = _gui->automation_state()->begin_undo_region();
    _gui->param_changed(_module_index, _module_slot, _count_param, 0, _current_seg_count);
    for (int i = 0; i < _current_seg_count; i++)
    {
      _gui->param_changed(_module_index, _module_slot, _w_param, i, _gui_segs[i].w);
      _gui->param_changed(_module_index, _module_slot, _y_param, i, _gui_segs[i].y);
      _gui->param_changed(_module_index, _module_slot, _slope_param, i, _gui_segs[i].slope);
    }
    int this_module_global = desc.module_topo_to_index.at(_module_index) + _module_slot;
    _gui->automation_state()->end_undo_region(undo_token, action, desc.modules[this_module_global].info.name + " MSEG Point");
  };

  // case join  
  if(hit)
  {
    if (_gui_segs.size() > 1)
    {
      _gui_segs.erase(_gui_segs.begin() + hit_seg);
      _current_seg_count--;
      update_plug_all_params("Delete");
      repaint();
    }
    return;
  }

  // case splice
  float const x = padding;
  float const w = getLocalBounds().getWidth() - padding * 2.0f;

  if (event.x <= x) return;
  if (event.x >= x + w) return;
  if (_gui_segs.size() == _max_seg_count) return;

  float new_norm_x = (event.x - x) / w;
  for(int i = 0; i < _gui_segs.size(); i++)
    if (new_norm_x < get_seg_norm_x(i))
    {
      mseg_seg new_seg;
      new_seg.y = 0.5f;
      new_seg.slope = 0.5f;
      new_seg.w = _gui_segs[i].w;
      _gui_segs.insert(_gui_segs.begin() + i, new_seg);
      _current_seg_count++;
      update_plug_all_params("Add");
      repaint();
      break;
    }
}

void
mseg_editor::mouseDrag(MouseEvent const& event)
{
  // right mouse-up triggers context menu
  if (event.mods.isRightButtonDown()) return;
  if (isDragAndDropActive()) return;
  if (!hit_test(event, _drag_start_y, _drag_seg, _drag_seg_slope)) return;
  
  Image image(Image::PixelFormat::ARGB, point_size, point_size, true);
  Graphics g(image);
  if (_drag_seg_slope)
  {
    g.setColour(_lnf->colors().mseg_line);
    g.drawEllipse(0.0f, 0.0f, point_size, point_size, 1.0f);
  }
  else
  {
    g.setColour(_lnf->colors().mseg_line);
    g.drawEllipse(0.0f, 0.0f, point_size, point_size, 1.0f);
    g.setColour(_lnf->colors().mseg_line.withAlpha(0.5f));
    g.fillEllipse(0.0f, 0.0f, point_size, point_size);
  }

  if (_drag_start_y) 
    _gui->param_begin_changes(_module_index, _module_slot, _start_y_param, 0);
  else if (_drag_seg != -1)
  {
    if (_drag_seg_slope) 
      _gui->param_begin_changes(_module_index, _module_slot, _slope_param, _drag_seg);
    else
    {
      _drag_seg_initial_x = event.x;
      _drag_seg_initial_w = _gui_segs[_drag_seg].w;
      _undo_token = _gui->automation_state()->begin_undo_region();
      _gui->param_begin_changes(_module_index, _module_slot, _w_param, _drag_seg);
      _gui->param_begin_changes(_module_index, _module_slot, _y_param, _drag_seg);
    }
  }
  else 
    assert(false);
  Point<int> offset(image.getWidth() / 2 + point_size, image.getHeight() / 2 + point_size);
  startDragging(String(mseg_magic), this, ScaledImage(image), false, &offset);
}

bool
mseg_editor::isInterestedInDragSource(DragAndDropTarget::SourceDetails const& details)
{
  return details.description == String(mseg_magic);
}

void 
mseg_editor::mouseUp(juce::MouseEvent const& event)
{
  auto const& desc = _gui->automation_state()->desc();

  if (event.mods.isRightButtonDown())
  {
    int hit_seg;
    bool hit_start_y;
    bool hit_seg_slope;

    // pop up the host menu for points and slopes
    if (hit_test(event, hit_start_y, hit_seg, hit_seg_slope))
    {
      PopupMenu menu;
      PopupMenu::Options options;
      options = options.withTargetComponent(this);
      options = options.withMousePosition();
      menu.setLookAndFeel(&getLookAndFeel());

      auto lnf = dynamic_cast<plugin_base::lnf*>(&getLookAndFeel());
      auto colors = lnf->module_gui_colors(desc.plugin->modules[_module_index].info.tag.full_name);
      
      if (hit_start_y || hit_seg_slope)
      {
        int param_index;
        if(hit_start_y)
          param_index = desc.param_mappings.topo_to_index[_module_index][_module_slot][_start_y_param][0];
        else
          param_index = desc.param_mappings.topo_to_index[_module_index][_module_slot][_slope_param][hit_seg];

        auto host_menu = desc.menu_handler->context_menu(desc.params[param_index]->info.id_hash);
        if (!host_menu || host_menu->root.children.empty()) return;

        menu.addColouredItem(-1, "Host", colors.tab_text, false, false, nullptr);
        fill_host_menu(menu, 0, host_menu->root.children);

        menu.showMenuAsync(options, [this, host_menu = host_menu.release()](int id) {
          if (id > 0) host_menu->clicked(id - 1);
          delete host_menu;
        });
      }
      else
      {
        int param_w_index = desc.param_mappings.topo_to_index[_module_index][_module_slot][_w_param][hit_seg];
        auto host_menu_w = desc.menu_handler->context_menu(desc.params[param_w_index]->info.id_hash);
        if (!host_menu_w || host_menu_w->root.children.empty()) return;

        int param_y_index = desc.param_mappings.topo_to_index[_module_index][_module_slot][_y_param][hit_seg];
        auto host_menu_y = desc.menu_handler->context_menu(desc.params[param_y_index]->info.id_hash);
        if (!host_menu_y || host_menu_y->root.children.empty()) return;

        menu.addColouredItem(-1, "Host W", colors.tab_text, false, false, nullptr);
        fill_host_menu(menu, 0, host_menu_w->root.children);
        host_menu_w.release();

        menu.addColouredItem(-1, "Host Y", colors.tab_text, false, false, nullptr);
        fill_host_menu(menu, 10000, host_menu_y->root.children);
        host_menu_y.release();

        // reaper doesnt like both menus active so recreate them on the spot
        menu.showMenuAsync(options, [this, param_w_index, param_y_index](int id) {
          auto const& desc = _gui->automation_state()->desc();
          if (id > 10000)
          {
            auto host_menu_y = desc.menu_handler->context_menu(desc.params[param_y_index]->info.id_hash);
            host_menu_y->clicked(id - 1 - 10000);
          }
          else if (id > 0)
          {
            auto host_menu_w = desc.menu_handler->context_menu(desc.params[param_w_index]->info.id_hash);
            host_menu_w->clicked(id - 1);
          }
        });
      }
    }
    else
    {
      // pop up the snap-to-grid menu when no seg is hit
      
      PopupMenu menu;
      PopupMenu::Options options;
      options = options.withTargetComponent(this);
      options = options.withMousePosition();
      menu.setLookAndFeel(&getLookAndFeel());

      PopupMenu x_menu;
      int max_x = desc.plugin->modules[_module_index].params[_grid_x_param].domain.max;
      int current_x = _gui->automation_state()->get_plain_at(_module_index, _module_slot, _grid_x_param, 0).step();
      for (int i = 0; i < max_x; i++)
        x_menu.addItem(i + 1, i == 0 ? "Off" : std::to_string(i + 1), true, i == current_x);

      PopupMenu y_menu;
      int max_y = desc.plugin->modules[_module_index].params[_grid_y_param].domain.max;
      int current_y = _gui->automation_state()->get_plain_at(_module_index, _module_slot, _grid_y_param, 0).step();
      for (int i = 0; i < max_y; i++)
        y_menu.addItem(i + 1000 + 1, i == 0 ? "Off" : std::to_string(i + 1), true, i == current_y);

      menu.addSubMenu("Snap X", x_menu);
      menu.addSubMenu("Snap Y", y_menu);
      if (_is_external)
        menu.addItem(2000 + 1, "Close Editor");
      else
        menu.addItem(3000 + 1, "Open Editor");

      menu.showMenuAsync(options, [this, max_x, max_y](int result) {
        if (1 <= result && result <= max_x)
          _gui->automation_state()->set_raw_at(_module_index, _module_slot, _grid_x_param, 0, result - 1);
        else if (1000 + 1 <= result && result <= 1000 + max_y)
          _gui->automation_state()->set_raw_at(_module_index, _module_slot, _grid_y_param, 0, result - 1000 - 1);
        else if (result == 2000 + 1)
        {
          auto* dialog = findParentComponentOfClass<DialogWindow>();
          if(dialog) dialog->exitModalState();
        } else if (result == 3000 + 1)
        {
          auto const& desc = _gui->automation_state()->desc();
          int module_global = desc.module_topo_to_index.at(_module_index) + _module_slot;

          DialogWindow::LaunchOptions options;
          options.resizable = true;
          options.useNativeTitleBar = false;
          options.componentToCentreAround = nullptr;
          options.useBottomRightCornerResizer = true;
          options.escapeKeyTriggersCloseButton = true;
          options.dialogTitle = desc.modules[module_global].info.name;
          auto editor = new mseg_editor(
            _gui, _lnf, _module_index, _module_slot, _start_y_param,
            _count_param, _w_param, _y_param, _slope_param, _grid_x_param, _grid_y_param, true);
          editor->setSize(600, 200);
          editor->setLookAndFeel(_lnf);
          options.content.setOwned(editor);
          auto editor_window = options.launchAsync();
          editor_window->setTitleBarHeight(0); // DialogWindow seems not easy to style, so just drop the titlebar
          editor_window->setLookAndFeel(_lnf);
          editor_window->setTopLeftPosition(_gui->getScreenBounds().getTopLeft());
          editor_window->setResizeLimits(200, 100, _gui->getScreenBounds().getWidth(), _gui->getScreenBounds().getHeight());
        }
      });
    }

    return;
  }

  if (_drag_start_y) _gui->param_end_changes(_module_index, _module_slot, _start_y_param, 0);
  else if (_drag_seg != -1 && _drag_seg_slope) _gui->param_end_changes(_module_index, _module_slot, _slope_param, _drag_seg);
  else if(_drag_seg != -1)
  {
    _gui->param_end_changes(_module_index, _module_slot, _y_param, _drag_seg);
    _gui->param_end_changes(_module_index, _module_slot, _w_param, _drag_seg);
    int this_module_global = desc.module_topo_to_index.at(_module_index) + _module_slot;
    _gui->automation_state()->end_undo_region(_undo_token, "Change", desc.modules[this_module_global].info.name + " MSEG Point " + std::to_string(_drag_seg + 1));
    _undo_token = -1;
  }

  _drag_seg = -1;
  _drag_seg_slope = false;
  _drag_start_y = false;
  _drag_seg_initial_x = 0.0f;
  _drag_seg_initial_w = 0.0f;

  setMouseCursor(MouseCursor::ParentCursor);
  repaint();
}

void 
mseg_editor::itemDragMove(juce::DragAndDropTarget::SourceDetails const& details)
{
  float const y = padding;
  float const h = getLocalBounds().getHeight() - padding * 2.0f;
  float drag_y_amt = 1.0f - std::clamp((details.localPosition.y - y) / h, 0.0f, 1.0f);

  if (_drag_start_y)
  {
    _gui_start_y = drag_y_amt;
    _gui->param_changing(_module_index, _module_slot, _start_y_param, 0, drag_y_amt);
    repaint();
    return;
  }

  if (_drag_seg != -1 && _drag_seg_slope)
  {
    float y1 = _drag_seg == 0 ? _gui_start_y : _gui_segs[_drag_seg - 1].y;
    float y2 = _gui_segs[_drag_seg].y;
    if (y1 > y2) drag_y_amt = 1.0f - drag_y_amt;
    _gui_segs[_drag_seg].slope = drag_y_amt;
    _gui->param_changing(_module_index, _module_slot, _slope_param, _drag_seg, drag_y_amt);
    repaint();
    return;
  }

  if (_drag_seg != -1 && !_drag_seg_slope)
  {
    float norm_add;
    float diff = details.localPosition.x - _drag_seg_initial_x;
    float sensitivity_l = _drag_seg_initial_x;
    float sensitivity_r = getLocalBounds().getWidth() - _drag_seg_initial_x;
    if (diff >= 0)
      norm_add = diff / sensitivity_r;
    else
      norm_add = -1.0f + details.localPosition.x / sensitivity_l;
    float norm_w = (_drag_seg_initial_w - seg_w_min) / (seg_w_max - seg_w_min);
    norm_w += norm_add;
    norm_w = std::clamp(norm_w, 0.0f, 1.0f);
    float new_width = seg_w_min + norm_w * (seg_w_max - seg_w_min);
    _gui_segs[_drag_seg].w = new_width;
    _gui->param_changing(_module_index, _module_slot, _w_param, _drag_seg, new_width);
    _gui_segs[_drag_seg].y = drag_y_amt;
    _gui->param_changing(_module_index, _module_slot, _y_param, _drag_seg, drag_y_amt);
    repaint();
    return;
  }
}

bool
mseg_editor::hit_test(
  MouseEvent const& e, bool& hit_start_y,
  int& hit_seg, bool& hit_seg_slope) const
{
  float const x = padding;
  float const y = padding;
  float const start_y = _gui_start_y;
  float const w = getLocalBounds().getWidth() - padding * 2.0f;
  float const h = getLocalBounds().getHeight() - padding * 2.0f;

  hit_seg = -1;
  hit_start_y = false;
  hit_seg_slope = false;

  float start_y_x1 = x - point_size / 2;
  float start_y_y1 = y + h - start_y * h - point_size / 2;
  float start_y_x2 = start_y_x1 + point_size;
  float start_y_y2 = start_y_y1 + point_size;
  if (start_y_x1 <= e.x && e.x <= start_y_x2 && start_y_y1 <= e.y && e.y <= start_y_y2)
  {
    hit_start_y = true;
    return true;
  }

  for (int i = 0; i < _gui_segs.size(); i++)
  {
    float prev_x = i == 0 ? 0.0f : get_seg_norm_x(i - 1);
    float this_slope_x1 = x + (prev_x + get_seg_norm_x(i)) * 0.5f * w - point_size / 2;
    float this_slope_y1 = y + h - h * sloped_y_pos(0.5f, i) - point_size / 2;
    float this_slope_x2 = this_slope_x1 + point_size;
    float this_slope_y2 = this_slope_y1 + point_size;
    if (this_slope_x1 <= e.x && e.x <= this_slope_x2 && this_slope_y1 <= e.y && e.y <= this_slope_y2)
    {
      hit_seg = i;
      hit_seg_slope = true;
      return true;
    }

    float this_point_x1 = x + w * get_seg_norm_x(i) - point_size / 2;
    float this_point_y1 = y + h - h * _gui_segs[i].y - point_size / 2;
    float this_point_x2 = this_point_x1 + point_size;
    float this_point_y2 = this_point_y1 + point_size;
    if (this_point_x1 <= e.x && e.x <= this_point_x2 && this_point_y1 <= e.y && e.y <= this_point_y2)
    {
      hit_seg = i;
      hit_seg_slope = false;
      return true;
    }
  }

  return false;
}

void
mseg_editor::mouseMove(MouseEvent const& event)
{
  int hit_seg;
  bool hit_start_y;
  bool hit_seg_slope;

  setTooltip("");
  setMouseCursor(MouseCursor::ParentCursor);

  float const x = padding;
  float const w = getLocalBounds().getWidth() - padding * 2.0f;

  if (!hit_test(event, hit_start_y, hit_seg, hit_seg_slope))
  {
    if (x < event.x && event.x < x + w)
      setMouseCursor(MouseCursor::PointingHandCursor);
    return;
  }

  setMouseCursor(MouseCursor::DraggingHandCursor);
  auto const& topo = *_gui->automation_state()->desc().plugin;
  if (hit_start_y)
  {
    std::string text = _gui->automation_state()->desc().plugin->modules[_module_index].params[_start_y_param].domain.raw_to_text(false, _gui_start_y);
    setTooltip(topo.modules[_module_index].params[_start_y_param].info.tag.display_name + ": " + text);
  }
  else if (hit_seg != -1)
  {
    if (hit_seg_slope)
    {
      std::string text = _gui->automation_state()->desc().plugin->modules[_module_index].params[_slope_param].domain.raw_to_text(false, _gui_segs[hit_seg].slope);
      setTooltip(topo.modules[_module_index].params[_slope_param].info.tag.display_name + " " + std::to_string(hit_seg + 1) + ": " + text);
    }
    else
    {
      std::string text_w = _gui->automation_state()->desc().plugin->modules[_module_index].params[_w_param].domain.raw_to_text(false, _gui_segs[hit_seg].w);
      std::string text_y = _gui->automation_state()->desc().plugin->modules[_module_index].params[_y_param].domain.raw_to_text(false, _gui_segs[hit_seg].y);
      setTooltip(
        topo.modules[_module_index].params[_w_param].info.tag.display_name + " " + std::to_string(hit_seg + 1) + ": " + text_w + ", " +
        topo.modules[_module_index].params[_y_param].info.tag.display_name + " " + std::to_string(hit_seg + 1) + ": " + text_y);
    }
  }

  repaint();
}

void
mseg_editor::paint(Graphics& g)
{
  float const bg_text_factor = 1.33f;
  float const bg_text_padding = 3.0f;

  float x = padding;
  float y = padding;
  float w = getLocalBounds().getWidth() - padding * 2.0f;
  float h = getLocalBounds().getHeight() - padding * 2.0f;

  if (_is_external)
  {
    y += _lnf->font().getHeight() * bg_text_factor + bg_text_padding * 2.0f;
    h -= _lnf->font().getHeight() * bg_text_factor + bg_text_padding * 2.0f;
  }

  if (_is_dirty)
  {
    init_from_plug_state();
    _is_dirty = false;
  }

  Path sloped_path;
  float slope_marker_x;
  float slope_marker_y;

  float start_y = _gui_start_y;
  float end_y = _gui_segs[_gui_segs.size() - 1].y;

  // bg
  g.setColour(_lnf->colors().mseg_background);
  g.fillRect(getLocalBounds());

  // filler
  g.setColour(_lnf->colors().mseg_area);
  g.fillRect(x, y + h, w, padding);
  g.fillRect(x + w, y + h - end_y * h, padding, end_y * h + padding);
  g.fillRect(0.0f, y + h - start_y * h, padding, start_y * h + padding);

  // bg text
  if (_is_external)
  {
    auto const& desc = _gui->automation_state()->desc();
    int this_module_global = desc.module_topo_to_index.at(_module_index) + _module_slot;
    auto bg_text = desc.modules[this_module_global].info.name + " MSEG";
    g.setFont(_lnf->font().withHeight(_lnf->font().getHeight() * bg_text_factor));
    g.setColour(_lnf->colors().mseg_text.withAlpha(0.5f));
    g.drawText(bg_text, bg_text_padding, bg_text_padding, 
      getLocalBounds().getWidth() - bg_text_padding, 
      getLocalBounds().getHeight() - bg_text_padding, 
      Justification::centredTop, false);
  }

  // grid
  g.setColour(_lnf->colors().mseg_grid);
  g.drawRect(getLocalBounds(), 1.0f);

  // start y point marker
  g.setColour(_lnf->colors().mseg_point.withAlpha(0.5f));
  g.fillEllipse(x - point_size / 2, y + h - start_y * h - point_size / 2, point_size, point_size);
  g.setColour(_lnf->colors().mseg_point);
  g.drawEllipse(x - point_size / 2, y + h - start_y * h - point_size / 2, point_size, point_size, 1);

  // actual segments
  for (int i = 0; i < _gui_segs.size(); i++)
  {
    g.setColour(_lnf->colors().mseg_line);
    make_slope_path(x, y, w, h, i, false, sloped_path);
    g.strokePath(sloped_path, PathStrokeType(line_thickness));
    g.setColour(_lnf->colors().mseg_area);
    make_slope_path(x, y, w, h, i, true, sloped_path);
    g.fillPath(sloped_path);

    // point marker
    g.setColour(_lnf->colors().mseg_point.withAlpha(0.5f));
    g.fillEllipse(x + w * get_seg_norm_x(i) - point_size / 2, y + h - h * _gui_segs[i].y - point_size / 2, point_size, point_size);
    g.setColour(_lnf->colors().mseg_point.withAlpha(0.5f));
    g.drawEllipse(x + w * get_seg_norm_x(i) - point_size / 2, y + h - h * _gui_segs[i].y - point_size / 2, point_size, point_size, 1);

    // slope marker
    float prev_x = i == 0 ? 0.0f : get_seg_norm_x(i - 1);
    slope_marker_x = x + (prev_x + get_seg_norm_x(i)) * 0.5f * w - point_size / 2;
    slope_marker_y = y + h - h * sloped_y_pos(0.5f, i) - point_size / 2;
    g.setColour(_lnf->colors().mseg_point);
    g.drawEllipse(slope_marker_x, slope_marker_y, point_size, point_size, 1.0f);
  }
}

}