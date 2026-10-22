// SPDX-License-Identifier: GPL-3.0-or-later
#include "TwoonemoonSoundLab.h"
#include "IPlug_include_in_plug_src.h"

#include "Cabinet/Cabinet.h"

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
    case tsl::ParamUnit::Cm: return "cm";
    default: return "";
  }
}

} // namespace

TwoonemoonSoundLab::TwoonemoonSoundLab(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(tsl::kNumParams, 0))
{
  initParams();
  for (int i = 0; i < tsl::kNumParams; ++i) mLoadedValues[static_cast<size_t>(i)] = tsl::paramSpec(i).defaultValue;

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

void TwoonemoonSoundLab::setChainConfig(const tsl::ChainConfig& cfg)
{
  mEngine.setChainConfig(cfg);
  mStateSerial.fetch_add(1);
  DirtyParametersFromUI();
  pushUndo();
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
  // v1: пути пользовательских IR (слоты A, B)
  chunk.PutStr(mUserIR[0].c_str());
  chunk.PutStr(mUserIR[1].c_str());
  // Параллельная секция: флаг, границы (индексы в порядке), путь B — по ключам модулей
  const tsl::ChainConfig cfg = mEngine.chainConfig();
  const int32_t par = cfg.parallel ? 1 : 0, ss = cfg.splitStart, se = cfg.splitEnd;
  chunk.Put(&par);
  chunk.Put(&ss);
  chunk.Put(&se);
  int32_t nb = 0;
  for (int i = 0; i < tsl::kNumModules; ++i) if (cfg.onLaneB(static_cast<tsl::ModuleId>(i))) ++nb;
  chunk.Put(&nb);
  for (int i = 0; i < tsl::kNumModules; ++i)
    if (cfg.onLaneB(static_cast<tsl::ModuleId>(i))) chunk.PutStr(tsl::moduleKey(static_cast<tsl::ModuleId>(i)));
  chunk.PutStr(mCurrentName.c_str());
  chunk.PutStr(mCurrentId.c_str());
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
    // double без потерь (IParam сам ограничивает диапазон); нечисловые значения — по умолчанию
    if (id >= 0) std::isfinite(v) ? GetParam(id)->Set(v) : GetParam(id)->SetToDefault();
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
  // Пользовательские IR: отсутствующий файл не ломает загрузку — слот покажет ошибку и предложит перепривязку
  for (int s = 0; s < 2 && p >= 0; ++s)
  {
    WDL_String path;
    const int np = chunk.GetStr(path, p);
    if (np < 0) break;
    p = np;
    if (std::string(path.Get()) != mUserIR[s]) loadUserIR(s, path.Get());
  }
  {
    tsl::ChainConfig cfg = mEngine.chainConfig();
    cfg.parallel = false;
    cfg.laneB = 0;
    int32_t par = 0, ss = 0, se = 0, nb = 0;
    if (p >= 0 && (p = chunk.Get(&par, p)) >= 0 && (p = chunk.Get(&ss, p)) >= 0 && (p = chunk.Get(&se, p)) >= 0 &&
        (p = chunk.Get(&nb, p)) >= 0 && nb >= 0 && nb <= 64)
    {
      cfg.parallel = par != 0;
      cfg.splitStart = static_cast<uint8_t>(std::clamp(ss, 0, tsl::kNumModules));
      cfg.splitEnd = static_cast<uint8_t>(std::clamp(se, static_cast<int>(cfg.splitStart), tsl::kNumModules));
      WDL_String key;
      for (int i = 0; i < nb && p >= 0; ++i)
      {
        p = chunk.GetStr(key, p);
        const tsl::ModuleId id = tsl::moduleIdForKey(key.Get());
        if (p >= 0 && id != tsl::ModuleId::Count) cfg.laneB |= 1u << static_cast<int>(id);
      }
    }
    mEngine.setChainConfig(cfg);
  }
  {
    WDL_String name, id;
    if (p >= 0 && (p = chunk.GetStr(name, p)) >= 0 && (p = chunk.GetStr(id, p)) >= 0)
    {
      mCurrentName = name.Get();
      mCurrentId = id.Get();
    }
    // Восстановленное состояние считаем «сохранённым» для индикатора изменений
    for (int i = 0; i < tsl::kNumParams; ++i) mLoadedValues[static_cast<size_t>(i)] = static_cast<float>(GetParam(i)->Value());
    mLoadedChain = mEngine.chainConfig();
  }
  mStateSerial.fetch_add(1);
  return p < 0 ? chunk.Size() : p;
}

void TwoonemoonSoundLab::loadUserIR(int slot, const std::string& path)
{
  slot &= 1;
  mUserIR[slot] = path;
  if (auto* cab = static_cast<tsl::Cabinet*>(mEngine.module(tsl::ModuleId::Cabinet))) cab->requestUserIR(slot, path);
  mStateSerial.fetch_add(1);
}

void TwoonemoonSoundLab::OnParamChangeUI(int paramIdx, EParamSource)
{
  if (mUIParamListener) mUIParamListener(paramIdx);
}

void TwoonemoonSoundLab::OnUIClose()
{
  mUIParamListener = nullptr;
}

#if defined(CLAP_API)
bool TwoonemoonSoundLab::stateSave(const clap_ostream* pStream) noexcept
{
  IByteChunk chunk;
  if (!SerializeState(chunk)) return false;
  // Поток может принимать данные порциями — пишем до конца
  const uint8_t* data = chunk.GetData();
  int64_t left = chunk.Size();
  while (left > 0)
  {
    const int64_t w = pStream->write(pStream, data, static_cast<uint64_t>(left));
    if (w <= 0) return false;
    data += w;
    left -= w;
  }
  return true;
}

bool TwoonemoonSoundLab::stateLoad(const clap_istream* pStream) noexcept
{
  IByteChunk chunk;
  char buffer[256];
  int64_t bytesRead = 0;
  while ((bytesRead = pStream->read(pStream, buffer, sizeof(buffer))) > 0) chunk.PutBytes(buffer, static_cast<int>(bytesRead));
  if (bytesRead != 0 || chunk.Size() == 0) return false; // ошибка потока или пустое состояние
  // Чужие/повреждённые данные отклоняем (текущее состояние остаётся прежним)
  uint32_t magic = 0;
  if (chunk.Get(&magic, 0) < 0 || magic != kStateMagic) return false;
  const bool ok = UnserializeState(chunk, 0) > 0;
  if (ok) OnRestoreState();
  // Значения параметров изменились вне событий хоста — по спецификации CLAP нужно запросить rescan
  if (ok)
  {
    const clap_host* h = _host.host();
    const auto* params = h ? static_cast<const clap_host_params*>(h->get_extension(h, CLAP_EXT_PARAMS)) : nullptr;
    if (params && params->rescan) params->rescan(h, CLAP_PARAM_RESCAN_VALUES | CLAP_PARAM_RESCAN_TEXT);
  }
  return ok;
}

bool TwoonemoonSoundLab::paramsValueToText(clap_id paramIdx, double value, char* display, uint32_t size) noexcept
{
  // Ступенчатые параметры (Bool/Enum/Int) отображаются для округлённого значения
  if (paramIdx >= static_cast<clap_id>(NParams())) return false;
  const IParam* pParam = GetParam(static_cast<int>(paramIdx));
  const bool isDouble = pParam->Type() == IParam::kTypeDouble;
  if (!isDouble) value = std::round(value);
  WDL_String str;
  pParam->GetDisplay(value, isDouble, str);
  if (CStringHasContents(pParam->GetLabel()))
  {
    str.Append(" ");
    str.Append(pParam->GetLabel());
  }
  if (size <= static_cast<uint32_t>(str.GetLength())) return false;
  strcpy(display, str.Get());
  return true;
}

bool TwoonemoonSoundLab::audioPortsGetConfig(uint32_t index, clap_audio_ports_config* pConfig) const noexcept
{
  // Исправленная версия: у каждой конфигурации ровно один главный вход и выход
  if (index >= static_cast<uint32_t>(NIOConfigs())) return false;
  const IOConfig* cfg = GetIOConfig(static_cast<int>(index));
  const uint32_t nIn = static_cast<uint32_t>(cfg->NChansOnBusSAFE(ERoute::kInput, 0));
  const uint32_t nOut = static_cast<uint32_t>(cfg->NChansOnBusSAFE(ERoute::kOutput, 0));
  auto portType = [](uint32_t n) { return n == 2 ? CLAP_PORT_STEREO : (n == 1 ? CLAP_PORT_MONO : nullptr); };
  pConfig->id = index;
  snprintf(pConfig->name, CLAP_NAME_SIZE, "%u-%u", nIn, nOut);
  pConfig->input_port_count = static_cast<uint32_t>(cfg->NBuses(ERoute::kInput));
  pConfig->output_port_count = static_cast<uint32_t>(cfg->NBuses(ERoute::kOutput));
  pConfig->has_main_input = pConfig->input_port_count >= 1;
  pConfig->main_input_channel_count = pConfig->has_main_input ? nIn : 0;
  pConfig->main_input_port_type = portType(pConfig->main_input_channel_count);
  pConfig->has_main_output = pConfig->output_port_count >= 1;
  pConfig->main_output_channel_count = pConfig->has_main_output ? nOut : 0;
  pConfig->main_output_port_type = portType(pConfig->main_output_channel_count);
  return true;
}
#endif

void TwoonemoonSoundLab::OnRestoreState()
{
  iplug::Plugin::OnRestoreState();
}

// ---------------------------------------------------------------------------

void TwoonemoonSoundLab::OnIdle()
{
  const int st = mPresetState.load(std::memory_order_acquire);
  if (st == kPresetApplied)
  {
    finishPresetLoad();
  }
  else if (st == kPresetRequested || st == kPresetFading)
  {
    // Резерв: если аудиопоток не идёт (устройство остановлено, хост не вызывает обработку),
    // применяем пресет прямо здесь.
    const auto waited = std::chrono::steady_clock::now() - mRequestTime;
    if (mEngine.processedBlocks() == mRequestBlocks && waited > std::chrono::milliseconds(150))
    {
      applyNow(mPendingValues, mPendingChain);
      mPresetState.store(kPresetApplied, std::memory_order_release);
      finishPresetLoad();
    }
  }
}

// ---------------------------------------------------------------------------
// Пресеты

tsl::PresetLibrary& TwoonemoonSoundLab::library()
{
  if (!mLibrary) mLibrary = std::make_unique<tsl::PresetLibrary>();
  return *mLibrary;
}

std::vector<const tsl::Preset*> TwoonemoonSoundLab::allPresets()
{
  std::vector<const tsl::Preset*> v;
  for (const auto& p : tsl::factoryPresets()) v.push_back(&p);
  for (const auto& p : library().userPresets()) v.push_back(&p);
  return v;
}

tsl::Preset TwoonemoonSoundLab::snapshot() const
{
  tsl::ParamValues v{};
  for (int i = 0; i < tsl::kNumParams; ++i) v[static_cast<size_t>(i)] = static_cast<float>(GetParam(i)->Value());
  tsl::Preset p = tsl::makePresetFromState(v, mEngine.chainConfig(), mUserIR[0], mUserIR[1]);
  p.name = mCurrentName;
  return p;
}

namespace {
bool isGlobalParam(int i)
{
  switch (i)
  {
    case tsl::kInputTrim:
    case tsl::kOutputTrim:
    case tsl::kBypass:
    case tsl::kInputMode:
    case tsl::kTunerMute:
    case tsl::kTunerRef:
    case tsl::kTempo:
    case tsl::kHostTempo:
    case tsl::kOversampling: return true;
    default: return false;
  }
}
} // namespace

bool TwoonemoonSoundLab::presetModified() const
{
  for (int i = 0; i < tsl::kNumParams; ++i)
  {
    if (isGlobalParam(i)) continue;
    if (std::fabs(static_cast<float>(GetParam(i)->Value()) - mLoadedValues[static_cast<size_t>(i)]) > 1e-4f) return true;
  }
  return !(mEngine.chainConfig() == mLoadedChain);
}

void TwoonemoonSoundLab::markSaved(const tsl::Preset& p)
{
  mCurrentName = p.name;
  mCurrentId = p.id();
  for (int i = 0; i < tsl::kNumParams; ++i) mLoadedValues[static_cast<size_t>(i)] = static_cast<float>(GetParam(i)->Value());
  mLoadedChain = mEngine.chainConfig();
  if (mUIParamListener) mUIParamListener(-2);
}

void TwoonemoonSoundLab::loadPreset(const tsl::Preset& p, bool withFade)
{
  if (mPresetState.load(std::memory_order_acquire) != kPresetIdle)
  {
    // Идёт переход: буфер ожидания принадлежит audio thread — ставим запрос в очередь
    mQueued = std::make_unique<tsl::Preset>(p);
    mQueuedFade = withFade;
    return;
  }
  tsl::ParamValues v = tsl::resolvePresetParams(p);
  // Глобальные параметры (Input/Output, Bypass, темп, качество) пресет не меняет
  for (int i = 0; i < tsl::kNumParams; ++i)
    if (isGlobalParam(i)) v[static_cast<size_t>(i)] = static_cast<float>(GetParam(i)->Value());
  const tsl::ChainConfig chain = p.hasChain ? p.chain : tsl::ChainConfig{};
  mCurrentName = p.name;
  mCurrentId = p.id();
  mLoadedValues = v;
  mLoadedChain = chain;
  mPendingIR[0] = p.irA;
  mPendingIR[1] = p.irB;
  if (!p.id().empty() && mLibrary) mLibrary->pushRecent(p.id());
  else library().pushRecent(p.id());

  if (!withFade)
  {
    // Без затухания (undo/redo): параметры сглаживаются модулями
    applyNow(v, chain);
    mPresetState.store(kPresetApplied, std::memory_order_release);
    finishPresetLoad();
    return;
  }
  mPendingValues = v;
  mPendingChain = chain;
  mRequestBlocks = mEngine.processedBlocks();
  mRequestTime = std::chrono::steady_clock::now();
  mPresetState.store(kPresetRequested, std::memory_order_release);
}

void TwoonemoonSoundLab::applyNow(const tsl::ParamValues& v, const tsl::ChainConfig& chain)
{
  for (int i = 0; i < tsl::kNumParams; ++i) GetParam(i)->Set(v[static_cast<size_t>(i)]);
  mEngine.setChainConfig(chain);
}

void TwoonemoonSoundLab::finishPresetLoad()
{
  for (int s = 0; s < 2; ++s)
    if (mPendingIR[s] != mUserIR[s]) loadUserIR(s, mPendingIR[s]);
  mStateSerial.fetch_add(1);
  SendCurrentParamValuesFromDelegate();
  InformHostOfPresetChange();
  DirtyParametersFromUI();
  mPresetState.store(kPresetIdle, std::memory_order_release);
  if (!mApplyingHistory) pushUndo();
  if (mUIParamListener) mUIParamListener(-2);
  if (mQueued)
  {
    auto q = std::move(mQueued);
    loadPreset(*q, mQueuedFade);
  }
}

void TwoonemoonSoundLab::handlePresetAudio()
{
  const int st = mPresetState.load(std::memory_order_acquire);
  if (st == kPresetRequested)
  {
    mEngine.beginTransitionFade();
    mPresetState.store(kPresetFading, std::memory_order_release);
  }
  else if (st == kPresetFading && mEngine.transitionSilent())
  {
    // Тишина: подменяем все параметры и граф, сбрасываем хвосты, затем нарастание
    for (int i = 0; i < tsl::kNumParams; ++i) GetParam(i)->Set(mPendingValues[static_cast<size_t>(i)]);
    mEngine.setChainConfig(mPendingChain);
    mEngine.reset();
    mEngine.endTransitionFade();
    mPresetState.store(kPresetApplied, std::memory_order_release);
  }
}

void TwoonemoonSoundLab::stepPreset(int dir)
{
  const auto all = allPresets();
  if (all.empty()) return;
  int idx = -1;
  for (size_t i = 0; i < all.size(); ++i)
    if (all[i]->id() == mCurrentId) idx = static_cast<int>(i);
  idx = idx < 0 ? 0 : (idx + dir + static_cast<int>(all.size())) % static_cast<int>(all.size());
  loadPreset(*all[static_cast<size_t>(idx)]);
}

void TwoonemoonSoundLab::switchAB()
{
  mAB[mABActive] = snapshot();
  mABValid[mABActive] = true;
  mABActive ^= 1;
  if (mABValid[mABActive])
  {
    const std::string keepName = mCurrentName, keepId = mCurrentId;
    loadPreset(mAB[mABActive]);
    mCurrentName = keepName;
    mCurrentId = keepId;
  }
  else
  {
    mAB[mABActive] = mAB[mABActive ^ 1];
    mABValid[mABActive] = true;
    if (mUIParamListener) mUIParamListener(-2);
  }
}

void TwoonemoonSoundLab::copyToOtherAB()
{
  mAB[mABActive ^ 1] = snapshot();
  mABValid[mABActive ^ 1] = true;
  if (mUIParamListener) mUIParamListener(-2);
}

void TwoonemoonSoundLab::pushUndo()
{
  tsl::Preset s = snapshot();
  if (mUndoPos >= 0 && mUndoPos < static_cast<int>(mUndo.size()))
  {
    const auto& cur = mUndo[static_cast<size_t>(mUndoPos)];
    if (cur.params == s.params && cur.chain == s.chain) return; // ничего не изменилось
  }
  mUndo.resize(static_cast<size_t>(mUndoPos + 1));
  mUndo.push_back(std::move(s));
  if (mUndo.size() > 100) mUndo.erase(mUndo.begin());
  mUndoPos = static_cast<int>(mUndo.size()) - 1;
}

bool TwoonemoonSoundLab::undo()
{
  if (!canUndo()) return false;
  --mUndoPos;
  mApplyingHistory = true;
  const std::string name = mCurrentName, id = mCurrentId;
  const tsl::ParamValues loaded = mLoadedValues;
  const tsl::ChainConfig loadedChain = mLoadedChain;
  loadPreset(mUndo[static_cast<size_t>(mUndoPos)], false);
  mCurrentName = name; mCurrentId = id; mLoadedValues = loaded; mLoadedChain = loadedChain;
  mApplyingHistory = false;
  return true;
}

bool TwoonemoonSoundLab::redo()
{
  if (!canRedo()) return false;
  ++mUndoPos;
  mApplyingHistory = true;
  const std::string name = mCurrentName, id = mCurrentId;
  const tsl::ParamValues loaded = mLoadedValues;
  const tsl::ChainConfig loadedChain = mLoadedChain;
  loadPreset(mUndo[static_cast<size_t>(mUndoPos)], false);
  mCurrentName = name; mCurrentId = id; mLoadedValues = loaded; mLoadedChain = loadedChain;
  mApplyingHistory = false;
  return true;
}

void TwoonemoonSoundLab::EndInformHostOfParamChangeFromUI(int paramIdx)
{
  iplug::Plugin::EndInformHostOfParamChangeFromUI(paramIdx);
  pushUndo();
}

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
  // Сообщать хосту только об изменении (CLAP: latency.changed допустим лишь внутри activate)
  const int lat = mEngine.latencySamples();
  if (lat != GetLatency()) SetLatency(lat);
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

  handlePresetAudio();
  for (int i = 0; i < tsl::kNumParams; ++i) mEngine.setParam(i, static_cast<float>(GetParam(i)->Value()));
  updateTailSize();

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

void TwoonemoonSoundLab::updateTailSize()
{
  // «Хвост» после затихания входа: фильтры/усилитель/кабинет + задержка + реверберация.
  // Хост может перестать вызывать обработку только после этого времени тишины на входе.
  const double sr = GetSampleRate();
  auto on = [this](int id) { return GetParam(id)->Bool(); };
  if (on(tsl::kRvbOn) && on(tsl::kRvbFreeze))
  {
    if (!GetTailIsInfinite()) SetTailSize(kTailInfinite);
    return;
  }
  double sec = 0.5; // кабинет, усилитель, модуляции, сдвиг высоты
  if (on(tsl::kDlyOn))
  {
    const double fb = std::clamp(GetParam(tsl::kDlyFeedback)->Value() * 0.01, 0.0, 0.98);
    const double t = std::max(0.001, mEngine.isPrepared() ? 2.0 : 2.0); // максимум времени задержки
    const double repeats = fb > 0.001 ? std::log(0.001) / std::log(fb) : 1.0; // до −60 дБ
    sec += t * std::min(repeats, 400.0);
  }
  if (on(tsl::kRvbOn)) sec += GetParam(tsl::kRvbDecay)->Value() * 1.5;
  const int samples = static_cast<int>(std::min(sec, 600.0) * sr);
  const int cur = GetTailSize();
  if (GetTailIsInfinite() || std::abs(samples - cur) > cur / 10 + 1) SetTailSize(samples);
}

void TwoonemoonSoundLab::ProcessMidiMsg(const IMidiMsg& msg)
{
  // MIDI Learn / педаль экспрессии — Milestone 7. Пока входящие сообщения принимаются и игнорируются.
  (void)msg;
}
#endif
