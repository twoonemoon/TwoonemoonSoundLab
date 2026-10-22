#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Сборка TwoonemoonSoundLab: ядро, тесты, плагины (AUv2, VST3, CLAP) и standalone.
#   Scripts/build.sh [Release|Debug] [доп. аргументы cmake --build]
set -euo pipefail
source "$(dirname "$0")/env.sh"

CONFIG="${1:-Release}"
shift || true
BUILD_DIR="$TSL_ROOT/build/$(echo "$CONFIG" | tr '[:upper:]' '[:lower:]')"

if [ ! -f "$TSL_ROOT/ThirdParty/iPlug2/Dependencies/IPlug/VST3_SDK/pluginterfaces/base/funknown.h" ]; then
  echo "SDK не найдены — запускаю Scripts/bootstrap.sh"
  "$TSL_ROOT/Scripts/bootstrap.sh"
fi

echo "==> SDK: $TSL_SYSROOT"
echo "==> Deployment target: macOS $TSL_DEPLOYMENT_TARGET, arch arm64"

cmake -S "$TSL_ROOT" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE="$CONFIG" \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_SYSROOT="$TSL_SYSROOT" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="$TSL_DEPLOYMENT_TARGET" \
  -DIGRAPHICS_BACKEND=NANOVG \
  -DIGRAPHICS_RENDERER=METAL

cmake --build "$BUILD_DIR" "$@"

echo "==> Артефакты: $BUILD_DIR/out"
ls -1 "$BUILD_DIR/out" 2>/dev/null || true
