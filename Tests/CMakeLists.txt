# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(tsl_tests
  TestMain.cpp
  Unit/TestDspCore.cpp
  Unit/TestEffects.cpp
  Unit/TestAmp.cpp
)
target_link_libraries(tsl_tests PRIVATE TwoonemoonCore)
target_compile_options(tsl_tests PRIVATE -Wall -Wno-unused-parameter)
add_test(NAME tsl_unit_tests COMMAND tsl_tests)
