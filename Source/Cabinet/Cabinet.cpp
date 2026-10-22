// SPDX-License-Identifier: GPL-3.0-or-later
#include "Cabinet.h"

#include "../DSP/AudioFile.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace tsl {

namespace {

/** Нормировать громкость IR: средняя мощность АЧХ в полосе 120 Гц…5 кГц → 0 дБ. */
void normalizeIR(std::vector<float>& ir, double sr)
{
  if (ir.empty()) return;
  int N = 1024;
  while (N < static_cast<int>(ir.size()) * 2) N <<= 1;
  RealFFT fft;
  fft.prepare(N);
  std::vector<float> t(static_cast<size_t>(N), 0.f), re(static_cast<size_t>(N / 2)), im(static_cast<size_t>(N / 2));
  std::copy(ir.begin(), ir.end(), t.begin());
  fft.forward(t.data(), re.data(), im.data());
  double acc = 0.0;
  int count = 0;
  for (int k = 1; k < N / 2; ++k)
  {
    const double f = k * sr / N;
    if (f < 120.0 || f > 5000.0) continue;
    // Лог-взвешивание: вес 1/f выравнивает вклад октав
    const double w = 1.0 / f;
    acc += w * (static_cast<double>(re[static_cast<size_t>(k)]) * re[static_cast<size_t>(k)] +
                static_cast<double>(im[static_cast<size_t>(k)]) * im[static_cast<size_t>(k)]);
    count += 1;
  }
  double wsum = 0.0;
  for (int k = 1; k < N / 2; ++k)
  {
    const double f = k * sr / N;
    if (f >= 120.0 && f <= 5000.0) wsum += 1.0 / f;
  }
  if (count == 0 || acc <= 0.0) return;
  const float g = static_cast<float>(1.0 / std::sqrt(acc / wsum));
  for (auto& v : ir) v *= g;
}

std::string fileNameOf(const std::string& path)
{
  const size_t p = path.find_last_of('/');
  return p == std::string::npos ? path : path.substr(p + 1);
}

} // namespace

Cabinet::Cabinet() : Module(ModuleId::Cabinet)
{
  mSlot[1].speaker.store(2);
  mSlot[1].mic.store(2);
  mSlot[1].position.store(0.6f);
  mSlot[1].distance.store(8.f);
  mSlot[1].angle.store(15.f);
}

Cabinet::~Cabinet()
{
  mQuit.store(true);
  if (mWorker.joinable()) mWorker.join();
  for (auto& s : mSlot)
  {
    delete s.active;
    delete s.fadingFrom;
    delete s.pending.exchange(nullptr);
    delete s.retired.exchange(nullptr);
  }
}

void Cabinet::prepare(const PrepareInfo& info)
{
  mSampleRate = info.sampleRate;
  mMaxBlock = info.maxBlockSize;
  const float sr = static_cast<float>(mSampleRate);
  const int maxParts = static_cast<int>(std::ceil(kMaxIRSeconds * mSampleRate / kPartSize)) + 1;
  mFadeLen = static_cast<int>(0.025 * mSampleRate);
  mBuildRate.store(mSampleRate);

  std::lock_guard<std::mutex> lock(mMutex);
  mBuildFft.prepare(2 * kPartSize);
  for (int i = 0; i < 2; ++i)
  {
    Slot& s = mSlot[i];
    delete s.active;
    delete s.fadingFrom;
    delete s.pending.exchange(nullptr);
    delete s.retired.exchange(nullptr);
    s.fadingFrom = nullptr;
    for (auto& c : s.conv) c.prepare(kPartSize, maxParts, mMaxBlock);
    for (auto& d : s.delay) d.prepare(static_cast<int>(0.0025 * mSampleRate) + 4);
    s.level.prepare(sr, 0.02f);
    s.panL.prepare(sr, 0.03f);
    s.panR.prepare(sr, 0.03f);
    s.onMix.prepare(sr, 0.02f);
    s.delaySmp.prepare(sr, 0.05f);
    s.level.snap(s.level.target());
    s.panL.snap(s.panL.target());
    s.panR.snap(s.panR.target());
    s.onMix.snap(s.onMix.target());
    s.delaySmp.snap(s.delaySmp.target());
    // Ядро собирается синхронно, чтобы звук был корректен с первого блока
    s.active = buildKernel(s, i, mSampleRate);
    s.builtVersion = s.cfgVersion.load();
    for (int c = 0; c < 2; ++c) mTmp[i][c].assign(static_cast<size_t>(mMaxBlock), 0.f);
  }
  for (auto& v : mInCopy) v.assign(static_cast<size_t>(mMaxBlock), 0.f);
  const auto lc = design::make(FilterType::HighPass, mSampleRate, mLowCutHz, 0.707);
  const auto hc = design::make(FilterType::LowPass, mSampleRate, mHighCutHz, 0.707);
  for (int c = 0; c < 2; ++c) { mLowCut[c].setCoefs(lc); mHighCut[c].setCoefs(hc); }
  mSnapOnUpdate = true;
  if (!mWorker.joinable()) mWorker = std::thread([this] { workerLoop(); });
}

