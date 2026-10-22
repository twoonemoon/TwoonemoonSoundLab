#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Загружает закреплённые версии SDK, необходимых для сборки TwoonemoonSoundLab.
# Все зависимости берутся только из официальных репозиториев и фиксируются по тегу/SHA.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IPLUG2="$ROOT/ThirdParty/iPlug2"
DEPS="$IPLUG2/Dependencies/IPlug"

# --- Закреплённые версии (см. docs/DECISIONS.md, THIRD_PARTY_NOTICES.md) ---
VST3_TAG="v3.8.1_build_84"                                  # MIT
CLAP_SHA="a47f6badb49d948fd009998f28309cdab78979c9"         # MIT, ветка main (требуется iPlug2 / clap-helpers)
CLAP_HELPERS_SHA="a380c978f429103e08afc1d17e9efbf3e3c96ee2" # MIT

# Command Line Tools достаточно; если лицензия Xcode не принята, git/clang из Xcode недоступны.
if ! xcrun --find clang >/dev/null 2>&1 || ! git --version >/dev/null 2>&1; then
  export DEVELOPER_DIR=/Library/Developer/CommandLineTools
fi

echo "==> iPlug2 submodule"
git -C "$ROOT" submodule update --init ThirdParty/iPlug2

fetch_sha() { # repo dir sha
  local repo="$1" dir="$2" sha="$3"
  rm -rf "$dir"
  git init -q "$dir"
  git -C "$dir" remote add origin "$repo"
  git -C "$dir" fetch -q --depth=1 origin "$sha"
  git -C "$dir" checkout -q FETCH_HEAD
  rm -rf "$dir/.git"
}

echo "==> VST3 SDK $VST3_TAG"
rm -rf "$DEPS/VST3_SDK"
git clone -q --depth=1 --branch "$VST3_TAG" https://github.com/steinbergmedia/vst3sdk.git "$DEPS/VST3_SDK"
( cd "$DEPS/VST3_SDK"
  git submodule update -q --init --depth=1 pluginterfaces base public.sdk cmake
  rm -rf public.sdk/samples vstgui4 .git* */.git* )

echo "==> CLAP SDK $CLAP_SHA"
fetch_sha https://github.com/free-audio/clap.git "$DEPS/CLAP_SDK" "$CLAP_SHA"
echo "==> clap-helpers $CLAP_HELPERS_SHA"
fetch_sha https://github.com/free-audio/clap-helpers.git "$DEPS/CLAP_HELPERS" "$CLAP_HELPERS_SHA"

# Вернуть файлы, которые отслеживаются самим iPlug2.
git -C "$IPLUG2" checkout -- Dependencies/IPlug/VST3_SDK/README.md \
  Dependencies/IPlug/CLAP_SDK/readme.txt Dependencies/IPlug/CLAP_HELPERS/readme.txt 2>/dev/null || true

echo "==> Готово"
