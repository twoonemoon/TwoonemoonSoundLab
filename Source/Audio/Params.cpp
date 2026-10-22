// SPDX-License-Identifier: GPL-3.0-or-later
#include "Params.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace tsl {

namespace {

#define TSL_SPEC_ENTRY(e, key, name, group, kind, mn, mx, def, step, shape, unit, lbl, nl) \
  ParamSpec{ key, name, group, ParamKind::kind, mn, mx, def, step, ParamShape::shape, ParamUnit::unit, lbl, nl },

const ParamSpec kSpecs[kNumParams] = { TSL_PARAM_LIST(TSL_SPEC_ENTRY) };

#undef TSL_SPEC_ENTRY

} // namespace

const ParamSpec& paramSpec(int id) noexcept
{
  return kSpecs[std::clamp(id, 0, kNumParams - 1)];
}

int paramIdForKey(const char* key) noexcept
{
  if (!key) return -1;
  for (int i = 0; i < kNumParams; ++i)
    if (std::strcmp(kSpecs[i].key, key) == 0) return i;
  return -1;
}

float paramClamp(int id, float plain) noexcept
{
  const ParamSpec& s = paramSpec(id);
  if (!std::isfinite(plain)) return s.defaultValue;
  float v = std::clamp(plain, s.minValue, s.maxValue);
  if (s.kind != ParamKind::Float) v = std::round(v);
  return v;
}

float paramToNormalized(int id, float plain) noexcept
{
  const ParamSpec& s = paramSpec(id);
  const float v = paramClamp(id, plain);
  const float range = s.maxValue - s.minValue;
  if (range <= 0.0f) return 0.0f;
  switch (s.shape)
  {
    case ParamShape::Exp:
      if (s.minValue > 0.0f) return std::log(v / s.minValue) / std::log(s.maxValue / s.minValue);
      return (v - s.minValue) / range;
    case ParamShape::Pow2: return std::sqrt((v - s.minValue) / range);
    case ParamShape::Pow3: return std::cbrt((v - s.minValue) / range);
    case ParamShape::Linear:
    default: return (v - s.minValue) / range;
  }
}

float paramFromNormalized(int id, float n) noexcept
{
  const ParamSpec& s = paramSpec(id);
  n = std::clamp(n, 0.0f, 1.0f);
  const float range = s.maxValue - s.minValue;
  float v;
  switch (s.shape)
  {
    case ParamShape::Exp:
      v = s.minValue > 0.0f ? s.minValue * std::pow(s.maxValue / s.minValue, n) : s.minValue + n * range;
      break;
    case ParamShape::Pow2: v = s.minValue + n * n * range; break;
    case ParamShape::Pow3: v = s.minValue + n * n * n * range; break;
    case ParamShape::Linear:
    default: v = s.minValue + n * range; break;
  }
  return paramClamp(id, v);
}

} // namespace tsl
