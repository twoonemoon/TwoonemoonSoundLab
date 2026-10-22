// SPDX-License-Identifier: GPL-3.0-or-later
#include "Engine.h"

#include "../DSP/DspCommon.h"
#include "../Amps/Amp.h"
#include "../Cabinet/Cabinet.h"
#include "../Effects/Compressor.h"
#include "../Effects/Delay.h"
#include "../Effects/Eq.h"
#include "../Effects/Modulation.h"
#include "../Effects/PitchFx.h"
#include "../Effects/Reverb.h"
#include "../Effects/Drives.h"
#include "../Effects/NoiseGate.h"

namespace tsl {

// ---------------------------------------------------------------------------
// Метаданные модулей

namespace {

struct ModuleMeta
{
  ModuleId id;
  const char* key;
  const char* displayName;
  int enableParam;
  const char* shortName;
};

const ModuleMeta kModuleMeta[kNumModules] = {
  { ModuleId::Gate, "gate", "Noise Gate", kGateOn, "GATE" },
  { ModuleId::Compressor, "comp", "Compressor", kCompOn, "COMP" },
  { ModuleId::Boost, "boost", "Boost", kBoostOn, "BST" },
  { ModuleId::Overdrive, "od", "Overdrive", kOdOn, "OD" },
  { ModuleId::Distortion, "dist", "Distortion", kDistOn, "DIST" },
  { ModuleId::Fuzz, "fuzz", "Fuzz", kFuzzOn, "FUZZ" },
  { ModuleId::Amp, "amp", "Amp", kAmpOn, "AMP" },
  { ModuleId::Cabinet, "cab", "Cabinet", kCabOn, "CAB" },
  { ModuleId::Eq, "eq", "EQ", kEqOn, "EQ" },
  { ModuleId::Chorus, "chorus", "Chorus", kChoOn, "CHO" },
  { ModuleId::Phaser, "phaser", "Phaser", kPhaOn, "PHA" },
  { ModuleId::Flanger, "flanger", "Flanger", kFlaOn, "FLG" },
  { ModuleId::Tremolo, "tremolo", "Tremolo", kTrmOn, "TRM" },
  { ModuleId::Vibrato, "vibrato", "Vibrato", kVibOn, "VIB" },
  { ModuleId::Rotary, "rotary", "Rotary", kRotOn, "ROT" },
  { ModuleId::EnvFilter, "envfilter", "Env Filter", kEnvOn, "ENV" },
  { ModuleId::Delay, "delay", "Delay", kDlyOn, "DLY" },
  { ModuleId::Reverb, "reverb", "Reverb", kRvbOn, "RVB" },
  { ModuleId::Wah, "wah", "Wah", kWahOn, "WAH" },
  { ModuleId::Octave, "octave", "Octave", kOctOn, "OCT" },
  { ModuleId::Pitch, "pitch", "Pitch", kPitchOn, "PIT" },
  { ModuleId::Harmonizer, "harmonizer", "Harmonizer", kHarmOn, "HRM" },
};

constexpr float kSafetyCeiling = 4.0f; // +12 дБFS — аварийный потолок, не «звуковой» лимитер

} // namespace

const char* moduleKey(ModuleId id) noexcept { return kModuleMeta[static_cast<int>(id)].key; }
const char* moduleDisplayName(ModuleId id) noexcept { return kModuleMeta[static_cast<int>(id)].displayName; }
const char* moduleShortName(ModuleId id) noexcept { return kModuleMeta[static_cast<int>(id)].shortName; }
int moduleEnableParam(ModuleId id) noexcept { return kModuleMeta[static_cast<int>(id)].enableParam; }

ModuleId moduleIdForKey(const char* key) noexcept
{
  if (!key) return ModuleId::Count;
  for (const auto& m : kModuleMeta)
    if (std::strcmp(m.key, key) == 0) return m.id;
  return ModuleId::Count;
}

ChainOrder defaultChainOrder() noexcept
{
  return { ModuleId::Gate,    ModuleId::Compressor, ModuleId::Wah,        ModuleId::EnvFilter, ModuleId::Octave,  ModuleId::Pitch,
           ModuleId::Boost,   ModuleId::Overdrive,  ModuleId::Distortion, ModuleId::Fuzz,      ModuleId::Amp,     ModuleId::Cabinet,
           ModuleId::Harmonizer, ModuleId::Eq,      ModuleId::Chorus,     ModuleId::Phaser,    ModuleId::Flanger, ModuleId::Vibrato,
           ModuleId::Tremolo, ModuleId::Rotary,     ModuleId::Delay,      ModuleId::Reverb };
}

bool isValidChainOrder(const ChainOrder& order) noexcept
{
  std::array<bool, kNumModules> seen{};
  for (ModuleId id : order)
  {
    const int i = static_cast<int>(id);
    if (i < 0 || i >= kNumModules || seen[static_cast<size_t>(i)]) return false;
    seen[static_cast<size_t>(i)] = true;
  }
  return true;
}

bool isValidChainConfig(const ChainConfig& c) noexcept
{
  if (!isValidChainOrder(c.order)) return false;
  if (c.splitStart > c.splitEnd || c.splitEnd > kNumModules) return false;
  return true;
}

// ---------------------------------------------------------------------------

Engine::Engine()
{
  mModules[static_cast<size_t>(ModuleId::Gate)] = std::make_unique<NoiseGate>();
  mModules[static_cast<size_t>(ModuleId::Compressor)] = std::make_unique<Compressor>();
  mModules[static_cast<size_t>(ModuleId::Boost)] = std::make_unique<Boost>();
  mModules[static_cast<size_t>(ModuleId::Overdrive)] = std::make_unique<Overdrive>();
  mModules[static_cast<size_t>(ModuleId::Distortion)] = std::make_unique<Distortion>();
  mModules[static_cast<size_t>(ModuleId::Fuzz)] = std::make_unique<Fuzz>();
  mModules[static_cast<size_t>(ModuleId::Amp)] = std::make_unique<Amp>();
  mModules[static_cast<size_t>(ModuleId::Cabinet)] = std::make_unique<Cabinet>();
  mModules[static_cast<size_t>(ModuleId::Eq)] = std::make_unique<Eq>();
  mModules[static_cast<size_t>(ModuleId::Chorus)] = std::make_unique<Chorus>();
  mModules[static_cast<size_t>(ModuleId::Phaser)] = std::make_unique<Phaser>();
  mModules[static_cast<size_t>(ModuleId::Flanger)] = std::make_unique<Flanger>();
  mModules[static_cast<size_t>(ModuleId::Tremolo)] = std::make_unique<Tremolo>();
  mModules[static_cast<size_t>(ModuleId::Vibrato)] = std::make_unique<Vibrato>();
  mModules[static_cast<size_t>(ModuleId::Rotary)] = std::make_unique<Rotary>();
  mModules[static_cast<size_t>(ModuleId::EnvFilter)] = std::make_unique<EnvFilter>();
  mModules[static_cast<size_t>(ModuleId::Delay)] = std::make_unique<Delay>();
  mModules[static_cast<size_t>(ModuleId::Reverb)] = std::make_unique<Reverb>();
  mModules[static_cast<size_t>(ModuleId::Wah)] = std::make_unique<Wah>();
  mModules[static_cast<size_t>(ModuleId::Octave)] = std::make_unique<Octave>();
  mModules[static_cast<size_t>(ModuleId::Pitch)] = std::make_unique<PitchShift>();
  mModules[static_cast<size_t>(ModuleId::Harmonizer)] = std::make_unique<Harmonizer>();

  for (int i = 0; i < kNumParams; ++i) mParams[static_cast<size_t>(i)] = paramSpec(i).defaultValue;
  mOrderSlots[0] = mOrderSlots[1] = mActiveCfg = mPendingCfg = ChainConfig{};
}

Engine::~Engine() = default;

void Engine::prepare(double sampleRate, int maxBlockSize)
{
  mSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
  mMaxBlock = std::max(16, maxBlockSize);
  const float sr = static_cast<float>(mSampleRate);
  const size_t n = static_cast<size_t>(mMaxBlock);
  for (auto* v : { &mL, &mR, &mDryL, &mDryR, &mKey, &mInL, &mInR, &mBL, &mBR }) v->assign(n, 0.0f);

  PrepareInfo info{ mSampleRate, mMaxBlock };
  for (auto& m : mModules) m->prepare(info);

  mInGain.prepare(sr, 0.02f);
  mOutGain.prepare(sr, 0.02f);
  mBypassMix.prepare(sr, 0.015f);
  mTunerMute.prepare(sr, 0.015f);
  mTransition.prepare(sr, 0.015f);
  mTransition.snap(1.f);
  mOrderFade.prepare(sr, 0.012f);
  mInGain.snap(dbToGain(mParams[kInputTrim]));
  mOutGain.snap(dbToGain(mParams[kOutputTrim]));
  mBypassMix.snap(mParams[kBypass] >= 0.5f ? 0.0f : 1.0f);
  mTunerMute.snap(mParams[kTunerMute] >= 0.5f ? 1.0f : 0.0f);
  mOrderFade.snap(1.0f);
  mParallelA.prepare(sr, 0.02f);
  mParallelB.prepare(sr, 0.02f);
  mParallelA.snap(1.f);
  mParallelB.snap(1.f);
  for (int i = 0; i < kNumModules; ++i)
  {
    const bool on = mParams[static_cast<size_t>(moduleEnableParam(static_cast<ModuleId>(i)))] >= 0.5f;
    mModuleMix[static_cast<size_t>(i)].prepare(sr, 0.010f);
    mModuleMix[static_cast<size_t>(i)].snap(on ? 1.0f : 0.0f);
    mModuleActive[static_cast<size_t>(i)] = on;
  }

  mInMeter.prepare(sr);
  mOutMeter.prepare(sr);
  mTunerRing.prepare(static_cast<size_t>(sr)); // ~1 с
  mAnalyzerRing.prepare(16384);
  mParamsDirty = true;
  mPrepared = true;
}

void Engine::reset() noexcept
{
  for (auto& m : mModules) m->reset();
  mInMeter.reset();
  mOutMeter.reset();
}

void Engine::setParam(int id, float v) noexcept
{
  if (id < 0 || id >= kNumParams) return;
  v = paramClamp(id, v);
  float& slot = mParams[static_cast<size_t>(id)];
  if (slot != v)
  {
    slot = v;
    mParamsDirty = true;
  }
}

void Engine::setChainConfig(const ChainConfig& cfg) noexcept
{
  if (!isValidChainConfig(cfg)) return;
  const int next = 1 - mPublishedOrder.load(std::memory_order_acquire);
  mOrderSerial.fetch_add(1, std::memory_order_acq_rel); // нечётный — запись идёт
  mOrderSlots[static_cast<size_t>(next)] = cfg;
  mPublishedOrder.store(next, std::memory_order_release);
  mOrderSerial.fetch_add(1, std::memory_order_acq_rel); // чётный — запись завершена
}

ChainConfig Engine::chainConfig() const noexcept
{
  return mOrderSlots[static_cast<size_t>(mPublishedOrder.load(std::memory_order_acquire))];
}

void Engine::setChainOrder(const ChainOrder& order) noexcept
{
  ChainConfig c = chainConfig();
  c.order = order;
  setChainConfig(c);
}

ChainOrder Engine::chainOrder() const noexcept { return chainConfig().order; }

int Engine::latencySamples() const noexcept
{
  int total = 0;
  for (const auto& m : mModules) total += m->latencySamples();
  return total;
}

void Engine::updateModules() noexcept
{
  for (auto& m : mModules) m->update(mParams);
  mInGain.setTarget(dbToGain(mParams[kInputTrim]));
  mOutGain.setTarget(dbToGain(mParams[kOutputTrim]));
  mBypassMix.setTarget(mParams[kBypass] >= 0.5f ? 0.0f : 1.0f);
  mTunerMute.setTarget(mParams[kTunerMute] >= 0.5f ? 1.0f : 0.0f);
  const float pm = mParams[kParallelMix] * 0.01f;
  mParallelA.setTarget(std::min(1.f, 2.f * (1.f - pm)));
  mParallelB.setTarget(std::min(1.f, 2.f * pm));
  for (int i = 0; i < kNumModules; ++i)
  {
    const bool on = mParams[static_cast<size_t>(moduleEnableParam(static_cast<ModuleId>(i)))] >= 0.5f;
    mModuleMix[static_cast<size_t>(i)].setTarget(on ? 1.0f : 0.0f);
  }
}

void Engine::runModule(ModuleId id, AudioBlock& block) noexcept
{
  const size_t idx = static_cast<size_t>(id);
  const int n = block.numFrames;
  Module* m = mModules[idx].get();
  LinearSmoother& mix = mModuleMix[idx];
  const bool targetOn = mix.target() > 0.5f;

  if (!mix.isSmoothing() && !targetOn)
  {
    mModuleActive[idx] = false;
    return;
  }
  if (!mModuleActive[idx])
  {
    m->reset(); // чистое состояние при включении (без «застрявших» хвостов)
    mModuleActive[idx] = true;
  }
  if (!mix.isSmoothing() && targetOn)
  {
    m->process(block);
    return;
  }
  // Crossfade dry/wet при переключении
  float* dL = mInL.data();
  float* dR = mInR.data();
  copyBuffer(dL, block.l, n);
  const bool wasStereo = block.stereo;
  if (wasStereo) copyBuffer(dR, block.r, n);
  m->process(block);
  if (block.stereo && !wasStereo) copyBuffer(dR, dL, n);
  for (int i = 0; i < n; ++i)
  {
    const float g = mix.next();
    block.l[i] = dL[i] + g * (block.l[i] - dL[i]);
    if (block.stereo) block.r[i] = dR[i] + g * (block.r[i] - dR[i]);
  }
}

void Engine::process(const float* const* in, int nIn, float** out, int nOut, int numFrames,
                     const TransportInfo& t) noexcept
{
  if (!mPrepared || numFrames <= 0 || nOut <= 0 || !out) return;
  ScopedNoDenormals noDenormals;

  if (mParamsDirty)
  {
    updateModules();
    mParamsDirty = false;
  }

  // Подхватить новый порядок цепочки (seqlock-чтение)
  const uint32_t serial = mOrderSerial.load(std::memory_order_acquire);
  if (serial != mAppliedSerial && (serial & 1u) == 0u && !mOrderFadeOut)
  {
    const ChainConfig candidate = mOrderSlots[static_cast<size_t>(mPublishedOrder.load(std::memory_order_acquire))];
    if (mOrderSerial.load(std::memory_order_acquire) == serial && isValidChainConfig(candidate))
    {
      mAppliedSerial = serial;
      if (candidate != mActiveCfg)
      {
        mPendingCfg = candidate;
        mOrderFadeOut = true;
        mOrderFade.setTarget(0.0f);
      }
    }
  }

  mBlocks.fetch_add(1, std::memory_order_relaxed);
  int offset = 0;
  while (offset < numFrames)
  {
    const int n = std::min(mMaxBlock, numFrames - offset);
    processChunk(in, nIn, out, nOut, offset, n, t);
    offset += n;
  }
}

void Engine::processChunk(const float* const* in, int nIn, float** out, int nOut, int off, int n,
                          const TransportInfo& t) noexcept
{
  float* L = mL.data();
  float* R = mR.data();
  bool stereo = false;

  // --- 1. Выбор входа ---
  const int mode = static_cast<int>(mParams[kInputMode]);
  const float* inL = (in && nIn > 0 && in[0]) ? in[0] + off : nullptr;
  const float* inR = (in && nIn > 1 && in[1]) ? in[1] + off : inL;
  if (!inL)
    clearBuffer(L, n);
  else if (mode == 1 && inR)
    copyBuffer(L, inR, n);
  else if (mode == 2 && inR && nIn > 1)
    for (int i = 0; i < n; ++i) L[i] = 0.5f * (inL[i] + inR[i]);
  else if (mode == 3 && nIn > 1 && inR)
  {
    copyBuffer(L, inL, n);
    copyBuffer(R, inR, n);
    stereo = true;
  }
  else
    copyBuffer(L, inL, n);

  // Защита входа от NaN/Inf
  for (int i = 0; i < n; ++i) L[i] = sanitize(L[i]);
  if (stereo) for (int i = 0; i < n; ++i) R[i] = sanitize(R[i]);

  // Сухой сигнал для глобального bypass (до Input Trim)
  copyBuffer(mDryL.data(), L, n);
  if (stereo) copyBuffer(mDryR.data(), R, n);
  const bool dryStereo = stereo;

  // --- 2. Input Trim ---
  for (int i = 0; i < n; ++i)
  {
    const float g = mInGain.next();
    L[i] *= g;
    if (stereo) R[i] *= g;
  }
  if (stereo) mInMeter.processStereo(L, R, n); else mInMeter.process(L, n);
  copyBuffer(mKey.data(), L, n);
  mTunerRing.push(L, static_cast<size_t>(n));

  // --- 3. Цепочка модулей ---
  AudioBlock block;
  block.l = L;
  block.r = R;
  block.numFrames = n;
  block.stereo = stereo;
  block.keyInput = mKey.data();
  block.transport = &t;

  const ChainConfig& cfg = mActiveCfg;
  const bool par = cfg.parallel && cfg.splitEnd > cfg.splitStart;
  AudioBlock blockB = block;
  blockB.l = mBL.data();
  blockB.r = mBR.data();
  for (int i = 0; i < kNumModules; ++i)
  {
    const ModuleId id = cfg.order[static_cast<size_t>(i)];
    if (par && i == cfg.splitStart)
    {
      // Разветвление: путь B получает копию сигнала
      copyBuffer(blockB.l, block.l, n);
      if (block.stereo) copyBuffer(blockB.r, block.r, n);
      blockB.stereo = block.stereo;
    }
    const bool inSection = par && i >= cfg.splitStart && i < cfg.splitEnd;
    runModule(id, inSection && cfg.onLaneB(id) ? blockB : block);
    if (par && i == cfg.splitEnd - 1)
    {
      // Слияние путей (задержки путей равны: все модули сейчас имеют нулевую латентность)
      const bool st = block.stereo || blockB.stereo;
      if (st && !block.stereo) copyBuffer(block.r, block.l, n);
      if (st && !blockB.stereo) copyBuffer(blockB.r, blockB.l, n);
      for (int k = 0; k < n; ++k)
      {
        const float ga = mParallelA.next(), gb = mParallelB.next();
        block.l[k] = block.l[k] * ga + blockB.l[k] * gb;
        if (st) block.r[k] = block.r[k] * ga + blockB.r[k] * gb;
      }
      block.stereo = st;
    }
  }
  stereo = block.stereo;

  // --- 4. Смена порядка цепочки: короткое затухание → перестановка → нарастание ---
  if (mOrderFadeOut || mOrderFade.isSmoothing())
  {
    for (int i = 0; i < n; ++i)
    {
      const float g = mOrderFade.next();
      L[i] *= g;
      if (stereo) R[i] *= g;
    }
    if (mOrderFadeOut && !mOrderFade.isSmoothing())
    {
      mActiveCfg = mPendingCfg;
      mOrderFadeOut = false;
      for (auto& m : mModules) m->reset();
      mOrderFade.setTarget(1.0f);
    }
  }

  // --- 5. Output Trim + аварийная защита ---
  bool bad = false;
  for (int i = 0; i < n; ++i)
  {
    const float g = mOutGain.next();
    float l = L[i] * g;
    if (!std::isfinite(l)) { bad = true; l = 0.0f; }
    L[i] = clampv(l, -kSafetyCeiling, kSafetyCeiling);
    if (stereo)
    {
      float r = R[i] * g;
      if (!std::isfinite(r)) { bad = true; r = 0.0f; }
      R[i] = clampv(r, -kSafetyCeiling, kSafetyCeiling);
    }
  }
  if (bad)
  {
    for (auto& m : mModules) m->reset();
    clearBuffer(L, n);
    if (stereo) clearBuffer(R, n);
    mSafetyTrips.fetch_add(1, std::memory_order_relaxed);
  }

  // --- 6. Глобальный bypass, mute тюнера, вывод ---
  if (!stereo && (dryStereo || nOut > 1))
  {
    // Расширяем моно до стерео, если выход стерео (или сухой сигнал стерео)
    copyBuffer(R, L, n);
    stereo = true;
  }
  const float* dL = mDryL.data();
  const float* dR = dryStereo ? mDryR.data() : mDryL.data();
  float* oL = out[0] + off;
  float* oR = (nOut > 1 && out[1]) ? out[1] + off : nullptr;
  for (int i = 0; i < n; ++i)
  {
    const float w = mBypassMix.next();
    const float mute = (1.0f - mTunerMute.next()) * mTransition.next();
    const float l = (dL[i] + w * (L[i] - dL[i])) * mute;
    const float r = stereo ? (dR[i] + w * (R[i] - dR[i])) * mute : l;
    if (oR)
    {
      oL[i] = l;
      oR[i] = r;
    }
    else
      oL[i] = stereo ? 0.5f * (l + r) : l;
    L[i] = l;
    R[i] = r;
  }
  if (oR) mOutMeter.processStereo(oL, oR, n); else mOutMeter.process(oL, n);
  // В анализатор — моно-сумма выхода
  for (int i = 0; i < n; ++i) mInL[i] = 0.5f * (L[i] + R[i]);
  mAnalyzerRing.push(mInL.data(), static_cast<size_t>(n));
  // Дополнительные выходные каналы (если хост дал >2) — тишина
  for (int c = 2; c < nOut; ++c)
    if (out[c]) clearBuffer(out[c] + off, n);
}

} // namespace tsl
