# Тестирование

## Команды
- `Scripts/test.sh` — unit/integration тесты ядра (62 теста, ~9 200 проверок; последний прогон: 0 ошибок).
- `build/tests/Tests/tsl_bench` — CPU/латентность.
- `build/tests/Tests/tsl_render --calibrate | --render <dir>` — рендер 60 заводских пресетов, уровни, NaN.

## Валидация плагинов (2026-10-22, macOS 26.6.2, M1)
| Проверка | Результат |
|---|---|
| `auval -strict -v aumf TmSL Twmn` | **PASS** |
| VST3 validator (собран из VST3 SDK 3.8.1) | **47 passed, 0 failed** |
| clap-validator 0.4.1 | **29 passed, 7 failed, 2 warnings** — см. ниже |
| pluginval | загружен, не запускался |

### Известные провалы clap-validator (нужно исправить)
1. state-reproducibility-* (3): после загрузки состояния параметры не совпадают — вероятно, CLAP-обёртка iPlug2 вызывает наш UnserializeState иначе, требует отладки.
2. param-conversions: Bool-параметры («Bypass»): строка «0» не парсится обратно в «Off».
3. process-sleep-process-status: плагин сообщает «sleep», а выход не тишина (хвосты эффектов).
4. process-reset-reactivate: `clap_host_latency::changed` вызывается вне activate (SetLatency в OnReset).
5. layout-audio-ports-config: тип порта 1-1 не совпадает с audio-ports.
Предупреждения: пустое/случайное состояние принимается без ошибки (данные игнорируются).

## CPU (M1, 1 ядро, 48 кГц, блок 64, 4x)
gate+amp+cab ≈ 2.0 %; heavy ≈ 2.9 %; все модули + двойной кабинет ≈ 4.4 %. Добавленная латентность 0; групповая задержка оверсэмплинга 0.128 мс.

## Не проверено
Реальная гитара и аудиоинтерфейс, DAW (Logic, Reaper и др.), round-trip латентность (нужен loopback), drag&drop и ввод текста в UI вручную.
Требуют прослушивания: все 12 моделей усилителей, 6 кабинетов × 3 микрофона, все эффекты, все 60 пресетов.
