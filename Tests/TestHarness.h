// SPDX-License-Identifier: GPL-3.0-or-later
// Минимальный тестовый фреймворк без внешних зависимостей.
#pragma once

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace tsltest {

struct TestCase
{
  const char* name;
  std::function<void()> fn;
};

inline std::vector<TestCase>& registry()
{
  static std::vector<TestCase> r;
  return r;
}

struct Registrar
{
  Registrar(const char* name, std::function<void()> fn) { registry().push_back({ name, std::move(fn) }); }
};

struct State
{
  int checks = 0;
  int failures = 0;
  const char* current = "";
};

inline State& state()
{
  static State s;
  return s;
}

inline void fail(const char* file, int line, const std::string& msg)
{
  state().failures++;
  std::printf("  FAIL [%s] %s:%d: %s\n", state().current, file, line, msg.c_str());
}

} // namespace tsltest

#define TSL_CONCAT2(a, b) a##b
#define TSL_CONCAT(a, b) TSL_CONCAT2(a, b)
#define TEST_CASE(name)                                                                                    \
  static void TSL_CONCAT(tslTest_, __LINE__)();                                                            \
  static tsltest::Registrar TSL_CONCAT(tslReg_, __LINE__)(name, TSL_CONCAT(tslTest_, __LINE__));           \
  static void TSL_CONCAT(tslTest_, __LINE__)()

#define CHECK(cond)                                                                                        \
  do                                                                                                       \
  {                                                                                                        \
    tsltest::state().checks++;                                                                             \
    if (!(cond)) tsltest::fail(__FILE__, __LINE__, #cond);                                                 \
  } while (0)

#define CHECK_MSG(cond, ...)                                                                               \
  do                                                                                                       \
  {                                                                                                        \
    tsltest::state().checks++;                                                                             \
    if (!(cond))                                                                                           \
    {                                                                                                      \
      char buf_[512];                                                                                      \
      std::snprintf(buf_, sizeof(buf_), __VA_ARGS__);                                                      \
      tsltest::fail(__FILE__, __LINE__, std::string(#cond) + " — " + buf_);                                \
    }                                                                                                      \
  } while (0)

#define CHECK_NEAR(a, b, tol)                                                                              \
  do                                                                                                       \
  {                                                                                                        \
    tsltest::state().checks++;                                                                             \
    const double va_ = static_cast<double>(a), vb_ = static_cast<double>(b);                               \
    if (!(std::fabs(va_ - vb_) <= (tol)))                                                                  \
    {                                                                                                      \
      char buf_[256];                                                                                      \
      std::snprintf(buf_, sizeof(buf_), "%s = %.6g, %s = %.6g, tol %.3g", #a, va_, #b, vb_, (double)(tol)); \
      tsltest::fail(__FILE__, __LINE__, buf_);                                                             \
    }                                                                                                      \
  } while (0)
