#include <infernal.base/topo.hpp>
#include <infernal.base/support.hpp>
#include <infernal.base/engine.hpp>
#include <infernal.synth/synth.hpp>

using namespace infernal::base;

namespace infernal::synth {

class osc_engine: 
public module_engine {
  float _phase = 0;
public:
  virtual void 
  process(plugin_topo const& topo, int module_index, plugin_block const& block) override;
};

enum osc_type { osc_type_saw, osc_type_sine };
enum osc_group { osc_group_main, osc_group_pitch };
enum osc_param { osc_param_on, osc_param_gain, osc_param_bal, osc_param_oct, osc_param_note, osc_param_cent };

module_group_topo
osc_topo()
{
  module_group_topo result(make_module_group("{45C2CCFE-48D9-4231-A327-319DAE5C9366}", "Osc", 2, module_scope::voice, module_output::audio));
  result.param_groups.emplace_back(param_group_topo(osc_group_main, "Main"));
  result.param_groups.emplace_back(param_group_topo(osc_group_pitch, "Pitch"));
  result.engine_factory = [](int sample_rate, int max_frame_count) -> std::unique_ptr<module_engine> { return std::make_unique<osc_engine>(); };
  result.params.emplace_back(make_param_toggle("{AA9D7DA6-A719-4FDA-9F2E-E00ABB784845}", "On", "Off", osc_group_main, config_input_toggle()));
  result.params.emplace_back(make_param_pct("{75E49B1F-0601-4E62-81FD-D01D778EDCB5}", "Gain", "100", 0, 1, osc_group_main, config_input_linear_accurate_knob()));
  result.params.emplace_back(make_param_pct("{23C6BC03-0978-4582-981B-092D68338ADA}", "Bal", "0", -1, 1, osc_group_pitch, config_input_linear_accurate_knob()));
  result.params.emplace_back(make_param_step("{38C78D40-840A-4EBE-A336-2C81D23B426D}", "Oct", "0", 0, 9, osc_group_pitch, config_input_step_knob()));
  result.params.emplace_back(make_param_list("{78856BE3-31E2-4E06-A6DF-2C9BB534789F}", "Note", "C", note_name_items(), osc_group_pitch, config_input_list_knob()));
  result.params.emplace_back(make_param_pct("{691F82E5-00C8-4962-89FE-9862092131CB}", "Cent", "0", -1, 1, osc_group_pitch, config_input_linear_accurate_knob()));
  return result;
}

void
osc_engine::process(plugin_topo const& topo, int module_index, plugin_block const& block)
{

}

}