void Cabinet::reset() noexcept
{
  for (auto& s : mSlot)
  {
    for (auto& c : s.conv) c.reset();
    for (auto& d : s.delay) d.reset();
  }
  for (int c = 0; c < 2; ++c) { mLowCut[c].reset(); mHighCut[c].reset(); }
}

void Cabinet::update(const ParamValues& p) noexcept
{
  const int base[2] = { kCabAOn, kCabBOn };
  for (int i = 0; i < 2; ++i)
  {
    Slot& s = mSlot[i];
    const int b = base[i];
    // Порядок параметров слота: On, Speaker, Mic, Position, Distance, Angle, Level, Pan, Phase, Delay
    const int spk = static_cast<int>(p[static_cast<size_t>(b + 1)]);
    const int mic = static_cast<int>(p[static_cast<size_t>(b + 2)]);
    const float pos = p[static_cast<size_t>(b + 3)] * 0.01f;
    const float dist = p[static_cast<size_t>(b + 4)];
    const float ang = p[static_cast<size_t>(b + 5)];
    if (spk != s.speaker.load(std::memory_order_relaxed) || mic != s.mic.load(std::memory_order_relaxed) ||
        pos != s.position.load(std::memory_order_relaxed) || dist != s.distance.load(std::memory_order_relaxed) ||
        ang != s.angle.load(std::memory_order_relaxed))
    {
      s.speaker.store(spk, std::memory_order_relaxed);
      s.mic.store(mic, std::memory_order_relaxed);
      s.position.store(pos, std::memory_order_relaxed);
      s.distance.store(dist, std::memory_order_relaxed);
      s.angle.store(ang, std::memory_order_relaxed);
      s.cfgVersion.fetch_add(1, std::memory_order_release);
    }
    const bool on = p[static_cast<size_t>(b)] >= 0.5f;
    const bool invert = p[static_cast<size_t>(b + 8)] >= 0.5f;
    s.onMix.setTarget(on ? 1.f : 0.f);
    s.level.setTarget(dbToGain(p[static_cast<size_t>(b + 6)]) * (invert ? -1.f : 1.f));
    const float pan = p[static_cast<size_t>(b + 7)] * 0.01f;
    s.panL.setTarget(std::min(1.f, 1.f - pan));
    s.panR.setTarget(std::min(1.f, 1.f + pan));
    s.delaySmp.setTarget(p[static_cast<size_t>(b + 9)] * 0.001f * static_cast<float>(mSampleRate));
  }
  if (mSnapOnUpdate)
  {
    // Первое обновление после prepare(): без рампы от нулевых значений
    for (auto& s : mSlot)
    {
      s.level.snap(s.level.target()); s.panL.snap(s.panL.target()); s.panR.snap(s.panR.target());
      s.onMix.snap(s.onMix.target()); s.delaySmp.snap(s.delaySmp.target());
    }
    mSnapOnUpdate = false;
  }
  if (p[kCabLowCut] != mLowCutHz || p[kCabHighCut] != mHighCutHz)
  {
    mLowCutHz = p[kCabLowCut];
    mHighCutHz = p[kCabHighCut];
    const auto lc = design::make(FilterType::HighPass, mSampleRate, mLowCutHz, 0.707);
    const auto hc = design::make(FilterType::LowPass, mSampleRate, mHighCutHz, 0.707);
    for (int c = 0; c < 2; ++c) { mLowCut[c].setCoefs(lc); mHighCut[c].setCoefs(hc); }
  }
}

