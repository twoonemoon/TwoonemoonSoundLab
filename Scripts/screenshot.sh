#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Скриншот standalone-приложения (без аудиоустройства). Scripts/screenshot.sh <out.png> [page]
set -euo pipefail
OUT="$1"; PAGE="${2:-0}"
APP="$(cd "$(dirname "$0")/.." && pwd)/build/release/out/TwoonemoonSoundLab.app/Contents/MacOS/TwoonemoonSoundLab"
rm -f "$OUT"
TSL_START_PAGE="$PAGE" "$APP" --screenshot "$OUT" >/dev/null 2>&1 &
PID=$!
for i in $(seq 1 40); do sleep 0.25; [ -f "$OUT" ] && break; done
sleep 0.5
kill "$PID" 2>/dev/null || true
[ -f "$OUT" ] && echo "$OUT"
