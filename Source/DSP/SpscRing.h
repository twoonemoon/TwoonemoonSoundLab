// SPDX-License-Identifier: GPL-3.0-or-later
// Lock-free кольцевой буфер «один писатель — один читатель» для передачи
// аудиоданных из audio thread в UI (тюнер, анализатор). Writer никогда не ждёт:
// при переполнении новые данные отбрасываются.
#pragma once

#include <atomic>
#include <cstddef>
#include <vector>

namespace tsl {

template <typename T>
class SpscRing
{
public:
  /** Не для audio thread. capacity округляется до степени двойки. */
  void prepare(size_t capacity)
  {
    size_t c = 1;
    while (c < capacity) c <<= 1;
    mData.assign(c, T{});
    mMask = c - 1;
    mWrite.store(0);
    mRead.store(0);
  }

  size_t capacity() const noexcept { return mData.size(); }

  /** Writer (audio thread). Возвращает число записанных элементов. */
  size_t push(const T* src, size_t n) noexcept
  {
    if (mData.empty()) return 0;
    const size_t w = mWrite.load(std::memory_order_relaxed);
    const size_t r = mRead.load(std::memory_order_acquire);
    const size_t freeSpace = mData.size() - (w - r);
    const size_t count = n < freeSpace ? n : freeSpace;
    for (size_t i = 0; i < count; ++i) mData[(w + i) & mMask] = src[i];
    mWrite.store(w + count, std::memory_order_release);
    return count;
  }

  size_t available() const noexcept
  {
    return mWrite.load(std::memory_order_acquire) - mRead.load(std::memory_order_relaxed);
  }

  /** Reader (UI thread). */
  size_t pop(T* dst, size_t n) noexcept
  {
    if (mData.empty()) return 0;
    const size_t r = mRead.load(std::memory_order_relaxed);
    const size_t w = mWrite.load(std::memory_order_acquire);
    const size_t avail = w - r;
    const size_t count = n < avail ? n : avail;
    for (size_t i = 0; i < count; ++i) dst[i] = mData[(r + i) & mMask];
    mRead.store(r + count, std::memory_order_release);
    return count;
  }

  /** Reader: отбросить устаревшие данные, оставив не более keep элементов. */
  void discardOldest(size_t keep) noexcept
  {
    const size_t r = mRead.load(std::memory_order_relaxed);
    const size_t w = mWrite.load(std::memory_order_acquire);
    if (w - r > keep) mRead.store(w - keep, std::memory_order_release);
  }

private:
  std::vector<T> mData;
  size_t mMask = 0;
  alignas(64) std::atomic<size_t> mWrite{ 0 };
  alignas(64) std::atomic<size_t> mRead{ 0 };
};

} // namespace tsl
