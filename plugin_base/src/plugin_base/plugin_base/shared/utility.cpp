#include <plugin_base/desc/plugin.hpp>
#include <plugin_base/shared/utility.hpp>
#include <juce_core/juce_core.h>

#include <chrono>
#include <cassert>
#include <complex>
#include <fstream>

using namespace juce;

namespace plugin_base {

// https://rosettacode.org/wiki/Fast_Fourier_transform#C.2B.2B
static void
fft(std::complex<float>* inout, std::complex<float>* scratch, int count)
{
  if (count < 2) return;
  assert(count == next_pow2(count));
  std::complex<float>* even = scratch;
  std::complex<float>* odd = scratch + count / 2;
  for (std::size_t i = 0; i < count / 2; i++) even[i] = inout[i * 2];
  for (std::size_t i = 0; i < count / 2; i++) odd[i] = inout[i * 2 + 1];
  fft(odd, inout, count / 2);
  fft(even, inout, count / 2);
  for (std::size_t i = 0; i < count / 2; i++)
  {
    float im = -2.0f * pi32 * i / count;
    std::complex<float> t = std::polar(1.0f, im) * odd[i];
    inout[i] = even[i] + t;
    inout[i + count / 2] = even[i] - t;
  }
}

double
seconds_since_epoch()
{
  auto ticks = std::chrono::system_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::nanoseconds>(ticks).count() / 1000000000.0;
}

std::filesystem::path
get_resource_location(format_config const* config)
{
  File file(File::getSpecialLocation(File::currentExecutableFile));
  return config->resources_folder(file.getFullPathName().toStdString());
}

std::vector<char>
file_load(std::filesystem::path const& path)
{
  std::ifstream stream(path, std::ios::binary | std::ios::ate);
  if (stream.bad()) return {};
  std::streamsize size = stream.tellg();
  if (size <= 0) return {};
  stream.seekg(0, std::ios::beg);
  std::vector<char> data(size, 0);
  stream.read(data.data(), size);
  if (stream.bad()) return {};
  return data;
}

std::vector<float> 
fft(std::vector<float> const& in)
{
  std::size_t pow2 = next_pow2(in.size());
  std::vector<std::complex<float>> inout(pow2, std::complex<float>());
  std::vector<std::complex<float>> scratch(pow2, std::complex<float>());
  for (std::size_t i = 0; i < in.size(); i++)
    inout[i] = std::complex<float>(in[i], 0.0f);
  fft(inout.data(), scratch.data(), pow2);

  // drop above nyquist
  inout.erase(inout.begin() + pow2 / 2, inout.end());
  
  // scale to 0..1
  std::vector<float> result;
  float max = std::numeric_limits<float>::min();
  float min = std::numeric_limits<float>::max();
  for (int i = 0; i < inout.size(); i++)
  {
    max = std::max(max, std::abs(inout[i].real()));
    min = std::min(min, std::abs(inout[i].real()));
  }
  for (int i = 0; i < inout.size(); i++)
    result.push_back((std::abs(inout[i].real()) - min) / (max - min));
  return result;
}

}