void Cabinet::process(AudioBlock& b) noexcept
{
  const int n = b.numFrames;
  const bool inStereo = b.stereo;
  const int nch = inStereo ? 2 : 1;
  copyBuffer(mInCopy[0].data(), b.l, n);
  if (inStereo) copyBuffer(mInCopy[1].data(), b.r, n);

  bool slotActive[2] = { false, false };
  bool needStereo = inStereo;
  for (int i = 0; i < 2; ++i)
  {
    Slot& s = mSlot[i];
    // Захват нового ядра
    if (!s.fadingFrom)
    {
      if (ConvolutionKernel* k = s.pending.exchange(nullptr, std::memory_order_acq_rel))
      {
        if (!s.active) s.active = k;
        else
        {
          s.fadingFrom = s.active;
          s.active = k;
          s.fadePos = 0;
        }
      }
    }
    const bool targetOn = s.onMix.target() > 0.5f;
    if (!targetOn && !s.onMix.isSmoothing())
    {
      s.wasOn = false;
      continue;
    }
    if (!s.wasOn)
    {
      for (auto& c : s.conv) c.reset();
      for (auto& d : s.delay) d.reset();
      s.wasOn = true;
    }
    slotActive[i] = true;
    if (s.panL.target() < 0.999f || s.panR.target() < 0.999f || s.panL.isSmoothing() || s.panR.isSmoothing())
      needStereo = true;

    const ConvolutionKernel* a = s.fadingFrom ? s.fadingFrom : s.active;
    const ConvolutionKernel* bk = s.fadingFrom ? s.active : nullptr;
    const float g0 = std::min(1.f, static_cast<float>(s.fadePos) / mFadeLen);
    const float g1 = std::min(1.f, static_cast<float>(s.fadePos + n) / mFadeLen);
    for (int c = 0; c < nch; ++c) s.conv[c].process(a, bk, mInCopy[c].data(), mTmp[i][c].data(), n, g0, g1);
    if (s.fadingFrom)
    {
      s.fadePos += n;
      if (s.fadePos >= mFadeLen && s.retired.load(std::memory_order_acquire) == nullptr)
      {
        for (auto& c : s.conv) c.promoteSecond();
        s.retired.store(s.fadingFrom, std::memory_order_release);
        s.fadingFrom = nullptr;
      }
    }
    // Выравнивание по времени (дробная задержка)
    const float d0 = s.delaySmp.current();
    const float d1 = s.delaySmp.skip(n);
    if (d0 > 1e-3f || d1 > 1e-3f)
    {
      for (int c = 0; c < nch; ++c)
      {
        float* x = mTmp[i][c].data();
        for (int k = 0; k < n; ++k)
        {
          s.delay[c].push(x[k]);
          const float d = d0 + (d1 - d0) * (static_cast<float>(k + 1) / n);
          x[k] = s.delay[c].readLinear(1.f + d);
        }
      }
    }
    else
      for (int c = 0; c < nch; ++c)
        for (int k = 0; k < n; ++k) s.delay[c].push(mTmp[i][c][static_cast<size_t>(k)]);
  }

  // Микс слотов
  clearBuffer(b.l, n);
  if (needStereo) clearBuffer(b.r, n);
  for (int i = 0; i < 2; ++i)
  {
    Slot& s = mSlot[i];
    if (!slotActive[i]) continue;
    const float* xl = mTmp[i][0].data();
    const float* xr = inStereo ? mTmp[i][1].data() : xl;
    for (int k = 0; k < n; ++k)
    {
      const float g = s.level.next() * s.onMix.next();
      const float pl = s.panL.next(), pr = s.panR.next();
      if (needStereo)
      {
        b.l[k] += xl[k] * g * pl;
        b.r[k] += xr[k] * g * pr;
      }
      else
        b.l[k] += xl[k] * g;
    }
  }
  b.stereo = needStereo;
  mLowCut[0].processBlock(b.l, n);
  mHighCut[0].processBlock(b.l, n);
  if (b.stereo)
  {
    mLowCut[1].processBlock(b.r, n);
    mHighCut[1].processBlock(b.r, n);
  }
}

// ---------------------------------------------------------------------------
// UI API

void Cabinet::requestUserIR(int slot, const std::string& path)
{
  if (slot < 0 || slot > 1) return;
  std::lock_guard<std::mutex> lock(mMutex);
  mSlot[slot].userPath = path;
  mSlot[slot].userRequest++;
}

Cabinet::SlotInfo Cabinet::slotInfo(int slot) const
{
  std::lock_guard<std::mutex> lock(mMutex);
  return mSlot[std::clamp(slot, 0, 1)].info;
}

std::vector<float> Cabinet::irPreview(int slot) const
{
  std::lock_guard<std::mutex> lock(mMutex);
  return mSlot[std::clamp(slot, 0, 1)].preview;
}

