cmake_minimum_required(VERSION 3.21)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15 CACHE STRING "Minimum macOS version")

project(firefly_synth)
set_property(GLOBAL PROPERTY USE_FOLDERS YES)
add_compile_definitions(JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1)

set(PLUGIN_VERSION "1")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/plugin_base/cmake")
include(plugin_base.vst3)
include(plugin_base.juce)
include(plugin_base.config)
include(plugin_base.utility)
include(plugin_base.mts_esp)

add_subdirectory(plugin_base)
declare_local_target(FALSE STATIC firefly_synth firefly_synth)
target_include_directories(firefly_synth PRIVATE plugin_base/src/plugin_base)

declare_vst3_target(firefly_synth firefly_synth.vst3 firefly_synth synth 0)
declare_vst3_target(firefly_synth_fx firefly_synth.vst3 firefly_synth fx 1)
declare_clap_target(firefly_synth firefly_synth.clap firefly_synth synth 0)
declare_clap_target(firefly_synth_fx firefly_synth.clap firefly_synth fx 1)