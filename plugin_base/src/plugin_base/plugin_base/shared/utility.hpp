#pragma once

#include <juce_dsp/juce_dsp.h>

#include <set>
#include <string>
#include <vector>
#include <sstream>
#include <cstring>
#include <cassert>
#include <utility>
#include <cstdint>
#include <iterator>
#include <algorithm>
#include <filesystem>

#define PB_PREVENT_ACCIDENTAL_COPY(x)  \
  x(x&&) = default;               \
  x& operator = (x&&) = default;  \
  explicit x(x const&) = default; \
  x& operator = (x const&) = delete
#define PB_PREVENT_ACCIDENTAL_COPY_DEFAULT_CTOR(x) \
  PB_PREVENT_ACCIDENTAL_COPY(x); \
  x() = default

#if (WIN32)
#define PB_EXPORT __declspec(dllexport)
#elif (defined __linux__) || (defined __FreeBSD__) || (defined __APPLE__)
#define PB_EXPORT __attribute__((visibility("default")))
#else
#error
#endif

#define PB_STR_(x) #x
#define PB_STR(x) PB_STR_(x)
#define PB_COMBINE_(x, y) x##y
#define PB_COMBINE(x, y) PB_COMBINE_(x, y)
#define PB_VERSION_TEXT(major, minor, patch) PB_STR(major.minor.patch)
#define PB_ASSERT_EXEC(x) do { if(!(x)) assert(false); } while(false)

namespace plugin_base {

// wraps a juce fft and retains the output buffer
class cached_fft
{
  int const _in_samples;
  std::vector<float> _output;
  juce::dsp::FFT const _juce_fft;

public:
  cached_fft(int in_samples);
  std::vector<float> const& perform(std::vector<float> const& in);
};

struct format_basic_config;
inline char constexpr resource_folder_themes[] = "themes";
inline char constexpr resource_folder_presets[] = "presets";
inline float constexpr pi32 = 3.14159265358979323846264338327950288f;
inline double constexpr pi64 = 3.14159265358979323846264338327950288;

double seconds_since_epoch();
std::vector<char> file_load(std::filesystem::path const& path);
std::filesystem::path get_resource_location(format_basic_config const* config);
std::vector<float> log_remap_series_x(std::vector<float> const& in, float midpoint);

template <class T> 
inline int signum(T val) 
{ return (T(0) < val) - (val < T(0)); }

inline std::string
float_to_string(float x, int prec)
{
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(prec) << x;
  return stream.str();
}

inline std::uint64_t
next_pow2(std::uint64_t x)
{
  std::uint64_t result = 0;
  if (x == 0) return 0;
  if (x && !(x & (x - 1))) return x;
  while (x != 0) x >>= 1, result++;
  return 1ULL << result;
}

template <class T> std::string 
to_8bit_string(T const* source)
{
  T c = *source;
  std::string result;
  while (c != static_cast<T>('\0'))
  {
    result += static_cast<char>(c);
    c = *++source;
  }
  return result;
}

template <class T>
void from_8bit_string(T* dest, int count, char const* source)
{
  memset(dest, 0, sizeof(*dest) * count);
  for (int i = 0; i < count - 1 && i < strlen(source); i++)
    dest[i] = source[i];
}

template <class T, int N>
void from_8bit_string(T(&dest)[N], char const* source)
{ from_8bit_string(dest, N, source); }

template <class T> std::vector<T> 
vector_explicit_copy(std::vector<T> const& in)
{
  std::vector<T> result;
  for (int i = 0; i < in.size(); i++)
    result.push_back(T(in[i]));
  return result;
}

template <class T> std::vector<std::vector<int>>
vector_index_count(std::vector<std::vector<T>> const& vs)
{
  int count = 0;
  std::vector<std::vector<int>> result(vs.size(), std::vector<int>());
  for (int i = 0; i < vs.size(); i++)
    for (int j = 0; j < vs[i].size(); j++)
      result[i].push_back(count++);
  return result;
}

template <class T, class Pred> std::vector<T> 
vector_filter(std::vector<T> const& in, Pred pred)
{
  std::vector<T> result;
  std::copy_if(in.begin(), in.end(), std::back_inserter(result), pred);
  return result;
}

template <class T> std::vector<T>
vector_join(std::vector<std::vector<T>> const& vs)
{
  std::vector<T> result;
  for (int i = 0; i < vs.size(); i++)
    std::copy(vs[i].begin(), vs[i].end(), std::back_inserter(result));
  return result;
}

template <class T, class Unary> auto 
vector_map(std::vector<T> const& in, Unary op) -> 
std::vector<decltype(op(in[0]))>
{
  std::vector<decltype(op(in[0]))> result(in.size(), decltype(op(in[0])) {});
  std::transform(in.begin(), in.end(), result.begin(), op);
  return result;
}

}
