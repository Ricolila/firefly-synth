#include <firefly_synth/waves.hpp>
#include <plugin_base/topo/support.hpp>

using namespace plugin_base;

// waveforms for lfos and shapers
namespace firefly_synth
{

static std::string
wave_make_name_skew(int skew)
{
  switch (skew)
  {
  case wave_skew_type_off: return "Off";
  case wave_skew_type_lin: return "Linear";
  case wave_skew_type_xpb: return "Exp Bi";
  case wave_skew_type_xpu: return "Exp Uni";
  case wave_skew_type_scb: return "Scl Bi";
  case wave_skew_type_scu: return "Scl Uni";
  default: assert(false); return {};
  }
}

static std::string
wave_make_name_shape(int shape, wave_target target)
{
  switch (shape)
  {
  case wave_shape_type_saw: return target == wave_target::shaper? "Off": "Saw";
  case wave_shape_type_tri: return "Tri";
  case wave_shape_type_sin: return "Sin";
  case wave_shape_type_cos: return "Cos";
  case wave_shape_type_sin_sin: return "SinSin";
  case wave_shape_type_sin_cos: return "SinCos";
  case wave_shape_type_cos_sin: return "CosSin";
  case wave_shape_type_cos_cos: return "CosCos";
  case wave_shape_type_sin_sin_sin: return "SnSnSn";
  case wave_shape_type_sin_sin_cos: return "SnSnCs";
  case wave_shape_type_sin_cos_sin: return "SnCsSn";
  case wave_shape_type_sin_cos_cos: return "SnCsCs";
  case wave_shape_type_cos_sin_sin: return "CsSnSn";
  case wave_shape_type_cos_sin_cos: return "CsSnCs";
  case wave_shape_type_cos_cos_sin: return "CsCsSn";
  case wave_shape_type_cos_cos_cos: return "CsCsCs";
  case wave_shape_type_sqr_or_fold: return target == wave_target::shaper ? "Fldbk": "Sqr";
  case wave_shape_type_smooth_1: return "Smooth 1";
  case wave_shape_type_smooth_free_1: return "FreeSmth 1";
  case wave_shape_type_static_1: return "Static 1";
  case wave_shape_type_static_free_1: return "FreeStatic 1";
  case wave_shape_type_smooth_2: return "Smooth 2";
  case wave_shape_type_smooth_free_2: return "FreeSmth 2";
  case wave_shape_type_static_2: return "Static 2";
  case wave_shape_type_static_free_2: return "FreeStatic 2";
  default: assert(false); return {};
  }
}

static std::vector<topo_tag>
wave_skew_type_tags()
{
  std::vector<topo_tag> result;
  result.push_back(make_topo_tag_basic("{B15C7C6E-B1A4-49D3-85EF-12A7DC9EAA83}", wave_make_name_skew(wave_skew_type_off)));
  result.push_back(make_topo_tag_basic("{431D0E01-096B-4229-9ACE-25EFF7F2D4F0}", wave_make_name_skew(wave_skew_type_lin)));
  result.push_back(make_topo_tag_basic("{106A1510-3B99-4CC8-88D4-6D82C117EC33}", wave_make_name_skew(wave_skew_type_scu)));
  result.push_back(make_topo_tag_basic("{905936B8-3083-4293-A549-89F3979E02B7}", wave_make_name_skew(wave_skew_type_scb)));
  result.push_back(make_topo_tag_basic("{606B62CB-1C17-42CA-931B-61FA4C22A9F0}", wave_make_name_skew(wave_skew_type_xpu)));
  result.push_back(make_topo_tag_basic("{66CE54E3-84A7-4279-BF93-F0367266B389}", wave_make_name_skew(wave_skew_type_xpb)));
  return result;
}

static std::vector<topo_tag>
wave_shape_type_tags(wave_target target, bool global)
{
  std::vector<topo_tag> result;
  result.push_back(make_topo_tag_basic("{CA30E83B-2A11-4833-8A45-81F666A3A4F5}", wave_make_name_shape(wave_shape_type_saw, target)));
  result.push_back(make_topo_tag_basic("{41D6859E-3A16-432A-8851-D4E5D3F39662}", wave_make_name_shape(wave_shape_type_tri, target)));
  result.push_back(make_topo_tag_basic("{4A873C32-8B89-47ED-8C93-44FE0B6A7DCC}", wave_make_name_shape(wave_shape_type_sin, target)));
  result.push_back(make_topo_tag_basic("{102A7369-1994-41B1-9E2E-EC96AB60162E}", wave_make_name_shape(wave_shape_type_cos, target)));
  result.push_back(make_topo_tag_basic("{B6B07567-00C8-4076-B60F-D2AC10CE935A}", wave_make_name_shape(wave_shape_type_sin_sin, target)));
  result.push_back(make_topo_tag_basic("{B1305EE8-57EF-4BC6-8F3A-7A8BBD2359F2}", wave_make_name_shape(wave_shape_type_sin_cos, target)));
  result.push_back(make_topo_tag_basic("{FA227C0D-C604-45B3-B5DF-0E6C46FD9C2F}", wave_make_name_shape(wave_shape_type_cos_sin, target)));
  result.push_back(make_topo_tag_basic("{37D39A3C-2058-4DC6-A9AE-DFBB423EB0D2}", wave_make_name_shape(wave_shape_type_cos_cos, target)));
  result.push_back(make_topo_tag_basic("{CE36CD8E-5D1F-40F0-85E3-5DAD99AFC53E}", wave_make_name_shape(wave_shape_type_sin_sin_sin, target)));
  result.push_back(make_topo_tag_basic("{D921DA52-4D30-4AB7-95F3-CAA65C4F83AA}", wave_make_name_shape(wave_shape_type_sin_sin_cos, target)));
  result.push_back(make_topo_tag_basic("{C8D7BA33-6458-4972-8C31-D9BDAE0A3A54}", wave_make_name_shape(wave_shape_type_sin_cos_sin, target)));
  result.push_back(make_topo_tag_basic("{F67FB33F-CEDF-4F43-AD23-356775EECED2}", wave_make_name_shape(wave_shape_type_sin_cos_cos, target)));
  result.push_back(make_topo_tag_basic("{84E3B508-2AAA-4EBA-AD8C-B5AD1A055342}", wave_make_name_shape(wave_shape_type_cos_sin_sin, target)));
  result.push_back(make_topo_tag_basic("{B191D364-1951-449A-ABC7-09AEE9DB9FC4}", wave_make_name_shape(wave_shape_type_cos_sin_cos, target)));
  result.push_back(make_topo_tag_basic("{094482D1-5BAC-4F70-80F3-CA3924DDFBE6}", wave_make_name_shape(wave_shape_type_cos_cos_sin, target)));
  result.push_back(make_topo_tag_basic("{6A56691C-0F9C-4CE1-B835-85CF4D3B1F9B}", wave_make_name_shape(wave_shape_type_cos_cos_cos, target)));
  result.push_back(make_topo_tag_basic("{E16E6DC4-ACB3-4313-A094-A6EA9F8ACA85}", wave_make_name_shape(wave_shape_type_sqr_or_fold, target)));

  if (target != wave_target::lfo) return result;
  result.push_back(make_topo_tag_basic("{7176FE9E-D2A8-44FE-B312-93D712173D29}", wave_make_name_shape(wave_shape_type_smooth_1, target)));
  result.push_back(make_topo_tag_basic("{FA26FEFB-CACD-4D00-A986-246F09959F5E}", wave_make_name_shape(wave_shape_type_static_1, target)));
  result.push_back(make_topo_tag_basic("{54A731B7-1E4E-4F5C-9507-2A7FA3F79B20}", wave_make_name_shape(wave_shape_type_smooth_free_1, target)));
  result.push_back(make_topo_tag_basic("{FA86B2EE-12F7-40FB-BEB9-070E62C7C691}", wave_make_name_shape(wave_shape_type_static_free_1, target)));

  if (global) return result;
  result.push_back(make_topo_tag_basic("{4CB433AA-C15E-4560-999D-4C2D5DAF14B3}", wave_make_name_shape(wave_shape_type_smooth_2, target)));
  result.push_back(make_topo_tag_basic("{E3735241-E420-4E25-9B82-D6CD2D9E8C2C}", wave_make_name_shape(wave_shape_type_static_2, target)));
  result.push_back(make_topo_tag_basic("{23356ED1-CC60-475C-B927-541FBC0012C6}", wave_make_name_shape(wave_shape_type_smooth_free_2, target)));
  result.push_back(make_topo_tag_basic("{B4A2ABBF-2433-4B12-96B2-221B3F56FDAE}", wave_make_name_shape(wave_shape_type_static_free_2, target)));
  return result;
}

std::vector<list_item>
wave_skew_type_items()
{
  std::vector<list_item> result;
  auto tags = wave_skew_type_tags();
  for (int i = 0; i < tags.size(); i++)
    result.push_back({ tags[i].id, tags[i].menu_display_name });
  return result;
}

std::vector<list_item>
wave_shape_type_items(wave_target target, bool global)
{
  std::vector<list_item> result;
  auto tags = wave_shape_type_tags(target, global);
  for (int i = 0; i < tags.size(); i++)
    result.push_back({ tags[i].id, tags[i].menu_display_name });
  return result;
}

}