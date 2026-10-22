// SPDX-License-Identifier: GPL-3.0-or-later
// MIDI Learn и педаль экспрессии: таблица привязок MIDI CC → параметр (с диапазоном и инверсией).
// Аудиопоток читает таблицу и обучается без блокировок (атомики); UI редактирует таблицу.
#pragma once

#include "Params.h"

#include <array>
#include <atomic>

namespace tsl {

class MidiMap
{
public:
  static constexpr int kSlots = 32;

  struct Mapping
  {
    int cc = -1;      // 0…127, −1 — пусто
    int channel = -1; // 0…15, −1 — любой канал
    int param = -1;
    float lo = 0.f, hi = 1.f; // нормированный диапазон (lo > hi — инверсия)
  };

  MidiMap() { clear(); }

  void clear() noexcept
  {
    for (auto& s : mSlots) s.cc.store(-1, std::memory_order_relaxed);
    mVersion.fetch_add(1);
  }

  /** UI: начать обучение для параметра (следующий CC будет привязан к нему). −1 — отменить. */
  void startLearn(int param) noexcept { mLearn.store(param, std::memory_order_release); }
  int learnTarget() const noexcept { return mLearn.load(std::memory_order_acquire); }

  /**
   * Аудиопоток: обработать CC. При обучении — привязать; иначе вызвать apply(param, normalized)
   * для всех совпавших привязок. Возвращает true, если сообщение использовано.
   */
  template <typename ApplyFn>
  bool handleCC(int channel, int cc, int value, ApplyFn&& apply) noexcept
  {
    if (cc < 0 || cc > 127) return false;
    const int learn = mLearn.load(std::memory_order_acquire);
    if (learn >= 0)
    {
      assign(learn, cc, -1, 0.f, 1.f);
      mLearn.store(-1, std::memory_order_release);
      return true;
    }
    bool used = false;
    const float v = static_cast<float>(value) / 127.f;
    for (auto& s : mSlots)
    {
      const int scc = s.cc.load(std::memory_order_acquire);
      if (scc != cc) continue;
      const int ch = s.channel.load(std::memory_order_relaxed);
      if (ch >= 0 && ch != channel) continue;
      const float lo = s.lo.load(std::memory_order_relaxed), hi = s.hi.load(std::memory_order_relaxed);
      apply(s.param.load(std::memory_order_relaxed), lo + (hi - lo) * v);
      used = true;
    }
    return used;
  }

  /** Назначить привязку (заменяет существующую для того же параметра). Любой поток. */
  void assign(int param, int cc, int channel, float lo, float hi) noexcept
  {
    if (param < 0 || param >= kNumParams) return;
    int target = -1;
    for (int i = 0; i < kSlots; ++i)
      if (mSlots[static_cast<size_t>(i)].cc.load() >= 0 && mSlots[static_cast<size_t>(i)].param.load() == param) target = i;
    if (target < 0)
      for (int i = 0; i < kSlots; ++i)
        if (mSlots[static_cast<size_t>(i)].cc.load() < 0) { target = i; break; }
    if (target < 0) target = kSlots - 1; // таблица заполнена — заменяем последнюю
    Slot& s = mSlots[static_cast<size_t>(target)];
    s.cc.store(-1, std::memory_order_release); // сначала «выключить» слот, затем переписать поля
    s.param.store(param, std::memory_order_relaxed);
    s.channel.store(channel, std::memory_order_relaxed);
    s.lo.store(lo, std::memory_order_relaxed);
    s.hi.store(hi, std::memory_order_relaxed);
    s.cc.store(cc, std::memory_order_release);
    mVersion.fetch_add(1);
  }

  void removeForParam(int param) noexcept
  {
    for (auto& s : mSlots)
      if (s.cc.load() >= 0 && s.param.load() == param) s.cc.store(-1, std::memory_order_release);
    mVersion.fetch_add(1);
  }

  /** Привязка параметра (cc = −1, если нет). */
  Mapping mappingFor(int param) const noexcept
  {
    for (const auto& s : mSlots)
      if (s.cc.load() >= 0 && s.param.load() == param) return read(s);
    return {};
  }

  Mapping slot(int i) const noexcept { return read(mSlots[static_cast<size_t>(i)]); }
  void setRange(int param, float lo, float hi) noexcept
  {
    for (auto& s : mSlots)
      if (s.cc.load() >= 0 && s.param.load() == param) { s.lo.store(lo); s.hi.store(hi); }
    mVersion.fetch_add(1);
  }

  /** Счётчик изменений — UI перерисовывает индикаторы привязок. */
  uint32_t version() const noexcept { return mVersion.load(std::memory_order_acquire); }

private:
  struct Slot
  {
    std::atomic<int> cc{ -1 }, channel{ -1 }, param{ -1 };
    std::atomic<float> lo{ 0.f }, hi{ 1.f };
  };
  static Mapping read(const Slot& s) noexcept
  {
    Mapping m;
    m.cc = s.cc.load();
    m.channel = s.channel.load();
    m.param = s.param.load();
    m.lo = s.lo.load();
    m.hi = s.hi.load();
    return m;
  }
  std::array<Slot, kSlots> mSlots;
  std::atomic<int> mLearn{ -1 };
  std::atomic<uint32_t> mVersion{ 0 };
};

} // namespace tsl
