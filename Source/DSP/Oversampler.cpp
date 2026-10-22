// SPDX-License-Identifier: GPL-3.0-or-later
#include "Oversampler.h"

#include <complex>

namespace tsl {

namespace halfband {
namespace {

double ipow(double x, int n)
{
  double r = 1.0;
  while (n-- > 0) r *= x;
  return r;
}

void transitionParams(double transition, double& k, double& q)
{
  k = std::tan((1.0 - transition * 2.0) * kPiD / 4.0);
  k *= k;
  const double kk = std::pow(1.0 - k * k, 0.25);
  const double e = 0.5 * (1.0 - kk) / (1.0 + kk);
  const double e2 = e * e;
  const double e4 = e2 * e2;
  q = e * (1.0 + e4 * (2.0 + e4 * (15.0 + 150.0 * e4)));
}

double accNum(double q, int order, int c)
{
  double acc = 0.0, term = 0.0;
  int i = 0, sign = 1;
  do
  {
    term = ipow(q, i * (i + 1)) * std::sin((i * 2 + 1) * c * kPiD / order) * sign;
    acc += term;
    sign = -sign;
    ++i;
  } while (std::fabs(term) > 1e-100 && i < 1000);
  return acc;
}

double accDen(double q, int order, int c)
{
  double acc = 0.0, term = 0.0;
  int i = 1, sign = -1;
  do
  {
    term = ipow(q, i * i) * std::cos(i * 2 * c * kPiD / order) * sign;
    acc += term;
    sign = -sign;
    ++i;
  } while (std::fabs(term) > 1e-100 && i < 1000);
  return acc;
}

} // namespace

std::vector<double> designCoefs(int nCoefs, double transition)
{
  double k = 0.0, q = 0.0;
  transitionParams(transition, k, q);
  const int order = nCoefs * 2 + 1;
  std::vector<double> coefs(static_cast<size_t>(nCoefs));
  for (int index = 0; index < nCoefs; ++index)
  {
    const int c = index + 1;
    const double num = accNum(q, order, c) * std::pow(q, 0.25);
    const double den = accDen(q, order, c) + 0.5;
    const double ww = num / den;
    const double wwsq = ww * ww;
    const double x = std::sqrt((1.0 - wwsq * k) * (1.0 - wwsq / k)) / (1.0 + wwsq);
    coefs[static_cast<size_t>(index)] = (1.0 - x) / (1.0 + x);
  }
  return coefs;
}

double estimateAttenuationDb(int nCoefs, double transition)
{
  double k = 0.0, q = 0.0;
  transitionParams(transition, k, q);
  const int order = nCoefs * 2 + 1;
  const double t = ipow(q, order);
  // Классическая оценка для эллиптических halfband: atten ≈ -10 log10(t / (1 - t)) - 6
  return -10.0 * std::log10(t / std::max(1e-300, 1.0 - t)) - 6.02;
}

} // namespace halfband

// ---------------------------------------------------------------------------

void HalfbandStage::init(const std::vector<double>& coefs) noexcept
{
  mN = std::min(static_cast<int>(coefs.size()), kMaxCoefs);
  for (int i = 0; i < mN; ++i) mC[static_cast<size_t>(i)] = static_cast<float>(coefs[static_cast<size_t>(i)]);
  reset();
}

void HalfbandStage::reset() noexcept
{
  mUpX.fill(0.0f); mUpY.fill(0.0f);
  mDnX.fill(0.0f); mDnY.fill(0.0f);
}

void HalfbandStage::flushDenormals() noexcept
{
  for (int i = 0; i < mN; ++i)
  {
    mUpX[i] = undenormal(mUpX[i]); mUpY[i] = undenormal(mUpY[i]);
    mDnX[i] = undenormal(mDnX[i]); mDnY[i] = undenormal(mDnY[i]);
  }
}

namespace {

struct StageSpec
{
  int nCoefs;
  double transition;
};

// Ступень 0: fs↔2fs, полоса пропускания до ~0.21·2fs (≈20 кГц при 48 кГц).
// Последующие ступени обрабатывают уже ограниченный по полосе сигнал → шире переход, меньше коэффициентов.
constexpr StageSpec kSpecs[Oversampler::kMaxStages] = { { 10, 0.040 }, { 6, 0.120 }, { 4, 0.180 } };

/** Групповая задержка halfband-фильтра на низкой частоте (в отсчётах повышенной частоты). */
double stageGroupDelay(const std::vector<double>& c)
{
  auto phaseAt = [&](double w) {
    std::complex<double> a0(1.0, 0.0), a1(1.0, 0.0);
    const std::complex<double> z2inv = std::polar(1.0, -2.0 * w);
    for (size_t i = 0; i < c.size(); ++i)
    {
      const std::complex<double> h = (c[i] + z2inv) / (1.0 + c[i] * z2inv);
      if ((i & 1) == 0) a0 *= h; else a1 *= h;
    }
    // Порядок путей соответствует реализации: путь 0 получает задержанный отсчёт.
    const std::complex<double> hTotal = 0.5 * (a0 * std::polar(1.0, -w) + a1);
    return std::arg(hTotal);
  };
  const double w1 = 0.001, w2 = 0.002;
  return -(phaseAt(w2) - phaseAt(w1)) / (w2 - w1);
}

} // namespace

void Oversampler::prepare(int maxBlockSize, int maxFactor)
{
  mMaxBlock = std::max(1, maxBlockSize);
  const int f = std::max(1, std::min(8, maxFactor));
  mBufA.assign(static_cast<size_t>(mMaxBlock * f), 0.0f);
  mBufB.assign(static_cast<size_t>(mMaxBlock * f), 0.0f);
  for (int s = 0; s < kMaxStages; ++s)
    mStage[static_cast<size_t>(s)].init(halfband::designCoefs(kSpecs[s].nCoefs, kSpecs[s].transition));
  setFactor(mFactor);
}

void Oversampler::setFactor(int factor) noexcept
{
  if (factor >= 8) { mFactor = 8; mStages = 3; }
  else if (factor >= 4) { mFactor = 4; mStages = 2; }
  else if (factor >= 2) { mFactor = 2; mStages = 1; }
  else { mFactor = 1; mStages = 0; }
  if (!mBufA.empty() && static_cast<int>(mBufA.size()) < mMaxBlock * mFactor)
  {
    // Буфер подготовлен под меньший коэффициент — ограничиваем, а не аллоцируем.
    mFactor = std::max(1, static_cast<int>(mBufA.size()) / std::max(1, mMaxBlock));
    mStages = mFactor >= 8 ? 3 : mFactor >= 4 ? 2 : mFactor >= 2 ? 1 : 0;
  }
}

void Oversampler::reset() noexcept
{
  for (auto& s : mStage) s.reset();
}

float* Oversampler::upsample(const float* in, int n) noexcept
{
  n = std::min(n, mMaxBlock);
  if (mStages == 0)
  {
    copyBuffer(mBufA.data(), in, n);
    return mBufA.data();
  }
  // Ступени поочерёдно пишут в A/B так, чтобы итог оказался в A.
  const float* src = in;
  int len = n;
  for (int s = 0; s < mStages; ++s)
  {
    const bool toA = ((mStages - 1 - s) % 2) == 0;
    float* dst = toA ? mBufA.data() : mBufB.data();
    auto& st = mStage[static_cast<size_t>(s)];
    for (int i = 0; i < len; ++i)
      st.upsample(src[i], dst[2 * i], dst[2 * i + 1]);
    src = dst;
    len *= 2;
  }
  return mBufA.data();
}

void Oversampler::downsample(float* out, int n) noexcept
{
  n = std::min(n, mMaxBlock);
  if (mStages == 0)
  {
    copyBuffer(out, mBufA.data(), n);
    return;
  }
  const float* src = mBufA.data();
  int len = n * mFactor;
  bool inA = true;
  for (int s = mStages - 1; s >= 0; --s)
  {
    len /= 2;
    auto& st = mStage[static_cast<size_t>(s)];
    float* dst = (s == 0) ? out : (inA ? mBufB.data() : mBufA.data());
    for (int i = 0; i < len; ++i)
      dst[i] = st.downsample(src[2 * i], src[2 * i + 1]);
    src = dst;
    inA = !inA;
  }
  for (int s = 0; s < mStages; ++s) mStage[static_cast<size_t>(s)].flushDenormals();
}

float Oversampler::groupDelaySamples() const noexcept
{
  double total = 0.0;
  for (int s = 0; s < mStages; ++s)
  {
    const auto c = halfband::designCoefs(kSpecs[s].nCoefs, kSpecs[s].transition);
    const double rateMul = static_cast<double>(1 << (s + 1));
    total += 2.0 * stageGroupDelay(c) / rateMul; // апсемплинг + даунсемплинг
  }
  return static_cast<float>(total);
}

} // namespace tsl
