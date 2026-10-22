# CLAUDE.md — правила разработки TwoonemoonSoundLab

Постоянные правила для последующих сессий. Исходное ТЗ: «ToneLab — мастер-промпт» (рабочее название продукта
по решению владельца — **TwoonemoonSoundLab**; не переименовывать без явного указания).

## Язык и коммуникация
- Коммиты, документация пользователя, UI — **на русском**. Подписи регуляторов — общепринятые английские
  термины (Gain, Master, Presence, Mix, Feedback, Decay…).
- Не упоминать AI/Claude/кодогенерацию в UI и product copy.
- Не утверждать готовность/качество/задержку/прохождение тестов без фактической проверки.

## Сборка
- `Scripts/bootstrap.sh` — закреплённые SDK (VST3 3.8.1 MIT, CLAP/clap-helpers по SHA, iPlug2 — submodule по SHA).
- `Scripts/build.sh [Release|Debug]` — всё (ядро, тесты, AU/VST3/CLAP/app). Работает с одними Command Line Tools:
  `Scripts/env.sh` сам выбирает `DEVELOPER_DIR` и самый новый линкуемый SDK.
- Тесты: `build/<config>/Tests/tsl_tests` (или `Scripts/test.sh`).
- Standalone собирается **без MainMenu.xib** — меню/делегат создаются в `Source/Standalone/AppBootstrap.mm`.
- Пост-билд iPlug2 копирует плагины в `~/Library/Audio/Plug-Ins/*` и app в `~/Applications`.

## Архитектура (кратко, подробно — docs/ARCHITECTURE.md)
- `Source/DSP`, `Source/Effects`, `Source/Amps`, `Source/Cabinet`, `Source/Audio` — библиотека `TwoonemoonCore`,
  **без зависимостей от iPlug2**, покрыта тестами.
- `Source/Plugin` — мост iPlug2 ↔ `tsl::Engine`. `Source/UI` — IGraphics (NanoVG/Metal).
- Единственная authoritative-модель — параметры плагина. UI не хранит своих копий DSP-состояния.

## Параметры
- Все параметры описаны в `Source/Audio/Params.h` (X-macro `TSL_PARAM_LIST`).
- **Индекс = ID для хоста.** Новые параметры — только в конец списка. После релиза 1.0 нельзя удалять/переставлять.
- `key` — стабильный строковый ключ для пресетов и состояния. Не менять.
- Состояние плагина сериализуется по ключам (устойчиво к добавлению параметров), формат версионирован.

## Real-time safety (audio thread)
Запрещено: аллокации, файлы, сеть, mutex/ожидания, логирование, JSON, создание FFT/конволверов/оверсэмплеров.
Всё готовится в `prepare()`. Обмен с UI — атомики и SPSC-кольца. Новый IR/граф — через подготовленный объект
и атомарную публикацию; освобождение — вне audio thread.

## Модули
- Каждый модуль: `prepare/reset/update/process`, корректная работа с моно/стерео (`AudioBlock::stereo`),
  любым размером блока ≤ maxBlock, сглаживание параметров, тесты (тишина, импульс, синус, экстремумы, NaN).
- Не добавлять UI для несуществующего DSP. Незавершённые модули не показывать как доступные.

## Коммиты
- Мелкими осмысленными шагами, сообщение на русском, в конце — строка соавторства.
- После каждого значимого шага — обновлять `docs/PROGRESS.md` (implemented / partially / planned / blocked).
