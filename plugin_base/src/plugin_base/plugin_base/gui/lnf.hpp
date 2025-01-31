#pragma once

#include <plugin_base/desc/plugin.hpp>
#include <juce_gui_basics/juce_gui_basics.h>

#include <map>
#include <filesystem>

namespace plugin_base {

class lnf:
public juce::LookAndFeel_V4 {
  
  int const _module = -1;
  int const _module_section = -1;
  int const _custom_section = -1;

  std::string const _theme;
  plugin_desc const* const _desc;
  juce::Typeface::Ptr _typeface = {};

  gui_colors _default_colors = {};
  plugin_topo_gui_theme_settings _global_settings = {};
  section_topo_gui_theme_settings _default_settings = {};
  std::map<std::string, gui_colors> _module_colors = {};
  std::map<std::string, gui_colors> _section_colors = {};
  std::map<std::string, section_topo_gui_theme_settings> _module_settings = {};
  std::map<std::string, section_topo_gui_theme_settings> _section_settings = {};

  int tab_width() const;
  module_section_gui const& get_module_section_gui() const;
  void init_theme(std::filesystem::path const& theme_folder, juce::var const& json);

public:
  juce::Font font() const;
  gui_colors const& colors() const;
  std::string const& theme() const { return _theme; }
  lnf(plugin_desc const* desc, std::string const& theme, int custom_section, int module_section, int module);

  gui_colors module_gui_colors(std::string const& module_full_name);
  gui_colors section_gui_colors(std::string const& section_full_name);
  
  plugin_topo_gui_theme_settings const& global_settings() const { return _global_settings; }
  int combo_height(bool tabular) const { return _global_settings.get_font_height() + (tabular ? 11 : 7); }
  int toggle_height(bool tabular) const { return _global_settings.get_font_height() + (tabular ? 9 : 5); }

  int getDefaultScrollbarWidth() override { return 8; }
  bool areScrollbarButtonsVisible() override { return true; }

  juce::Font getLabelFont(juce::Label&) override;
  juce::Font getPopupMenuFont() override { return font(); }
  juce::Font getComboBoxFont(juce::ComboBox&) override { return font(); }
  juce::Font getSliderPopupFont(juce::Slider&) override { return font(); }
  juce::Font getTextButtonFont(juce::TextButton&, int) override { return font(); }
  juce::Font getTabButtonFont(juce::TabBarButton& b, float) override { return font(); }
  
  juce::Path getTickShape(float) override;
  juce::Button* createTabBarExtrasButton() override;
  int	getTabButtonBestWidth(juce::TabBarButton&, int) override;
  void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
  void getIdealPopupMenuItemSize(juce::String const&, bool, int, int& , int&) override;

  void drawLabel(juce::Graphics&, juce::Label& label) override;
  void drawTooltip(juce::Graphics&, juce::String const&, int, int) override;
  void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;
  void drawTabButton(juce::TabBarButton&, juce::Graphics&, bool, bool) override;
  void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
  void drawTextEditorOutline(juce::Graphics&, int, int, juce::TextEditor&) override;
  void drawTabbedButtonBarBackground(juce::TabbedButtonBar&, juce::Graphics&) override;
  void drawComboBox(juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
  void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
  void drawScrollbar(juce::Graphics&, juce::ScrollBar&, int, int, int, int, bool, int, int, bool, bool) override;
  void drawTickBox(juce::Graphics&, juce::Component&, float, float, float, float, bool, bool, bool, bool) override;
  void drawBubble(juce::Graphics&, juce::BubbleComponent&, juce::Point<float> const&, juce::Rectangle<float> const& body) override;
  void drawLinearSlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
  void drawButtonBackground(juce::Graphics& g, juce::Button& button, juce::Colour const& background, bool highlighted, bool down) override;
  void drawPopupMenuItemWithOptions(juce::Graphics&, juce::Rectangle<int> const&, bool, juce::PopupMenu::Item const& item, juce::PopupMenu::Options const&) override;
};

}
