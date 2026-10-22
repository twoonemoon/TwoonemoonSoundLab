#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Установка собранного пакета dist/TwoonemoonSoundLab-Installer.pkg из командной строки.
#
#   Scripts/install.sh [--user]
#     без ключей — для всех пользователей (/Applications, /Library/...; запросит пароль администратора)
#     --user     — только для текущего пользователя (~/Applications, ~/Library/...; без пароля)
#
# Обычным пользователям достаточно открыть .pkg двойным щелчком.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PKG="$ROOT/dist/TwoonemoonSoundLab-Installer.pkg"
[ -f "$PKG" ] || { echo "Нет $PKG — сначала выполните Scripts/package.sh" >&2; exit 1; }

if [ -f "$ROOT/dist/SHA256SUMS.txt" ]; then
  ( cd "$ROOT/dist" && grep "TwoonemoonSoundLab-Installer.pkg" SHA256SUMS.txt | shasum -a 256 -c - ) || { echo "Контрольная сумма пакета не совпадает" >&2; exit 1; }
fi

if [ "${1:-}" = "--user" ]; then
  installer -pkg "$PKG" -target CurrentUserHomeDirectory
else
  sudo installer -pkg "$PKG" -target /
fi
echo "Готово. Перезапустите DAW или выполните повторное сканирование плагинов."
