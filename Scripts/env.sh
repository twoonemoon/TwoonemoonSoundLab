#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Общее окружение сборки. Подключается остальными скриптами: source Scripts/env.sh
#
# * Если лицензия Xcode не принята (или Xcode не установлен), используются Command Line Tools.
# * Линкер CLT может не понимать самый новый SDK — выбираем самый новый SDK, с которым линкуется тестовая программа.

TSL_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export TSL_ROOT

if ! /usr/bin/xcrun --find clang >/dev/null 2>&1 || ! /usr/bin/git --version >/dev/null 2>&1; then
  export DEVELOPER_DIR=/Library/Developer/CommandLineTools
fi

# Дополнительные инструменты (cmake/ninja из PyPI-venv, gh), если установлены через Scripts/install-tools.sh
if [ -d "$HOME/.tonelab-tools/bin" ]; then
  export PATH="$HOME/.tonelab-tools/bin:$PATH"
fi

if [ -z "${TSL_SYSROOT:-}" ]; then
  _probe_dir="$(mktemp -d)"
  echo 'int main(void){return 0;}' > "$_probe_dir/p.c"
  _sdk_root="$(/usr/bin/xcrun --show-sdk-platform-path 2>/dev/null || true)/Developer/SDKs"
  [ -d "$_sdk_root" ] || _sdk_root="${DEVELOPER_DIR:-/Library/Developer/CommandLineTools}/SDKs"
  for _sdk in $(ls -d "$_sdk_root"/MacOSX[0-9]*.sdk 2>/dev/null | sort -t X -k 2 -V -r); do
    if /usr/bin/clang -isysroot "$_sdk" -arch arm64 -o "$_probe_dir/p" "$_probe_dir/p.c" >/dev/null 2>&1; then
      TSL_SYSROOT="$_sdk"
      break
    fi
  done
  rm -rf "$_probe_dir"
fi
export TSL_SYSROOT

# Минимальная версия macOS (см. docs/DECISIONS.md, ADR-004)
export TSL_DEPLOYMENT_TARGET="${TSL_DEPLOYMENT_TARGET:-12.0}"
