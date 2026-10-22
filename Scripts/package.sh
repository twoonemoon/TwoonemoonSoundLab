#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Сборка единого установщика dist/TwoonemoonSoundLab-Installer.pkg (+ SHA256SUMS.txt).
#
#   Scripts/package.sh [--no-build]
#
# Локальная/тестовая сборка (по умолчанию): бандлы подписываются ad-hoc, пакет без подписи.
# Публичный релиз (необязательно) — задайте переменные окружения (секреты НЕ хранятся в репозитории):
#   TSL_SIGN_APP="Developer ID Application: Имя (TEAMID)"    — подпись бандлов (hardened runtime, timestamp)
#   TSL_SIGN_INSTALLER="Developer ID Installer: Имя (TEAMID)" — подпись .pkg
#   TSL_NOTARY_PROFILE="profile"  — профиль `xcrun notarytool store-credentials` для нотаризации и stapling
set -euo pipefail
source "$(dirname "$0")/env.sh"

NAME="TwoonemoonSoundLab"
OUT="$TSL_ROOT/build/release/out"
DIST="$TSL_ROOT/dist"
WORK="$TSL_ROOT/build/package"
VERSION="$(sed -n 's/^#define PLUG_VERSION_STR "\(.*\)"/\1/p' "$TSL_ROOT/Source/Plugin/config.h")"
[ -n "$VERSION" ] || { echo "Не удалось определить версию из config.h" >&2; exit 1; }

if [ "${1:-}" != "--no-build" ]; then
  "$TSL_ROOT/Scripts/build.sh" Release
fi

echo "==> Версия $VERSION"
rm -rf "$WORK"
mkdir -p "$WORK"/{stage,pkgs,scripts,resources} "$DIST"

# component | bundle | ожидаемый bundle id | папка установки
COMPONENTS=(
  "app|$NAME.app|com.twoonemoon.app.$NAME|/Applications"
  "au|$NAME.component|com.twoonemoon.audiounit.$NAME|/Library/Audio/Plug-Ins/Components"
  "vst3|$NAME.vst3|com.twoonemoon.vst3.$NAME|/Library/Audio/Plug-Ins/VST3"
  "clap|$NAME.clap|com.twoonemoon.clap.$NAME|/Library/Audio/Plug-Ins/CLAP"
)

fail() { echo "ОШИБКА: $*" >&2; exit 1; }

for entry in "${COMPONENTS[@]}"; do
  IFS='|' read -r comp bundle bid dest <<<"$entry"
  src="$OUT/$bundle"
  [ -d "$src" ] || fail "нет артефакта $src — сначала соберите проект"
  mkdir -p "$WORK/stage/$comp"
  ditto "$src" "$WORK/stage/$comp/$bundle"
  b="$WORK/stage/$comp/$bundle"

  # --- Проверки целостности
  exe="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleExecutable' "$b/Contents/Info.plist")"
  found_id="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$b/Contents/Info.plist")"
  found_ver="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$b/Contents/Info.plist")"
  [ "$found_id" = "$bid" ] || fail "$bundle: bundle id '$found_id', ожидался '$bid'"
  [ "$found_ver" = "$VERSION" ] || fail "$bundle: версия '$found_ver', ожидалась '$VERSION'"
  archs="$(lipo -archs "$b/Contents/MacOS/$exe")"
  [[ " $archs " == *" arm64 "* ]] || fail "$bundle: нет архитектуры arm64 (есть: $archs)"
  find "$b" -name '.DS_Store' -delete

  # --- Подпись бандла
  if [ -n "${TSL_SIGN_APP:-}" ]; then
    codesign --force --options runtime --timestamp --sign "$TSL_SIGN_APP" "$b"
  else
    codesign --force --sign - "$b"
  fi
  codesign --verify --strict --deep "$b" || fail "$bundle: подпись не прошла проверку"
  echo "    $bundle: $bid $found_ver [$archs] — OK"

  # --- Скрипты компонента
  mkdir -p "$WORK/scripts/$comp"
  sed -e "s|@BUNDLE@|$bundle|g" -e "s|@BUNDLE_ID@|$bid|g" "$TSL_ROOT/installer/scripts/preinstall.in" > "$WORK/scripts/$comp/preinstall"
  chmod 755 "$WORK/scripts/$comp/preinstall"
  if [ "$comp" = "au" ]; then
    cp "$TSL_ROOT/installer/scripts/postinstall-au" "$WORK/scripts/$comp/postinstall"
    chmod 755 "$WORK/scripts/$comp/postinstall"
  fi

  # --- Бандл не перемещается в «найденную» копию с тем же id (например, dev-сборку в другом месте)
  cat > "$WORK/pkgs/$comp.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<array>
  <dict>
    <key>RootRelativeBundlePath</key><string>$bundle</string>
    <key>BundleIsRelocatable</key><false/>
    <key>BundleIsVersionChecked</key><false/>
    <key>BundleHasStrictIdentifier</key><true/>
    <key>BundleOverwriteAction</key><string>upgrade</string>
  </dict>
