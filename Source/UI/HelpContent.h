// SPDX-License-Identifier: GPL-3.0-or-later
// База встроенных подсказок и справки (рус.). Источник для docs/HELP_CONTENT.md.
#pragma once

#include <string>
#include <vector>

namespace tslui {

/** Подсказка для параметра по его ключу (nullptr, если нет). */
const char* paramTooltip(const char* paramKey);

struct HelpTopic
{
  const char* title;
  const char* body;
  const char* keywords; // для поиска
};

const std::vector<HelpTopic>& helpTopics();

/** Шаги первого знакомства (onboarding). */
struct OnboardingStep
{
  const char* title;
  const char* body;
  int page; // страница, которую стоит показать (−1 — не переключать)
};
const std::vector<OnboardingStep>& onboardingSteps();

} // namespace tslui
