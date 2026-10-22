// SPDX-License-Identifier: GPL-3.0-or-later
#include "Drives.h"
#include "../DSP/Waveshapers.h"

namespace tsl {

namespace {

/** Блочная рампа: значения на начало и конец блока для интерполяции внутри оверсэмплинга. */
struct Ramp
{
  float v0, v1;
  float at(int i, int len) const noexcept { return v0 + (v1 - v0) * (static_cast<float>(i + 1) / static_cast<float>(len)); }
};

} // namespace

// ============================================================================
// Boost: чистое усиление с плотным низом (Tight) и наклоном верха (Bright).
// ============================================================================
void Boost::prepare(const PrepareInfo& info)
{
  mSampleRate = info.sampleRate;
  mMaxBlock = info.maxBlockSize;
  for (auto& c : mCh) { c.os.prepare(mMaxBlock, 2); c.os.setFactor(2); }
  mGain.prepare(static_cast<float>(mSampleRate), 0.03f);
  mGain.snap(dbToGain(6.f));
  mTight = -1.f;
  mBright = -100.f;
  reset();
}

void Boost::reset() noexcept
{
  for (auto& c : mCh) c.reset();
}

void Boost::update(const ParamValues& p) noexcept
{
  mGain.setTarget(dbToGain(p[kBoostLevel]));
  if (p[kBoostTight] != mTight || p[kBoostBright] != mBright)
  {
    mTight = p[kBoostTight];
    mBright = p[kBoostBright];
    const auto hp = design::make(FilterType::HighPass, mSampleRate, mTight, 0.6);
    const auto hs = design::make(FilterType::HighShelf, mSampleRate, 2500.0, 0.6, mBright);
    for (auto& c : mCh) { c.pre1.setCoefs(hp); c.post1.setCoefs(hs); }
  }
}

void Boost::processChannel(DriveChannel& c, float* x, int n, bool) noexcept
{
  c.pre1.processBlock(x, n);
  c.post1.processBlock(x, n);
  // Гейн с рампой; мягкий «потолок питания» на +12 дБFS (ограничение по питанию реального бустера)
  float* up = c.os.upsample(x, n);
  const int len = n * c.os.factor();
  const Ramp g{ mBlockGain, mGain.current() };
  for (int i = 0; i < len; ++i) up[i] = 4.0f * shape::softClip(up[i] * g.at(i, len) * 0.25f);
  c.os.downsample(x, n);
}

void Boost::process(AudioBlock& b) noexcept
{
  mBlockGain = mGain.current();
  mGain.skip(b.numFrames);
  processChannel(mCh[0], b.l, b.numFrames, true);
  if (b.stereo) processChannel(mCh[1], b.r, b.numFrames, true);
}

// ============================================================================
// Overdrive: ОУ с диодами в обратной связи (чистый сигнал + ограниченная HPF-часть).
// Transparent — шире полоса, больше низа, мягче; Mid Focus — характерный горб середины.
// ============================================================================
void Overdrive::prepare(const PrepareInfo& info)
{
  mSampleRate = info.sampleRate;
  mMaxBlock = info.maxBlockSize;
  const float sr = static_cast<float>(mSampleRate);
  for (auto& c : mCh) { c.os.prepare(mMaxBlock, 4); c.os.setFactor(4); }
  mDrive.prepare(sr, 0.03f); mLevel.prepare(sr, 0.03f); mMix.prepare(sr, 0.03f);
  mDrive.snap(1.f); mLevel.snap(1.f); mMix.snap(1.f);
  mMode = -1;
  mTone = -1.f;
  reset();
}

void Overdrive::reset() noexcept
{
  for (auto& c : mCh) c.reset();
}

void Overdrive::designFilters() noexcept
{
  const bool mid = mMode == 1;
  // HPF перед клиппингом: определяет, сколько низа уходит в ограничение
  const auto pre = design::make(FilterType::HighPass1, mSampleRate, mid ? 720.0 : 200.0, 0.707);
  // ФНЧ обратной связи ОУ (сглаживает верх искажения)
  const auto fb = design::make(FilterType::LowPass1, mSampleRate, mid ? 5600.0 : 7200.0, 0.707);
  // Tone: полка верха −12…+6 дБ
  const auto tone = design::make(FilterType::HighShelf, mSampleRate, 1600.0, 0.55, -12.0 + 18.0 * mTone);
  const auto post = design::make(FilterType::LowPass, mSampleRate, mid ? 7500.0 : 9500.0, 0.707);
  for (auto& c : mCh)
  {
    c.pre1.setCoefs(pre);
    c.post1.setCoefs(fb);
    c.tone.setCoefs(tone);
    c.post2.setCoefs(post);
  }
}

void Overdrive::update(const ParamValues& p) noexcept
{
  const int mode = static_cast<int>(p[kOdMode]);
  const float tone = p[kOdTone] * 0.1f;
  if (mode != mMode || tone != mTone)
  {
    mMode = mode;
    mTone = tone;
    designFilters();
  }
  const float d = p[kOdDrive] * 0.1f;
  // Mid Focus: до ~+38 дБ; Transparent: до ~+30 дБ
  mDrive.setTarget(1.0f + (mMode == 1 ? 80.0f : 32.0f) * d * d);
  mLevel.setTarget(dbToGain(p[kOdLevel]) * 0.5f);
  mMix.setTarget(p[kOdMix] * 0.01f);
}

void Overdrive::processChannel(DriveChannel& c, float* x, int n) noexcept
{
  // Подготовка ветви искажения в базовой частоте (линейные фильтры)
  // x остаётся «чистым», ветвь — HPF.
  const bool mid = mMode == 1;
  const float vd = mid ? 0.30f : 0.42f;
  float* up = c.os.upsample(x, n); // чистый сигнал в оверсэмплинге
  const int len = n * c.os.factor();
  const Ramp drive{ mBlockDrive, mDrive.current() };
  const Ramp mix{ mBlockMix, mMix.current() };
  // HPF ветви применяем в оверсэмплинге через однополюсный ФВЧ (state хранится в c.state)
  const float fHp = mid ? 720.0f : 200.0f;
  const float a = std::exp(-kTwoPi * fHp / (static_cast<float>(mSampleRate) * static_cast<float>(c.os.factor())));
  float lp = c.state;
  for (int i = 0; i < len; ++i)
  {
    const float clean = up[i];
    lp = clean + a * (lp - clean);
    const float hp = clean - lp;
    const float clipped = shape::feedbackDiodes(hp * drive.at(i, len), vd);
    const float wet = clean + clipped;
    up[i] = clean + mix.at(i, len) * (wet - clean);
  }
  c.state = undenormal(lp);
  c.os.downsample(x, n);
  c.post1.processBlock(x, n);
  c.tone.processBlock(x, n);
  c.post2.processBlock(x, n);
  const Ramp level{ mBlockLevel, mLevel.current() };
  for (int i = 0; i < n; ++i) x[i] *= level.at(i, n);
}

void Overdrive::process(AudioBlock& b) noexcept
{
  const int n = b.numFrames;
  mBlockDrive = mDrive.current(); mBlockLevel = mLevel.current(); mBlockMix = mMix.current();
  mDrive.skip(n); mLevel.skip(n); mMix.skip(n);
  processChannel(mCh[0], b.l, n);
  if (b.stereo) processChannel(mCh[1], b.r, n);
}

// ============================================================================
// Distortion: высокоусиливающий ОУ (ограничение полосы GBW) + диоды на землю.
// ============================================================================
void Distortion::prepare(const PrepareInfo& info)
{
  mSampleRate = info.sampleRate;
  mMaxBlock = info.maxBlockSize;
  const float sr = static_cast<float>(mSampleRate);
  for (auto& c : mCh) { c.os.prepare(mMaxBlock, 4); c.os.setFactor(4); }
  mDrive.prepare(sr, 0.03f); mLevel.prepare(sr, 0.03f);
  mDrive.snap(10.f); mLevel.snap(0.5f);
  mTone = mTight = mDriveKnob = -1.f;
  reset();
}

void Distortion::reset() noexcept
{
  for (auto& c : mCh) c.reset();
}

void Distortion::designFilters() noexcept
{
  const double gain = std::pow(10.0, (mDriveKnob * 54.0 + 6.0) / 20.0);
  const double gbwFc = clampv(1.2e6 / gain, 2200.0, 18000.0); // ОУ с GBW ~1.2 МГц
  const auto tight = design::make(FilterType::HighPass, mSampleRate, mTight, 0.65);
  mGbwA = std::exp(static_cast<float>(-kTwoPiD * gbwFc / (mSampleRate * 4.0)));
  const double toneFc = 700.0 * std::pow(12.0, mTone); // 700 Гц … 8.4 кГц
  const auto tone = design::make(FilterType::LowPass, mSampleRate, toneFc, 0.62);
  const auto post = design::make(FilterType::LowPass, mSampleRate, 11000.0, 0.707);
  for (auto& c : mCh)
  {
    c.pre1.setCoefs(tight);
    c.tone.setCoefs(tone);
    c.post1.setCoefs(post);
  }
}

void Distortion::update(const ParamValues& p) noexcept
{
  mMode = static_cast<int>(p[kDistMode]);
  const float drive = p[kDistDrive] * 0.1f, tone = p[kDistTone] * 0.1f, tight = p[kDistTight];
  if (drive != mDriveKnob || tone != mTone || tight != mTight)
  {
    mDriveKnob = drive; mTone = tone; mTight = tight;
    designFilters();
  }
  mDrive.setTarget(std::pow(10.0f, (drive * 54.0f + 6.0f) / 20.0f));
  mLevel.setTarget(dbToGain(p[kDistLevel]) * 0.7f);
}

void Distortion::processChannel(DriveChannel& c, float* x, int n) noexcept
{
  c.pre1.processBlock(x, n);
  float* up = c.os.upsample(x, n);
  const int len = n * c.os.factor();
  const Ramp drive{ mBlockDrive, mDrive.current() };
  float lp = c.state;
  const float a = mGbwA;
  for (int i = 0; i < len; ++i)
  {
    // Предусилитель ОУ: усиление → ограничение по питанию → полоса GBW (зависит от усиления)
    float v = up[i] * drive.at(i, len);
    v = 4.5f * shape::softClip(v / 4.5f);
    lp = v + a * (lp - v);
    v = lp;
    switch (mMode)
    {
      case 0: v = shape::shuntDiodes(v, 0.55f, 2.2f) + 0.04f * shape::softClip(v); break; // кремний, лёгкая асимметрия
      case 1: v = shape::shuntDiodes(v, 1.15f, 3.0f) * 0.55f; break;                     // LED: больше запаса
      default: v = shape::hardClip(v * 1.2f, 0.62f) * 0.9f; break;                       // жёсткое
    }
    up[i] = v;
  }
  c.state = undenormal(lp);
  c.os.downsample(x, n);
  c.tone.processBlock(x, n);
  c.post1.processBlock(x, n);
  const Ramp level{ mBlockLevel, mLevel.current() };
  for (int i = 0; i < n; ++i) x[i] = c.dc.highPass(x[i]) * level.at(i, n);
}

void Distortion::process(AudioBlock& b) noexcept
{
  const int n = b.numFrames;
  mBlockDrive = mDrive.current(); mBlockLevel = mLevel.current();
  mDrive.skip(n); mLevel.skip(n);
  for (auto& c : mCh) c.dc.setLowPass(12.0f, static_cast<float>(mSampleRate));
  processChannel(mCh[0], b.l, n);
  if (b.stereo) processChannel(mCh[1], b.r, n);
}

// ============================================================================
// Fuzz: двухтранзисторный фузз — сильное асимметричное ограничение, смещение рабочей
// точки (Bias: от «голодного» сплаттера до плотного), режим Octave — выпрямление.
// ============================================================================
void Fuzz::prepare(const PrepareInfo& info)
{
  mSampleRate = info.sampleRate;
  mMaxBlock = info.maxBlockSize;
  const float sr = static_cast<float>(mSampleRate);
  for (auto& c : mCh) { c.os.prepare(mMaxBlock, 4); c.os.setFactor(4); }
  mGain.prepare(sr, 0.03f); mLevel.prepare(sr, 0.03f); mBias.prepare(sr, 0.05f);
  mGain.snap(50.f); mLevel.snap(0.4f); mBias.snap(0.f);
  mTone = -1.f;
  reset();
}

void Fuzz::reset() noexcept
{
  for (auto& c : mCh) c.reset();
}

void Fuzz::designFilters() noexcept
{
  const auto inHp = design::make(FilterType::HighPass1, mSampleRate, mMode == 1 ? 140.0 : 90.0, 0.707);
  const double toneFc = 900.0 * std::pow(9.0, mTone); // 900 Гц … 8.1 кГц
  const auto tone = design::make(FilterType::LowPass, mSampleRate, toneFc, 0.6);
  const auto post = design::make(FilterType::HighPass1, mSampleRate, 30.0, 0.707);
  for (auto& c : mCh)
  {
    c.pre1.setCoefs(inHp);
    c.tone.setCoefs(tone);
    c.post1.setCoefs(post);
  }
}

void Fuzz::update(const ParamValues& p) noexcept
{
  const int mode = static_cast<int>(p[kFuzzMode]);
  const float tone = p[kFuzzTone] * 0.1f;
  if (mode != mMode || tone != mTone) { mMode = mode; mTone = tone; designFilters(); }
  const float f = p[kFuzzFuzz] * 0.1f;
  mGain.setTarget(2.0f + 420.0f * f * f);
  mLevel.setTarget(dbToGain(p[kFuzzLevel]) * 0.45f);
  mBias.setTarget((p[kFuzzBias] * 0.1f - 0.5f) * 2.0f); // −1…+1
}

void Fuzz::processChannel(DriveChannel& c, float* x, int n) noexcept
{
  c.pre1.processBlock(x, n);
  float* up = c.os.upsample(x, n);
  const int len = n * c.os.factor();
  const Ramp gain{ mBlockGain, mGain.current() };
  const Ramp bias{ mBlockBias, mBias.current() };
  for (int i = 0; i < len; ++i)
  {
    const float b = bias.at(i, len);
    float v = up[i] * gain.at(i, len);
    if (mMode == 2) v = std::fabs(v) * 1.4f - 0.2f; // выпрямление → октава вверх
    // «Голодное» питание при низком bias: мёртвая зона у нуля (сплаттер/гейтинг)
    const float dz = b < 0.0f ? -b * 0.18f : 0.0f;
    const float av = std::fabs(v);
    v = av > dz ? std::copysign(av - dz, v) : 0.0f;
    const float offset = 0.45f * b;
    float y;
    if (mMode == 1) y = shape::shuntDiodes(v + offset, 0.6f, 5.0f) - shape::shuntDiodes(offset, 0.6f, 5.0f);
    else y = shape::asymClip(v + offset, 0.55f, 0.95f) - shape::asymClip(offset, 0.55f, 0.95f);
    up[i] = y;
  }
  c.os.downsample(x, n);
  c.tone.processBlock(x, n);
  c.post1.processBlock(x, n);
  const Ramp level{ mBlockLevel, mLevel.current() };
  for (int i = 0; i < n; ++i) x[i] *= level.at(i, n);
}

void Fuzz::process(AudioBlock& b) noexcept
{
  const int n = b.numFrames;
  mBlockGain = mGain.current(); mBlockLevel = mLevel.current(); mBlockBias = mBias.current();
  mGain.skip(n); mLevel.skip(n); mBias.skip(n);
  processChannel(mCh[0], b.l, n);
  if (b.stereo) processChannel(mCh[1], b.r, n);
}

} // namespace tsl
