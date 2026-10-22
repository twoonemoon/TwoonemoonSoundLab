// SPDX-License-Identifier: GPL-3.0-or-later
#include "AudioFile.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace tsl {

const char* audioFileErrorText(AudioFileError e) noexcept
{
  switch (e)
  {
    case AudioFileError::None: return "";
    case AudioFileError::NotFound: return "Файл не найден или недоступен";
    case AudioFileError::TooLarge: return "Файл слишком большой";
    case AudioFileError::NotWav: return "Это не WAV-файл";
    case AudioFileError::UnsupportedFormat: return "Неподдерживаемый формат WAV (нужен PCM 16/24/32 бит или float)";
    case AudioFileError::Corrupted: return "Файл повреждён";
    case AudioFileError::Empty: return "В файле нет аудиоданных";
  }
  return "";
}

namespace {

uint32_t rd32(const unsigned char* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24); }
uint16_t rd16(const unsigned char* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }

} // namespace

AudioFileError readWavFromMemory(const unsigned char* d, size_t size, AudioFileData& out)
{
  if (size < 12 || std::memcmp(d, "RIFF", 4) != 0 || std::memcmp(d + 8, "WAVE", 4) != 0) return AudioFileError::NotWav;
  size_t pos = 12;
  int fmtTag = 0, channels = 0, bits = 0;
  uint32_t rate = 0;
  const unsigned char* data = nullptr;
  size_t dataSize = 0;
  while (pos + 8 <= size)
  {
    const unsigned char* ck = d + pos;
    const uint32_t ckSize = rd32(ck + 4);
    const size_t body = pos + 8;
    if (body + ckSize > size)
    {
      // Обрезанный файл: допускаем усечённый data-чанк
      if (std::memcmp(ck, "data", 4) == 0) { data = d + body; dataSize = size - body; break; }
      return AudioFileError::Corrupted;
    }
    if (std::memcmp(ck, "fmt ", 4) == 0)
    {
      if (ckSize < 16) return AudioFileError::Corrupted;
      fmtTag = rd16(d + body);
      channels = rd16(d + body + 2);
      rate = rd32(d + body + 4);
      bits = rd16(d + body + 14);
      if (fmtTag == 0xFFFE && ckSize >= 40) fmtTag = rd16(d + body + 24); // подформат EXTENSIBLE
    }
    else if (std::memcmp(ck, "data", 4) == 0)
    {
      data = d + body;
      dataSize = ckSize;
    }
    pos = body + ckSize + (ckSize & 1u);
  }
  if (!data || channels <= 0 || rate == 0) return AudioFileError::Corrupted;
  if (channels > 8 || rate < 8000 || rate > 768000) return AudioFileError::UnsupportedFormat;
  const bool isFloat = fmtTag == 3;
  if (!(fmtTag == 1 || isFloat)) return AudioFileError::UnsupportedFormat;
  if (isFloat && !(bits == 32 || bits == 64)) return AudioFileError::UnsupportedFormat;
  if (!isFloat && !(bits == 16 || bits == 24 || bits == 32)) return AudioFileError::UnsupportedFormat;

  const int bytesPer = bits / 8;
  const size_t frameBytes = static_cast<size_t>(bytesPer * channels);
  const size_t frames = dataSize / frameBytes;
  if (frames == 0) return AudioFileError::Empty;

  out.sampleRate = rate;
  out.numChannels = channels;
  out.numFrames = static_cast<int>(frames);
  out.channels.assign(static_cast<size_t>(channels), std::vector<float>(frames));
  for (size_t f = 0; f < frames; ++f)
  {
    for (int c = 0; c < channels; ++c)
    {
      const unsigned char* s = data + f * frameBytes + static_cast<size_t>(c * bytesPer);
      float v = 0.f;
      if (isFloat)
      {
        if (bits == 32) { float x; std::memcpy(&x, s, 4); v = x; }
        else { double x; std::memcpy(&x, s, 8); v = static_cast<float>(x); }
      }
      else if (bits == 16) v = static_cast<int16_t>(rd16(s)) / 32768.f;
      else if (bits == 24)
      {
        int32_t x = s[0] | (s[1] << 8) | (s[2] << 16);
        if (x & 0x800000) x |= ~0xFFFFFF;
        v = static_cast<float>(x) / 8388608.f;
      }
      else v = static_cast<float>(static_cast<int32_t>(rd32(s)) / 2147483648.0);
      out.channels[static_cast<size_t>(c)][f] = std::isfinite(v) ? v : 0.f;
    }
  }
  return AudioFileError::None;
}

