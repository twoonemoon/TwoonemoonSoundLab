#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Генерирует docs/HELP_CONTENT.md из Source/UI/HelpContent.cpp и Source/Audio/Params.h.
Запуск: python3 -I Scripts/tools/gen_help_doc.py  (из корня репозитория)."""
import re, pathlib

root = pathlib.Path(__file__).resolve().parents[2]
src = (root / "Source/UI/HelpContent.cpp").read_text(encoding="utf-8")
params = (root / "Source/Audio/Params.h").read_text(encoding="utf-8")

def join_c_strings(s):
    return "".join(re.findall(r'"((?:[^"\\]|\\.)*)"', s))

tips = dict(re.findall(r'\{ "([a-z0-9]+\.[A-Za-z0-9.]+)", "((?:[^"\\]|\\.)*)" \}', src))
plist = re.findall(r'X\((\w+), "([^"]+)", "([^"]+)", "([^"]+)"', params)

topics_block = src[src.index("helpTopics()"):src.index("onboardingSteps()")]
topics = []
for m in re.finditer(r'\{ ((?:"(?:[^"\\]|\\.)*"\s*)+), ((?:"(?:[^"\\]|\\.)*"\s*)+), ((?:"(?:[^"\\]|\\.)*"\s*)+)\}', topics_block):
    topics.append((join_c_strings(m.group(1)), join_c_strings(m.group(2)), join_c_strings(m.group(3))))
steps_block = src[src.index("onboardingSteps()"):]
steps = [(join_c_strings(a), join_c_strings(b)) for a, b in re.findall(r'\{ ("(?:[^"\\]|\\.)*"), ((?:"(?:[^"\\]|\\.)*"\s*)+), \d+ \}', steps_block)]

def tip_for(key):
    if key in tips: return tips[key], ""
    if key.startswith("cab.b.") and "cab.a." + key[6:] in tips: return tips["cab.a." + key[6:]], "общий с кабинетом A"
    m = re.match(r"eq\.b\d(\..*)", key)
    if m and "eq.b1" + m.group(1) in tips: return tips["eq.b1" + m.group(1)], "общий для полос EQ"
    m = re.match(r"harm\.v\d(\..*)", key)
    if m and "harm.v1" + m.group(1) in tips: return tips["harm.v1" + m.group(1)], "общий для голосов"
    if key.endswith(".on"): return "Включить/выключить модуль. Переключение плавное, без щелчков.", "общий для переключателей"
    return "", ""

out = ["# Встроенная справка и подсказки", "",
       "> Файл сгенерирован `Scripts/tools/gen_help_doc.py` из `Source/UI/HelpContent.cpp` — правьте исходник, затем перегенерируйте.", "",
       "## Разделы справки (страница «Справка», поиск по названию, тексту и ключевым словам)", ""]
for t, b, k in topics:
    out += [f"### {t}", "", b, "", f"*Ключевые слова:* {k}", ""]
out += ["## Знакомство (первый запуск, повтор — «Настройки → Показать знакомство снова»)", ""]
for i, (t, b) in enumerate(steps):
    out += [f"{i + 1}. **{t}** — {b}"]
out += ["", "## Подсказки регуляторов (наведение курсора; отключаются в «Настройках»)", "",
        f"Параметров: {len(plist)}. Подсказка показывается для каждого.", ""]
group = None
for enum, key, name, grp in plist:
    if grp != group:
        group = grp
        out += ["", f"### {grp}", "", "| Регулятор | Ключ | Подсказка |", "|---|---|---|"]
    text, note = tip_for(key)
    if note: text += f" *({note})*"
    out.append(f"| {name} | `{key}` | {text} |")
(root / "docs/HELP_CONTENT.md").write_text("\n".join(out) + "\n", encoding="utf-8")
print(f"docs/HELP_CONTENT.md: {len(topics)} разделов, {len(steps)} шагов, {len(plist)} параметров")
