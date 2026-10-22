// SPDX-License-Identifier: GPL-3.0-or-later
#include "TestHarness.h"

#include <cstring>

int main(int argc, char** argv)
{
  const char* filter = argc > 1 ? argv[1] : nullptr;
  int run = 0;
  for (auto& t : tsltest::registry())
  {
    if (filter && !std::strstr(t.name, filter)) continue;
    tsltest::state().current = t.name;
    const int before = tsltest::state().failures;
    t.fn();
    ++run;
    std::printf("%s %s\n", tsltest::state().failures == before ? "[ OK ]" : "[FAIL]", t.name);
  }
  std::printf("\n%d tests, %d checks, %d failures\n", run, tsltest::state().checks, tsltest::state().failures);
  return tsltest::state().failures == 0 ? 0 : 1;
}
