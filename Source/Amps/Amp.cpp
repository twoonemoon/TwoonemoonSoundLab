// SPDX-License-Identifier: GPL-3.0-or-later
#include "Amp.h"
#include "../DSP/Waveshapers.h"

namespace tsl {

namespace {

inline float onePoleA(float hz, float rate) noexcept
{
  return std::exp(-kTwoPi * clampv(hz, 1.0f, rate * 0.45f) / rate);
}

/** Проводимость лампы оконечного каскада: гладкий порог (softplus) с насыщением тока. */
inline float tubeConduction(float u, float a) noexcept
{
  const float au = a * u;
  const float sp = au > 20.f ? u : (au < -20.f ? 0.f : std::log1p(std::exp(au)) / a);
  return sp / (1.0f + sp);
}

/** Двухтактный каскад: разность токов двух плеч (только нечётные гармоники при симметрии). */
inline float pushPull(float v, float bias, float a) noexcept
{
  return tubeConduction(v + bias, a) - tubeConduction(-v + bias, a);
}

struct Ramp
{
  float v0, v1;
  float at(int i, int len) const noexcept { return v0 + (v1 - v0) * (static_cast<float>(i + 1) / static_cast<float>(len)); }
};

constexpr float kRoomTapsMsL[6] = { 7.3f, 11.9f, 17.1f, 23.7f, 29.3f, 37.9f };
constexpr float kRoomTapsMsR[6] = { 8.9f, 13.3f, 19.7f, 21.1f, 31.7f, 41.3f };
constexpr float kRoomTapGain[6] = { 0.62f, 0.50f, 0.41f, 0.33f, 0.26f, 0.20f };

} // namespace

void Amp::Channel::resetState() noexcept
{
  os.reset();
  inHp.reset(); bright.reset(); lowCut.reset(); highCut.reset();
  for (auto& s : st) s = StageState{};
  ts.reset();
  fbPrev = presLp = depthLp1 = sagEnv = xfHp = xfLp = xfLp2 = 0.f;
  for (auto& b : body) b.reset();
  piezoNotch.reset(); piezoShelf.reset(); warmth.reset(); eqBass.reset(); eqMid.reset(); eqTreble.reset(); air.reset();
  transientEnv = 0.f;
  room.reset();
}

void Amp::prepare(const PrepareInfo& info)
{
  mSampleRate = info.sampleRate;
  mMaxBlock = info.maxBlockSize;
  const float sr = static_cast<float>(mSampleRate);
  for (auto& c : mCh)
  {
    c.os.prepare(mMaxBlock, 8);
    c.os.setFactor(mOsFactor);
    c.room.prepare(static_cast<int>(0.05 * mSampleRate) + 8);
  }
  mRoomL.assign(static_cast<size_t>(mMaxBlock), 0.f);
  mRoomR.assign(static_cast<size_t>(mMaxBlock), 0.f);
  mGain.prepare(sr, 0.03f); mMaster.prepare(sr, 0.03f); mOutGain.prepare(sr, 0.03f); mRoomMix.prepare(sr, 0.05f);
  mSwitchFade.prepare(sr, 0.012f);
  mSwitchFade.snap(1.f);
  mBassS.prepare(sr / 64.f, 0.03f); mMidS.prepare(sr / 64.f, 0.03f); mTrebleS.prepare(sr / 64.f, 0.03f);
  mBassS.snap(mBass); mMidS.snap(mMid); mTrebleS.snap(mTreble);
  mTsBass = mTsMid = mTsTreble = -1.f;
  applyModel(mModel);
  mGain.snap(mGain.target()); mMaster.snap(mMaster.target()); mOutGain.snap(mOutGain.target()); mRoomMix.snap(mRoomMix.target());
}

void Amp::reset() noexcept
{
  for (auto& c : mCh) c.resetState();
}

void Amp::applyModel(int model) noexcept
{
  mModel = clampv(model, 0, kNumAmpModels - 1);
  for (auto& c : mCh) { c.os.setFactor(mOsFactor); c.resetState(); }
  mCoefsDirty = true;
  mTsBass = mTsMid = mTsTreble = -1.f;
  recalcCoefs();
  updateTargets();
}

void Amp::recalcCoefs() noexcept
{
  const AmpVoice& v = ampVoice(mModel);
  const float sr = static_cast<float>(mSampleRate);
  const float osr = sr * static_cast<float>(mCh[0].os.factor());
  const float gainN = mKnobGain * 0.1f;

  // --- Базовая частота: вход, bright, пользовательские срезы
  const auto inHp = design::make(FilterType::HighPass, mSampleRate, v.inputHpHz, 0.6);
  const float brightDb = mBright ? std::max(v.brightDb, 3.0f) * (1.0f - 0.7f * gainN) : 0.0f;
  const auto bright = design::make(FilterType::HighShelf, mSampleRate, v.brightHz > 0.f ? v.brightHz : 2500.0, 0.6, brightDb);
  const auto lowCut = design::make(FilterType::HighPass, mSampleRate, mLowCut, 0.707);
  const auto highCut = design::make(FilterType::LowPass, mSampleRate, mHighCut, 0.707);
  for (auto& c : mCh)
  {
    c.inHp.setCoefs(inHp);
    c.bright.setCoefs(bright);
    c.lowCut.setCoefs(lowCut);
    c.highCut.setCoefs(highCut);
  }

  if (v.category == AmpCategory::Acoustic)
  {
    const AcousticSpec& a = v.acoustic;
    const double bodyAmt = mAcBody * 0.1;
    const double tame = mAcTame * 0.1;
    BiquadCoefs body[3];
    for (int i = 0; i < 3; ++i) body[i] = design::make(FilterType::Peak, mSampleRate, a.bodyHz[i], a.bodyQ[i], a.bodyMaxDb[i] * bodyAmt);
    const auto notch = design::make(FilterType::Peak, mSampleRate, a.piezoNotchHz, 1.1, -9.0 * tame);
    const auto shelf = design::make(FilterType::HighShelf, mSampleRate, 6000.0, 0.6, -8.0 * tame);
    const auto warm = design::make(FilterType::LowPass, mSampleRate, a.warmthHz, 0.6);
    const auto bass = design::make(FilterType::LowShelf, mSampleRate, 120.0, 0.6, (mBass - 0.5) * 24.0);
    const auto mid = design::make(FilterType::Peak, mSampleRate, 750.0, 0.8, (mMid - 0.5) * 18.0);
    const auto treble = design::make(FilterType::HighShelf, mSampleRate, 3500.0, 0.6, (mTreble - 0.5) * 24.0);
    const auto airC = design::make(FilterType::HighShelf, mSampleRate, 10000.0, 0.6, mAcAir * 0.8);
    for (auto& c : mCh)
    {
      for (int i = 0; i < 3; ++i) c.body[i].setCoefs(body[i]);
      c.piezoNotch.setCoefs(notch);
      c.piezoShelf.setCoefs(shelf);
      c.warmth.setCoefs(warm);
      c.eqBass.setCoefs(bass);
      c.eqMid.setCoefs(mid);
      c.eqTreble.setCoefs(treble);
      c.air.setCoefs(airC);
    }
    mTransientAtt = onePoleCoef(0.0002f, sr);
    mTransientRel = onePoleCoef(0.030f, sr);
    mCoefsDirty = false;
    return;
  }

  // --- Каскады предусилителя (на частоте оверсэмплинга)
  for (int i = 0; i < v.numStages; ++i)
  {
    const TriodeStageSpec& s = v.stages[i];
    mStageA_coupling[i] = onePoleA(s.couplingHz, osr);
    mStageA_cathode[i] = onePoleA(s.cathodeHz, osr);
    mStageA_miller[i] = onePoleA(s.millerHz, osr);
    mGridRelease[i] = onePoleCoef(s.gridReleaseMs * 0.001f, osr);
  }
  mGridAttack = onePoleCoef(0.0007f, osr);
  mTsMakeup = dbToGain(v.toneStackMakeupDb);

  // --- Оконечный каскад
  mPaA = v.paHardness;
  // Bias 0…10: 5 — номинал модели; меньше — «холоднее» (переходные искажения), больше — «горячее»
  const float biasN = (mBiasKnob - 5.f) * 0.2f; // −1…1
  mPaB = clampv(v.paBias * std::pow(2.2f, biasN), 0.03f, 0.9f);
  // Нормировка малосигнального усиления (производная в нуле = 2·c'(b))
  {
    const float h = 1e-3f;
    const float d = (pushPull(h, mPaB, mPaA) - pushPull(-h, mPaB, mPaA)) / (2.f * h);
    mPaNorm = 1.0f / std::max(d, 1e-3f);
  }
  // ООС: минимальная глубина 0.1, чтобы Presence/Depth действовали и при Feedback = 0
  mNfbK = clampv(v.nfb * (mFbKnob / 5.f), 0.1f, 0.92f);
  mPresAmt = mPresence * 1.6f;
  mDepthAmt = mDepth * 1.6f;
  mPresA = onePoleA(v.presenceHz, osr);
  mDepthA = onePoleA(v.depthHz * 1.4f, osr);
  mSagAmt = clampv(v.sag * (mSagKnob / 5.f), 0.f, 1.6f);
  mSagAtt = onePoleCoef(0.004f, osr);
  mSagRel = onePoleCoef(0.090f + 0.06f * mSagAmt, osr);
  mXfHpA = onePoleA(v.xfmrHpHz, osr);
  mXfLpA = onePoleA(v.xfmrLpHz, osr);
  mCoefsDirty = false;
}

void Amp::update(const ParamValues& p) noexcept
{
  const int model = static_cast<int>(p[kAmpModel]);
  const int osIdx = static_cast<int>(p[kOversampling]);
  const int osFactor = osIdx <= 0 ? 2 : (osIdx == 1 ? 4 : 8);
  if ((model != mModel || osFactor != mOsFactor) && !mSwitching)
  {
    mPendingModel = model;
    mPendingOsFactor = osFactor;
    mSwitching = true;
    mSwitchFade.setTarget(0.f);
  }

  auto changed = [this](float& dst, float v) {
    if (dst != v) { dst = v; mCoefsDirty = true; }
  };
  changed(mKnobGain, p[kAmpGain]);
  mKnobMaster = p[kAmpMaster];
  mLevelDb = p[kAmpLevel];
  mBass = p[kAmpBass] * 0.1f; mMid = p[kAmpMid] * 0.1f; mTreble = p[kAmpTreble] * 0.1f;
  mBassS.setTarget(mBass); mMidS.setTarget(mMid); mTrebleS.setTarget(mTreble);
  float pres = p[kAmpPresence] * 0.1f, depth = p[kAmpDepth] * 0.1f;
  changed(mPresence, pres);
  changed(mDepth, depth);
  changed(mSagKnob, p[kAmpSag]);
  changed(mBiasKnob, p[kAmpBias]);
  changed(mFbKnob, p[kAmpFeedback]);
  const bool br = p[kAmpBright] >= 0.5f;
  if (br != mBright) { mBright = br; mCoefsDirty = true; }
  changed(mLowCut, p[kAmpLowCut]);
  changed(mHighCut, p[kAmpHighCut]);
  changed(mAcBody, p[kAcBody]);
  changed(mAcTame, p[kAcPiezoTame]);
  changed(mAcAir, p[kAcAir]);
  mAcRoom = p[kAcRoom];

  // Эквалайзер акустики зависит от Bass/Mid/Treble напрямую
  if (ampVoice(mModel).category == AmpCategory::Acoustic) mCoefsDirty = true;
  updateTargets();
}

void Amp::updateTargets() noexcept
{
  const AmpVoice& v = ampVoice(mModel);
  const float gainN = std::pow(mKnobGain * 0.1f, 1.2f);
  mGain.setTarget(dbToGain(v.gainMinDb + (v.gainMaxDb - v.gainMinDb) * gainN));
  const float masterDb = v.masterMinDb + (v.masterMaxDb - v.masterMinDb) * (mKnobMaster * 0.1f);
  mMaster.setTarget(dbToGain(masterDb));
  const float masterMid = 0.5f * (v.masterMinDb + v.masterMaxDb);
  mOutGain.setTarget(dbToGain(mLevelDb + v.outputDb - 0.45f * (masterDb - masterMid)));
  mRoomMix.setTarget(mAcRoom * 0.1f * v.acoustic.roomScale);
}

void Amp::processElectric(Channel& c, float* x, int n) noexcept
{
  const AmpVoice& v = ampVoice(mModel);
  c.inHp.processBlock(x, n);
  c.bright.processBlock(x, n);

  float* up = c.os.upsample(x, n);
  const int len = n * c.os.factor();
  const Ramp gain{ mBlockGain, mGain.current() };
  const Ramp master{ mBlockMaster, mMaster.current() };
  float maxDrive = 0.f;

  for (int i = 0; i < len; ++i)
  {
    float s = up[i] * gain.at(i, len);

    // ---- Предусилитель
    for (int k = 0; k < v.numStages; ++k)
    {
      const TriodeStageSpec& sp = v.stages[k];
      StageState& st = c.st[k];
      // Сопрягающий конденсатор (ФВЧ)
      st.coupling = s + mStageA_coupling[k] * (st.coupling - s);
      float u = s - st.coupling;
      // Частично шунтированный катод: ослабление НЧ
      st.cathode = u + mStageA_cathode[k] * (st.cathode - u);
      u += (sp.cathodeLowGain - 1.0f) * st.cathode;
      // Сеточный ток: положительная полуволна сдвигает смещение к отсечке
      const float drive = u * sp.gain;
      const float pos = drive + sp.bias > 0.f ? drive + sp.bias : 0.f;
      st.gridEnv = pos > st.gridEnv ? pos + mGridAttack * (st.gridEnv - pos) : st.gridEnv * mGridRelease[k];
      float y = shape::triode(drive, clampv(sp.bias - sp.gridK * st.gridEnv, -1.f, 1.f));
      // Ёмкость Миллера
      st.miller = y + mStageA_miller[k] * (st.miller - y);
      s = st.miller;
      if (k == v.toneStackAfter) s = c.ts.process(s) * mTsMakeup;
    }
    if (v.cathodeFollower) s = shape::asymClip(s, 0.95f, 1.7f);

    // ---- Оконечный каскад с ООС и проседанием питания
    const float in = s * master.at(i, len);
    // Сигнал ООС: Presence уменьшает обратную связь на ВЧ, Depth — на НЧ
    c.presLp = c.fbPrev + mPresA * (c.presLp - c.fbPrev);
    const float fbHp = c.fbPrev - c.presLp;
    c.depthLp1 = c.fbPrev + mDepthA * (c.depthLp1 - c.fbPrev);
    // Однополюсные фильтры (фаза ≤ 90°): уменьшение ООС в полосе даёт подъём, а при значениях
    // больше 1 — частичную положительную ОС в полосе (резонансный подъём), петля остаётся < 1.
    const float fb = c.fbPrev - mPresAmt * fbHp - mDepthAmt * c.depthLp1;
    float uPa = in * (1.0f + mNfbK) - mNfbK * fb;
    maxDrive = std::max(maxDrive, std::fabs(uPa));
    // Питание проседает под нагрузкой: меньше запас → раньше насыщение
    const float supply = 1.0f / (1.0f + mSagAmt * 1.5f * c.sagEnv);
    float y = supply * mPaNorm * pushPull(uPa / supply, mPaB, mPaA);
    const float ay = std::fabs(y);
    c.sagEnv = ay > c.sagEnv ? ay + mSagAtt * (c.sagEnv - ay) : ay + mSagRel * (c.sagEnv - ay);
    c.fbPrev = y;
    // Выходной трансформатор: полоса пропускания
    c.xfHp = y + mXfHpA * (c.xfHp - y);
    y -= c.xfHp;
    c.xfLp = y + mXfLpA * (c.xfLp - y);
    c.xfLp2 = c.xfLp + mXfLpA * (c.xfLp2 - c.xfLp);
    up[i] = c.xfLp2;
  }

  // Защита состояний
  for (auto& st : c.st)
  {
    st.coupling = undenormal(st.coupling); st.cathode = undenormal(st.cathode);
    st.miller = undenormal(st.miller); st.gridEnv = undenormal(st.gridEnv);
  }
  c.fbPrev = sanitize(undenormal(c.fbPrev));
  c.presLp = undenormal(c.presLp); c.depthLp1 = undenormal(c.depthLp1);
  c.sagEnv = undenormal(c.sagEnv); c.xfHp = undenormal(c.xfHp); c.xfLp = undenormal(c.xfLp); c.xfLp2 = undenormal(c.xfLp2);
  c.ts.flush();

  c.os.downsample(x, n);
  c.lowCut.processBlock(x, n);
  c.highCut.processBlock(x, n);
  const Ramp out{ mBlockOut, mOutGain.current() };
  for (int i = 0; i < n; ++i) x[i] *= out.at(i, n);
  mPowerDrive.store(maxDrive, std::memory_order_relaxed);
}

void Amp::processAcoustic(Channel& c, float* x, float* roomL, float* roomR, int n, bool firstChannel) noexcept
{
  c.inHp.processBlock(x, n);
  const Ramp gain{ mBlockGain, mGain.current() };
  const float sr = static_cast<float>(mSampleRate);
  const float tame = mAcTame * 0.1f;
  const float limThr = 0.35f;
  for (int i = 0; i < n; ++i)
  {
    // Чистый преамп с очень мягким ограничением на больших уровнях
    float s = x[i] * gain.at(i, n);
    s = 1.5f * fastTanh(s / 1.5f);
    // Пьезо-«щелчки»: быстрый ограничитель транзиентов, глубина — Piezo Tame
    const float a = std::fabs(s);
    c.transientEnv = a > c.transientEnv ? a + mTransientAtt * (c.transientEnv - a) : a + mTransientRel * (c.transientEnv - a);
    if (c.transientEnv > limThr && tame > 0.f)
    {
      const float over = c.transientEnv / limThr;
      s *= 1.0f / (1.0f + tame * (over - 1.0f) * 0.7f);
    }
    x[i] = s;
  }
  c.transientEnv = undenormal(c.transientEnv);
  c.piezoNotch.processBlock(x, n);
  c.piezoShelf.processBlock(x, n);
  for (auto& b : c.body) b.processBlock(x, n);
  c.warmth.processBlock(x, n);
  c.eqBass.processBlock(x, n);
  c.eqMid.processBlock(x, n);
  c.eqTreble.processBlock(x, n);
  c.air.processBlock(x, n);
  c.lowCut.processBlock(x, n);
  c.highCut.processBlock(x, n);

  // Room: ранние отражения короткого помещения (собственный алгоритм, не измеренная комната)
  for (int i = 0; i < n; ++i)
  {
    c.room.push(x[i]);
    float l = 0.f, r = 0.f;
    for (int t = 0; t < 6; ++t)
    {
      l += kRoomTapGain[t] * c.room.readInt(static_cast<int>(kRoomTapsMsL[t] * 0.001f * sr));
      r += kRoomTapGain[t] * c.room.readInt(static_cast<int>(kRoomTapsMsR[t] * 0.001f * sr));
    }
    if (firstChannel) { roomL[i] = l * 0.35f; roomR[i] = r * 0.35f; }
    else { roomL[i] += r * 0.35f; roomR[i] += l * 0.35f; }
  }
}

void Amp::process(AudioBlock& b) noexcept
{
  const int n = b.numFrames;
  if (mCoefsDirty) recalcCoefs();

  // Сглаживание ручек tone stack (раз в блок) и перерасчёт коэффициентов
  const AmpVoice& v = ampVoice(mModel);
  if (v.category != AmpCategory::Acoustic)
  {
    float bs = mBassS.current(), ms = mMidS.current(), ts = mTrebleS.current();
    for (int k = 0; k < std::max(1, n / 64); ++k) { bs = mBassS.next(); ms = mMidS.next(); ts = mTrebleS.next(); }
    if (std::fabs(bs - mTsBass) > 1e-4f || std::fabs(ms - mTsMid) > 1e-4f || std::fabs(ts - mTsTreble) > 1e-4f)
    {
      mTsBass = bs; mTsMid = ms; mTsTreble = ts;
      const double osr = mSampleRate * mCh[0].os.factor();
      for (auto& c : mCh) c.ts.design(v.ts, bs, ms, ts, osr);
    }
  }

  mBlockGain = mGain.current(); mBlockMaster = mMaster.current(); mBlockOut = mOutGain.current();
  mGain.skip(n); mMaster.skip(n); mOutGain.skip(n);

  if (v.category == AmpCategory::Acoustic)
  {
    float* rl = mRoomL.data();
    float* rr = mRoomR.data();
    processAcoustic(mCh[0], b.l, rl, rr, n, true);
    if (b.stereo) processAcoustic(mCh[1], b.r, rl, rr, n, false);
    const float m0 = mRoomMix.current();
    const float m1 = mRoomMix.skip(n);
    const bool wasStereo = b.stereo;
    if (m0 > 0.f || m1 > 0.f || wasStereo)
    {
      if (!wasStereo) copyBuffer(b.r, b.l, n);
      b.stereo = true;
      const Ramp mix{ m0, m1 };
      const Ramp out{ mBlockOut, mOutGain.current() };
      for (int i = 0; i < n; ++i)
      {
        const float m = mix.at(i, n), g = out.at(i, n);
        b.l[i] = (b.l[i] + m * rl[i]) * g;
        b.r[i] = (b.r[i] + m * rr[i]) * g;
      }
    }
    else
    {
      const Ramp out{ mBlockOut, mOutGain.current() };
      for (int i = 0; i < n; ++i) b.l[i] *= out.at(i, n);
    }
  }
  else
  {
    processElectric(mCh[0], b.l, n);
    if (b.stereo) processElectric(mCh[1], b.r, n);
  }

  // Плавная смена модели/оверсэмплинга: затухание → переключение → нарастание
  if (mSwitching || mSwitchFade.isSmoothing())
  {
    for (int i = 0; i < n; ++i)
    {
      const float g = mSwitchFade.next();
      b.l[i] *= g;
      if (b.stereo) b.r[i] *= g;
    }
    if (mSwitching && !mSwitchFade.isSmoothing())
    {
      mOsFactor = mPendingOsFactor;
      applyModel(mPendingModel);
      mSwitching = false;
      mSwitchFade.setTarget(1.f);
    }
  }
}

} // namespace tsl
