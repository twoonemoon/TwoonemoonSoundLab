// SPDX-License-Identifier: GPL-3.0-or-later
// Модуляционные эффекты — у каждого собственный алгоритм (не «один эффект под семью именами»):
//  Chorus   — две модулированные линии задержки (стерео-разнесённые фазы LFO), тон-фильтр;
//  Flanger  — короткая задержка с ОС (±), ручка Manual задаёт базовое время;
//  Phaser   — цепочка allpass первого порядка (TPT) с ОС и стерео-сдвигом фаз LFO;
//  Tremolo  — амплитудная модуляция (sine/triangle/square), режим стерео-панорамы;
//  Vibrato  — 100% модулированная задержка (частотная модуляция);
//  Rotary   — разделение полос (рупор/барабан), доплер + амплитудная модуляция, инерция разгона;
//  EnvFilter— огибающая управляет частотой SVF (LP/BP/HP, вверх/вниз).
#pragma once

#include "../Audio/Module.h"
#include "../DSP/Biquad.h"
#include "../DSP/DelayLine.h"
#include "../DSP/Lfo.h"
#include "../DSP/Smoother.h"
#include "../DSP/Svf.h"

namespace tsl {

/** Общая часть: темп для синхронизации LFO. */
struct TempoState
{
  bool hostTempo = true;
  double manual = 120.0;
  double tempo(const TransportInfo* t) const noexcept
  {
    return (hostTempo && t && t->hostTempoValid && t->tempo > 1.0) ? t->tempo : manual;
  }
  void read(const ParamValues& p) noexcept
  {
    hostTempo = p[kHostTempo] >= 0.5f;
    manual = p[kTempo];
  }
};

class Chorus final : public Module
{
public:
  Chorus() noexcept : Module(ModuleId::Chorus) {}
  void prepare(const PrepareInfo& info) override;
  void reset() noexcept override;
  void update(const ParamValues& p) noexcept override;
  void process(AudioBlock& block) noexcept override;

private:
  DelayLine mLine[2];
  Lfo mLfo;
  Biquad mTone[2];
  float mRate = 0.8f, mDepth = 0.45f, mWidth = 0.8f, mToneHz = 9000.f;
  bool mSync = false;
  int mDiv = 1;
  TempoState mTempo;
  LinearSmoother mMix;
};

class Flanger final : public Module
{
public:
  Flanger() noexcept : Module(ModuleId::Flanger) {}
  void prepare(const PrepareInfo& info) override;
  void reset() noexcept override;
  void update(const ParamValues& p) noexcept override;
  void process(AudioBlock& block) noexcept override;

private:
  DelayLine mLine[2];
  Lfo mLfo;
  float mFb[2]{};
  float mRate = 0.25f, mDepth = 0.6f, mFeedback = 0.5f;
  OnePoleSmoother mManual;
  bool mSync = false;
  int mDiv = 0;
  TempoState mTempo;
  LinearSmoother mMix;
};

class Phaser final : public Module
{
public:
  static constexpr int kMaxStages = 12;
  Phaser() noexcept : Module(ModuleId::Phaser) {}
  void prepare(const PrepareInfo& info) override;
  void reset() noexcept override;
  void update(const ParamValues& p) noexcept override;
  void process(AudioBlock& block) noexcept override;

private:
  float mState[2][kMaxStages]{};
  float mFbState[2]{};
  Lfo mLfo;
  int mStages = 6;
  float mRate = 0.4f, mDepth = 0.7f, mFeedback = 0.4f, mStereo = 0.25f;
  bool mSync = false;
  int mDiv = 0;
  TempoState mTempo;
  LinearSmoother mMix;
};

class Tremolo final : public Module
{
public:
  Tremolo() noexcept : Module(ModuleId::Tremolo) {}
  void prepare(const PrepareInfo& info) override;
  void reset() noexcept override;
  void update(const ParamValues& p) noexcept override;
  void process(AudioBlock& block) noexcept override;

private:
  Lfo mLfo;
  float mRate = 5.f;
  LinearSmoother mDepth;
  int mShape = 0;
  bool mStereo = false, mSync = false;
  int mDiv = 7;
  TempoState mTempo;
};

class Vibrato final : public Module
{
public:
  Vibrato() noexcept : Module(ModuleId::Vibrato) {}
  void prepare(const PrepareInfo& info) override;
  void reset() noexcept override;
  void update(const ParamValues& p) noexcept override;
  void process(AudioBlock& block) noexcept override;

private:
  DelayLine mLine[2];
  Lfo mLfo;
  float mRate = 5.f;
  LinearSmoother mDepth;
  bool mSync = false;
  int mDiv = 7;
  TempoState mTempo;
};

class Rotary final : public Module
{
public:
  Rotary() noexcept : Module(ModuleId::Rotary) {}
  void prepare(const PrepareInfo& info) override;
  void reset() noexcept override;
  void update(const ParamValues& p) noexcept override;
  void process(AudioBlock& block) noexcept override;

private:
  Biquad mXoverLp, mXoverHp, mXoverLp2, mXoverHp2;
  DelayLine mHornLine, mDrumLine;
  float mHornPhase = 0.f, mDrumPhase = 0.f;
  float mHornHz = 0.8f, mDrumHz = 0.7f;
  float mHornTarget = 0.8f, mDrumTarget = 0.7f;
  float mDrive = 0.3f, mBalance = 0.f, mWidth = 0.8f;
  LinearSmoother mMix;
};

class EnvFilter final : public Module
{
public:
  EnvFilter() noexcept : Module(ModuleId::EnvFilter) {}
  void prepare(const PrepareInfo& info) override;
  void reset() noexcept override;
  void update(const ParamValues& p) noexcept override;
  void process(AudioBlock& block) noexcept override;

private:
  Svf mSvf[2];
  float mEnv = 0.f;
  int mMode = 1;
  bool mDown = false;
  float mSens = 0.5f, mLow = 250.f, mHigh = 2500.f, mQ = 4.f;
  float mAtt = 0.f, mRel = 0.f;
  OnePoleSmoother mFreq;
  LinearSmoother mMix;
};

} // namespace tsl
