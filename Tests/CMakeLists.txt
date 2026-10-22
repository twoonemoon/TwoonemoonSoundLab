# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(tsl_tests
  TestMain.cpp
  Unit/TestDspCore.cpp
  Unit/TestEffects.cpp
  Unit/TestAmp.cpp
  Unit/TestCabinet.cpp
  Unit/TestTuner.cpp
  Unit/TestFx.cpp
  Unit/TestPitchFx.cpp
  Unit/TestPresets.cpp
)
target_link_libraries(tsl_tests PRIVATE TwoonemoonCore)
target_compile_options(tsl_tests PRIVATE -Wall -Wno-unused-parameter)
add_test(NAME tsl_unit_tests COMMAND tsl_tests)

add_executable(tsl_bench Bench/Bench.cpp)
target_link_libraries(tsl_bench PRIVATE TwoonemoonCore)

add_executable(tsl_render Renders/RenderTool.cpp)
target_link_libraries(tsl_render PRIVATE TwoonemoonCore)
