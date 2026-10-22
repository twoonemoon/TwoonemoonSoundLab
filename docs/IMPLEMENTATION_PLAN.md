# План реализации

Работа ведётся вертикальными срезами; после каждого — сборка, тесты, коммит, обновление PROGRESS.md.

| Milestone | Содержание | Критерий завершения |
|---|---|---|
| M0 аудит | окружение, лицензии, фреймворк, референсы, структура | ENVIRONMENT_AUDIT, DECISIONS, сборка-проба |
| M1 сквозной путь | standalone + AU/VST3/CLAP, input→DSP→output, meters, trim, bypass, автоматизация, state | auval PASS, звук проходит |
| M2 базовый DSP | фильтры, gain staging, gate, compressor, boost/drive, защита от NaN, unit tests | тесты зелёные |
| M3 усилители | общая архитектура (каскады триодов, tone stack, power amp, sag, NFB) → 12 моделей + фасады | тесты, рендеры |
| M4 кабинет | zero-latency partitioned convolution, IR import, dual cab/mic, генерация собственных IR | тесты latency=0 |
| M5 time/mod/pitch | delay ×5, reverb ×5, chorus/phaser/flanger/trem/vibrato/rotary/env, octave, pitch, harmonizer | тесты устойчивости |
| M6 workflow | drag&drop chain, параллельные ветки, A/B, undo/redo, ≥50 пресетов, теги/поиск/избранное | |
| M7 интерфейс | все экраны, фасады, масштаб 50–250%, EQ/analyzer, tuner, onboarding, MIDI Learn | |
| M8 standalone/installer | audio setup, .pkg установщик, удаление, подпись (опционально) | установка/удаление проверены |
| M9 RC | чистая пересборка, валидация, аудит лицензий, документация | |

## Статус (2026-10-22)
M0–M9 выполнены в объёме, проверяемом без реальной гитары и DAW: все модули реализованы и покрыты тестами,
плагины проходят валидаторы, установщик проверен установкой/обновлением/удалением, документация полная.
Открытые пункты (см. PROGRESS.md): прослушивание и подстройка звука на реальной гитаре, ручная проверка в DAW,
подпись Developer ID и нотаризация (нужны учётные данные владельца), измерение round-trip задержки.