bool Cabinet::waitIdle(int timeoutMs)
{
  for (int t = 0; t < timeoutMs; t += 2)
  {
    bool idle = !mBusy.load();
    {
      std::lock_guard<std::mutex> lock(mMutex);
      for (auto& s : mSlot)
        if (s.userRequest != s.userLoaded || s.builtVersion != s.cfgVersion.load()) idle = false;
    }
    if (idle) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  return false;
}

// ---------------------------------------------------------------------------
// Worker

ConvolutionKernel* Cabinet::buildKernel(Slot& s, int slotIdx, double sr)
{
  std::vector<float> ir;
  const int spk = s.speaker.load();
  if (spk >= kCabUserIRIndex)
  {
    if (!s.userIROrig.empty())
    {
      ir = resample(s.userIROrig, s.userRate, sr);
      const size_t maxLen = static_cast<size_t>(kMaxIRSeconds * sr);
      if (ir.size() > maxLen)
      {
        ir.resize(maxLen);
        const size_t fade = static_cast<size_t>(0.01 * sr);
        for (size_t i = 0; i < fade; ++i)
          ir[maxLen - fade + i] *= 0.5f + 0.5f * std::cos(kPi * static_cast<float>(i) / static_cast<float>(fade));
      }
      normalizeIR(ir, sr);
    }
    else
    {
      CabMicSetup fallback;
      ir = synthesizeCabIR(fallback, sr);
      if (!s.info.error) s.info.message = "IR не загружен — временно звучит Monolith 4x12";
    }
  }
  else
  {
    CabMicSetup setup;
    setup.speaker = spk;
    setup.mic = s.mic.load();
    setup.position = s.position.load();
    setup.distanceCm = s.distance.load();
    setup.angleDeg = s.angle.load();
    ir = synthesizeCabIR(setup, sr);
    if (!s.info.error) s.info.message.clear();
  }
  (void)slotIdx;
  s.info.irLength = static_cast<int>(ir.size());
  s.preview = ir;
  mPreviewSerial.fetch_add(1, std::memory_order_acq_rel);
  return ConvolutionKernel::create(ir.data(), static_cast<int>(ir.size()), kPartSize, mBuildFft).release();
}

bool Cabinet::processRequests()
{
  bool did = false;
  std::lock_guard<std::mutex> lock(mMutex);
  const double sr = mBuildRate.load();
  for (int i = 0; i < 2; ++i)
  {
    Slot& s = mSlot[i];
    bool rebuild = false;
    if (s.userRequest != s.userLoaded)
    {
      const uint32_t req = s.userRequest;
      const std::string path = s.userPath;
      if (path.empty())
      {
        s.userIROrig.clear();
        s.loadedPath.clear();
        s.info = SlotInfo{};
      }
      else
      {
        AudioFileData data;
        const AudioFileError err = readWav(path, data);
        if (err != AudioFileError::None)
        {
          s.info.error = true;
          s.info.message = std::string(audioFileErrorText(err)) + ": " + fileNameOf(path);
        }
        else if (data.numFrames > static_cast<int>(data.sampleRate * 10.0))
        {
          s.info.error = true;
          s.info.message = "Файл длиннее 10 с — это не IR кабинета: " + fileNameOf(path);
        }
        else
        {
          // Стерео-IR сводится в моно (среднее каналов); многоканальные — первые два канала
          const int chans = std::min(2, data.numChannels);
          std::vector<float> mono(static_cast<size_t>(data.numFrames), 0.f);
          for (int c = 0; c < chans; ++c)
            for (int f = 0; f < data.numFrames; ++f) mono[static_cast<size_t>(f)] += data.channels[static_cast<size_t>(c)][static_cast<size_t>(f)] / chans;
          float pk = 0.f;
          for (float v : mono) pk = std::max(pk, std::fabs(v));
          if (pk < 1e-6f)
          {
            s.info.error = true;
            s.info.message = "Файл содержит тишину: " + fileNameOf(path);
          }
          else
          {
            s.userIROrig = std::move(mono);
            s.userRate = data.sampleRate;
            s.loadedPath = path;
            s.info.error = false;
            s.info.userPath = path;
            s.info.fileRate = data.sampleRate;
            s.info.fileChannels = data.numChannels;
            char buf[256];
            const double ms = 1000.0 * data.numFrames / data.sampleRate;
            snprintf(buf, sizeof(buf), "%s — %.0f мс, %.1f кГц, %d кан.%s", fileNameOf(path).c_str(), ms,
                     data.sampleRate / 1000.0, data.numChannels, ms > kMaxIRSeconds * 1000.0 ? " (обрезан до 350 мс)" : "");
            s.info.message = buf;
          }
        }
      }
      s.userLoaded = req;
      rebuild = s.speaker.load() >= kCabUserIRIndex;
      did = true;
    }
    const uint32_t v = s.cfgVersion.load(std::memory_order_acquire);
    if (v != s.builtVersion || rebuild)
    {
      ConvolutionKernel* k = buildKernel(s, i, sr);
      delete s.pending.exchange(k, std::memory_order_acq_rel); // не захваченное audio thread ядро можно удалить
      s.builtVersion = v;
      did = true;
    }
  }
  return did;
}

void Cabinet::collectRetired()
{
  for (auto& s : mSlot) delete s.retired.exchange(nullptr, std::memory_order_acq_rel);
}

void Cabinet::workerLoop()
{
  while (!mQuit.load())
  {
    mBusy.store(true);
    const bool did = processRequests();
    collectRetired();
    mBusy.store(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(did ? 1 : 8));
  }
}

} // namespace tsl
