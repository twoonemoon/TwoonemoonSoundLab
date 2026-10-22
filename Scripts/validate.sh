#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Валидация собранных плагинов всеми доступными инструментами.
#   Scripts/validate.sh [build/release/out]
# Инструменты (не входят в репозиторий; ставятся Scripts/install-validators.sh):
#   auval (macOS), VST3 validator (из VST3 SDK), pluginval (Tracktion), clap-validator (free-audio)
set -uo pipefail
source "$(dirname "$0")/env.sh"
OUT="${1:-$TSL_ROOT/build/release/out}"
T="$HOME/.tonelab-tools"
FAIL=0
run() { echo "==> $1"; shift; if "$@"; then echo "    OK"; else echo "    FAILED"; FAIL=1; fi; }

run "auval -strict (AUv2)" bash -c "auval -strict -v aumf TmSL Twmn | tail -3 | grep -q 'AU VALIDATION SUCCEEDED'"
if [ -x "$T/src/vst3sdk/build/bin/Release/validator" ]; then
  run "VST3 validator" bash -c "'$T/src/vst3sdk/build/bin/Release/validator' '$OUT/TwoonemoonSoundLab.vst3' | tail -3 | grep -q ', 0 tests failed'"
else echo "==> VST3 validator: не установлен (пропущено)"; fi
PV="$T/validators/pluginval/pluginval.app/Contents/MacOS/pluginval"
if [ -x "$PV" ]; then
  run "pluginval VST3 (strictness 10)" "$PV" --strictness-level 10 --validate "$OUT/TwoonemoonSoundLab.vst3"
  run "pluginval AU (strictness 10)" "$PV" --strictness-level 10 --validate "$OUT/TwoonemoonSoundLab.component"
else echo "==> pluginval: не установлен (пропущено)"; fi
CV="$T/validators/clapval/clap-validator"
if [ -x "$CV" ]; then
  run "clap-validator" bash -c "'$CV' validate '$OUT/TwoonemoonSoundLab.clap' | sed 's/\x1b\[[0-9;]*m//g' | grep -q ' 0 failed'"
else echo "==> clap-validator: не установлен (пропущено)"; fi
exit $FAIL