AudioFileError readWav(const std::string& path, AudioFileData& out, size_t maxBytes)
{
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f) return AudioFileError::NotFound;
  const std::streamoff len = f.tellg();
  if (len <= 0) return AudioFileError::Empty;
  if (static_cast<size_t>(len) > maxBytes) return AudioFileError::TooLarge;
  std::vector<unsigned char> buf(static_cast<size_t>(len));
  f.seekg(0);
  if (!f.read(reinterpret_cast<char*>(buf.data()), len)) return AudioFileError::Corrupted;
  return readWavFromMemory(buf.data(), buf.size(), out);
}

bool writeWav(const std::string& path, const std::vector<std::vector<float>>& ch, double sampleRate)
{
  if (ch.empty()) return false;
  const uint16_t nch = static_cast<uint16_t>(ch.size());
  const uint32_t frames = static_cast<uint32_t>(ch[0].size());
  const uint32_t dataBytes = frames * nch * 4;
  std::ofstream f(path, std::ios::binary);
  if (!f) return false;
  auto w32 = [&](uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); };
  auto w16 = [&](uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); };
  f.write("RIFF", 4); w32(36 + dataBytes); f.write("WAVE", 4);
  f.write("fmt ", 4); w32(16); w16(3); w16(nch); w32(static_cast<uint32_t>(sampleRate));
  w32(static_cast<uint32_t>(sampleRate) * nch * 4); w16(static_cast<uint16_t>(nch * 4)); w16(32);
  f.write("data", 4); w32(dataBytes);
  for (uint32_t i = 0; i < frames; ++i)
    for (uint16_t c = 0; c < nch; ++c)
    {
      const float v = i < ch[c].size() ? ch[c][i] : 0.f;
      f.write(reinterpret_cast<const char*>(&v), 4);
    }
  return static_cast<bool>(f);
}

namespace {

double besselI0(double x)
{
  double sum = 1.0, term = 1.0;
  for (int k = 1; k < 50; ++k)
  {
    term *= (x / (2.0 * k)) * (x / (2.0 * k));
    sum += term;
    if (term < 1e-12 * sum) break;
  }
  return sum;
}

} // namespace

std::vector<float> resample(const std::vector<float>& in, double fromRate, double toRate)
{
  if (in.empty() || fromRate <= 0.0 || toRate <= 0.0) return {};
  if (std::fabs(fromRate - toRate) < 1e-6) return in;
  const double ratio = toRate / fromRate;
  const double cutoff = std::min(1.0, ratio) * 0.97; // небольшой запас ниже Найквиста
  const int halfWidth = 32;
  const double beta = 9.0;
  const double i0b = besselI0(beta);
  const size_t outLen = static_cast<size_t>(std::ceil(static_cast<double>(in.size()) * ratio));
  std::vector<float> out(outLen, 0.f);
  const double support = halfWidth / cutoff;
  for (size_t n = 0; n < outLen; ++n)
  {
    const double t = static_cast<double>(n) / ratio; // позиция во входе
    const int k0 = static_cast<int>(std::floor(t - support));
    const int k1 = static_cast<int>(std::ceil(t + support));
    double acc = 0.0;
    for (int k = std::max(0, k0); k <= std::min(static_cast<int>(in.size()) - 1, k1); ++k)
    {
      const double x = (t - k) * cutoff;
      const double sinc = std::fabs(x) < 1e-9 ? 1.0 : std::sin(M_PI * x) / (M_PI * x);
      const double r = x / halfWidth;
      if (std::fabs(r) >= 1.0) continue;
      const double win = besselI0(beta * std::sqrt(1.0 - r * r)) / i0b;
      acc += in[static_cast<size_t>(k)] * sinc * win * cutoff;
    }
    out[n] = static_cast<float>(acc);
  }
  return out;
}

} // namespace tsl
