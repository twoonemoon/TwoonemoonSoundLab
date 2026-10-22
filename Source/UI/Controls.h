// SPDX-License-Identifier: GPL-3.0-or-later
// Собственные контролы TwoonemoonSoundLab. Все контролы, привязанные к параметрам,
// управляют единственной authoritative-моделью — параметрами плагина (хост/автоматизация/UI).
#pragma once

#include "Theme.h"
#include "IControl.h"

#include <functional>
#include <string>
#include <vector>

namespace tsl {
class LevelMeter;
}

namespace tslui {

enum class KnobStyle
{
  Steel,    // шлифованная сталь (по умолчанию)
  Pearl,    // перламутр
  Black,    // матовый чёрный с металлической вставкой
  Small,    // компактная стальная (вторичные параметры)
};

/**
 * Ручка: дуга значения, накатанный обод, шлифованная шапка, указатель.
 * Двойной клик — сброс к default; Cmd+клик — ввод точного значения; Shift/колесо — точная подстройка.
 */
class Knob : public IKnobControlBase
{
public:
  Knob(const IRECT& bounds, int paramIdx, const char* label, KnobStyle style = KnobStyle::Steel,
       const IColor& accent = col::accent, bool bipolar = false);

  void Draw(IGraphics& g) override;
  void OnMouseDown(float x, float y, const IMouseMod& mod) override;
  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override;
  void OnMouseOver(float x, float y, const IMouseMod& mod) override;
  void OnMouseOut() override;
  void OnResize() override;
  void SetAccent(const IColor& c) { mAccent = c; SetDirty(false); }
  void SetStyle(KnobStyle s) { mStyle = s; SetDirty(false); }
  void SetLabel(const char* l) { mLabel = l; SetDirty(false); }
  void SetLabelColor(const IColor& c) { mLabelColor = c; SetDirty(false); }

protected:
  IRECT GetKnobDragBounds() override { return mKnobRect; }

private:
  std::string mLabel;
  KnobStyle mStyle;
  IColor mAccent;
  IColor mLabelColor = col::text2;
  bool mBipolar;
  IRECT mKnobRect, mLabelRect, mValueRect;
  bool mHover = false;
};

/** Ножной переключатель/тумблер с LED. Привязывается к Bool-параметру. */
class FootSwitch : public IControl
{
public:
  FootSwitch(const IRECT& bounds, int paramIdx, const char* label = nullptr, const IColor& led = col::accent);
  void Draw(IGraphics& g) override;
  void OnMouseDown(float x, float y, const IMouseMod& mod) override;

private:
  std::string mLabel;
  IColor mLed;
};

/** Компактный LED-переключатель (pill). */
class ToggleChip : public IControl
{
public:
  ToggleChip(const IRECT& bounds, int paramIdx, const char* label, const IColor& on = col::accent);
  void Draw(IGraphics& g) override;
  void OnMouseDown(float x, float y, const IMouseMod& mod) override;

private:
  std::string mLabel;
  IColor mOn;
};

/** Выбор значения Enum-параметра: «пилюля» со стрелками и всплывающим меню. */
class EnumSelector : public IControl
{
public:
  EnumSelector(const IRECT& bounds, int paramIdx, const char* label = nullptr);
  void Draw(IGraphics& g) override;
  void OnMouseDown(float x, float y, const IMouseMod& mod) override;
  void OnPopupMenuSelection(IPopupMenu* pMenu, int valIdx) override;

private:
  std::string mLabel;
  IPopupMenu mMenu;
  IRECT mLeft, mRight, mBody;
};

/** Вертикальный измеритель уровня (peak + RMS + clip), читает LevelMeter без блокировок. */
class LevelMeterView : public IControl
{
public:
  LevelMeterView(const IRECT& bounds, tsl::LevelMeter* meter, const char* label);
  void Draw(IGraphics& g) override;
  bool IsDirty() override;
  void OnMouseDown(float x, float y, const IMouseMod& mod) override; // клик — сброс индикатора клипа

private:
  tsl::LevelMeter* mMeter;
  std::string mLabel;
  float mShownPeak = 0.f, mShownRms = 0.f, mHold = 0.f;
  int mHoldFrames = 0;
};

/** Горизонтальная индикация ослабления (gate/comp). */
class ReductionMeter : public IControl
{
public:
  ReductionMeter(const IRECT& bounds, std::function<float()> source, const char* label, float rangeDb = 24.f);
  void Draw(IGraphics& g) override;
  bool IsDirty() override;

private:
  std::function<float()> mSource;
  std::string mLabel;
  float mRange;
  float mShown = 0.f;
};

/** Навигация: сегментированная панель «жидкого стекла». */
class TabBar : public IControl
{
public:
  TabBar(const IRECT& bounds, std::vector<std::string> tabs, std::function<void(int)> onSelect);
  void Draw(IGraphics& g) override;
  void OnMouseDown(float x, float y, const IMouseMod& mod) override;
  void OnMouseOver(float x, float y, const IMouseMod& mod) override;
  void OnMouseOut() override;
  void SetSelected(int i);
  int Selected() const { return mSelected; }

private:
  int hitTest(float x) const;
  std::vector<std::string> mTabs;
  std::function<void(int)> mOnSelect;
  int mSelected = 0, mHover = -1;
  float mAnimPos = 0.f;
};

/** Статичный текст/заголовок секции. */
class Label : public IControl
{
public:
  Label(const IRECT& bounds, const char* str, const IText& text, bool caps = false);
  void Draw(IGraphics& g) override;
  void SetStr(const char* s) { mStr = s; SetDirty(false); }

private:
  std::string mStr;
  IText mText;
  bool mCaps;
};

/** Кнопка с действием (не параметр). */
class ActionButton : public IControl
{
public:
  ActionButton(const IRECT& bounds, const char* label, std::function<void()> action, bool glass = true);
  void Draw(IGraphics& g) override;
  void OnMouseDown(float x, float y, const IMouseMod& mod) override;
  void OnMouseOver(float x, float y, const IMouseMod& mod) override;
  void OnMouseOut() override;
  void SetLabelStr(const char* l) { mLabel = l; SetDirty(false); }
  void SetHighlighted(bool h) { mHighlight = h; SetDirty(false); }

private:
  std::string mLabel;
  std::function<void()> mAction;
  bool mGlass, mHover = false, mHighlight = false;
};

/** Кнопка-иконка (векторные значки: шевроны, отмена/повтор). */
class IconButton : public IControl
{
public:
  enum class Icon { Left, Right, Undo, Redo };
  IconButton(const IRECT& bounds, Icon icon, std::function<void()> action, std::function<bool()> enabled = nullptr);
  void Draw(IGraphics& g) override;
  void OnMouseDown(float x, float y, const IMouseMod& mod) override;
  void OnMouseOver(float x, float y, const IMouseMod& mod) override { mHover = true; SetDirty(false); }
  void OnMouseOut() override { mHover = false; SetDirty(false); }
  bool IsDirty() override;

private:
  Icon mIcon;
  std::function<void()> mAction;
  std::function<bool()> mEnabled;
  bool mHover = false, mShownEnabled = true;
};

/** Произвольная отрисовка (фон секции, декоративная панель). Не перехватывает мышь. */
class Canvas : public IControl
{
public:
  using DrawFn = std::function<void(IGraphics&, const IRECT&)>;
  Canvas(const IRECT& bounds, DrawFn fn) : IControl(bounds), mFn(std::move(fn)) { mIgnoreMouse = true; }
  void Draw(IGraphics& g) override { if (mFn) mFn(g, mRECT); }

private:
  DrawFn mFn;
};

} // namespace tslui
