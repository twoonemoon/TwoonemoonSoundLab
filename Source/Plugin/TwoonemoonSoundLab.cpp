// SPDX-License-Identifier: GPL-3.0-or-later
#include "TwoonemoonSoundLab.h"
#include "IPlug_include_in_plug_src.h"

#if IPLUG_EDITOR
#include "UI/MainUI.h"
#endif

using namespace iplug;
#if IPLUG_EDITOR
using namespace iplug::igraphics;
#endif

namespace {

constexpr uint32_t kStateMagic = 0x544D534Cu; // 'TMSL'
constexpr int32_t kStateVersion = 1;

const char* unitLabel(tsl::ParamUnit u)
{
  switch (u)
  {
    case tsl::ParamUnit::dB: return "dB";
    case tsl::ParamUnit::Percent: return "%";
    case tsl::ParamUnit::Hz: return "Hz";
    case tsl::ParamUnit::Ms: return "ms";
    case tsl::ParamUnit::Seconds: return "s";
    case tsl::ParamUnit::Semitones: return "st";
    case tsl::ParamUnit::Cents: return "ct";
    case tsl::ParamUnit::Ratio: return ":1";
    case tsl::ParamUnit::Bpm: return "BPM";
    case tsl::ParamUnit::Degrees: return "deg";
    default: return "";
  }
}

} // namespace

TwoonemoonSoundLab::TwoonemoonSoundLab(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(tsl::kNumParams, 0))
{
  initParams();

#if IPLUG_EDITOR
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, 1.f);
  };
  mLayoutFunc = [&](IGraphics* pGraphics) { tslui::buildMainUI(pGraphics, *this); };
#endif
}

void TwoonemoonSoundLab::initParams()
{
  for (int i = 0; i < tsl::kNumParams; ++i)
  {
    const tsl::ParamSpec& s = tsl::paramSpec(i);
    IParam* p = GetParam(i);
    switch (s.kind)
    {
      case tsl::ParamKind::Bool:
        p->InitBool(s.name, s.defaultValue >= 0.5f, "", IParam::kFlagsNone, s.group, "Off", "On");
        break;
      case tsl::ParamKind::Enum:
        p->InitEnum(s.name, static_cast<int>(s.defaultValue), s.numLabels, "", IParam::kFlagsNone, s.group);
        for (int e = 0; e < s.numLabels; ++e) p->SetDisplayText(e, s.labels[e]);
        break;
      case tsl::ParamKind::Int:
        p->InitInt(s.name, static_cast<int>(s.defaultValue), static_cast<int>(s.minValue), static_cast<int>(s.maxValue),
                   unitLabel(s.unit), IParam::kFlagsNone, s.group);
        break;
      case tsl::ParamKind::Float:
      default:
      {
        IParam::Shape* shape = nullptr;
        IParam::ShapeLinear lin;
        IParam::ShapeExp ex;
        IParam::ShapePowCurve p2(2.0), p3(3.0);
        switch (s.shape)
        {
          case tsl::ParamShape::Exp: shape = &ex; break;
          case tsl::ParamShape::Pow2: shape = &p2; break;
          case tsl::ParamShape::Pow3: shape = &p3; break;
          default: shape = &lin; break;
        }
        p->InitDouble(s.name, s.defaultValue, s.minValue, s.maxValue, s.step, unitLabel(s.unit), IParam::kFlagsNone,
                      s.group, *shape);
        break;
      }
    }
  }
}

void TwoonemoonSoundLab::setChainOrder(const tsl::ChainOrder& order)
{
  mEngine.setChainOrder(order);
  mStateSerial.fetch_add(1);
  // Сообщить хосту, что состояние изменилось (проект нужно сохранить)
  DirtyParametersFromUI();
}

// ---------------------------------------------------------------------------
// Состояние: параметры сохраняются по строковым ключам (устойчиво к расширению списка
// параметров), затем порядок цепочки. Формат версионируется.

bool TwoonemoonSoundLab::SerializeState(IByteChunk& chunk) const
{
  chunk.Put(&kStateMagic);
  chunk.Put(&kStateVersion);
  const int32_t n = tsl::kNumParams;
  chunk.Put(&n);
  for (int i = 0; i < tsl::kNumParams; ++i)
  {
    chunk.PutStr(tsl::paramSpec(i).key);
    const double v = GetParam(i)->Value();
    chunk.Put(&v);
  }
  const tsl::ChainOrder order = mEngine.chainOrder();
  const int32_t nm = tsl::kNumModules;
  chunk.Put(&nm);
  for (tsl::ModuleId id : order) chunk.PutStr(tsl::moduleKey(id));
  return true;
}

