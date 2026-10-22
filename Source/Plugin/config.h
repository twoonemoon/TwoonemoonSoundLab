// SPDX-License-Identifier: GPL-3.0-or-later
// Конфигурация плагина для iPlug2. Идентификаторы (PLUG_UNIQUE_ID, PLUG_MFR_ID, bundle IDs)
// не менять после публичного релиза — хосты по ним находят плагин в проектах.
#pragma once

#define PLUG_NAME "TwoonemoonSoundLab"
#define PLUG_MFR "Twoonemoon"
#define PLUG_VERSION_HEX 0x00000100
#define PLUG_VERSION_STR "0.1.0"
#define PLUG_UNIQUE_ID 'TmSL'
#define PLUG_MFR_ID 'Twmn'
#define PLUG_URL_STR "https://github.com/twoonemoon/TwoonemoonSoundLab"
#define PLUG_EMAIL_STR ""
#define PLUG_COPYRIGHT_STR "Copyright 2026 Twoonemoon. GPL-3.0-or-later"
#define PLUG_CLASS_NAME TwoonemoonSoundLab

#define BUNDLE_NAME "TwoonemoonSoundLab"
#define BUNDLE_MFR "twoonemoon"
#define BUNDLE_DOMAIN "com"

#define SHARED_RESOURCES_SUBPATH "TwoonemoonSoundLab"

// Моно-вход (гитара) → стерео-выход; также стерео → стерео.
#define PLUG_CHANNEL_IO "1-1 1-2 2-2"

#define PLUG_LATENCY 0
#define PLUG_TYPE 0
#define PLUG_DOES_MIDI_IN 1 // MIDI Learn, педаль экспрессии
#define PLUG_DOES_MIDI_OUT 0
#define PLUG_DOES_MPE 0
#define PLUG_DOES_STATE_CHUNKS 1
#define PLUG_HAS_UI 1
#define PLUG_WIDTH 1200
#define PLUG_HEIGHT 760
#define PLUG_MIN_WIDTH 600
#define PLUG_MIN_HEIGHT 380
#define PLUG_MAX_WIDTH 3000
#define PLUG_MAX_HEIGHT 1900
#define PLUG_FPS 60
#define PLUG_SHARED_RESOURCES 0
#define PLUG_HOST_RESIZE 0

#define AUV2_ENTRY TwoonemoonSoundLab_Entry
#define AUV2_ENTRY_STR "TwoonemoonSoundLab_Entry"
#define AUV2_FACTORY TwoonemoonSoundLab_Factory
#define AUV2_VIEW_CLASS TwoonemoonSoundLab_View
#define AUV2_VIEW_CLASS_STR "TwoonemoonSoundLab_View"

#define AAX_TYPE_IDS 'TSL1', 'TSL2'
#define AAX_TYPE_IDS_AUDIOSUITE 'TSA1', 'TSA2'
#define AAX_PLUG_MFR_STR "Twoonemoon"
#define AAX_PLUG_NAME_STR "TwoonemoonSoundLab\nTMSL"
#define AAX_PLUG_CATEGORY_STR "Effect"
#define AAX_DOES_AUDIOSUITE 0

#define VST3_SUBCATEGORY "Fx|Distortion"

#define CLAP_MANUAL_URL "https://github.com/twoonemoon/TwoonemoonSoundLab/blob/main/docs/USER_GUIDE.md"
#define CLAP_SUPPORT_URL "https://github.com/twoonemoon/TwoonemoonSoundLab/issues"
#define CLAP_DESCRIPTION "Guitar amp, cabinet and effects processor"
#define CLAP_FEATURES "audio-effect", "distortion", "stereo", "mono"

#define APP_NUM_CHANNELS 2
#define APP_N_VECTOR_WAIT 0
#define APP_MULT 1
#define APP_COPY_AUV3 0
// Внутренний вектор standalone: все размеры буфера устройства (32…8192) кратны 32.
#define APP_SIGNAL_VECTOR_SIZE 32

// Шрифты (OFL), см. Resources/Fonts
#define FONT_REGULAR_FN "Inter-Regular.ttf"
#define FONT_MEDIUM_FN "Inter-Medium.ttf"
#define FONT_SEMIBOLD_FN "Inter-SemiBold.ttf"
#define FONT_BOLD_FN "Inter-Bold.ttf"
#define FONT_MONO_FN "JetBrainsMono-Regular.ttf"
#define FONT_MONO_MEDIUM_FN "JetBrainsMono-Medium.ttf"
