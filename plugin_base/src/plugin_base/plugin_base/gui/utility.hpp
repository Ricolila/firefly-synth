#pragma once

#include <plugin_base/shared/value.hpp>
#include <juce_gui_basics/juce_gui_basics.h>
#include <filesystem>

namespace plugin_base {

void 
add_and_make_visible(
  juce::Component& parent, 
  juce::Component& child);

juce::Colour
color_to_grayscale(juce::Colour const& c);

juce::ScaledImage
make_drag_source_image(juce::Font const& font, std::string const& text, juce::Colour border_color);

}
