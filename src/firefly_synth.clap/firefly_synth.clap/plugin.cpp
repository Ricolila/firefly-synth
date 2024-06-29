#include <firefly_synth/synth.hpp>
#include <firefly_synth/plugin.hpp>

#include <plugin_base/gui/gui.hpp>
#include <plugin_base/shared/logger.hpp>
#include <plugin_base/gui/utility.hpp>
#include <plugin_base.clap/pb_plugin.hpp>

#include <juce_gui_basics/juce_gui_basics.h>
#include <clap/clap.h>
#include <cstring>

#ifndef PB_IS_FX
#error
#elif PB_IS_FX
#define FF_SYNTH_ID FF_SYNTH_FX_ID
#define FF_SYNTH_FULL_NAME FF_SYNTH_FX_FULL_NAME " CLAP"
#define FF_PLUGIN_FEATURE CLAP_PLUGIN_FEATURE_AUDIO_EFFECT
#else
#define FF_SYNTH_ID FF_SYNTH_INST_ID
#define FF_SYNTH_FULL_NAME FF_SYNTH_INST_FULL_NAME " CLAP"
#define FF_PLUGIN_FEATURE CLAP_PLUGIN_FEATURE_INSTRUMENT
#endif

using namespace plugin_base;
using namespace firefly_synth;
using namespace plugin_base::clap;

static std::unique_ptr<plugin_topo> _topo = {};

static char const*
features[] = { FF_PLUGIN_FEATURE, CLAP_PLUGIN_FEATURE_STEREO, nullptr };

static void CLAP_ABI
deinit()
{
  PB_LOG_FUNC_ENTRY_EXIT();
  juce::shutdownJuce_GUI();
  _topo.reset();
  cleanup_logging();
}

static bool CLAP_ABI
init(char const*)
{
  init_logging(FF_SYNTH_VENDOR_NAME, FF_SYNTH_FULL_NAME);
  PB_LOG_FUNC_ENTRY_EXIT();
  _topo = synth_topo(PB_IS_FX, FF_SYNTH_FULL_NAME);
  juce::initialiseJuce_GUI();
  return true;
}

static clap_plugin_descriptor_t const descriptor =
{
  .clap_version = CLAP_VERSION_INIT,
  .id = FF_SYNTH_REVERSE_URI FF_SYNTH_ID,
  .name = FF_SYNTH_FULL_NAME,
  .vendor = FF_SYNTH_VENDOR_NAME,
  .url = FF_SYNTH_VENDOR_URL,
  .manual_url = FF_SYNTH_VENDOR_URL,
  .support_url = FF_SYNTH_VENDOR_URL,
  .version = FF_SYNTH_VERSION_TEXT,
  .description = FF_SYNTH_FULL_NAME,
  .features = features
};

static void const* 
get_plugin_factory(char const* factory_id)
{
  static clap_plugin_factory result;
  if (strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID)) return nullptr;
  result.get_plugin_count = [](clap_plugin_factory const*) { return 1u; };
  result.get_plugin_descriptor = [](clap_plugin_factory const*, std::uint32_t) { return &descriptor; };
  result.create_plugin = [](clap_plugin_factory const*, clap_host const* host, char const* plugin_id)
  { 
    if(!strcmp(descriptor.id, plugin_id))
      return (new pb_plugin(&descriptor, host, _topo.get()))->clapPlugin(); 
    return static_cast<clap_plugin_t const*>(nullptr);
  };
  return &result;
}

extern "C" CLAP_EXPORT 
clap_plugin_entry_t const clap_entry =
{
  .clap_version = CLAP_VERSION_INIT,
  .init = init,
  .deinit = deinit,
  .get_factory = get_plugin_factory
};

// for param list generator
extern "C" PB_EXPORT plugin_topo const*
pb_plugin_topo_create() { return synth_topo(PB_IS_FX, FF_SYNTH_FULL_NAME).release(); }
extern "C" PB_EXPORT void
pb_plugin_topo_destroy(plugin_topo const* topo) { delete topo; }
