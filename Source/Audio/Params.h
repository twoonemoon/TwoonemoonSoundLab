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

enum class ParamUnit : int { None, dB, Percent, Hz, Ms, Seconds, Semitones, Cents, Ratio, Bpm, Degrees, Cm };
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
inline constexpr const char* kCabSpeaker[] = { "Aurum 1x12 Open", "Lumen 2x12 Open", "Monolith 4x12", "Vanta 4x12",
                                               "Cedar 2x12",      "Ember 1x10",      "User IR" };
inline constexpr const char* kCabMic[] = { "Forge D7 (dynamic)", "Pearl C9 (condenser)", "Silk R2 (ribbon)" };
inline constexpr const char* kEqType[] = { "Bell", "Low Shelf", "High Shelf", "Notch" };
inline constexpr const char* kSlope[] = { "12 dB/oct", "24 dB/oct" };
inline constexpr const char* kDelayMode[] = { "Digital", "Tape", "Analog", "Ping-Pong", "Multi-Tap" };
inline constexpr const char* kDivision[] = { "1/1",  "1/2",   "1/2 dotted",  "1/2 triplet",  "1/4",  "1/4 dotted",  "1/4 triplet",
                                             "1/8",  "1/8 dotted",  "1/8 triplet", "1/16", "1/16 dotted", "1/16 triplet", "1/32" };
