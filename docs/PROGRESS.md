# Прогресс

Статусы: **implemented** — реализовано и проверено тестом/сборкой; **partial** — работает, но не в полном объёме;
**planned** — не начато; **blocked** — требует внешнего действия.

## Итоговый статус
Продукт **не готов к релизу**. Реализованы: M0–M5 (аудиопуть, базовый DSP, 12 усилителей, Cab Studio, delay/reverb/модуляции/pitch/harmonizer), большая часть M6 (цепочка с параллельной секцией, 60 пресетов, A/B, undo/redo) и M7 (7 страниц UI, тюнер, EQ-анализатор). Не сделаны: установщик, MIDI Learn, онбординг/справка, утилиты (limiter, widener, looper), компактный режим 50%, исправления CLAP (см. TESTING.md), полная документация.

## Артефакты (последняя сборка)
`build/release/out/`: `TwoonemoonSoundLab.app`, `.component` (AUv2), `.vst3`, `.clap` — arm64, macOS 12.0+.

## Компоненты

| Компонент | Статус | Примечание |
|---|---|---|
| Сборка CMake/Ninja (CLT, без Xcode) | implemented | `Scripts/build.sh` |
| AUv2 / VST3 / CLAP / standalone | implemented | auval -strict PASS; VST3/CLAP валидаторы не запускались |
| Параметры, автоматизация, state (по ключам) | implemented | append-only реестр |
| Input/Output Trim, Bypass, Mute, Input Mode | implemented | |
| Peak/RMS-измерители, индикатор клипа | implemented | |
| Аварийная защита (NaN/Inf, потолок +12 дБFS) | implemented | |
| Порядок цепочки (lock-free смена) | partial | DSP готов; UI перетаскивания — planned |
| Noise Gate (гистерезис, hold, range, DI-ключ) | implemented | |
| Compressor (soft knee, SC HPF, mix, GR) | implemented | |
| Boost / Overdrive / Distortion / Fuzz | implemented | оверсэмплинг 2x/4x, требуют прослушивания |
| Oversampler (IIR halfband 2/4/8x) | implemented | подавление зеркал > 80 дБ (тест) |
| Усилители (12 моделей) | planned | M3 |
| Кабинет / IR | planned | M4 |
| EQ, Analyzer | planned | |
| Тюнер | planned | кольцевой буфер в движке уже есть |
| Delay, Reverb, модуляция, pitch | planned | |
| Пресеты (≥50) | planned | |
| Масштаб UI 50–250% | partial | 6 масштабов через меню, векторная перерисовка; компактная раскладка для 50% — planned |
| MIDI Learn / экспрессия | planned | MIDI-вход объявлен |
| Установщик .pkg | planned | |
| Подпись/нотаризация | blocked | нужны Developer ID credentials владельца |

## Известные ограничения
- Звучание педалей не прослушивалось на реальной гитаре — требуется audition.
- Не проверено в DAW (кроме auval). Хосты для проверки: Logic/GarageBand (AU), Reaper (VST3/CLAP).
- Орнамент на корпусах педалей — первая версия, требует доработки дизайна.

## Список на прослушивание
Boost, Overdrive (оба режима), Distortion (3 режима), Fuzz (3 режима), Noise Gate (high-gain), Compressor.
