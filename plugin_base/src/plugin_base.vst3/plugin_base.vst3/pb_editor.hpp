#pragma once

#include <plugin_base/gui/gui.hpp>
#include <plugin_base.vst3/pb_controller.hpp>

#include <public.sdk/source/vst/vsteditcontroller.h>
#include <pluginterfaces/gui/iplugviewcontentscalesupport.h>

#include <utility>

namespace plugin_base::vst3 {

class pb_editor final:
public Steinberg::Vst::EditorView,
public Steinberg::IPlugViewContentScaleSupport
#if (defined __linux__) || (defined  __FreeBSD__)
, public Steinberg::Linux::IEventHandler
#endif
{
  std::unique_ptr<plugin_gui> _gui = {};
  pb_controller* const _controller = {};

public: 
  pb_editor(pb_controller* controller, std::vector<mod_indicator_state>* mod_indicator_states);
  PB_PREVENT_ACCIDENTAL_COPY(pb_editor);

#if (defined __linux__) || (defined  __FreeBSD__)
  void PLUGIN_API onFDIsSet(Steinberg::Linux::FileDescriptor fd) override;
#endif

  Steinberg::tresult PLUGIN_API removed() override;
  Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect* new_size) override;
  Steinberg::tresult PLUGIN_API getSize(Steinberg::ViewRect* new_size) override;
  Steinberg::tresult PLUGIN_API setContentScaleFactor(ScaleFactor factor) override;
  Steinberg::tresult PLUGIN_API checkSizeConstraint(Steinberg::ViewRect* rect) override;
  Steinberg::tresult PLUGIN_API attached(void* parent, Steinberg::FIDString type) override;
  Steinberg::tresult PLUGIN_API isPlatformTypeSupported(Steinberg::FIDString type) override;
  Steinberg::tresult PLUGIN_API canResize() override { return Steinberg::kResultTrue; }

  Steinberg::uint32 PLUGIN_API addRef() override { return EditorView::addRef(); }
  Steinberg::uint32 PLUGIN_API release() override { return EditorView::release(); }
  Steinberg::tresult PLUGIN_API queryInterface(Steinberg::TUID const iid, void** obj) override;

  void mod_indicator_states_changed() { if (_gui) _gui->mod_indicator_states_changed(); }
};

}