inline constexpr const char* kReverbMode[] = { "Room", "Hall", "Plate", "Spring", "Shimmer" };
inline constexpr const char* kPhaserStages[] = { "4 stages", "6 stages", "8 stages", "12 stages" };
inline constexpr const char* kTremShape[] = { "Sine", "Triangle", "Square" };
inline constexpr const char* kRotarySpeed[] = { "Slow", "Fast", "Brake" };
inline constexpr const char* kEnvMode[] = { "Low-pass", "Band-pass", "High-pass" };
inline constexpr const char* kEnvDirection[] = { "Up", "Down" };
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
  X(AcRoom, "amp.acRoom", "Room", "Amp", Float, 0.f, 10.f, 2.f, 0.01f, Linear, None, nullptr, 0)                     \
  /* ---------------- Cabinet ---------------- */                                                                     \
  X(CabOn, "cab.on", "Cab On", "Cabinet", Bool, 0.f, 1.f, 1.f, 1.f, Linear, None, nullptr, 0)                         \
  X(CabAOn, "cab.a.on", "Cab A On", "Cab A", Bool, 0.f, 1.f, 1.f, 1.f, Linear, None, nullptr, 0)              \
  X(CabASpeaker, "cab.a.speaker", "Cab A Speaker", "Cab A", Enum, 0.f, 6.f, 2.f, 1.f, Linear, None, labels::kCabSpeaker, \
    TSL_N(labels::kCabSpeaker))                                                                                        \
  X(CabAMic, "cab.a.mic", "Cab A Mic", "Cab A", Enum, 0.f, 2.f, 0.f, 1.f, Linear, None, labels::kCabMic,           \
    TSL_N(labels::kCabMic))                                                                                            \
  X(CabAPosition, "cab.a.position", "Cab A Position", "Cab A", Float, 0.f, 100.f, 30.f, 0.1f, Linear, Percent, nullptr, 0) \
  X(CabADistance, "cab.a.distance", "Cab A Distance", "Cab A", Float, 1.f, 30.f, 3.f, 0.1f, Exp, Cm, nullptr, 0)    \
  X(CabAAngle, "cab.a.angle", "Cab A Angle", "Cab A", Float, 0.f, 45.f, 0.f, 0.1f, Linear, Degrees, nullptr, 0)      \
  X(CabALevel, "cab.a.level", "Cab A Level", "Cab A", Float, -24.f, 6.f, 0.f, 0.1f, Linear, dB, nullptr, 0)        \
  X(CabAPan, "cab.a.pan", "Cab A Pan", "Cab A", Float, -100.f, 100.f, 0.f, 0.1f, Linear, Percent, nullptr, 0)         \
  X(CabAPhase, "cab.a.phase", "Cab A Phase Invert", "Cab A", Bool, 0.f, 1.f, 0.f, 1.f, Linear, None, nullptr, 0)    \
  X(CabADelay, "cab.a.delay", "Cab A Delay", "Cab A", Float, 0.f, 2.f, 0.f, 0.001f, Linear, Ms, nullptr, 0) \
  X(CabBOn, "cab.b.on", "Cab B On", "Cab B", Bool, 0.f, 1.f, 0.f, 1.f, Linear, None, nullptr, 0)              \
  X(CabBSpeaker, "cab.b.speaker", "Cab B Speaker", "Cab B", Enum, 0.f, 6.f, 2.f, 1.f, Linear, None, labels::kCabSpeaker, \
    TSL_N(labels::kCabSpeaker))                                                                                        \
  X(CabBMic, "cab.b.mic", "Cab B Mic", "Cab B", Enum, 0.f, 2.f, 2.f, 1.f, Linear, None, labels::kCabMic,           \
    TSL_N(labels::kCabMic))                                                                                            \
  X(CabBPosition, "cab.b.position", "Cab B Position", "Cab B", Float, 0.f, 100.f, 60.f, 0.1f, Linear, Percent, nullptr, 0) \
  X(CabBDistance, "cab.b.distance", "Cab B Distance", "Cab B", Float, 1.f, 30.f, 8.f, 0.1f, Exp, Cm, nullptr, 0)    \
  X(CabBAngle, "cab.b.angle", "Cab B Angle", "Cab B", Float, 0.f, 45.f, 15.f, 0.1f, Linear, Degrees, nullptr, 0)      \
  X(CabBLevel, "cab.b.level", "Cab B Level", "Cab B", Float, -24.f, 6.f, -3.f, 0.1f, Linear, dB, nullptr, 0)        \
  X(CabBPan, "cab.b.pan", "Cab B Pan", "Cab B", Float, -100.f, 100.f, 0.f, 0.1f, Linear, Percent, nullptr, 0)         \
  X(CabBPhase, "cab.b.phase", "Cab B Phase Invert", "Cab B", Bool, 0.f, 1.f, 0.f, 1.f, Linear, None, nullptr, 0)    \
  X(CabBDelay, "cab.b.delay", "Cab B Delay", "Cab B", Float, 0.f, 2.f, 0.f, 0.001f, Linear, Ms, nullptr, 0) \
  X(CabLowCut, "cab.lowCut", "Cab Low Cut", "Cabinet", Float, 20.f, 400.f, 60.f, 1.f, Exp, Hz, nullptr, 0)            \
  X(CabHighCut, "cab.highCut", "Cab High Cut", "Cabinet", Float, 2000.f, 20000.f, 12000.f, 1.f, Exp, Hz, nullptr, 0) \
  X(EqOn, "eq.on", "EQ On", "EQ", Bool, 0.0f, 1.0f, 0.0f, 1.0f, Linear, None, nullptr, 0) \
  X(EqLowCut, "eq.lowCut", "EQ Low Cut", "EQ", Float, 20.0f, 1000.0f, 20.0f, 1.0f, Exp, Hz, nullptr, 0) \
  X(EqLowSlope, "eq.lowSlope", "EQ Low Slope", "EQ", Enum, 0.0f, 1.0f, 0.0f, 1.0f, Linear, None, labels::kSlope, TSL_N(labels::kSlope)) \
  X(EqHighCut, "eq.highCut", "EQ High Cut", "EQ", Float, 1000.0f, 20000.0f, 20000.0f, 1.0f, Exp, Hz, nullptr, 0) \
  X(EqHighSlope, "eq.highSlope", "EQ High Slope", "EQ", Enum, 0.0f, 1.0f, 0.0f, 1.0f, Linear, None, labels::kSlope, TSL_N(labels::kSlope)) \
  X(EqB1On, "eq.b1.on", "EQ Band 1 On", "EQ", Bool, 0.0f, 1.0f, 1.0f, 1.0f, Linear, None, nullptr, 0) \
  X(EqB1Type, "eq.b1.type", "EQ Band 1 Type", "EQ", Enum, 0.0f, 3.0f, 1.0f, 1.0f, Linear, None, labels::kEqType, TSL_N(labels::kEqType)) \
  X(EqB1Freq, "eq.b1.freq", "EQ Band 1 Freq", "EQ", Float, 20.0f, 20000.0f, 100.0f, 1.0f, Exp, Hz, nullptr, 0) \
  X(EqB1Gain, "eq.b1.gain", "EQ Band 1 Gain", "EQ", Float, -18.0f, 18.0f, 0.0f, 0.1f, Linear, dB, nullptr, 0) \
  X(EqB1Q, "eq.b1.q", "EQ Band 1 Q", "EQ", Float, 0.1f, 10.0f, 0.7f, 0.01f, Exp, None, nullptr, 0) \
  X(EqB2On, "eq.b2.on", "EQ Band 2 On", "EQ", Bool, 0.0f, 1.0f, 1.0f, 1.0f, Linear, None, nullptr, 0) \
  X(EqB2Type, "eq.b2.type", "EQ Band 2 Type", "EQ", Enum, 0.0f, 3.0f, 0.0f, 1.0f, Linear, None, labels::kEqType, TSL_N(labels::kEqType)) \
  X(EqB2Freq, "eq.b2.freq", "EQ Band 2 Freq", "EQ", Float, 20.0f, 20000.0f, 350.0f, 1.0f, Exp, Hz, nullptr, 0) \
  X(EqB2Gain, "eq.b2.gain", "EQ Band 2 Gain", "EQ", Float, -18.0f, 18.0f, 0.0f, 0.1f, Linear, dB, nullptr, 0) \
  X(EqB2Q, "eq.b2.q", "EQ Band 2 Q", "EQ", Float, 0.1f, 10.0f, 1.0f, 0.01f, Exp, None, nullptr, 0) \
  X(EqB3On, "eq.b3.on", "EQ Band 3 On", "EQ", Bool, 0.0f, 1.0f, 1.0f, 1.0f, Linear, None, nullptr, 0) \
  X(EqB3Type, "eq.b3.type", "EQ Band 3 Type", "EQ", Enum, 0.0f, 3.0f, 0.0f, 1.0f, Linear, None, labels::kEqType, TSL_N(labels::kEqType)) \
  X(EqB3Freq, "eq.b3.freq", "EQ Band 3 Freq", "EQ", Float, 20.0f, 20000.0f, 1000.0f, 1.0f, Exp, Hz, nullptr, 0) \
  X(EqB3Gain, "eq.b3.gain", "EQ Band 3 Gain", "EQ", Float, -18.0f, 18.0f, 0.0f, 0.1f, Linear, dB, nullptr, 0) \
  X(EqB3Q, "eq.b3.q", "EQ Band 3 Q", "EQ", Float, 0.1f, 10.0f, 1.0f, 0.01f, Exp, None, nullptr, 0) \
  X(EqB4On, "eq.b4.on", "EQ Band 4 On", "EQ", Bool, 0.0f, 1.0f, 1.0f, 1.0f, Linear, None, nullptr, 0) \
  X(EqB4Type, "eq.b4.type", "EQ Band 4 Type", "EQ", Enum, 0.0f, 3.0f, 0.0f, 1.0f, Linear, None, labels::kEqType, TSL_N(labels::kEqType)) \
  X(EqB4Freq, "eq.b4.freq", "EQ Band 4 Freq", "EQ", Float, 20.0f, 20000.0f, 3000.0f, 1.0f, Exp, Hz, nullptr, 0) \
  X(EqB4Gain, "eq.b4.gain", "EQ Band 4 Gain", "EQ", Float, -18.0f, 18.0f, 0.0f, 0.1f, Linear, dB, nullptr, 0) \
  X(EqB4Q, "eq.b4.q", "EQ Band 4 Q", "EQ", Float, 0.1f, 10.0f, 1.0f, 0.01f, Exp, None, nullptr, 0) \
  X(EqB5On, "eq.b5.on", "EQ Band 5 On", "EQ", Bool, 0.0f, 1.0f, 1.0f, 1.0f, Linear, None, nullptr, 0) \
  X(EqB5Type, "eq.b5.type", "EQ Band 5 Type", "EQ", Enum, 0.0f, 3.0f, 2.0f, 1.0f, Linear, None, labels::kEqType, TSL_N(labels::kEqType)) \
  X(EqB5Freq, "eq.b5.freq", "EQ Band 5 Freq", "EQ", Float, 20.0f, 20000.0f, 8000.0f, 1.0f, Exp, Hz, nullptr, 0) \
  X(EqB5Gain, "eq.b5.gain", "EQ Band 5 Gain", "EQ", Float, -18.0f, 18.0f, 0.0f, 0.1f, Linear, dB, nullptr, 0) \
  X(EqB5Q, "eq.b5.q", "EQ Band 5 Q", "EQ", Float, 0.1f, 10.0f, 0.7f, 0.01f, Exp, None, nullptr, 0) \
  X(EqOutput, "eq.output", "EQ Output", "EQ", Float, -12.0f, 12.0f, 0.0f, 0.1f, Linear, dB, nullptr, 0) \
  X(DlyOn, "dly.on", "Delay On", "Delay", Bool, 0.0f, 1.0f, 0.0f, 1.0f, Linear, None, nullptr, 0) \
  X(DlyMode, "dly.mode", "Delay Mode", "Delay", Enum, 0.0f, 4.0f, 0.0f, 1.0f, Linear, None, labels::kDelayMode, TSL_N(labels::kDelayMode)) \
  X(DlyTime, "dly.time", "Delay Time", "Delay", Float, 1.0f, 2000.0f, 380.0f, 0.1f, Exp, Ms, nullptr, 0) \
  X(DlySync, "dly.sync", "Delay Sync", "Delay", Bool, 0.0f, 1.0f, 0.0f, 1.0f, Linear, None, nullptr, 0) \
  X(DlyDivision, "dly.division", "Delay Division", "Delay", Enum, 0.0f, 13.0f, 5.0f, 1.0f, Linear, None, labels::kDivision, TSL_N(labels::kDivision)) \
  X(DlyFeedback, "dly.feedback", "Delay Feedback", "Delay", Float, 0.0f, 100.0f, 35.0f, 0.1f, Linear, Percent, nullptr, 0) \
  X(DlyMix, "dly.mix", "Delay Mix", "Delay", Float, 0.0f, 100.0f, 25.0f, 0.1f, Linear, Percent, nullptr, 0) \
  X(DlyLowCut, "dly.lowCut", "Delay Low Cut", "Delay", Float, 20.0f, 1000.0f, 80.0f, 1.0f, Exp, Hz, nullptr, 0) \
  X(DlyHighCut, "dly.highCut", "Delay High Cut", "Delay", Float, 1000.0f, 20000.0f, 7000.0f, 1.0f, Exp, Hz, nullptr, 0) \
  X(DlySaturation, "dly.saturation", "Delay Saturation", "Delay", Float, 0.0f, 10.0f, 2.0f, 0.01f, Linear, None, nullptr, 0) \
  X(DlyModDepth, "dly.modDepth", "Delay Mod Depth", "Delay", Float, 0.0f, 100.0f, 15.0f, 0.1f, Linear, Percent, nullptr, 0) \
  X(DlyModRate, "dly.modRate", "Delay Mod Rate", "Delay", Float, 0.05f, 8.0f, 0.6f, 0.01f, Exp, Hz, nullptr, 0) \
  X(DlyWidth, "dly.width", "Delay Width", "Delay", Float, 0.0f, 100.0f, 60.0f, 0.1f, Linear, Percent, nullptr, 0) \
  X(RvbOn, "rvb.on", "Reverb On", "Reverb", Bool, 0.0f, 1.0f, 0.0f, 1.0f, Linear, None, nullptr, 0) \
  X(RvbMode, "rvb.mode", "Reverb Mode", "Reverb", Enum, 0.0f, 4.0f, 1.0f, 1.0f, Linear, None, labels::kReverbMode, TSL_N(labels::kReverbMode)) \
  X(RvbMix, "rvb.mix", "Reverb Mix", "Reverb", Float, 0.0f, 100.0f, 22.0f, 0.1f, Linear, Percent, nullptr, 0) \
  X(RvbDecay, "rvb.decay", "Reverb Decay", "Reverb", Float, 0.1f, 20.0f, 2.5f, 0.01f, Exp, Seconds, nullptr, 0) \
  X(RvbSize, "rvb.size", "Reverb Size", "Reverb", Float, 0.0f, 100.0f, 60.0f, 0.1f, Linear, Percent, nullptr, 0) \
  X(RvbPreDelay, "rvb.preDelay", "Reverb Pre-delay", "Reverb", Float, 0.0f, 250.0f, 20.0f, 0.1f, Linear, Ms, nullptr, 0) \
  X(RvbEarly, "rvb.early", "Reverb Early", "Reverb", Float, 0.0f, 100.0f, 40.0f, 0.1f, Linear, Percent, nullptr, 0) \
  X(RvbDiffusion, "rvb.diffusion", "Reverb Diffusion", "Reverb", Float, 0.0f, 100.0f, 75.0f, 0.1f, Linear, Percent, nullptr, 0) \
  X(RvbHighDamp, "rvb.highDamp", "Reverb High Damp", "Reverb", Float, 1000.0f, 20000.0f, 6000.0f, 1.0f, Exp, Hz, nullptr, 0) \
  X(RvbLowCut, "rvb.lowCut", "Reverb Low Cut", "Reverb", Float, 20.0f, 1000.0f, 120.0f, 1.0f, Exp, Hz, nullptr, 0) \
  X(RvbModulation, "rvb.modulation", "Reverb Modulation", "Reverb", Float, 0.0f, 100.0f, 30.0f, 0.1f, Linear, Percent, nullptr, 0) \
  X(RvbWidth, "rvb.width", "Reverb Width", "Reverb", Float, 0.0f, 100.0f, 100.0f, 0.1f, Linear, Percent, nullptr, 0) \
  X(RvbFreeze, "rvb.freeze", "Reverb Freeze", "Reverb", Bool, 0.0f, 1.0f, 0.0f, 1.0f, Linear, None, nullptr, 0) \
  X(RvbShimmer, "rvb.shimmer", "Reverb Shimmer", "Reverb", Float, 0.0f, 100.0f, 40.0f, 0.1f, Linear, Percent, nullptr, 0) \
  X(ChoOn, "cho.on", "Chorus On", "Chorus", Bool, 0.0f, 1.0f, 0.0f, 1.0f, Linear, None, nullptr, 0) \
  X(ChoRate, "cho.rate", "Chorus Rate", "Chorus", Float, 0.05f, 10.0f, 0.8f, 0.01f, Exp, Hz, nullptr, 0) \
  X(ChoDepth, "cho.depth", "Chorus Depth", "Chorus", Float, 0.0f, 100.0f, 45.0f, 0.1f, Linear, Percent, nullptr, 0) \
  X(ChoMix, "cho.mix", "Chorus Mix", "Chorus", Float, 0.0f, 100.0f, 50.0f, 0.1f, Linear, Percent, nullptr, 0) \
  X(ChoWidth, "cho.width", "Chorus Width", "Chorus", Float, 0.0f, 100.0f, 80.0f, 0.1f, Linear, Percent, nullptr, 0) \
  X(ChoTone, "cho.tone", "Chorus Tone", "Chorus", Float, 1000.0f, 20000.0f, 9000.0f, 1.0f, Exp, Hz, nullptr, 0) \
  X(ChoSync, "cho.sync", "Chorus Sync", "Chorus", Bool, 0.0f, 1.0f, 0.0f, 1.0f, Linear, None, nullptr, 0) \
  X(ChoDivision, "cho.division", "Chorus Division", "Chorus", Enum, 0.0f, 13.0f, 1.0f, 1.0f, Linear, None, labels::kDivision, TSL_N(labels::kDivision)) \
  X(PhaOn, "pha.on", "Phaser On", "Phaser", Bool, 0.0f, 1.0f, 0.0f, 1.0f, Linear, None, nullptr, 0) \
  X(PhaRate, "pha.rate", "Phaser Rate", "Phaser", Float, 0.02f, 10.0f, 0.4f, 0.01f, Exp, Hz, nullptr, 0) \
  X(PhaDepth, "pha.depth", "Phaser Depth", "Phaser", Float, 0.0f, 100.0f, 70.0f, 0.1f, Linear, Percent, nullptr, 0) \
  X(PhaFeedback, "pha.feedback", "Phaser Feedback", "Phaser", Float, 0.0f, 95.0f, 40.0f, 0.1f, Linear, Percent, nullptr, 0) \
  X(PhaStages, "pha.stages", "Phaser Stages", "Phaser", Enum, 0.0f, 3.0f, 1.0f, 1.0f, Linear, None, labels::kPhaserStages, TSL_N(labels::kPhaserStages)) \
  X(PhaMix, "pha.mix", "Phaser Mix", "Phaser", Float, 0.0f, 100.0f, 50.0f, 0.1f, Linear, Percent, nullptr, 0) \
  X(PhaStereo, "pha.stereo", "Phaser Stereo", "Phaser", Float, 0.0f, 180.0f, 90.0f, 1.0f, Linear, Degrees, nullptr, 0) \
  X(PhaSync, "pha.sync", "Phaser Sync", "Phaser", Bool, 0.0f, 1.0f, 0.0f, 1.0f, Linear, None, nullptr, 0) \
  X(PhaDivision, "pha.division", "Phaser Division", "Phaser", Enum, 0.0f, 13.0f, 0.0f, 1.0f, Linear, None, labels::kDivision, TSL_N(labels::kDivision)) \
  X(FlaOn, "fla.on", "Flanger On", "Flanger", Bool, 0.0f, 1.0f, 0.0f, 1.0f, Linear, None, nullptr, 0) \
  X(FlaRate, "fla.rate", "Flanger Rate", "Flanger", Float, 0.02f, 10.0f, 0.25f, 0.01f, Exp, Hz, nullptr, 0) \
  X(FlaDepth, "fla.depth", "Flanger Depth", "Flanger", Float, 0.0f, 100.0f, 60.0f, 0.1f, Linear, Percent, nullptr, 0) \
  X(FlaManual, "fla.manual", "Flanger Manual", "Flanger", Float, 0.1f, 10.0f, 2.0f, 0.01f, Exp, Ms, nullptr, 0) \
  X(FlaFeedback, "fla.feedback", "Flanger Feedback", "Flanger", Float, -95.0f, 95.0f, 50.0f, 0.1f, Linear, Percent, nullptr, 0) \
  X(FlaMix, "fla.mix", "Flanger Mix", "Flanger", Float, 0.0f, 100.0f, 50.0f, 0.1f, Linear, Percent, nullptr, 0) \
  X(FlaSync, "fla.sync", "Flanger Sync", "Flanger", Bool, 0.0f, 1.0f, 0.0f, 1.0f, Linear, None, nullptr, 0) \
  X(FlaDivision, "fla.division", "Flanger Division", "Flanger", Enum, 0.0f, 13.0f, 0.0f, 1.0f, Linear, None, labels::kDivision, TSL_N(labels::kDivision)) \
  X(TrmOn, "trm.on", "Tremolo On", "Tremolo", Bool, 0.0f, 1.0f, 0.0f, 1.0f, Linear, None, nullptr, 0) \
  X(TrmRate, "trm.rate", "Tremolo Rate", "Tremolo", Float, 0.1f, 20.0f, 5.0f, 0.01f, Exp, Hz, nullptr, 0) \
  X(TrmDepth, "trm.depth", "Tremolo Depth", "Tremolo", Float, 0.0f, 100.0f, 50.0f, 0.1f, Linear, Percent, nullptr, 0) \
  X(TrmShape, "trm.shape", "Tremolo Shape", "Tremolo", Enum, 0.0f, 2.0f, 0.0f, 1.0f, Linear, None, labels::kTremShape, TSL_N(labels::kTremShape)) \
  X(TrmStereo, "trm.stereo", "Tremolo Stereo Pan", "Tremolo", Bool, 0.0f, 1.0f, 0.0f, 1.0f, Linear, None, nullptr, 0) \
  X(TrmSync, "trm.sync", "Tremolo Sync", "Tremolo", Bool, 0.0f, 1.0f, 0.0f, 1.0f, Linear, None, nullptr, 0) \
  X(TrmDivision, "trm.division", "Tremolo Division", "Tremolo", Enum, 0.0f, 13.0f, 7.0f, 1.0f, Linear, None, labels::kDivision, TSL_N(labels::kDivision)) \
  X(VibOn, "vib.on", "Vibrato On", "Vibrato", Bool, 0.0f, 1.0f, 0.0f, 1.0f, Linear, None, nullptr, 0) \
  X(VibRate, "vib.rate", "Vibrato Rate", "Vibrato", Float, 0.1f, 12.0f, 5.0f, 0.01f, Exp, Hz, nullptr, 0) \
  X(VibDepth, "vib.depth", "Vibrato Depth", "Vibrato", Float, 0.0f, 100.0f, 30.0f, 0.1f, Linear, Percent, nullptr, 0) \
  X(VibSync, "vib.sync", "Vibrato Sync", "Vibrato", Bool, 0.0f, 1.0f, 0.0f, 1.0f, Linear, None, nullptr, 0) \
  X(VibDivision, "vib.division", "Vibrato Division", "Vibrato", Enum, 0.0f, 13.0f, 7.0f, 1.0f, Linear, None, labels::kDivision, TSL_N(labels::kDivision)) \
  X(RotOn, "rot.on", "Rotary On", "Rotary", Bool, 0.0f, 1.0f, 0.0f, 1.0f, Linear, None, nullptr, 0) \
  X(RotSpeed, "rot.speed", "Rotary Speed", "Rotary", Enum, 0.0f, 2.0f, 0.0f, 1.0f, Linear, None, labels::kRotarySpeed, TSL_N(labels::kRotarySpeed)) \
  X(RotDrive, "rot.drive", "Rotary Drive", "Rotary", Float, 0.0f, 10.0f, 3.0f, 0.01f, Linear, None, nullptr, 0) \
  X(RotBalance, "rot.balance", "Rotary Horn/Drum", "Rotary", Float, -100.0f, 100.0f, 0.0f, 0.1f, Linear, Percent, nullptr, 0) \
  X(RotMix, "rot.mix", "Rotary Mix", "Rotary", Float, 0.0f, 100.0f, 100.0f, 0.1f, Linear, Percent, nullptr, 0) \
  X(RotWidth, "rot.width", "Rotary Width", "Rotary", Float, 0.0f, 100.0f, 80.0f, 0.1f, Linear, Percent, nullptr, 0) \
  X(EnvOn, "env.on", "Env Filter On", "Envelope Filter", Bool, 0.0f, 1.0f, 0.0f, 1.0f, Linear, None, nullptr, 0) \
  X(EnvMode, "env.mode", "Env Filter Mode", "Envelope Filter", Enum, 0.0f, 2.0f, 1.0f, 1.0f, Linear, None, labels::kEnvMode, TSL_N(labels::kEnvMode)) \
  X(EnvSensitivity, "env.sensitivity", "Env Filter Sensitivity", "Envelope Filter", Float, 0.0f, 10.0f, 5.0f, 0.01f, Linear, None, nullptr, 0) \
  X(EnvRangeLow, "env.rangeLow", "Env Filter Range Low", "Envelope Filter", Float, 100.0f, 1000.0f, 250.0f, 1.0f, Exp, Hz, nullptr, 0) \
  X(EnvRangeHigh, "env.rangeHigh", "Env Filter Range High", "Envelope Filter", Float, 1000.0f, 6000.0f, 2500.0f, 1.0f, Exp, Hz, nullptr, 0) \
  X(EnvQ, "env.q", "Env Filter Q", "Envelope Filter", Float, 0.5f, 12.0f, 4.0f, 0.01f, Exp, None, nullptr, 0) \
  X(EnvAttack, "env.attack", "Env Filter Attack", "Envelope Filter", Float, 1.0f, 100.0f, 8.0f, 0.1f, Exp, Ms, nullptr, 0) \
  X(EnvRelease, "env.release", "Env Filter Release", "Envelope Filter", Float, 20.0f, 1000.0f, 150.0f, 0.1f, Exp, Ms, nullptr, 0) \
  X(EnvDirection, "env.direction", "Env Filter Direction", "Envelope Filter", Enum, 0.0f, 1.0f, 0.0f, 1.0f, Linear, None, labels::kEnvDirection, TSL_N(labels::kEnvDirection)) \
  X(EnvMix, "env.mix", "Env Filter Mix", "Envelope Filter", Float, 0.0f, 100.0f, 100.0f, 0.1f, Linear, Percent, nullptr, 0)

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
