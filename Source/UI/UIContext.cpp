// SPDX-License-Identifier: GPL-3.0-or-later
#include "UIContext.h"

#include "TwoonemoonSoundLab.h"

namespace tslui {

void UIContext::setParam(int idx, double plain)
{
  IParam* p = plugin->GetParam(idx);
  const double norm = p->ToNormalized(plain);
  plugin->SendParameterValueFromUI(idx, norm);
  g->ForControlWithParam(idx, [norm](IControl* c) { c->SetValueFromDelegate(norm); });
}

void UIContext::beginEdit(int idx) { plugin->BeginInformHostOfParamChangeFromUI(idx); }
void UIContext::endEdit(int idx) { plugin->EndInformHostOfParamChangeFromUI(idx); }

} // namespace tslui
