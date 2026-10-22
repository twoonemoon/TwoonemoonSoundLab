# TwoonemoonSoundLab

Бесплатный open-source гитарный процессор для macOS на Apple Silicon: усилители, кабинеты, педали и
эффекты для игры и записи через аудиоинтерфейс. Форматы: **AUv2, VST3, CLAP и standalone-приложение**.

> Статус: ранняя разработка (0.1.0). Готовы сквозной аудиопуть, гейт, компрессор и четыре педали
> перегруза. Усилители, кабинет, эффекты пространства и пресеты — в работе. Актуальный статус — в
> [docs/PROGRESS.md](docs/PROGRESS.md).

## Требования
- Mac на Apple Silicon (M1 и новее), macOS 12.0+ (проверено на macOS 26.6.2).
- Для сборки: Command Line Tools (полный Xcode не обязателен), CMake ≥ 3.21, Ninja.

## Быстрая сборка
```bash
git clone --recursive https://github.com/twoonemoon/TwoonemoonSoundLab.git
cd TwoonemoonSoundLab
Scripts/bootstrap.sh   # закреплённые SDK (VST3, CLAP)
Scripts/build.sh       # AU, VST3, CLAP, standalone
Scripts/test.sh        # unit-тесты DSP
```
Подробнее — [docs/BUILDING.md](docs/BUILDING.md).

## Принципы
- Низкая задержка: модули без lookahead, оверсэмплинг на IIR halfband с минимальной групповой задержкой.
- Real-time safety: никаких аллокаций, блокировок и I/O в аудиопотоке.
- Никакой телеметрии, регистрации и сети.

## Лицензия
GPL-3.0-or-later, см. [LICENSE](LICENSE) и [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
