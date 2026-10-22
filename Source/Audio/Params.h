// SPDX-License-Identifier: GPL-3.0-or-later
// Единый реестр параметров TwoonemoonSoundLab.
//
// ПРАВИЛА (см. CLAUDE.md):
//  * Индекс параметра = ID для хоста (AU/VST3/CLAP). Новые параметры добавляются ТОЛЬКО в конец
//    списка. Удалять/переставлять нельзя после релиза 1.0 — это ломает автоматизацию в проектах.
//  * Строковый ключ (key) используется в пресетах и не должен меняться.
//  * Подписи регуляторов — общепринятые английские термины (Gain, Master, Mix...).
#pragma once

#include <cstddef>

namespace tsl {

enum class ParamUnit : int { None, dB, Percent, Hz, Ms, Seconds, Semitones, Cents, Ratio, Bpm, Degrees };
enum class ParamKind : int { Float, Bool, Enum, Int };
enum class ParamShape : int { Linear, Exp, Pow2, Pow3 };

struct ParamSpec
{
  const char* key;
  const char* name;
  const char* group;
  ParamKind kind;
  float minValue, maxValue, defaultValue, step;
  ParamShape shape;
  ParamUnit unit;
  const char* const* labels; // для Enum
  int numLabels;
};

// ---------------------------------------------------------------------------
// Списки значений для Enum-параметров
namespace labels {
inline constexpr const char* kInputMode[] = { "Left (In 1)", "Right (In 2)", "Sum L+R", "Stereo" };
inline constexpr const char* kGateKey[] = { "Local", "Input DI" };
inline constexpr const char* kOdMode[] = { "Transparent", "Mid Focus" };
inline constexpr const char* kDistMode[] = { "Silicon", "LED", "Hard" };
inline constexpr const char* kFuzzMode[] = { "Germanium", "Silicon", "Octave" };
inline constexpr const char* kOversampling[] = { "Live 2x", "Standard 4x", "High 8x" };
inline constexpr const char* kAmpModel[] = { "Lumen Twin",   "Opal Bloom",   "Glass Garden",          "Velvet Edge",
                                             "Brass Circuit", "Rust Monarch", "Thorn Drive",           "Iron Veil",
                                             "Catacomb",      "Abyssal Lead", "Silver Current Acoustic", "Cedar Room Acoustic" };
} // namespace labels

#define TSL_N(arr) static_cast<int>(sizeof(arr) / sizeof(arr[0]))

// X(Enum, key, name, group, kind, min, max, def, step, shape, unit, labels, nLabels)
#define TSL_PARAM_LIST(X)                                                                                              \
  /* ---------------- Global ---------------- */                                                                      \
  X(InputTrim, "global.input", "Input", "Global", Float, -24.f, 24.f, 0.f, 0.1f, Linear, dB, nullptr, 0)              \
  X(OutputTrim, "global.output", "Output", "Global", Float, -48.f, 12.f, 0.f, 0.1f, Linear, dB, nullptr, 0)           \
  X(Bypass, "global.bypass", "Bypass", "Global", Bool, 0.f, 1.f, 0.f, 1.f, Linear, None, nullptr, 0)                  \
  X(InputMode, "global.inputMode", "Input Mode", "Global", Enum, 0.f, 3.f, 0.f, 1.f, Linear, None, labels::kInputMode, \
    TSL_N(labels::kInputMode))                                                                                         \
  X(TunerMute, "global.tunerMute", "Tuner Mute", "Global", Bool, 0.f, 1.f, 0.f, 1.f, Linear, None, nullptr, 0)        \
  X(TunerRef, "global.tunerRef", "A4 Reference", "Global", Float, 425.f, 455.f, 440.f, 0.1f, Linear, Hz, nullptr, 0)  \
  X(Tempo, "global.tempo", "Tempo", "Global", Float, 30.f, 300.f, 120.f, 0.1f, Linear, Bpm, nullptr, 0)               \
  X(HostTempo, "global.hostTempo", "Follow Host Tempo", "Global", Bool, 0.f, 1.f, 1.f, 1.f, Linear, None, nullptr, 0) \
  X(Oversampling, "global.oversampling", "Oversampling", "Global", Enum, 0.f, 2.f, 1.f, 1.f, Linear, None,            \
    labels::kOversampling, TSL_N(labels::kOversampling))                                                               \
  /* ---------------- Noise Gate ---------------- */                                                                  \
  X(GateOn, "gate.on", "Gate On", "Gate", Bool, 0.f, 1.f, 1.f, 1.f, Linear, None, nullptr, 0)                         \
  X(GateThreshold, "gate.threshold", "Gate Threshold", "Gate", Float, -96.f, 0.f, -62.f, 0.1f, Linear, dB, nullptr, 0)\
  X(GateAttack, "gate.attack", "Gate Attack", "Gate", Float, 0.05f, 50.f, 0.5f, 0.01f, Exp, Ms, nullptr, 0)            \
  X(GateHold, "gate.hold", "Gate Hold", "Gate", Float, 0.f, 500.f, 30.f, 0.1f, Pow2, Ms, nullptr, 0)                   \
  X(GateRelease, "gate.release", "Gate Release", "Gate", Float, 5.f, 2000.f, 120.f, 0.1f, Exp, Ms, nullptr, 0)         \
  X(GateHysteresis, "gate.hysteresis", "Gate Hysteresis", "Gate", Float, 0.f, 20.f, 6.f, 0.1f, Linear, dB, nullptr, 0)\
  X(GateRange, "gate.range", "Gate Range", "Gate", Float, -96.f, 0.f, -90.f, 0.1f, Linear, dB, nullptr, 0)             \
  X(GateKey, "gate.key", "Gate Key", "Gate", Enum, 0.f, 1.f, 0.f, 1.f, Linear, None, labels::kGateKey,                 \
    TSL_N(labels::kGateKey))                                                                                           \
  /* ---------------- Compressor ---------------- */                                                                  \
  X(CompOn, "comp.on", "Comp On", "Compressor", Bool, 0.f, 1.f, 0.f, 1.f, Linear, None, nullptr, 0)                   \
  X(CompThreshold, "comp.threshold", "Comp Threshold", "Compressor", Float, -60.f, 0.f, -24.f, 0.1f, Linear, dB,       \
    nullptr, 0)                                                                                                        \
  X(CompRatio, "comp.ratio", "Comp Ratio", "Compressor", Float, 1.f, 20.f, 4.f, 0.01f, Exp, Ratio, nullptr, 0)        \
  X(CompAttack, "comp.attack", "Comp Attack", "Compressor", Float, 0.1f, 100.f, 8.f, 0.01f, Exp, Ms, nullptr, 0)      \
  X(CompRelease, "comp.release", "Comp Release", "Compressor", Float, 10.f, 2000.f, 160.f, 0.1f, Exp, Ms, nullptr, 0) \
  X(CompKnee, "comp.knee", "Comp Knee", "Compressor", Float, 0.f, 24.f, 6.f, 0.1f, Linear, dB, nullptr, 0)            \
  X(CompMakeup, "comp.makeup", "Comp Makeup", "Compressor", Float, 0.f, 24.f, 0.f, 0.1f, Linear, dB, nullptr, 0)      \
  X(CompMix, "comp.mix", "Comp Mix", "Compressor", Float, 0.f, 100.f, 100.f, 0.1f, Linear, Percent, nullptr, 0)       \
  X(CompScHpf, "comp.scHpf", "Comp SC HPF", "Compressor", Float, 20.f, 500.f, 20.f, 1.f, Exp, Hz, nullptr, 0)          \
  /* ---------------- Boost ---------------- */                                                                       \
  X(BoostOn, "boost.on", "Boost On", "Boost", Bool, 0.f, 1.f, 0.f, 1.f, Linear, None, nullptr, 0)                     \
  X(BoostLevel, "boost.level", "Boost Level", "Boost", Float, 0.f, 20.f, 6.f, 0.1f, Linear, dB, nullptr, 0)            \
  X(BoostTight, "boost.tight", "Boost Tight", "Boost", Float, 20.f, 800.f, 20.f, 1.f, Exp, Hz, nullptr, 0)              \
  X(BoostBright, "boost.bright", "Boost Bright", "Boost", Float, -6.f, 6.f, 0.f, 0.1f, Linear, dB, nullptr, 0)          \
  /* ---------------- Overdrive ---------------- */                                                                   \
  X(OdOn, "od.on", "Overdrive On", "Overdrive", Bool, 0.f, 1.f, 0.f, 1.f, Linear, None, nullptr, 0)                   \
  X(OdMode, "od.mode", "Overdrive Mode", "Overdrive", Enum, 0.f, 1.f, 0.f, 1.f, Linear, None, labels::kOdMode,         \
    TSL_N(labels::kOdMode))                                                                                            \
  X(OdDrive, "od.drive", "Overdrive Drive", "Overdrive", Float, 0.f, 10.f, 4.f, 0.01f, Linear, None, nullptr, 0)      \
  X(OdTone, "od.tone", "Overdrive Tone", "Overdrive", Float, 0.f, 10.f, 5.f, 0.01f, Linear, None, nullptr, 0)         \
  X(OdLevel, "od.level", "Overdrive Level", "Overdrive", Float, -24.f, 12.f, 0.f, 0.1f, Linear, dB, nullptr, 0)       \
  X(OdMix, "od.mix", "Overdrive Mix", "Overdrive", Float, 0.f, 100.f, 100.f, 0.1f, Linear, Percent, nullptr, 0)       \
  /* ---------------- Distortion ---------------- */                                                                  \
  X(DistOn, "dist.on", "Distortion On", "Distortion", Bool, 0.f, 1.f, 0.f, 1.f, Linear, None, nullptr, 0)             \
  X(DistMode, "dist.mode", "Distortion Mode", "Distortion", Enum, 0.f, 2.f, 0.f, 1.f, Linear, None, labels::kDistMode, \
    TSL_N(labels::kDistMode))                                                                                          \
  X(DistDrive, "dist.drive", "Distortion Drive", "Distortion", Float, 0.f, 10.f, 5.f, 0.01f, Linear, None, nullptr, 0)\
  X(DistTone, "dist.tone", "Distortion Tone", "Distortion", Float, 0.f, 10.f, 5.f, 0.01f, Linear, None, nullptr, 0)   \
  X(DistTight, "dist.tight", "Distortion Tight", "Distortion", Float, 20.f, 600.f, 80.f, 1.f, Exp, Hz, nullptr, 0)     \
  X(DistLevel, "dist.level", "Distortion Level", "Distortion", Float, -24.f, 12.f, -6.f, 0.1f, Linear, dB, nullptr, 0)\
  /* ---------------- Fuzz ---------------- */                                                                        \
  X(FuzzOn, "fuzz.on", "Fuzz On", "Fuzz", Bool, 0.f, 1.f, 0.f, 1.f, Linear, None, nullptr, 0)                         \
  X(FuzzMode, "fuzz.mode", "Fuzz Mode", "Fuzz", Enum, 0.f, 2.f, 0.f, 1.f, Linear, None, labels::kFuzzMode,             \
    TSL_N(labels::kFuzzMode))                                                                                          \
  X(FuzzFuzz, "fuzz.fuzz", "Fuzz", "Fuzz", Float, 0.f, 10.f, 6.f, 0.01f, Linear, None, nullptr, 0)                    \
  X(FuzzTone, "fuzz.tone", "Fuzz Tone", "Fuzz", Float, 0.f, 10.f, 5.f, 0.01f, Linear, None, nullptr, 0)               \
  X(FuzzBias, "fuzz.bias", "Fuzz Bias", "Fuzz", Float, 0.f, 10.f, 5.f, 0.01f, Linear, None, nullptr, 0)               \
  X(FuzzLevel, "fuzz.level", "Fuzz Level", "Fuzz", Float, -24.f, 12.f, -8.f, 0.1f, Linear, dB, nullptr, 0)        \
  /* ---------------- Amp ---------------- */                                                                         \
  X(AmpOn, "amp.on", "Amp On", "Amp", Bool, 0.f, 1.f, 1.f, 1.f, Linear, None, nullptr, 0)                             \
  X(AmpModel, "amp.model", "Amp Model", "Amp", Enum, 0.f, 11.f, 4.f, 1.f, Linear, None, labels::kAmpModel,             \
    TSL_N(labels::kAmpModel))                                                                                          \
  X(AmpGain, "amp.gain", "Gain", "Amp", Float, 0.f, 10.f, 5.f, 0.01f, Linear, None, nullptr, 0)                       \
  X(AmpBass, "amp.bass", "Bass", "Amp", Float, 0.f, 10.f, 5.f, 0.01f, Linear, None, nullptr, 0)                       \
  X(AmpMid, "amp.mid", "Middle", "Amp", Float, 0.f, 10.f, 5.f, 0.01f, Linear, None, nullptr, 0)                       \
  X(AmpTreble, "amp.treble", "Treble", "Amp", Float, 0.f, 10.f, 5.f, 0.01f, Linear, None, nullptr, 0)                 \
  X(AmpPresence, "amp.presence", "Presence", "Amp", Float, 0.f, 10.f, 5.f, 0.01f, Linear, None, nullptr, 0)           \
  X(AmpDepth, "amp.depth", "Depth", "Amp", Float, 0.f, 10.f, 5.f, 0.01f, Linear, None, nullptr, 0)                    \
  X(AmpMaster, "amp.master", "Master", "Amp", Float, 0.f, 10.f, 5.f, 0.01f, Linear, None, nullptr, 0)                 \
  X(AmpLevel, "amp.level", "Amp Level", "Amp", Float, -24.f, 12.f, 0.f, 0.1f, Linear, dB, nullptr, 0)                 \
  X(AmpSag, "amp.sag", "Sag", "Amp", Float, 0.f, 10.f, 5.f, 0.01f, Linear, None, nullptr, 0)                          \
  X(AmpBias, "amp.bias", "Bias", "Amp", Float, 0.f, 10.f, 5.f, 0.01f, Linear, None, nullptr, 0)                       \
  X(AmpFeedback, "amp.feedback", "Feedback", "Amp", Float, 0.f, 10.f, 5.f, 0.01f, Linear, None, nullptr, 0)           \
  X(AmpBright, "amp.bright", "Bright", "Amp", Bool, 0.f, 1.f, 0.f, 1.f, Linear, None, nullptr, 0)                     \
  X(AmpLowCut, "amp.lowCut", "Amp Low Cut", "Amp", Float, 20.f, 300.f, 20.f, 1.f, Exp, Hz, nullptr, 0)                \
  X(AmpHighCut, "amp.highCut", "Amp High Cut", "Amp", Float, 2000.f, 20000.f, 20000.f, 1.f, Exp, Hz, nullptr, 0)      \
  X(AcBody, "amp.acBody", "Body", "Amp", Float, 0.f, 10.f, 5.f, 0.01f, Linear, None, nullptr, 0)                      \
  X(AcPiezoTame, "amp.acPiezoTame", "Piezo Tame", "Amp", Float, 0.f, 10.f, 3.f, 0.01f, Linear, None, nullptr, 0)      \
  X(AcAir, "amp.acAir", "Air", "Amp", Float, 0.f, 10.f, 3.f, 0.01f, Linear, None, nullptr, 0)                         \
  X(AcRoom, "amp.acRoom", "Room", "Amp", Float, 0.f, 10.f, 2.f, 0.01f, Linear, None, nullptr, 0)

enum ParamId : int
{
#define TSL_ENUM_ENTRY(e, ...) k##e,
  TSL_PARAM_LIST(TSL_ENUM_ENTRY)
#undef TSL_ENUM_ENTRY
  kNumParams
};

/** Таблица спецификаций, индекс = ParamId. */
const ParamSpec& paramSpec(int id) noexcept;

/** Поиск по ключу пресета; -1 если не найдено. */
int paramIdForKey(const char* key) noexcept;

/** Нормализация plain ↔ [0..1] с учётом формы (используется UI и тестами; хост-нормализация — во фреймворке). */
float paramToNormalized(int id, float plain) noexcept;
float paramFromNormalized(int id, float normalized) noexcept;
float paramClamp(int id, float plain) noexcept;

} // namespace tsl
