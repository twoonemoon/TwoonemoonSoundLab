#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Сборка и запуск unit/integration тестов ядра (без плагинных таргетов).
set -euo pipefail
source "$(dirname "$0")/env.sh"
BUILD_DIR="$TSL_ROOT/build/tests"
cmake -S "$TSL_ROOT" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DTSL_BUILD_PLUGIN=OFF \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_SYSROOT="$TSL_SYSROOT" -DCMAKE_OSX_DEPLOYMENT_TARGET="$TSL_DEPLOYMENT_TARGET" >/dev/null
cmake --build "$BUILD_DIR"
"$BUILD_DIR/Tests/tsl_tests" "$@"
