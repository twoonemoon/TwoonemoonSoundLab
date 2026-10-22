// SPDX-License-Identifier: GPL-3.0-or-later
// Чтение/запись WAV и офлайн-ресемплинг. Только вне audio thread.
#pragma once

#include <string>
#include <vector>

namespace tsl {

struct AudioFileData
{
  double sampleRate = 0.0;
  int numChannels = 0;
  int numFrames = 0;
  std::vector<std::vector<float>> channels;
};

enum class AudioFileError
{
  None,
  NotFound,
  TooLarge,
  NotWav,
  UnsupportedFormat,
  Corrupted,
  Empty,
};

/** Описание ошибки для UI (рус.). */
const char* audioFileErrorText(AudioFileError e) noexcept;

/**
 * Прочитать WAV: PCM 16/24/32 бит, IEEE float 32/64, WAVE_FORMAT_EXTENSIBLE; 1–8 каналов.
 * maxBytes — защита от неадекватно больших файлов.
 */
AudioFileError readWav(const std::string& path, AudioFileData& out, size_t maxBytes = 64u * 1024u * 1024u);
AudioFileError readWavFromMemory(const unsigned char* data, size_t size, AudioFileData& out);

/** Записать 32-bit float WAV. */
bool writeWav(const std::string& path, const std::vector<std::vector<float>>& channels, double sampleRate);

/** Высококачественный офлайн-ресемплинг (windowed sinc, окно Кайзера). */
std::vector<float> resample(const std::vector<float>& in, double fromRate, double toRate);

} // namespace tsl
