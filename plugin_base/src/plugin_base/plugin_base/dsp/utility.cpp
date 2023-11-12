#include <plugin_base/dsp/utility.hpp>
#include <immintrin.h>

namespace plugin_base {

param_filter::
param_filter(float rate, float freq)
{
  float w = 2.0f * rate;
  float angle = freq * 2.0f * pi32;
  float norm = 1.0f / (angle + w);
  b = (w - angle) * norm;
  a = angle * norm;
}

std::pair<std::uint32_t, std::uint32_t>
disable_denormals()
{
  uint32_t ftz = _MM_GET_FLUSH_ZERO_MODE();
  uint32_t daz = _MM_GET_DENORMALS_ZERO_MODE();
  _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
  _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
  return std::make_pair(ftz, daz);
}

void
restore_denormals(std::pair<std::uint32_t, std::uint32_t> state)
{
  _MM_SET_FLUSH_ZERO_MODE(state.first);
  _MM_SET_DENORMALS_ZERO_MODE(state.second);
}

}