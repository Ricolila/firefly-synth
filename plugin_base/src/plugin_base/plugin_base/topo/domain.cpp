#include <plugin_base/topo/domain.hpp>

#include <sstream>
#include <iomanip>

namespace plugin_base {

plain_value
param_domain::default_plain() const
{
  plain_value result;
  INF_ASSERT_EXEC(text_to_plain(default_, result));
  return result;
}

std::string
param_domain::raw_to_text(
  double raw) const
{
  plain_value plain(raw_to_plain(raw));
  return plain_to_text(plain);
}

std::string 
param_domain::normalized_to_text(
  normalized_value normalized) const
{
  plain_value plain(normalized_to_plain(normalized));
  return plain_to_text(plain);
}

bool 
param_domain::text_to_normalized(
  std::string const& textual, normalized_value& normalized) const
{
  plain_value plain;
  if(!text_to_plain(textual, plain)) return false;
  normalized = plain_to_normalized(plain);
  return true;
}

std::string
param_domain::plain_to_text(plain_value plain) const
{
  std::string prefix = "";
  if(min < 0 &&  ((is_real() && plain.real() > 0) 
    || (!is_real() && plain.step() > 0)))
    prefix = "+";

  switch (type)
  {
  case domain_type::name: return names[plain.step()];
  case domain_type::item: return items[plain.step()].name;
  case domain_type::toggle: return plain.step() == 0 ? "Off" : "On";
  case domain_type::step: return prefix + std::to_string(plain.step() + display_offset);
  default: break;
  }

  std::ostringstream stream;
  int mul = display == domain_display::percentage ? 100 : 1;
  stream << std::fixed << std::setprecision(precision) << (plain.real() * mul);
  return prefix + stream.str();
}

bool 
param_domain::text_to_plain(
  std::string const& textual, plain_value& plain) const
{
  if (type == domain_type::name)
  {
    for (int i = 0; i < names.size(); i++)
      if (names[i] == textual)
        return plain = plain_value::from_step(i), true;
    return false;
  }

  if (type == domain_type::item)
  {
    for (int i = 0; i < items.size(); i++)
      if (items[i].name == textual)
        return plain = plain_value::from_step(i), true;
    return false;
  }

  if (type == domain_type::dependent)
  {
    assert(dependents.size() > 1);
    assert(min == 0 && max == 999);
    assert(0 <= dependent_index && dependent_index < topo_max);
    assert(dependents[0]->type == domain_type::item ||
      dependents[0]->type == domain_type::name ||
      dependents[0]->type == domain_type::step);
    for (int i = 1; i < dependents.size(); i++)
      assert(dependents[i]->type == dependents[0]->type);
  }

  if (type == domain_type::toggle)
  {
    if (textual == "On") return plain = plain_value::from_step(1), true;
    if (textual == "Off") return plain = plain_value::from_step(0), true;
    return false;
  }

  std::istringstream stream(textual);
  if (type == domain_type::step)
  {
    int step = std::numeric_limits<int>::max();
    stream >> step;
    plain = plain_value::from_step(step - display_offset);
    return min <= step && step <= max;
  }

  float real = std::numeric_limits<float>::max();
  stream >> real;
  real /= (display == domain_display::normal) ? 1 : 100;
  plain = plain_value::from_real(real);
  return min <= real && real <= max;
}

void
param_domain::validate() const
{
  assert(max > min);
  assert(default_.size());
  assert((type == domain_type::log) == (exp != 0));
  assert((type == domain_type::step) || (display_offset == 0));
  assert(display == domain_display::normal || type == domain_type::linear);

  if (type == domain_type::toggle)
  {
    assert(min == 0);
    assert(max == 1);
  }

  if (type == domain_type::name)
  {
    assert(min == 0);
    assert(unit.size() == 0);
    assert(names.size() > 0);
    assert(max == names.size() - 1);
  }

  if (type == domain_type::item)
  {
    assert(min == 0);
    assert(unit.size() == 0);
    assert(items.size() > 0);
    assert(max == items.size() - 1);
  }

  if (!is_real())
  {
    assert(precision == 0);
    assert((int)min == min);
    assert((int)max == max);
    assert(min <= default_plain().step());
    assert(max >= default_plain().step());
    assert(display == domain_display::normal);
  }

  if (is_real())
  {
    assert(min <= default_plain().real());
    assert(max >= default_plain().real());
    assert(0 <= precision && precision <= 10);
  }
}

}