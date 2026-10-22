// SPDX-License-Identifier: GPL-3.0-or-later
#include "../TestHarness.h"
#include "../TestSignals.h"

#include "../../Source/Cabinet/CabIRSynth.h"
#include "../../Source/Cabinet/Cabinet.h"
#include "../../Source/DSP/AudioFile.h"
#include "../../Source/DSP/Convolver.h"

#include <cstdio>
#include <unistd.h>

using namespace tsl;
using namespace tsltest;

namespace {

std::vector<float> directConvolve(const std::vector<float>& x, const std::vector<float>& h)
{
  std::vector<float> y(x.size(), 0.f);
  for (size_t n = 0; n < x.size(); ++n)
  {
    double acc = 0;
    for (size_t k = 0; k < h.size() && k <= n; ++k) acc += static_cast<double>(h[k]) * x[n - k];
    y[n] = static_cast<float>(acc);
  }
  return y;
}

std::string tmpPath(const char* name)
{
  char buf[512];
  snprintf(buf, sizeof(buf), "%s/tsl_test_%d_%s", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", getpid(), name);
  return buf;
}

ParamValues defaults()
{
  ParamValues p{};
  for (int i = 0; i < kNumParams; ++i) p[static_cast<size_t>(i)] = paramSpec(i).defaultValue;
  return p;
}

} // namespace

TEST_CASE("convolver: matches direct convolution with zero latency, any block size")
{
  auto h = noise(1500, 0.3f, 5);
  for (size_t i = 0; i < h.size(); ++i) h[i] *= std::exp(-static_cast<float>(i) / 300.f);
  auto x = noise(6000, 0.5f, 9);
  const auto ref = directConvolve(x, h);
  for (int block : { 1, 17, 64, 100, 256, 512 })
  {
    RealFFT fft;
    fft.prepare(128);
    auto k = ConvolutionKernel::create(h.data(), static_cast<int>(h.size()), 64, fft);
    ZeroLatencyConvolver c;
    c.prepare(64, k->numParts + 2, 512);
    std::vector<float> y(x.size());
    for (size_t pos = 0; pos < x.size(); pos += static_cast<size_t>(block))
    {
      const int n = static_cast<int>(std::min<size_t>(static_cast<size_t>(block), x.size() - pos));
      c.process(k.get(), nullptr, x.data() + pos, y.data() + pos, n);
    }
    float maxErr = 0.f;
    for (size_t i = 0; i < y.size(); ++i) maxErr = std::max(maxErr, std::fabs(y[i] - ref[i]));
    CHECK_MSG(maxErr < 1e-4f, "block %d maxErr %g", block, maxErr);
  }
}

TEST_CASE("convolver: impulse at sample 0 reproduces IR exactly from sample 0")
{
  std::vector<float> h(300);
  for (size_t i = 0; i < h.size(); ++i) h[i] = std::sin(i * 0.1f) * std::exp(-static_cast<float>(i) / 80.f);
  RealFFT fft;
  fft.prepare(128);
  auto k = ConvolutionKernel::create(h.data(), 300, 64, fft);
  ZeroLatencyConvolver c;
  c.prepare(64, 8, 64);
  auto x = impulse(512);
  std::vector<float> y(512);
  for (int p = 0; p < 512; p += 64) c.process(k.get(), nullptr, x.data() + p, y.data() + p, 64);
  for (int i = 0; i < 300; ++i) CHECK_NEAR(y[static_cast<size_t>(i)], h[static_cast<size_t>(i)], 1e-5);
}

TEST_CASE("convolver: crossfade between kernels is continuous")
{
  std::vector<float> h1(200, 0.f), h2(200, 0.f);
  h1[0] = 1.f;
  h2[0] = 0.5f;
  h2[150] = 0.3f;
  RealFFT fft;
  fft.prepare(128);
  auto k1 = ConvolutionKernel::create(h1.data(), 200, 64, fft);
  auto k2 = ConvolutionKernel::create(h2.data(), 200, 64, fft);
  ZeroLatencyConvolver c;
  c.prepare(64, 8, 128);
  auto x = sine(200.f, 0.5f, 48000.f, 4096);
  std::vector<float> y(4096);
  for (int p = 0; p < 4096; p += 128)
  {
    const float g0 = std::clamp((p - 1024) / 1024.f, 0.f, 1.f), g1 = std::clamp((p + 128 - 1024) / 1024.f, 0.f, 1.f);
    c.process(k1.get(), k2.get(), x.data() + p, y.data() + p, 128, g0, g1);
  }
  CHECK(allFinite(y));
  float maxStep = 0.f;
  for (size_t i = 1; i < y.size(); ++i) maxStep = std::max(maxStep, std::fabs(y[i] - y[i - 1]));
  CHECK_MSG(maxStep < 0.05f, "maxStep %g", maxStep);
}

TEST_CASE("wav: write/read roundtrip and malformed input")
{
  const std::string path = tmpPath("rt.wav");
  std::vector<std::vector<float>> ch = { sine(440.f, 0.5f, 44100.f, 1000), sine(880.f, 0.25f, 44100.f, 1000) };
  CHECK(writeWav(path, ch, 44100.0));
  AudioFileData d;
  CHECK(readWav(path, d) == AudioFileError::None);
  CHECK(d.numChannels == 2 && d.numFrames == 1000 && d.sampleRate == 44100.0);
  float maxErr = 0.f;
  for (int i = 0; i < 1000; ++i) maxErr = std::max(maxErr, std::fabs(d.channels[1][static_cast<size_t>(i)] - ch[1][static_cast<size_t>(i)]));
  CHECK(maxErr < 1e-7f);
  std::remove(path.c_str());

  CHECK(readWav("/nonexistent/file.wav", d) == AudioFileError::NotFound);
  const unsigned char junk[] = { 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'A', 'V', 'I', ' ' };
  CHECK(readWavFromMemory(junk, sizeof(junk), d) == AudioFileError::NotWav);
  // 16-bit PCM вручную
  std::vector<unsigned char> w = { 'R', 'I', 'F', 'F', 40, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 1, 0,
                                   0x80, 0xBB, 0, 0, 0, 0x77, 1, 0, 2, 0, 16, 0, 'd', 'a', 't', 'a', 4, 0, 0, 0, 0x00, 0x40, 0x00, 0xC0 };
  CHECK(readWavFromMemory(w.data(), w.size(), d) == AudioFileError::None);
  CHECK(d.sampleRate == 48000.0 && d.numFrames == 2);
  CHECK_NEAR(d.channels[0][0], 0.5, 1e-6);
  CHECK_NEAR(d.channels[0][1], -0.5, 1e-6);
  // Неподдерживаемая разрядность (8 бит)
  w[34] = 8;
  CHECK(readWavFromMemory(w.data(), w.size(), d) == AudioFileError::UnsupportedFormat);
}

TEST_CASE("resample: preserves in-band tone, length scales")
{
  auto x = sine(1000.f, 0.5f, 44100.f, 44100);
  auto y = resample(x, 44100.0, 48000.0);
  CHECK(std::abs(static_cast<int>(y.size()) - 48000) <= 1);
  CHECK_NEAR(toneAmplitude(y, 4800, 1000.f, 48000.f), 0.5, 0.01);
  auto z = resample(x, 44100.0, 22050.0);
  CHECK_NEAR(toneAmplitude(z, 2000, 1000.f, 22050.f), 0.5, 0.01);
}

TEST_CASE("cab synth: finite, normalized, position/angle/mic change tone as expected")
{
  for (int spk = 0; spk < kNumCabSpeakers; ++spk)
    for (int mic = 0; mic < kNumCabMics; ++mic)
    {
      CabMicSetup s;
      s.speaker = spk;
      s.mic = mic;
      auto ir = synthesizeCabIR(s, 48000.0);
      CHECK(allFinite(ir));
      CHECK(ir.size() == 2160);
      CHECK(peak(ir) < 4.f);
      // Средний уровень в полосе 120 Гц–5 кГц ≈ 0 дБ
      double acc = 0;
      for (int i = 0; i < 32; ++i)
      {
        const float f = 120.f * std::pow(5000.f / 120.f, i / 31.f);
        acc += std::pow(10.0, cabModelResponseDb(s, f) / 10.0);
      }
      CHECK_NEAR(10.0 * std::log10(acc / 32), 0.0, 1.0);
    }
  CabMicSetup cap, edge, offAxis, close, far;
  cap.position = 0.f;
  edge.position = 1.f;
  CHECK(cabModelResponseDb(cap, 4500.f) > cabModelResponseDb(edge, 4500.f) + 4.f);
  offAxis.angleDeg = 45.f;
  CHECK(cabModelResponseDb(offAxis, 6000.f) < cabModelResponseDb(CabMicSetup{}, 6000.f) - 4.f);
  close.distanceCm = 1.f;
  far.distanceCm = 30.f;
  CHECK(cabModelResponseDb(close, 80.f) > cabModelResponseDb(far, 80.f) + 3.f);
  // Ленточный микрофон темнее конденсаторного
  CabMicSetup ribbon, cond;
  ribbon.mic = 2;
  cond.mic = 1;
  CHECK(cabModelResponseDb(ribbon, 9000.f) < cabModelResponseDb(cond, 9000.f) - 4.f);
  // IR имеет энергию в начале (минимальная фаза)
  auto ir = synthesizeCabIR(CabMicSetup{}, 48000.0);
  double early = 0, total = 0;
  for (size_t i = 0; i < ir.size(); ++i) { total += ir[i] * ir[i]; if (i < 240) early += ir[i] * ir[i]; }
  CHECK(early / total > 0.8);
}

TEST_CASE("cabinet module: zero latency, mono path, stereo when panned, user IR load + error")
{
  Cabinet cab;
  auto p = defaults();
  cab.prepare({ 48000.0, 256 });
  cab.update(p);
  CHECK(cab.latencySamples() == 0);
  auto g = pluckedGuitar(48000.f, 1.f);
  std::vector<float> l = g, r(g.size(), 0.f);
  for (size_t pos = 0; pos + 256 <= l.size(); pos += 256)
  {
    AudioBlock b;
    b.l = l.data() + pos;
    b.r = r.data() + pos;
    b.numFrames = 256;
    cab.process(b);
    CHECK(!b.stereo);
  }
  CHECK(allFinite(l));
  CHECK(rms(l, 4800) > 1e-3f);

  // Пользовательский IR: импульс → выход совпадает со входом (после нормировки — масштаб)
  const std::string path = tmpPath("ir.wav");
  std::vector<float> ir(480, 0.f);
  ir[0] = 0.8f;
  CHECK(writeWav(path, { ir }, 44100.0));
  cab.requestUserIR(0, path);
  p[kCabASpeaker] = static_cast<float>(kCabUserIRIndex);
  p[kCabAPan] = -50.f;
  cab.update(p);
  CHECK(cab.waitIdle());
  auto info = cab.slotInfo(0);
  CHECK(!info.error);
  CHECK(info.fileRate == 44100.0);
  auto sig = sine(500.f, 0.3f, 48000.f, 24000);
  std::vector<float> l2 = sig, r2(sig.size(), 0.f);
  bool stereo = false;
  for (size_t pos = 0; pos + 256 <= l2.size(); pos += 256)
  {
    AudioBlock b;
    b.l = l2.data() + pos;
    b.r = r2.data() + pos;
    b.numFrames = 256;
    cab.process(b);
    stereo = b.stereo;
  }
  CHECK(stereo);
  CHECK(rms(l2, 12000) > rms(r2, 12000) * 1.5f); // панорама влево
  std::remove(path.c_str());

  cab.requestUserIR(1, "/nonexistent/ir.wav");
  CHECK(cab.waitIdle());
  auto info2 = cab.slotInfo(1);
  CHECK(info2.error);
  CHECK(!info2.message.empty());
}
