#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Загружает инструменты валидации в ~/.tonelab-tools (только для разработки, не распространяются с продуктом).
set -euo pipefail
source "$(dirname "$0")/env.sh"
T="$HOME/.tonelab-tools"; mkdir -p "$T/src" "$T/validators/pluginval" "$T/validators/clapval"
# VST3 validator — из официального VST3 SDK (тот же тег, что и для сборки)
if [ ! -x "$T/src/vst3sdk/build/bin/Release/validator" ]; then
  rm -rf "$T/src/vst3sdk"
  git clone -q --depth=1 --branch v3.8.1_build_84 https://github.com/steinbergmedia/vst3sdk.git "$T/src/vst3sdk"
  (cd "$T/src/vst3sdk" && git submodule update -q --init --depth=1 pluginterfaces base public.sdk cmake)
  XCODE_VERSION=16.0 SDKROOT="$TSL_SYSROOT" cmake -S "$T/src/vst3sdk" -B "$T/src/vst3sdk/build" -G Ninja -DXCODE_VERSION=16.0 \
    -DCMAKE_BUILD_TYPE=Release -DSMTG_ENABLE_VSTGUI_SUPPORT=OFF -DSMTG_ADD_VSTGUI=OFF -DSMTG_ENABLE_VST3_PLUGIN_EXAMPLES=OFF \
    -DSMTG_ENABLE_VST3_HOSTING_EXAMPLES=ON -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_SYSROOT="$TSL_SYSROOT" >/dev/null
  cmake --build "$T/src/vst3sdk/build" --target validator >/dev/null
fi
# pluginval (Tracktion, официальный релиз)
[ -d "$T/validators/pluginval/pluginval.app" ] || (cd "$T/validators" && curl -sL -o pv.zip https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_macOS.zip && unzip -q -o pv.zip -d pluginval)
# clap-validator (free-audio, официальный релиз)
if [ ! -x "$T/validators/clapval/clap-validator" ]; then
  (cd "$T/validators" && curl -sL -o cv.zip https://github.com/free-audio/clap-validator/releases/download/0.4.1/clap-validator-0.4.1-127-g152b982-macos-15-aarch64.zip \
    && unzip -q -o cv.zip -d clapval && cd clapval && tar xzf clap-validator-*.tar.gz)
fi
echo "Готово: $T"