</array>
</plist>
EOF
  pkgbuild --quiet --root "$WORK/stage/$comp" --component-plist "$WORK/pkgs/$comp.plist" \
    --identifier "com.twoonemoon.$NAME.pkg.$comp" --version "$VERSION" \
    --install-location "$dest" --scripts "$WORK/scripts/$comp" \
    "$WORK/pkgs/$NAME-$comp.pkg"
done

# --- Служебный компонент: скрипт удаления
mkdir -p "$WORK/stage/support"
cp "$TSL_ROOT/Scripts/uninstall.sh" "$WORK/stage/support/Uninstall $NAME.command"
chmod 755 "$WORK/stage/support/Uninstall $NAME.command"
pkgbuild --quiet --root "$WORK/stage/support" --identifier "com.twoonemoon.$NAME.pkg.support" --version "$VERSION" \
  --install-location "/Library/Application Support/$NAME" "$WORK/pkgs/$NAME-support.pkg"

# --- Ресурсы установщика
sed "s|@VERSION@|$VERSION|g" "$TSL_ROOT/installer/resources/welcome.html" > "$WORK/resources/welcome.html"
cp "$TSL_ROOT/installer/resources/conclusion.html" "$WORK/resources/conclusion.html"
cp "$TSL_ROOT/LICENSE" "$WORK/resources/license.txt"
sed "s|@VERSION@|$VERSION|g" "$TSL_ROOT/installer/Distribution.xml.in" > "$WORK/Distribution.xml"

PKG="$DIST/$NAME-Installer.pkg"
rm -f "$PKG"
SIGN_ARGS=()
[ -n "${TSL_SIGN_INSTALLER:-}" ] && SIGN_ARGS=(--sign "$TSL_SIGN_INSTALLER" --timestamp)
productbuild --quiet --distribution "$WORK/Distribution.xml" --resources "$WORK/resources" \
  --package-path "$WORK/pkgs" "${SIGN_ARGS[@]+"${SIGN_ARGS[@]}"}" "$PKG"

# --- Нотаризация (только при наличии профиля и подписи)
if [ -n "${TSL_NOTARY_PROFILE:-}" ]; then
  [ -n "${TSL_SIGN_INSTALLER:-}" ] || fail "нотаризация требует подписи пакета (TSL_SIGN_INSTALLER)"
  xcrun notarytool submit "$PKG" --keychain-profile "$TSL_NOTARY_PROFILE" --wait
  xcrun stapler staple "$PKG"
  xcrun stapler validate "$PKG"
fi

# --- Удаление и контрольные суммы рядом с установщиком
cp "$TSL_ROOT/Scripts/uninstall.sh" "$DIST/Uninstall $NAME.command"
chmod 755 "$DIST/Uninstall $NAME.command"
( cd "$DIST" && shasum -a 256 "$NAME-Installer.pkg" "Uninstall $NAME.command" > SHA256SUMS.txt )

echo "==> Установщик: $PKG ($(du -h "$PKG" | cut -f1))"
pkgutil --check-signature "$PKG" | head -3 || true
cat "$DIST/SHA256SUMS.txt"
if [ -z "${TSL_SIGN_INSTALLER:-}" ]; then
  echo "    Пакет без подписи Developer ID (локальная/тестовая сборка). См. docs/RELEASE.md."
fi
