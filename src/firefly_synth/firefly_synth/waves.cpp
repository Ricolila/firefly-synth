#include <firefly_synth/waves.hpp>
#include <plugin_base/topo/support.hpp>

using namespace plugin_base;

// waveforms for lfos and shapers
namespace firefly_synth
{

static std::string
wave_make_header_skew(int skew)
{
  switch (skew)
  {
  case wave_skew_type_off: return "Plain";
  case wave_skew_type_lin: return "Linear";
  case wave_skew_type_exp: return "Exp";
  default: assert(false); return {};
  }
}

static std::string
wave_make_name_skew(int skew)
{
  switch (skew)
  {
  case wave_skew_type_off: return "Pln";
  case wave_skew_type_lin: return "Lin";
  case wave_skew_type_exp: return "Exp";
  default: assert(false); return {};
  }
}

static std::string
wave_make_name_shape(int shape)
{
  switch (shape)
  {
  case wave_shape_type_saw: return "Saw";
  case wave_shape_type_sqr: return "Sqr";
  case wave_shape_type_sin: return "Sin";
  default: assert(false); return {};
  }
}

std::string 
wave_make_header(int skew_x, int skew_y)
{
  auto header_x = wave_make_header_skew(skew_x);
  auto header_y = wave_make_header_skew(skew_y);
  return header_x + "X/" + header_y + "Y"; 
}

std::string 
wave_make_name(int skew_x, int skew_y, int shape)
{
  auto name_x = wave_make_name_skew(skew_x);
  auto name_y = wave_make_name_skew(skew_y);
  auto name_shape = wave_make_name_shape(shape);
  return name_shape + "." + name_x + "X/" + name_y + "Y";
}

std::vector<topo_tag> 
wave_skew_type_tags()
{
  std::vector<topo_tag> result;
  result.push_back(make_topo_tag("{B15C7C6E-B1A4-49D3-85EF-12A7DC9EAA83}", wave_make_name_skew(wave_skew_type_off)));
  result.push_back(make_topo_tag("{431D0E01-096B-4229-9ACE-25EFF7F2D4F0}", wave_make_name_skew(wave_skew_type_lin)));
  result.push_back(make_topo_tag("{606B62CB-1C17-42CA-931B-61FA4C22A9F0}", wave_make_name_skew(wave_skew_type_exp)));
  return result;
}

std::vector<topo_tag> 
wave_shape_type_tags()
{
  std::vector<topo_tag> result;
  result.push_back(make_topo_tag("{CA30E83B-2A11-4833-8A45-81F666A3A4F5}", wave_make_name_shape(wave_shape_type_saw)));
  result.push_back(make_topo_tag("{7176FE9E-D2A8-44FE-B312-93D712173D29}", wave_make_name_shape(wave_shape_type_sqr)));
  result.push_back(make_topo_tag("{4A873C32-8B89-47ED-8C93-44FE0B6A7DCC}", wave_make_name_shape(wave_shape_type_sin)));
  return result;
}

multi_menu 
make_wave_multi_menu()
{
  return make_multi_menu(
    wave_skew_type_tags(), 
    wave_skew_type_tags(), 
    wave_shape_type_tags(), 
    wave_make_header, wave_make_name);
}

}