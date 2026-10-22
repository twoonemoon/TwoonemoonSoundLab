#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Удаление TwoonemoonSoundLab (приложение, AUv2, VST3, CLAP, служебные файлы, квитанции установщика).
# Этот же файл устанавливается как «Uninstall TwoonemoonSoundLab.command».
#
#   uninstall.sh [--user | --system] [--purge] [--yes]
#     --user    только установка «для меня» (~/Applications, ~/Library/...)
#     --system  только установка «для всех» (/Applications, /Library/...; потребуется пароль администратора)
#               без этих ключей проверяются оба расположения
#     --purge   дополнительно удалить пользовательские данные: пресеты, настройки, библиотеку
#               (~/Library/Application Support/TwoonemoonSoundLab) — с подтверждением
#     --yes     не задавать вопросов (кроме --purge без --yes)
#
# Удаляются только бандлы с идентификаторами TwoonemoonSoundLab — посторонние файлы не трогаются.
set -u

DO_USER=1
DO_SYSTEM=1
PURGE=0
ASSUME_YES=0
for a in "$@"; do
  case "$a" in
    --user) DO_SYSTEM=0 ;;
    --system) DO_USER=0 ;;
    --purge) PURGE=1 ;;
    --yes|-y) ASSUME_YES=1 ;;
    -h|--help) sed -n '3,16p' "$0"; exit 0 ;;
    *) echo "Неизвестный параметр: $a" >&2; exit 2 ;;
  esac
done

NAME="TwoonemoonSoundLab"
# Относительный путь бандла → ожидаемый CFBundleIdentifier
ITEMS=(
  "Applications/$NAME.app|com.twoonemoon.app.$NAME"
  "Library/Audio/Plug-Ins/Components/$NAME.component|com.twoonemoon.audiounit.$NAME"
  "Library/Audio/Plug-Ins/VST3/$NAME.vst3|com.twoonemoon.vst3.$NAME"
  "Library/Audio/Plug-Ins/CLAP/$NAME.clap|com.twoonemoon.clap.$NAME"
)
SUPPORT_REL="Library/Application Support/$NAME"
UNINSTALLER="Uninstall $NAME.command"
PKG_IDS=(app au vst3 clap support)

REMOVED=0
FAILED=0
AU_TOUCHED=0

say() { printf '%s\n' "$*"; }

run_rm() { # $1 — prefix ("" для системы, $HOME для пользователя), остальное — команда
  local scope="$1"; shift
  if [ "$scope" = "system" ] && [ "$(id -u)" -ne 0 ]; then
    sudo "$@"
  else
    "$@"
  fi
}

remove_in() { # $1 — корень ("/" или "$HOME/"), $2 — system|user
  local root="$1" scope="$2" entry rel id path found
  for entry in "${ITEMS[@]}"; do
    rel="${entry%%|*}"; id="${entry##*|}"
    path="${root}${rel}"
    [ -e "$path" ] || continue
    found="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$path/Contents/Info.plist" 2>/dev/null || true)"
    if [ "$found" != "$id" ]; then
      say "  пропуск: $path (чужой bundle id '${found:-нет}')"
      continue
    fi
    if run_rm "$scope" /bin/rm -rf "$path"; then
      say "  удалено: $path"
      REMOVED=$((REMOVED + 1))
      case "$rel" in *.component) AU_TOUCHED=1 ;; esac
    else
      say "  ОШИБКА: не удалось удалить $path"
      FAILED=$((FAILED + 1))
    fi
  done
  # Скрипт удаления, установленный пакетом (сама папка — общая с пользовательскими данными при установке «для меня»)
  if [ -f "${root}${SUPPORT_REL}/${UNINSTALLER}" ]; then
    run_rm "$scope" /bin/rm -f "${root}${SUPPORT_REL}/${UNINSTALLER}" && say "  удалено: ${root}${SUPPORT_REL}/${UNINSTALLER}"
    run_rm "$scope" /bin/rmdir "${root}${SUPPORT_REL}" 2>/dev/null || true
  fi
  # Квитанции установщика
  local p
  for p in "${PKG_IDS[@]}"; do
    if [ "$scope" = "system" ]; then
      if /usr/sbin/pkgutil --pkg-info "com.twoonemoon.$NAME.pkg.$p" >/dev/null 2>&1; then
        run_rm system /usr/sbin/pkgutil --forget "com.twoonemoon.$NAME.pkg.$p" >/dev/null 2>&1 || true
      fi
    else
      if /usr/sbin/pkgutil --volume "$HOME" --pkg-info "com.twoonemoon.$NAME.pkg.$p" >/dev/null 2>&1; then
        /usr/sbin/pkgutil --volume "$HOME" --forget "com.twoonemoon.$NAME.pkg.$p" >/dev/null 2>&1 || true
      fi
    fi
  done
}

say "Удаление $NAME"
if [ "$ASSUME_YES" -ne 1 ] && [ -t 0 ]; then
  read -r -p "Удалить приложение и плагины $NAME? [y/N] " ans
  case "$ans" in y|Y|д|Д|yes|да) ;; *) say "Отменено."; exit 0 ;; esac
fi

if [ "$DO_SYSTEM" -eq 1 ]; then
  say "Для всех пользователей (/Applications, /Library):"
  remove_in "/" system
fi
if [ "$DO_USER" -eq 1 ]; then
  say "Только для текущего пользователя (~/Applications, ~/Library):"
  remove_in "$HOME/" user
fi

if [ "$AU_TOUCHED" -eq 1 ]; then
  /usr/bin/killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
fi

DATA="$HOME/$SUPPORT_REL"
if [ "$PURGE" -eq 1 ] && [ -d "$DATA" ]; then
  ok=0
  if [ "$ASSUME_YES" -eq 1 ]; then ok=1
  elif [ -t 0 ]; then
    read -r -p "Удалить ВСЕ пользовательские пресеты и настройки в $DATA? Это необратимо. [y/N] " ans
    case "$ans" in y|Y|д|Д|yes|да) ok=1 ;; esac
  fi
  if [ "$ok" -eq 1 ]; then
    /bin/rm -rf "$DATA" && say "  удалены пользовательские данные: $DATA"
  else
    say "  пользовательские данные сохранены: $DATA"
  fi
elif [ -d "$DATA" ]; then
  say "Пользовательские пресеты и настройки сохранены: $DATA"
fi

if [ "$FAILED" -gt 0 ]; then
  say "Готово с ошибками: не удалено объектов — $FAILED."
  STATUS=1
elif [ "$REMOVED" -eq 0 ]; then
  say "Установленных компонентов $NAME не найдено."
  STATUS=0
else
  say "Готово. Удалено компонентов: $REMOVED. Перезапустите DAW, чтобы она обновила список плагинов."
  STATUS=0
fi

# При запуске двойным щелчком (.command) не закрывать окно Терминала сразу
case "$0" in *.command) [ -t 0 ] && read -r -p "Нажмите Return, чтобы закрыть окно…" _ ;; esac
exit "$STATUS"
