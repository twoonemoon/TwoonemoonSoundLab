# SPDX-License-Identifier: GPL-3.0-or-later
# Плагинные таргеты (AUv2, VST3, CLAP, standalone) через iPlug2.
# PROJECT_DIR для iplug_add_plugin = этот каталог (здесь config.h и resources/).

set(IPLUG2_DIR "${CMAKE_SOURCE_DIR}/ThirdParty/iPlug2" CACHE PATH "iPlug2 root directory")
if(NOT EXISTS "${IPLUG2_DIR}/iPlug2.cmake")
  message(FATAL_ERROR "iPlug2 не найден. Выполните: Scripts/bootstrap.sh")
endif()

include(${IPLUG2_DIR}/iPlug2.cmake)
find_package(iPlug2 REQUIRED)

set(TSL_SRC ${CMAKE_SOURCE_DIR}/Source)
set(TSL_RES ${CMAKE_SOURCE_DIR}/Resources)

iplug_add_plugin(TwoonemoonSoundLab
  SOURCES
    TwoonemoonSoundLab.cpp
    TwoonemoonSoundLab.h
    config.h
    resources/resource.h
    ${TSL_SRC}/UI/Theme.cpp
    ${TSL_SRC}/UI/Controls.cpp
    ${TSL_SRC}/UI/MainUI.cpp
    ${TSL_SRC}/UI/AmpFaces.cpp
    ${TSL_SRC}/UI/PagePlay.cpp
    ${TSL_SRC}/UI/PageAmp.cpp
    ${TSL_SRC}/UI/PageCab.cpp
    ${TSL_SRC}/UI/TunerView.cpp
    ${TSL_SRC}/UI/UIContext.cpp
    ${TSL_SRC}/UI/PageFx.cpp
    ${TSL_SRC}/UI/PageEq.cpp
    ${TSL_SRC}/UI/PageChain.cpp
    ${TSL_SRC}/UI/PagePresets.cpp
    ${TSL_SRC}/UI/PageSettings.cpp
    ${TSL_SRC}/UI/PageHelp.cpp
    ${TSL_SRC}/UI/HelpContent.cpp
    ${TSL_SRC}/Standalone/AppBootstrap.mm
  RESOURCES
    ${TSL_RES}/Fonts/Inter-Regular.ttf
    ${TSL_RES}/Fonts/Inter-Medium.ttf
    ${TSL_RES}/Fonts/Inter-SemiBold.ttf
    ${TSL_RES}/Fonts/Inter-Bold.ttf
    ${TSL_RES}/Fonts/JetBrainsMono-Regular.ttf
    ${TSL_RES}/Fonts/JetBrainsMono-Medium.ttf
    ${TSL_RES}/Icon/TwoonemoonSoundLab.icns
  FORMATS APP VST3 CLAP AU
  LINK TwoonemoonCore
)
