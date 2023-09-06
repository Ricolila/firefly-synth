#pragma once
#include <string>
#include <cassert>

#define INF_STR_(x) #x
#define INF_STR(x) INF_STR_(x)
#define INF_VERSION_TEXT(major, minor) INF_STR(major.minor)

#define INF_ASSERT_EXEC(x) \
  do { if(!(x)) assert(false); } while(false)

#define INF_DECLARE_MOVE_ONLY(x) \
  x() = default; \
  x(x&&) = default; \
  x(x const&) = delete; \
  x& operator = (x&&) = default; \
  x& operator = (x const&) = delete

namespace infernal::base {

// trims characters outside range
template <class T> std::string 
to_8bit_string(T const* source)
{
  T c;
  std::string result;
  while ((c = *source++) != static_cast<T>('\0'))
    result += static_cast<char>(c);
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

}
