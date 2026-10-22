// SPDX-License-Identifier: GPL-3.0-or-later
// Педали усиления/искажения: Boost, Overdrive, Distortion, Fuzz.
// Все нелинейности работают в оверсэмплинге (IIR halfband, без lookahead).
#pragma once

#include "../Audio/Module.h"
#include "../DSP/Biquad.h"
#include "../DSP/Oversampler.h"
#include "../DSP/Smoother.h"

namespace tsl {

/** Состояние одного канала педали с оверсэмплингом. */
struct DriveChannel
{
  Oversampler os;
  Biquad pre1, pre2, post1, post2, tone;
  OnePole dc;
  float state = 0.f;
  void reset() noexcept
  {
    os.reset();
    pre1.reset(); pre2.reset(); post1.reset(); post2.reset(); tone.reset();
    dc.reset();
    state = 0.f;
  }
};

class Boost final : public Module
{
public:
  Boost() noexcept : Module(ModuleId::Boost) {}
  void prepare(const PrepareInfo& info) override;
  void reset() noexcept override;
  void update(const ParamValues& p) noexcept override;
  void process(AudioBlock& block) noexcept override;

private:
  void processChannel(DriveChannel& c, float* x, int n, bool advanceSmoother) noexcept;
  DriveChannel mCh[2];
  LinearSmoother mGain;
  float mTight = -1.f, mBright = -100.f;
  float mBlockGain = 1.f;
};

class Overdrive final : public Module
{
public:
  Overdrive() noexcept : Module(ModuleId::Overdrive) {}
  void prepare(const PrepareInfo& info) override;
  void reset() noexcept override;
  void update(const ParamValues& p) noexcept override;
  void process(AudioBlock& block) noexcept override;

private:
  void designFilters() noexcept;
  void processChannel(DriveChannel& c, float* x, int n) noexcept;
  DriveChannel mCh[2];
  LinearSmoother mDrive, mLevel, mMix;
  int mMode = -1;
  float mTone = -1.f;
  float mBlockDrive = 1.f, mBlockLevel = 1.f, mBlockMix = 1.f;
};

class Distortion final : public Module
{
public:
  Distortion() noexcept : Module(ModuleId::Distortion) {}
  void prepare(const PrepareInfo& info) override;
  void reset() noexcept override;
  void update(const ParamValues& p) noexcept override;
  void process(AudioBlock& block) noexcept override;

private:
  void designFilters() noexcept;
  void processChannel(DriveChannel& c, float* x, int n) noexcept;
  DriveChannel mCh[2];
  LinearSmoother mDrive, mLevel;
  int mMode = 0;
  float mTone = -1.f, mTight = -1.f, mDriveKnob = -1.f, mGbwA = 0.f;
  float mBlockDrive = 1.f, mBlockLevel = 1.f;
};

class Fuzz final : public Module
{
public:
  Fuzz() noexcept : Module(ModuleId::Fuzz) {}
  void prepare(const PrepareInfo& info) override;
  void reset() noexcept override;
  void update(const ParamValues& p) noexcept override;
  void process(AudioBlock& block) noexcept override;

private:
  void designFilters() noexcept;
  void processChannel(DriveChannel& c, float* x, int n) noexcept;
  DriveChannel mCh[2];
  LinearSmoother mGain, mLevel, mBias;
  int mMode = 0;
  float mTone = -1.f;
  float mBlockGain = 1.f, mBlockLevel = 1.f, mBlockBias = 0.f;
};

} // namespace tsl
