# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.21)

project(TwoonemoonSoundLab VERSION 0.1.0 LANGUAGES C CXX OBJC OBJCXX)

option(TSL_BUILD_PLUGIN "Собрать плагины и standalone (требуется iPlug2 + SDK)" ON)
option(TSL_BUILD_TESTS "Собрать unit/integration тесты" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# Ядро: DSP + движок + пресеты. Не зависит от iPlug2, тестируется отдельно.
add_library(TwoonemoonCore STATIC
  Source/Audio/Params.cpp
  Source/Audio/Engine.cpp
  Source/DSP/Oversampler.cpp
  Source/Effects/NoiseGate.cpp
  Source/Effects/Compressor.cpp
  Source/Effects/Drives.cpp
)
target_include_directories(TwoonemoonCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Source)
target_compile_options(TwoonemoonCore PRIVATE -Wall -Wextra -Wno-unused-parameter)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  target_compile_options(TwoonemoonCore PRIVATE -O3)
endif()
if(APPLE)
  target_link_libraries(TwoonemoonCore PUBLIC "-framework Accelerate")
endif()

# ---------------------------------------------------------------------------
if(TSL_BUILD_TESTS)
  enable_testing()
  add_subdirectory(Tests)
endif()

if(TSL_BUILD_PLUGIN)
  add_subdirectory(Source/Plugin)
endif()
