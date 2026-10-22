// SPDX-License-Identifier: GPL-3.0-or-later
// Заводской банк TwoonemoonSoundLab: 60 пресетов. Каждый задаёт полную цепочку
// (неуказанные параметры = значения по умолчанию: включены Gate, Amp, Cab A).
// Громкость выровнена по рендеру синтетического гитарного сигнала (Tests/Renders), а не на слух —
// список для прослушивания на реальной гитаре — docs/TESTING.md.
#include "Preset.h"

#include <map>

namespace tsl {

namespace {

// Индексы перечислений (см. labels в Params.h)
enum Amp { LUMEN, OPAL, GLASS, VELVET, BRASS, RUST, THORN, IRON, CATACOMB, ABYSS, SILVER, CEDAR };
enum Spk { AURUM, LUMEN212, MONOLITH, VANTA, CEDAR212, EMBER };
enum Mic { DYN, COND, RIB };
enum Dly { DIGITAL, TAPE, ANALOG, PINGPONG, MULTITAP };
enum Div { D1, D2, D2D, D2T, D4, D4D, D4T, D8, D8D, D8T, D16, D16D, D16T, D32 };
enum Rvb { ROOM, HALL, PLATE, SPRING, SHIMMER };
enum Key { C, Cs, D_, Ds, E, F, Fs, G, Gs, A, As, B };
enum Scale { MAJOR, NMINOR, HMINOR, MMINOR, DORIAN, PHRYGIAN, LYDIAN, MIXOLYDIAN, LOCRIAN, MAJPENTA, MINPENTA, BLUES, CHROMATIC };
enum Interval { IV_MOCT, IV_M7, IV_M6, IV_M5, IV_M4, IV_M3, IV_M2, IV_UNI, IV_P2, IV_P3, IV_P4, IV_P5, IV_P6, IV_P7, IV_POCT, IV_P10 };

struct KV
{
  const char* key;
  float value;
};

struct ChainSpec
{
  std::vector<const char*> order; // пусто — порядок по умолчанию
  bool parallel = false;
  int start = 0, end = 0;
  std::vector<const char*> laneB;
};

// Поправки громкости (дБ к Amp Volume), рассчитаны tsl_render --calibrate: цель −20 дБFS RMS
// на синтетическом гитарном сигнале с пиком −12 дБFS. Пересчитывать при изменении DSP.
const std::map<std::string, float>& levelTrims()
{
  static const std::map<std::string, float> t = {
    { "Glass Morning", -1.4f },
    { "Lumen Single Coil", -8.5f },
    { "Opal Neck Warmth", -0.9f },
    { "Crystal Arpeggio", -2.4f },
    { "Studio DI Clean", -2.7f },
    { "Funk Pocket", -4.1f },
    { "Humbucker Jazz", 0.2f },
    { "Chorus Lagoon", 3.2f },
    { "Tremolo Surf", -5.2f },
    { "Velvet Clean Edge", -5.5f },
    { "Brass Rhythm", -7.7f },
    { "Velvet Breakup", -7.5f },
    { "Rust Royale", -8.0f },
    { "Arena Stack", -10.6f },
    { "Edge Garden", -16.0f },
    { "Indie Jangle Crunch", -5.4f },
    { "Seventies Arena", -8.2f },
    { "Blues Drive Push", -10.5f },
    { "Thorn Rhythm", -7.8f },
    { "Iron Veil Chug", -8.4f },
    { "Catacomb Djent", -8.3f },
    { "Abyss Lead", -8.0f },
    { "Modern Metal Tight", -11.8f },
    { "Drop Tuned Wall", -10.1f },
    { "Thrash Precision", -8.6f },
    { "Hard Rock Lead", -8.2f },
    { "Sludge Fuzz", -7.2f },
    { "Progressive Lead", -7.7f },
    { "Metalcore Breakdown", -8.4f },
    { "Shred Machine", -7.5f },
    { "Misty Valley", -2.1f },
    { "Shoegaze Haze", -4.6f },
    { "Cathedral Swell", 1.2f },
    { "Post-Rock Builder", -9.8f },
    { "Frozen Pad", 1.0f },
    { "Tape Echo Dream", -3.5f },
    { "Dream Pop Chorus", 2.7f },
    { "Infinite Shimmer", -1.5f },
    { "Underwater Phase", -0.4f },
    { "Granite Fog", -6.3f },
    { "Pop Rhythm Polish", 0.3f },
    { "Blues Tone Room", -7.5f },
    { "Velvet Lead Sing", -9.5f },
    { "Texas Push", -9.6f },
    { "Soul Comp Clean", 0.4f },
    { "Expressive Lead Wah", -9.7f },
    { "Silver Stage", -2.4f },
    { "Cedar Living Room", -3.8f },
    { "Piezo Fix", -2.2f },
    { "Acoustic Shimmer Strum", -1.7f },
    { "Harmony Thirds Lead", -8.2f },
    { "Twin Lead Fifths", -8.6f },
    { "Octave Fuzz Synth", -6.4f },
    { "Sub Bass Guitar", -2.5f },
    { "12-String Sparkle", 5.5f },
    { "Choir of Fifths", -2.1f },
    { "Major Pentatonic Duet", -4.7f },
    { "Rotary Organ Grind", -0.6f },
    { "Glitch Ring Flanger", -7.7f },
    { "Parallel Grit & Air", -8.4f },
  };
  return t;
}

Preset make(const char* name, const char* cat, const char* desc, std::vector<std::string> tags, std::vector<KV> kv,
            const ChainSpec& chain = {})
{
  Preset p;
  p.name = name;
  p.category = cat;
  p.description = desc;
  p.author = "Twoonemoon";
  p.tags = std::move(tags);
  p.factory = true;
  for (const auto& e : kv) p.params[e.key] = e.value;
  // Выравнивание громкости между пресетами — через Amp Volume (пользовательский Output остаётся на 0 дБ)
  const auto& trims = levelTrims();
  if (auto it = trims.find(name); it != trims.end())
  {
    const float base = p.params.count("amp.level") ? p.params["amp.level"] : 0.f;
    p.params["amp.level"] = clampv(base + it->second, -24.f, 12.f);
  }
  if (!chain.order.empty() || chain.parallel)
  {
    ChainConfig cfg;
    if (!chain.order.empty())
    {
      std::array<bool, kNumModules> used{};
      int n = 0;
      for (const char* k : chain.order)
      {
        const ModuleId id = moduleIdForKey(k);
        if (id != ModuleId::Count && !used[static_cast<size_t>(id)]) { cfg.order[static_cast<size_t>(n++)] = id; used[static_cast<size_t>(id)] = true; }
      }
      for (ModuleId id : defaultChainOrder())
        if (!used[static_cast<size_t>(id)]) cfg.order[static_cast<size_t>(n++)] = id;
    }
    cfg.parallel = chain.parallel;
    cfg.splitStart = static_cast<uint8_t>(chain.start);
    cfg.splitEnd = static_cast<uint8_t>(chain.end);
    for (const char* k : chain.laneB)
    {
      const ModuleId id = moduleIdForKey(k);
      if (id != ModuleId::Count) cfg.laneB |= 1u << static_cast<int>(id);
    }
    p.hasChain = true;
    p.chain = cfg;
  }
  return p;
}

std::vector<Preset> build()
{
  std::vector<Preset> v;
  v.reserve(64);

  // =========================================================================== CLEAN (10)
  v.push_back(make("Glass Morning", "clean", "Стеклянный clean с лёгкой компрессией и короткой пластиной. Звучит открыто на одиночных нотах и аккордах.",
                   { "clean", "single coil", "studio" },
                   { { "amp.model", GLASS }, { "amp.gain", 3.5f }, { "amp.bass", 4.5f }, { "amp.treble", 6.f }, { "amp.presence", 6.f },
                     { "comp.on", 1 }, { "comp.threshold", -26 }, { "comp.ratio", 3 }, { "comp.mix", 70 },
                     { "cab.a.speaker", LUMEN212 }, { "cab.a.mic", COND }, { "cab.a.position", 25 }, { "cab.a.distance", 6 },
                     { "rvb.on", 1 }, { "rvb.mode", PLATE }, { "rvb.mix", 16 }, { "rvb.decay", 1.6f }, { "gate.threshold", -70 } }));
  v.push_back(make("Lumen Single Coil", "clean", "Яркий запасной clean для синглов: звонкая атака и пружинное эхо в глубине.",
                   { "clean", "single coil", "spring" },
                   { { "amp.model", LUMEN }, { "amp.gain", 4.f }, { "amp.bass", 5.f }, { "amp.mid", 4.5f }, { "amp.treble", 6.5f }, { "amp.bright", 1 },
                     { "cab.a.speaker", AURUM }, { "cab.a.position", 35 },
                     { "rvb.on", 1 }, { "rvb.mode", SPRING }, { "rvb.mix", 22 }, { "rvb.decay", 2.f } }));
  v.push_back(make("Opal Neck Warmth", "clean", "Тёплый, округлый clean под звукосниматель у грифа. Мягкий верх, плотная середина.",
                   { "clean", "humbucker", "warm" },
                   { { "amp.model", OPAL }, { "amp.gain", 4.5f }, { "amp.treble", 4.5f }, { "amp.bass", 5.5f },
                     { "comp.on", 1 }, { "comp.threshold", -24 }, { "comp.ratio", 2.5f },
                     { "cab.a.speaker", CEDAR212 }, { "cab.a.mic", RIB }, { "cab.a.position", 45 }, { "cab.a.distance", 8 },
                     { "rvb.on", 1 }, { "rvb.mode", ROOM }, { "rvb.mix", 18 }, { "rvb.decay", 1.1f } }));
  v.push_back(make("Crystal Arpeggio", "clean", "Clean для переборов: эхо на пунктирную восьмую и широкий холл.",
                   { "clean", "delay", "arpeggio" },
                   { { "amp.model", GLASS }, { "amp.gain", 3.f }, { "amp.treble", 6.f },
                     { "cab.a.speaker", LUMEN212 }, { "cab.a.mic", COND },
                     { "dly.on", 1 }, { "dly.mode", DIGITAL }, { "dly.sync", 1 }, { "dly.division", D8D }, { "dly.feedback", 38 }, { "dly.mix", 28 }, { "dly.width", 80 },
                     { "rvb.on", 1 }, { "rvb.mode", HALL }, { "rvb.mix", 24 }, { "rvb.decay", 3.2f } }));
  v.push_back(make("Studio DI Clean", "clean", "Сухой чистый звук для записи: без пространства, с аккуратным эквалайзером.",
                   { "clean", "dry", "studio" },
                   { { "amp.model", LUMEN }, { "amp.gain", 3.f }, { "amp.treble", 5.5f },
                     { "cab.a.speaker", MONOLITH }, { "cab.a.mic", DYN }, { "cab.a.position", 30 },
                     { "eq.on", 1 }, { "eq.lowCut", 80 }, { "eq.b2.freq", 300 }, { "eq.b2.gain", -2 }, { "eq.b4.freq", 3200 }, { "eq.b4.gain", 1.5f } }));
  v.push_back(make("Funk Pocket", "clean", "Плотный перкуссионный clean: компрессор и автовау для ритма.",
                   { "clean", "funk", "auto-wah" },
                   { { "amp.model", LUMEN }, { "amp.gain", 3.5f }, { "amp.mid", 6.f },
                     { "comp.on", 1 }, { "comp.threshold", -30 }, { "comp.ratio", 6 }, { "comp.attack", 4 },
                     { "env.on", 1 }, { "env.sensitivity", 6 }, { "env.q", 5 },
                     { "cab.a.speaker", EMBER } }));
  v.push_back(make("Humbucker Jazz", "clean", "Мягкий джазовый clean: приглушённый верх, тёплая комната.",
                   { "clean", "jazz", "humbucker" },
                   { { "amp.model", OPAL }, { "amp.gain", 3.5f }, { "amp.treble", 3.f }, { "amp.bass", 6.f }, { "amp.presence", 3.5f },
                     { "cab.a.speaker", CEDAR212 }, { "cab.a.mic", RIB }, { "cab.a.distance", 10 },
                     { "rvb.on", 1 }, { "rvb.mode", ROOM }, { "rvb.mix", 15 } }));
  v.push_back(make("Chorus Lagoon", "clean", "Широкий хорус и мягкая задержка — прозрачный clean для баллад.",
                   { "clean", "chorus", "wide" },
                   { { "amp.model", GLASS }, { "amp.gain", 3.f },
                     { "cho.on", 1 }, { "cho.rate", 0.6f }, { "cho.depth", 55 }, { "cho.mix", 50 },
                     { "dly.on", 1 }, { "dly.mode", ANALOG }, { "dly.time", 420 }, { "dly.feedback", 30 }, { "dly.mix", 20 },
                     { "rvb.on", 1 }, { "rvb.mode", HALL }, { "rvb.mix", 18 } }));
  v.push_back(make("Tremolo Surf", "clean", "Пульсирующее тремоло и щедрая пружинная реверберация.",
                   { "clean", "surf", "tremolo", "spring" },
                   { { "amp.model", LUMEN }, { "amp.gain", 4.5f }, { "amp.bright", 1 },
                     { "trm.on", 1 }, { "trm.rate", 6.5f }, { "trm.depth", 60 },
                     { "cab.a.speaker", AURUM },
                     { "rvb.on", 1 }, { "rvb.mode", SPRING }, { "rvb.mix", 38 }, { "rvb.decay", 2.6f } }));
  v.push_back(make("Velvet Clean Edge", "clean", "Чистый, но живой звук: при сильной атаке начинает слегка «рычать».",
                   { "clean", "edge", "dynamic" },
                   { { "amp.model", VELVET }, { "amp.gain", 2.5f }, { "amp.master", 6.f },
                     { "cab.a.speaker", CEDAR212 },
                     { "rvb.on", 1 }, { "rvb.mode", ROOM }, { "rvb.mix", 14 } }));

  // =========================================================================== CRUNCH (8)
  v.push_back(make("Brass Rhythm", "crunch", "Упругий роковый кранч для ритма: аккорды читаются, низ собран.",
                   { "crunch", "rock", "rhythm" },
                   { { "amp.model", BRASS }, { "amp.gain", 5.f }, { "amp.mid", 6.f },
                     { "cab.a.speaker", MONOLITH }, { "cab.a.position", 30 } }));
  v.push_back(make("Velvet Breakup", "crunch", "Грань перегруза: тихо — почти чисто, сильно — плотный кранч.",
                   { "crunch", "breakup", "dynamic" },
                   { { "amp.model", VELVET }, { "amp.gain", 6.f }, { "amp.sag", 6.5f },
                     { "cab.a.speaker", CEDAR212 }, { "cab.a.mic", RIB }, { "cab.a.position", 40 },
                     { "rvb.on", 1 }, { "rvb.mode", ROOM }, { "rvb.mix", 12 } }));
  v.push_back(make("Rust Royale", "crunch", "Грубоватый насыщенный кранч с выпуклой серединой.",
                   { "crunch", "mid", "classic rock" },
                   { { "amp.model", RUST }, { "amp.gain", 6.5f }, { "amp.mid", 7.f }, { "amp.treble", 5.5f },
                     { "cab.a.speaker", MONOLITH }, { "cab.a.position", 20 } }));
  v.push_back(make("Arena Stack", "crunch", "Классический стек с бустом: больше плотности и сустейна для риффов.",
                   { "crunch", "boost", "rock" },
                   { { "amp.model", BRASS }, { "amp.gain", 6.f },
                     { "boost.on", 1 }, { "boost.level", 6 }, { "boost.tight", 120 },
                     { "cab.a.speaker", MONOLITH }, { "cab.b.on", 1 }, { "cab.b.mic", RIB }, { "cab.b.level", -6 } }));
  v.push_back(make("Edge Garden", "crunch", "Прозрачный овердрайв перед стеклянным усилителем: чистый характер с волосками перегруза.",
                   { "crunch", "overdrive", "transparent" },
                   { { "amp.model", GLASS }, { "amp.gain", 5.f },
                     { "od.on", 1 }, { "od.mode", 0 }, { "od.drive", 5 }, { "od.level", 2 },
                     { "cab.a.speaker", LUMEN212 } }));
  v.push_back(make("Indie Jangle Crunch", "crunch", "Звонкий лёгкий кранч с хорусом и комнатой — для инди-аккордов.",
                   { "crunch", "indie", "chorus" },
                   { { "amp.model", VELVET }, { "amp.gain", 4.5f }, { "amp.treble", 6.5f },
                     { "cho.on", 1 }, { "cho.depth", 30 }, { "cho.mix", 35 },
                     { "cab.a.speaker", AURUM },
                     { "rvb.on", 1 }, { "rvb.mode", ROOM }, { "rvb.mix", 18 } }));
  v.push_back(make("Seventies Arena", "crunch", "Насыщенный кранч с ленточным эхо: размашистые риффы и соло.",
                   { "crunch", "tape echo", "70s" },
                   { { "amp.model", RUST }, { "amp.gain", 7.f },
                     { "dly.on", 1 }, { "dly.mode", TAPE }, { "dly.time", 340 }, { "dly.feedback", 28 }, { "dly.mix", 18 },
                     { "cab.a.speaker", MONOLITH } }));
  v.push_back(make("Blues Drive Push", "crunch", "Овердрайв с горбом середины толкает лёгкий перегруз — поющий блюз.",
                   { "crunch", "blues", "overdrive" },
                   { { "amp.model", VELVET }, { "amp.gain", 4.5f },
                     { "od.on", 1 }, { "od.mode", 1 }, { "od.drive", 4 }, { "od.tone", 5.5f }, { "od.level", 3 },
                     { "cab.a.speaker", CEDAR212 },
                     { "rvb.on", 1 }, { "rvb.mode", ROOM }, { "rvb.mix", 14 } }));

  // =========================================================================== HIGH GAIN (12)
  v.push_back(make("Thorn Rhythm", "highgain", "Плотный modern rock: тяжёлые аккорды без каши.",
                   { "high gain", "rhythm", "modern rock" },
                   { { "amp.model", THORN }, { "amp.gain", 6.5f }, { "gate.threshold", -58 }, { "gate.key", 1 },
                     { "cab.a.speaker", VANTA } }));
  v.push_back(make("Iron Veil Chug", "highgain", "Tight metal: буст срезает низ до усилителя, гейт по DI — чёткий palm-mute.",
                   { "high gain", "metal", "tight" },
                   { { "amp.model", IRON }, { "amp.gain", 6.5f }, { "amp.bass", 5.5f }, { "amp.mid", 4.5f },
                     { "boost.on", 1 }, { "boost.level", 8 }, { "boost.tight", 220 },
                     { "gate.threshold", -54 }, { "gate.key", 1 }, { "gate.release", 70 },
                     { "cab.a.speaker", VANTA }, { "cab.a.position", 25 } }));
  v.push_back(make("Catacomb Djent", "highgain", "Экстремальный djent: собранный низ, детальная атака, глухие паузы.",
                   { "high gain", "djent", "extended range" },
                   { { "amp.model", CATACOMB }, { "amp.gain", 6.f }, { "amp.bass", 5.f }, { "amp.mid", 5.5f }, { "amp.treble", 6.5f },
                     { "boost.on", 1 }, { "boost.level", 9 }, { "boost.tight", 300 },
                     { "gate.threshold", -50 }, { "gate.key", 1 }, { "gate.hold", 15 }, { "gate.release", 50 },
                     { "eq.on", 1 }, { "eq.lowCut", 70 }, { "eq.lowSlope", 1 }, { "eq.b2.freq", 500 }, { "eq.b2.gain", -2.5f },
                     { "cab.a.speaker", VANTA } }));
  v.push_back(make("Abyss Lead", "highgain", "Длинный поющий lead: эхо и холл держат ноты в воздухе.",
                   { "high gain", "lead", "solo" },
                   { { "amp.model", ABYSS }, { "amp.gain", 7.f },
                     { "dly.on", 1 }, { "dly.mode", ANALOG }, { "dly.sync", 1 }, { "dly.division", D4 }, { "dly.feedback", 35 }, { "dly.mix", 24 },
                     { "rvb.on", 1 }, { "rvb.mode", HALL }, { "rvb.mix", 20 }, { "rvb.decay", 3 } }));
  v.push_back(make("Modern Metal Tight", "highgain", "Агрессивный современный металл с упругим низом.",
                   { "high gain", "metal" },
                   { { "amp.model", IRON }, { "amp.gain", 7.5f }, { "amp.presence", 6.f },
                     { "boost.on", 1 }, { "boost.level", 6 }, { "boost.tight", 180 },
                     { "gate.threshold", -52 }, { "gate.key", 1 },
                     { "cab.a.speaker", MONOLITH }, { "cab.b.on", 1 }, { "cab.b.speaker", VANTA }, { "cab.b.mic", COND }, { "cab.b.level", -5 } }));
  v.push_back(make("Drop Tuned Wall", "highgain", "Стена звука для низкого строя: двойной кабинет, расширенная панорама.",
                   { "high gain", "drop tuning", "wide" },
                   { { "amp.model", CATACOMB }, { "amp.gain", 6.5f }, { "amp.depth", 6.5f },
                     { "gate.threshold", -52 }, { "gate.key", 1 },
                     { "cab.a.speaker", VANTA }, { "cab.a.pan", -35 }, { "cab.b.on", 1 }, { "cab.b.speaker", MONOLITH }, { "cab.b.mic", DYN }, { "cab.b.pan", 35 }, { "cab.b.level", -1 } }));
  v.push_back(make("Thrash Precision", "highgain", "Быстрые риффы: выпуклая середина и короткий гейт.",
                   { "high gain", "thrash" },
                   { { "amp.model", THORN }, { "amp.gain", 7.f }, { "amp.mid", 7.f }, { "amp.bass", 4.5f },
                     { "gate.threshold", -56 }, { "gate.key", 1 }, { "gate.release", 60 },
                     { "eq.on", 1 }, { "eq.b3.freq", 900 }, { "eq.b3.gain", 2.5f },
                     { "cab.a.speaker", MONOLITH } }));
  v.push_back(make("Hard Rock Lead", "highgain", "Плотный lead с эхо на четверть — классические соло.",
                   { "high gain", "lead", "rock" },
                   { { "amp.model", THORN }, { "amp.gain", 7.5f },
                     { "dly.on", 1 }, { "dly.mode", DIGITAL }, { "dly.time", 380 }, { "dly.feedback", 30 }, { "dly.mix", 20 },
                     { "cab.a.speaker", MONOLITH } }));
  v.push_back(make("Sludge Fuzz", "highgain", "Вязкий фузз в насыщенный усилитель — медленные тяжёлые риффы.",
                   { "fuzz", "doom", "sludge" },
                   { { "amp.model", RUST }, { "amp.gain", 6.f }, { "amp.bass", 6.5f },
                     { "fuzz.on", 1 }, { "fuzz.fuzz", 7 }, { "fuzz.tone", 4 },
                     { "cab.a.speaker", MONOLITH }, { "cab.a.mic", RIB } }));
  v.push_back(make("Progressive Lead", "highgain", "Гладкий lead с компрессией, пластиной и пинг-понг эхо.",
                   { "high gain", "lead", "progressive" },
                   { { "amp.model", ABYSS }, { "amp.gain", 6.5f }, { "comp.on", 1 }, { "comp.threshold", -22 }, { "comp.ratio", 3 },
                     { "dly.on", 1 }, { "dly.mode", PINGPONG }, { "dly.sync", 1 }, { "dly.division", D8D }, { "dly.feedback", 32 }, { "dly.mix", 22 },
                     { "rvb.on", 1 }, { "rvb.mode", PLATE }, { "rvb.mix", 16 } }));
  v.push_back(make("Metalcore Breakdown", "highgain", "Максимально собранный брейкдаун: агрессивный гейт, тугой низ.",
                   { "high gain", "metalcore" },
                   { { "amp.model", CATACOMB }, { "amp.gain", 7.f }, { "amp.bass", 6.f },
                     { "boost.on", 1 }, { "boost.level", 10 }, { "boost.tight", 260 },
                     { "gate.threshold", -48 }, { "gate.key", 1 }, { "gate.hold", 10 }, { "gate.release", 40 },
                     { "cab.a.speaker", VANTA } }));
  v.push_back(make("Shred Machine", "highgain", "Скоростной lead: компрессор выравнивает легато, эхо добавляет размах.",
                   { "high gain", "shred", "lead" },
                   { { "amp.model", ABYSS }, { "amp.gain", 8.f }, { "comp.on", 1 }, { "comp.threshold", -20 }, { "comp.ratio", 4 },
                     { "gate.threshold", -60 }, { "gate.key", 1 },
                     { "dly.on", 1 }, { "dly.mode", PINGPONG }, { "dly.time", 300 }, { "dly.feedback", 25 }, { "dly.mix", 18 } }));

  // =========================================================================== AMBIENT / SHOEGAZE (10)
  v.push_back(make("Misty Valley", "ambient", "Туманное пространство: мерцающая реверберация и мягкое ленточное эхо.",
                   { "ambient", "shimmer", "pad" },
                   { { "amp.model", GLASS }, { "amp.gain", 3.f }, { "amp.treble", 5.5f },
                     { "dly.on", 1 }, { "dly.mode", TAPE }, { "dly.time", 520 }, { "dly.feedback", 45 }, { "dly.mix", 26 },
                     { "rvb.on", 1 }, { "rvb.mode", SHIMMER }, { "rvb.mix", 40 }, { "rvb.decay", 7 }, { "rvb.shimmer", 45 } }));
  v.push_back(make("Shoegaze Haze", "ambient", "Стена фузза, тонущая в огромной реверберации и хорусе.",
                   { "shoegaze", "fuzz", "wall" },
                   { { "amp.model", RUST }, { "amp.gain", 5.f },
                     { "fuzz.on", 1 }, { "fuzz.fuzz", 6 }, { "fuzz.tone", 6 },
                     { "cho.on", 1 }, { "cho.depth", 60 }, { "cho.mix", 45 },
                     { "rvb.on", 1 }, { "rvb.mode", HALL }, { "rvb.mix", 55 }, { "rvb.decay", 8 }, { "rvb.size", 90 } }));
  v.push_back(make("Cathedral Swell", "ambient", "Большой холл с длинным хвостом и мягким компрессором.",
                   { "ambient", "hall" },
                   { { "amp.model", OPAL }, { "amp.gain", 3.5f }, { "comp.on", 1 }, { "comp.threshold", -28 },
                     { "rvb.on", 1 }, { "rvb.mode", HALL }, { "rvb.mix", 45 }, { "rvb.decay", 9 }, { "rvb.size", 95 }, { "rvb.preDelay", 60 } }));
  v.push_back(make("Post-Rock Builder", "ambient", "Нарастающий кранч, пунктирная восьмая и холл — для кульминаций.",
                   { "post-rock", "delay" },
                   { { "amp.model", VELVET }, { "amp.gain", 5.5f },
                     { "dly.on", 1 }, { "dly.sync", 1 }, { "dly.division", D8D }, { "dly.feedback", 48 }, { "dly.mix", 32 },
                     { "rvb.on", 1 }, { "rvb.mode", HALL }, { "rvb.mix", 35 }, { "rvb.decay", 5 } }));
  v.push_back(make("Frozen Pad", "ambient", "Почти бесконечный пэд: длинная реверберация с модуляцией. Включите Freeze, чтобы «заморозить» аккорд.",
                   { "ambient", "pad", "freeze" },
                   { { "amp.model", GLASS }, { "amp.gain", 2.5f },
                     { "rvb.on", 1 }, { "rvb.mode", HALL }, { "rvb.mix", 60 }, { "rvb.decay", 15 }, { "rvb.modulation", 60 } }));
  v.push_back(make("Tape Echo Dream", "ambient", "Чистый звук в «плывущем» ленточном эхо с пружиной.",
                   { "ambient", "tape", "spring" },
                   { { "amp.model", LUMEN }, { "amp.gain", 3.5f },
                     { "dly.on", 1 }, { "dly.mode", TAPE }, { "dly.time", 460 }, { "dly.feedback", 55 }, { "dly.mix", 35 }, { "dly.modDepth", 45 },
                     { "rvb.on", 1 }, { "rvb.mode", SPRING }, { "rvb.mix", 28 } }));
  v.push_back(make("Dream Pop Chorus", "ambient", "Мечтательный хорус и пластина — мягкие аккорды дрим-попа.",
                   { "dream pop", "chorus" },
                   { { "amp.model", GLASS }, { "amp.gain", 3.5f },
                     { "cho.on", 1 }, { "cho.rate", 0.4f }, { "cho.depth", 70 }, { "cho.mix", 55 },
                     { "rvb.on", 1 }, { "rvb.mode", PLATE }, { "rvb.mix", 32 }, { "rvb.decay", 3.5f } }));
  v.push_back(make("Infinite Shimmer", "ambient", "Октава вверх, растущая в реверберации: органное сияние.",
                   { "shimmer", "ambient" },
                   { { "amp.model", OPAL }, { "amp.gain", 3.f },
                     { "rvb.on", 1 }, { "rvb.mode", SHIMMER }, { "rvb.mix", 50 }, { "rvb.decay", 11 }, { "rvb.shimmer", 70 } }));
  v.push_back(make("Underwater Phase", "ambient", "Медленный фазер и эхо — звук будто из-под воды.",
                   { "ambient", "phaser" },
                   { { "amp.model", GLASS }, { "amp.gain", 3.5f },
                     { "pha.on", 1 }, { "pha.rate", 0.15f }, { "pha.depth", 85 }, { "pha.feedback", 60 },
                     { "dly.on", 1 }, { "dly.mode", ANALOG }, { "dly.time", 500 }, { "dly.feedback", 40 }, { "dly.mix", 25 } }));
  v.push_back(make("Granite Fog", "ambient", "Медленный флэнжер над кранчем и тёмная реверберация.",
                   { "ambient", "flanger", "crunch" },
                   { { "amp.model", BRASS }, { "amp.gain", 4.5f },
                     { "fla.on", 1 }, { "fla.rate", 0.08f }, { "fla.depth", 70 }, { "fla.feedback", 55 }, { "fla.mix", 45 },
                     { "rvb.on", 1 }, { "rvb.mode", HALL }, { "rvb.mix", 30 }, { "rvb.highDamp", 3500 } }));

  // =========================================================================== POP / BLUES / LEAD (6)
  v.push_back(make("Pop Rhythm Polish", "lead", "Ровный блестящий ритм для поп-музыки: компрессия и яркий EQ.",
                   { "pop", "rhythm", "clean" },
                   { { "amp.model", LUMEN }, { "amp.gain", 4.f },
                     { "comp.on", 1 }, { "comp.threshold", -28 }, { "comp.ratio", 4 },
                     { "eq.on", 1 }, { "eq.b5.gain", 2.5f }, { "eq.b2.gain", -1.5f },
                     { "rvb.on", 1 }, { "rvb.mode", PLATE }, { "rvb.mix", 14 } }));
  v.push_back(make("Blues Tone Room", "lead", "Живой блюзовый звук в небольшой комнате.",
                   { "blues", "crunch" },
                   { { "amp.model", VELVET }, { "amp.gain", 5.f }, { "amp.mid", 6.f },
                     { "cab.a.speaker", CEDAR212 }, { "cab.a.mic", RIB },
                     { "rvb.on", 1 }, { "rvb.mode", ROOM }, { "rvb.mix", 20 } }));
  v.push_back(make("Velvet Lead Sing", "lead", "Выразительный поющий lead на грани перегруза с аналоговым эхо.",
                   { "lead", "blues", "expressive" },
                   { { "amp.model", VELVET }, { "amp.gain", 7.5f }, { "amp.sag", 7.f },
                     { "dly.on", 1 }, { "dly.mode", ANALOG }, { "dly.time", 420 }, { "dly.feedback", 30 }, { "dly.mix", 18 },
                     { "rvb.on", 1 }, { "rvb.mode", ROOM }, { "rvb.mix", 16 } }));
  v.push_back(make("Texas Push", "lead", "Овердрайв в кранч: жирный, напористый блюз-рок.",
                   { "blues rock", "overdrive" },
                   { { "amp.model", BRASS }, { "amp.gain", 4.5f },
                     { "od.on", 1 }, { "od.mode", 1 }, { "od.drive", 6 }, { "od.level", 2 },
                     { "cab.a.speaker", AURUM },
                     { "rvb.on", 1 }, { "rvb.mode", SPRING }, { "rvb.mix", 16 } }));
  v.push_back(make("Soul Comp Clean", "lead", "Соул-клин: компрессор, лёгкое тремоло и тёплая комната.",
                   { "soul", "clean", "tremolo" },
                   { { "amp.model", OPAL }, { "amp.gain", 4.f },
                     { "comp.on", 1 }, { "comp.threshold", -26 }, { "comp.ratio", 3 },
                     { "trm.on", 1 }, { "trm.rate", 4 }, { "trm.depth", 25 },
                     { "rvb.on", 1 }, { "rvb.mode", ROOM }, { "rvb.mix", 18 } }));
  v.push_back(make("Expressive Lead Wah", "lead", "Lead с автовау, реагирующим на силу атаки.",
                   { "lead", "wah" },
                   { { "amp.model", THORN }, { "amp.gain", 6.5f },
                     { "wah.on", 1 }, { "wah.mode", 1 }, { "wah.sensitivity", 6 }, { "wah.q", 7 },
                     { "dly.on", 1 }, { "dly.time", 360 }, { "dly.feedback", 25 }, { "dly.mix", 16 } }));

  // =========================================================================== ACOUSTIC (4)
  v.push_back(make("Silver Stage", "acoustic", "Сцена с пьезо: мягкий компрессор, приглушённая резкость, лёгкий воздух.",
                   { "acoustic", "piezo", "live" },
                   { { "amp.model", SILVER }, { "amp.acPiezoTame", 5 }, { "amp.acBody", 5 }, { "amp.acAir", 4 }, { "amp.acRoom", 2 },
                     { "comp.on", 1 }, { "comp.threshold", -24 }, { "comp.ratio", 2.5f },
                     { "cab.on", 0 }, { "gate.on", 0 },
                     { "rvb.on", 1 }, { "rvb.mode", PLATE }, { "rvb.mix", 12 } }));
  v.push_back(make("Cedar Living Room", "acoustic", "Тёплый корпус и ощущение небольшой гостиной.",
                   { "acoustic", "warm", "room" },
                   { { "amp.model", CEDAR }, { "amp.acBody", 7 }, { "amp.acRoom", 6 }, { "amp.acPiezoTame", 4 },
                     { "cab.on", 0 }, { "gate.on", 0 } }));
  v.push_back(make("Piezo Fix", "acoustic", "Максимальное подавление «кряка» пьезо и добавление корпуса — для дешёвых звукоснимателей.",
                   { "acoustic", "piezo" },
                   { { "amp.model", SILVER }, { "amp.acPiezoTame", 9 }, { "amp.acBody", 7.5f }, { "amp.acAir", 3 }, { "amp.treble", 4.5f },
                     { "cab.on", 0 }, { "gate.on", 0 } }));
  v.push_back(make("Acoustic Shimmer Strum", "acoustic", "Акустика с лёгким хорусом и пластиной — яркий бой.",
                   { "acoustic", "chorus", "strum" },
                   { { "amp.model", CEDAR }, { "amp.acBody", 5 }, { "amp.acAir", 6 }, { "amp.acRoom", 3 },
                     { "cho.on", 1 }, { "cho.depth", 25 }, { "cho.mix", 30 },
                     { "cab.on", 0 }, { "gate.on", 0 },
                     { "rvb.on", 1 }, { "rvb.mode", PLATE }, { "rvb.mix", 18 } }));

  // =========================================================================== HARMONY / OCTAVE / EXPERIMENTAL (10)
  v.push_back(make("Harmony Thirds Lead", "harmony", "Lead с терцией в ладу: выберите тональность — голос подстроится под каждую ноту. По умолчанию E минор.",
                   { "harmonizer", "lead", "thirds" },
                   { { "amp.model", ABYSS }, { "amp.gain", 6.5f },
                     { "harm.on", 1 }, { "harm.key", E }, { "harm.scale", NMINOR }, { "harm.v1.on", 1 }, { "harm.v1.interval", IV_P3 }, { "harm.v1.level", 75 },
                     { "harm.v1.pan", -30 }, { "harm.v2.on", 0 },
                     { "dly.on", 1 }, { "dly.time", 400 }, { "dly.feedback", 25 }, { "dly.mix", 16 } }));
  v.push_back(make("Twin Lead Fifths", "harmony", "Две гармонии — терция и квинта по сторонам стереобазы. A минор.",
                   { "harmonizer", "twin lead" },
                   { { "amp.model", THORN }, { "amp.gain", 6.5f },
                     { "harm.on", 1 }, { "harm.key", A }, { "harm.scale", NMINOR },
                     { "harm.v1.on", 1 }, { "harm.v1.interval", IV_P3 }, { "harm.v1.pan", -60 }, { "harm.v2.on", 1 }, { "harm.v2.interval", IV_P5 }, { "harm.v2.pan", 60 },
                     { "rvb.on", 1 }, { "rvb.mode", PLATE }, { "rvb.mix", 15 } }));
  v.push_back(make("Octave Fuzz Synth", "harmony", "Октавный фузз и аналоговый суб: синтезаторный лид для одиночных нот.",
                   { "octave", "fuzz", "synth" },
                   { { "amp.model", RUST }, { "amp.gain", 5.f },
                     { "fuzz.on", 1 }, { "fuzz.mode", 2 }, { "fuzz.fuzz", 5 },
                     { "oct.on", 1 }, { "oct.mode", 0 }, { "oct.sub1", 60 } }));
  v.push_back(make("Sub Bass Guitar", "harmony", "Аналоговый делитель частоты: на октаву и две ниже — гитара как бас. Играйте по одной ноте.",
                   { "octave", "sub", "bass" },
                   { { "amp.model", OPAL }, { "amp.gain", 4.f }, { "amp.bass", 7.f },
                     { "oct.on", 1 }, { "oct.mode", 0 }, { "oct.sub1", 90 }, { "oct.sub2", 40 }, { "oct.dry", 40 }, { "oct.tone", 800 },
                     { "cab.a.speaker", CEDAR212 } }));
  v.push_back(make("12-String Sparkle", "harmony", "Октава вверх и хорус — блеск двенадцатиструнной гитары.",
                   { "pitch", "12-string", "clean" },
                   { { "amp.model", GLASS }, { "amp.gain", 3.f },
                     { "pitch.on", 1 }, { "pitch.semitones", 12 }, { "pitch.mix", 35 },
                     { "cho.on", 1 }, { "cho.depth", 35 }, { "cho.mix", 40 },
                     { "rvb.on", 1 }, { "rvb.mode", PLATE }, { "rvb.mix", 16 } }));
  v.push_back(make("Choir of Fifths", "harmony", "Хор из октавы вниз, квинты и октавы вверх в мерцающем пространстве. D мажор.",
                   { "harmonizer", "ambient", "choir" },
                   { { "amp.model", OPAL }, { "amp.gain", 3.f },
                     { "harm.on", 1 }, { "harm.key", D_ }, { "harm.scale", MAJOR },
                     { "harm.v1.on", 1 }, { "harm.v1.interval", IV_MOCT }, { "harm.v1.level", 55 }, { "harm.v1.pan", 0 },
                     { "harm.v2.on", 1 }, { "harm.v2.interval", IV_P5 }, { "harm.v2.level", 50 }, { "harm.v2.pan", -50 },
                     { "harm.v3.on", 1 }, { "harm.v3.interval", IV_POCT }, { "harm.v3.level", 40 }, { "harm.v3.pan", 50 },
                     { "rvb.on", 1 }, { "rvb.mode", SHIMMER }, { "rvb.mix", 45 }, { "rvb.decay", 8 } }));
  v.push_back(make("Major Pentatonic Duet", "harmony", "Вторая гитара в мажорной пентатонике — светлые кантри- и поп-мелодии. G мажор.",
                   { "harmonizer", "pentatonic" },
                   { { "amp.model", LUMEN }, { "amp.gain", 4.5f },
                     { "harm.on", 1 }, { "harm.key", G }, { "harm.scale", MAJPENTA }, { "harm.v1.on", 1 }, { "harm.v1.interval", IV_P3 }, { "harm.v1.pan", 40 },
                     { "harm.v2.on", 0 },
                     { "rvb.on", 1 }, { "rvb.mode", SPRING }, { "rvb.mix", 18 } }));
  v.push_back(make("Rotary Organ Grind", "harmony", "Быстрый вращающийся динамик с перегрузом — органный рык.",
                   { "rotary", "organ" },
                   { { "amp.model", BRASS }, { "amp.gain", 4.5f },
                     { "rot.on", 1 }, { "rot.speed", 1 }, { "rot.drive", 5 },
                     { "rvb.on", 1 }, { "rvb.mode", ROOM }, { "rvb.mix", 15 } }));
  v.push_back(make("Glitch Ring Flanger", "harmony", "Резкий флэнжер с сильной обратной связью и многоотводным эхо.",
                   { "experimental", "flanger", "glitch" },
                   { { "amp.model", THORN }, { "amp.gain", 5.f },
                     { "fla.on", 1 }, { "fla.rate", 2.5f }, { "fla.depth", 80 }, { "fla.feedback", 85 }, { "fla.manual", 1.2f },
                     { "dly.on", 1 }, { "dly.mode", MULTITAP }, { "dly.time", 600 }, { "dly.feedback", 30 }, { "dly.mix", 30 } }));
  v.push_back(make("Parallel Grit & Air", "harmony", "Параллельная секция: путь A — перегруженный усилитель, путь B — чистый сигнал с октавой вверх и хорусом. Смесь — ручкой A/B Mix.",
                   { "parallel", "experimental", "blend" },
                   { { "amp.model", IRON }, { "amp.gain", 6.f },
                     { "pitch.on", 1 }, { "pitch.semitones", 12 }, { "pitch.mix", 45 },
                     { "cho.on", 1 }, { "cho.depth", 50 }, { "cho.mix", 60 },
                     { "chain.parallelMix", 50 },
                     { "rvb.on", 1 }, { "rvb.mode", HALL }, { "rvb.mix", 20 } },
                   ChainSpec{ { "gate", "comp", "wah", "envfilter", "octave", "boost", "od", "dist", "fuzz", "amp", "cab", "pitch", "chorus",
                                "harmonizer", "eq", "phaser", "flanger", "vibrato", "tremolo", "rotary", "delay", "reverb" },
                              true, 9, 13, { "pitch", "chorus" } }));
  return v;
}

} // namespace

const std::vector<Preset>& factoryPresets()
{
  static const std::vector<Preset> presets = build();
  return presets;
}

} // namespace tsl