int TwoonemoonSoundLab::UnserializeState(const IByteChunk& chunk, int pos)
{
  uint32_t magic = 0;
  int32_t version = 0, n = 0;
  int p = chunk.Get(&magic, pos);
  if (p < 0 || magic != kStateMagic)
  {
    // Неизвестный формат — оставляем текущие значения (восстановление после повреждённых данных)
    return pos;
  }
  p = chunk.Get(&version, p);
  p = chunk.Get(&n, p);
  if (p < 0 || version < 1 || n < 0 || n > 100000) return pos;

  // Параметры, отсутствующие в сохранении, получают значения по умолчанию
  for (int i = 0; i < tsl::kNumParams; ++i) GetParam(i)->SetToDefault();
  WDL_String key;
  for (int i = 0; i < n && p >= 0; ++i)
  {
    double v = 0.0;
    p = chunk.GetStr(key, p);
    if (p < 0) break;
    p = chunk.Get(&v, p);
    if (p < 0) break;
    const int id = tsl::paramIdForKey(key.Get());
    if (id >= 0) GetParam(id)->Set(tsl::paramClamp(id, static_cast<float>(v)));
  }

  int32_t nm = 0;
  if (p >= 0) p = chunk.Get(&nm, p);
  if (p >= 0 && nm > 0 && nm <= 64)
  {
    // Модули, которых нет в сохранении (появились в новой версии), добавляются в конец по умолчанию
    tsl::ChainOrder order{};
    std::array<bool, tsl::kNumModules> used{};
    int count = 0;
    for (int i = 0; i < nm && p >= 0; ++i)
    {
      p = chunk.GetStr(key, p);
      if (p < 0) break;
      const tsl::ModuleId id = tsl::moduleIdForKey(key.Get());
      if (id != tsl::ModuleId::Count && !used[static_cast<size_t>(id)] && count < tsl::kNumModules)
      {
        order[static_cast<size_t>(count++)] = id;
        used[static_cast<size_t>(id)] = true;
      }
    }
    for (tsl::ModuleId id : tsl::defaultChainOrder())
      if (!used[static_cast<size_t>(id)] && count < tsl::kNumModules) order[static_cast<size_t>(count++)] = id;
    if (count == tsl::kNumModules) mEngine.setChainOrder(order);
  }
  mStateSerial.fetch_add(1);
  return p < 0 ? chunk.Size() : p;
}

void TwoonemoonSoundLab::OnRestoreState()
{
  iplug::Plugin::OnRestoreState();
}

// ---------------------------------------------------------------------------

void TwoonemoonSoundLab::OnIdle() {}

#if IPLUG_DSP
void TwoonemoonSoundLab::OnReset()
{
  const int block = std::max(GetBlockSize(), 32);
  const double sr = GetSampleRate();
  mBufSize = std::max(block, 512);
  for (int c = 0; c < 2; ++c)
  {
    mInBuf[c].assign(static_cast<size_t>(mBufSize), 0.f);
    mOutBuf[c].assign(static_cast<size_t>(mBufSize), 0.f);
  }
  mEngine.prepare(sr, mBufSize);
  SetLatency(mEngine.latencySamples());
}

void TwoonemoonSoundLab::OnActivate(bool active)
{
  if (active) mEngine.reset();
}

void TwoonemoonSoundLab::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  const int nIn = std::min(NInChansConnected(), 2);
  const int nOutTotal = NOutChansConnected();
  const int nOut = std::min(nOutTotal, 2);

  if (!mEngine.isPrepared() || mBufSize == 0)
  {
    for (int c = 0; c < nOutTotal; ++c) std::fill(outputs[c], outputs[c] + nFrames, 0.0);
    return;
  }

  for (int i = 0; i < tsl::kNumParams; ++i) mEngine.setParam(i, static_cast<float>(GetParam(i)->Value()));

  tsl::TransportInfo t;
  t.tempo = GetTempo();
  t.ppqPos = GetPPQPos();
  t.playing = GetTransportIsRunning();
  t.hostTempoValid = t.tempo > 1.0;

  int offset = 0;
  while (offset < nFrames)
  {
    const int n = std::min(mBufSize, nFrames - offset);
    for (int c = 0; c < nIn; ++c)
      for (int i = 0; i < n; ++i) mInBuf[c][static_cast<size_t>(i)] = static_cast<float>(inputs[c][offset + i]);
    const float* in[2] = { mInBuf[0].data(), mInBuf[1].data() };
    float* out[2] = { mOutBuf[0].data(), mOutBuf[1].data() };
    mEngine.process(in, nIn, out, std::max(nOut, 1), n, t);
    for (int c = 0; c < nOut; ++c)
      for (int i = 0; i < n; ++i) outputs[c][offset + i] = static_cast<sample>(mOutBuf[c][static_cast<size_t>(i)]);
    for (int c = nOut; c < nOutTotal; ++c)
      for (int i = 0; i < n; ++i) outputs[c][offset + i] = 0.0;
    offset += n;
  }
}

void TwoonemoonSoundLab::ProcessMidiMsg(const IMidiMsg& msg)
{
  // MIDI Learn / педаль экспрессии — Milestone 7. Пока входящие сообщения принимаются и игнорируются.
  (void)msg;
}
#endif
