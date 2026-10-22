# Тестирование

## Команды
- `Scripts/test.sh` — unit/integration тесты ядра (62 теста, ~9 200 проверок; последний прогон: 0 ошибок).
- `build/tests/Tests/tsl_bench` — CPU/латентность.
- `build/tests/Tests/tsl_render --calibrate | --render <dir>` — рендер 60 заводских пресетов, уровни, NaN.

## Валидация плагинов (2026-10-22, macOS 26.6.2, M1)
Запуск: `Scripts/install-validators.sh` (один раз), затем `Scripts/validate.sh`.

| Проверка | Результат |
|---|---|
| `auval -strict -v aumf TmSL Twmn` | **PASS** |
| VST3 validator (собран из VST3 SDK 3.8.1) | **47 passed, 0 failed** |
| pluginval 1.0.4, VST3, strictness 10, с GUI-тестами | **SUCCESS** |
| pluginval 1.0.4, AU, strictness 10, с GUI-тестами | **SUCCESS** |
| clap-validator 0.4.1 | **38 passed, 0 failed, 0 warnings, 6 skipped** (необязательные расширения: preset-discovery, audio-ports-activation, configurable-audio-ports, note ports) |

### Исправленные проблемы CLAP (обнаружены clap-validator)
- после загрузки состояния не запрашивался `params.rescan` → добавлен;
- значения восстанавливались через float (потеря битов) → восстановление double без потерь;
- запись состояния целиком одним вызовом → запись порциями до конца потока;
- пустое/чужое состояние принималось → отклоняется (по сигнатуре формата);
- текст ступенчатых параметров для нецелых значений → округление перед форматированием;
- ошибка iPlug2 в `audio-ports-config` (`has_main_input = count > 1`) → исправленная реализация в классе плагина;
- `latency.changed` вне `activate` → задержка сообщается только при изменении;
- статус «sleep» при незатухшем выходе → плагин сообщает длину «хвоста» (кабинет/усилитель, delay по Feedback, reverb по Decay, Freeze — бесконечный).

## CPU (M1, 1 ядро, 48 кГц, блок 64, 4x)
gate+amp+cab ≈ 2.0 %; heavy ≈ 2.9 %; все модули + двойной кабинет ≈ 4.4 %. Добавленная латентность 0; групповая задержка оверсэмплинга 0.128 мс.

## Не проверено
Реальная гитара и аудиоинтерфейс, DAW (Logic, Reaper и др.), round-trip латентность (нужен loopback), drag&drop и ввод текста в UI вручную.
Требуют прослушивания: все 12 моделей усилителей, 6 кабинетов × 3 микрофона, все эффекты, все 60 пресетов.
