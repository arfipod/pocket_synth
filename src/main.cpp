#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/i2s_std.h"
#include "driver/usb_serial_jtag.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "soc/soc_caps.h"
#if defined(CONFIG_IDF_TARGET_ESP32S3)
#include "hal/i2s_ll.h"
#include "soc/i2s_struct.h"
#endif

#include "cardputer_display.h"
#include "cardputer_keyboard.h"
#include "carputer_pinmap.h"
#include "es8311.h"
#include "es8311_reg.h"
#include "main_params.h"

#include <algorithm>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* TAG = "pocketsynth";

static constexpr float PI = 3.14159265358979323846f;
static constexpr float TWO_PI = 2.0f * PI;
static constexpr float PER_NOTE_GAIN = 0.35f;
static constexpr float INITIAL_MASTER_VOLUME = 0.70f;
static constexpr float VOLUME_STEP = 0.05f;
static constexpr float PULSE_WIDTH = 0.25f;

static constexpr uint32_t INPUT_POLL_MS = 5;
static constexpr uint32_t UI_FRAME_MS = 50;

static constexpr UBaseType_t AUDIO_TASK_PRIORITY = 20;
static constexpr UBaseType_t CONTROL_TASK_PRIORITY = 9;
static constexpr UBaseType_t INPUT_TASK_PRIORITY = 8;
static constexpr UBaseType_t UI_TASK_PRIORITY = 4;
static constexpr UBaseType_t SERIAL_TASK_PRIORITY = 2;

static constexpr uint32_t AUDIO_TASK_STACK = 4096;
static constexpr uint32_t CONTROL_TASK_STACK = 4096;
static constexpr uint32_t INPUT_TASK_STACK = 4096;
static constexpr uint32_t UI_TASK_STACK = 8192;
static constexpr uint32_t SERIAL_TASK_STACK = 4096;

enum class Waveform : uint8_t {
  Sine,
  Square,
  Rectangle,
  Saw,
};

enum class SynthEventType : uint8_t {
  NoteOn,
  NoteOff,
  SetWaveform,
  SetVolume,
  AdjustVolume,
  AllNotesOff,
};

struct ActiveNote {
  bool active = false;
  uint8_t noteIndex = 0;
  uint8_t midi = 0;
  float frequency = 0.0f;
  float phase = 0.0f;
  float phaseIncrement = 0.0f;
};

struct SynthAudioState {
  Waveform waveform = Waveform::Sine;
  float masterVolume = INITIAL_MASTER_VOLUME;
  uint8_t activeCount = 0;
  uint32_t pressedMask = 0;
  ActiveNote notes[MAX_POLYPHONY] = {};
};

struct UiState {
  uint32_t version = 0;
  Waveform waveform = Waveform::Sine;
  float masterVolume = INITIAL_MASTER_VOLUME;
  uint8_t activeCount = 0;
  uint32_t pressedMask = 0;
  char chord[16] = "--";
  bool audioReady = false;
  bool codecReady = false;
  bool keyboardReady = false;
  char keyboardDiag[48] = "kbd: starting";
};

struct SynthEvent {
  SynthEventType type = SynthEventType::NoteOn;
  uint8_t noteIndex = 0;
  uint8_t midi = 0;
  Waveform waveform = Waveform::Sine;
  float value = 0.0f;
};

struct KeyNote {
  char key;
  uint8_t noteIndex;
  uint8_t midi;
  bool black;
};

static constexpr KeyNote KEY_NOTES[] = {
    {'z', 0, 60, false}, {'s', 1, 61, true},  {'x', 2, 62, false}, {'d', 3, 63, true},
    {'c', 4, 64, false}, {'v', 5, 65, false}, {'g', 6, 66, true},  {'b', 7, 67, false},
    {'h', 8, 68, true},  {'n', 9, 69, false}, {'j', 10, 70, true}, {'m', 11, 71, false},
    {'q', 12, 72, false}, {'2', 13, 73, true}, {'w', 14, 74, false}, {'3', 15, 75, true},
    {'e', 16, 76, false}, {'r', 17, 77, false}, {'5', 18, 78, true}, {'t', 19, 79, false},
    {'6', 20, 80, true},  {'y', 21, 81, false}, {'7', 22, 82, true}, {'u', 23, 83, false},
    {'i', 24, 84, false},
};

static constexpr float INV_SQRT[MAX_POLYPHONY + 1] = {
    1.0f, 1.0f, 0.70710678f, 0.57735027f, 0.5f, 0.44721360f, 0.40824829f, 0.37796447f, 0.35355339f,
};

static QueueHandle_t gSynthEventQueue = nullptr;
static portMUX_TYPE gAudioStateMux = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE gUiStateMux = portMUX_INITIALIZER_UNLOCKED;
static SynthAudioState gAudioState;
static UiState gUiState;
static i2s_chan_handle_t gI2sTx = nullptr;

static float clamp_float(float value, float lo, float hi) {
  if (value < lo) return lo;
  if (value > hi) return hi;
  return value;
}

static char normalize_key_char(char ch) {
  if (ch >= 'A' && ch <= 'Z') return static_cast<char>(ch - 'A' + 'a');
  return ch;
}

static const KeyNote* find_note_by_key(char key) {
  const char normalized = normalize_key_char(key);
  for (const auto& entry : KEY_NOTES) {
    if (entry.key == normalized) return &entry;
  }
  return nullptr;
}

static float midi_frequency(uint8_t midi) {
  return 440.0f * powf(2.0f, (static_cast<float>(midi) - 69.0f) / 12.0f);
}

static const char* waveform_short_name(Waveform waveform) {
  switch (waveform) {
    case Waveform::Sine:
      return "SIN";
    case Waveform::Square:
      return "SQR";
    case Waveform::Rectangle:
      return "RECT";
    case Waveform::Saw:
      return "SAW";
  }
  return "?";
}

static float oscillator_sample(float phase, Waveform waveform) {
  switch (waveform) {
    case Waveform::Sine:
      return sinf(phase * TWO_PI);
    case Waveform::Square:
      return phase < 0.5f ? 1.0f : -1.0f;
    case Waveform::Rectangle:
      return phase < PULSE_WIDTH ? 1.0f : -1.0f;
    case Waveform::Saw:
      return 2.0f * phase - 1.0f;
  }
  return 0.0f;
}

static int16_t float_to_i16(float sample) {
  sample = clamp_float(sample, -1.0f, 1.0f);
  return static_cast<int16_t>(sample * 32767.0f);
}

static int32_t pack_i2s_mono_pair(int16_t first, int16_t second) {
  return static_cast<int32_t>((static_cast<uint32_t>(static_cast<uint16_t>(first)) << 16) |
                              static_cast<uint16_t>(second));
}

static void copy_audio_state(SynthAudioState* out) {
  portENTER_CRITICAL(&gAudioStateMux);
  *out = gAudioState;
  portEXIT_CRITICAL(&gAudioStateMux);
}

static void store_rendered_phases(const SynthAudioState& rendered) {
  portENTER_CRITICAL(&gAudioStateMux);
  for (int i = 0; i < MAX_POLYPHONY; ++i) {
    if (gAudioState.notes[i].active && rendered.notes[i].active &&
        gAudioState.notes[i].noteIndex == rendered.notes[i].noteIndex) {
      gAudioState.notes[i].phase = rendered.notes[i].phase;
    }
  }
  portEXIT_CRITICAL(&gAudioStateMux);
}

static void copy_ui_state(UiState* out) {
  portENTER_CRITICAL(&gUiStateMux);
  *out = gUiState;
  portEXIT_CRITICAL(&gUiStateMux);
}

static void publish_ui_from_audio_state(const SynthAudioState& state, const char* chord) {
  portENTER_CRITICAL(&gUiStateMux);
  gUiState.waveform = state.waveform;
  gUiState.masterVolume = state.masterVolume;
  gUiState.activeCount = state.activeCount;
  gUiState.pressedMask = state.pressedMask;
  snprintf(gUiState.chord, sizeof(gUiState.chord), "%s", chord ? chord : "--");
  ++gUiState.version;
  portEXIT_CRITICAL(&gUiStateMux);
}

static void publish_hardware_status(bool audioReady, bool codecReady) {
  portENTER_CRITICAL(&gUiStateMux);
  gUiState.audioReady = audioReady;
  gUiState.codecReady = codecReady;
  ++gUiState.version;
  portEXIT_CRITICAL(&gUiStateMux);
}

static void publish_keyboard_status(bool ready, const char* diagnostic) {
  portENTER_CRITICAL(&gUiStateMux);
  gUiState.keyboardReady = ready;
  snprintf(gUiState.keyboardDiag, sizeof(gUiState.keyboardDiag), "%s", diagnostic ? diagnostic : "");
  ++gUiState.version;
  portEXIT_CRITICAL(&gUiStateMux);
}

static bool send_synth_event(const SynthEvent& event) {
  return gSynthEventQueue && xQueueSend(gSynthEventQueue, &event, 0) == pdTRUE;
}

static void send_note_event(const KeyNote& note, bool pressed) {
  SynthEvent event = {};
  event.type = pressed ? SynthEventType::NoteOn : SynthEventType::NoteOff;
  event.noteIndex = note.noteIndex;
  event.midi = note.midi;
  send_synth_event(event);
}

static void send_all_notes_off() {
  SynthEvent event = {};
  event.type = SynthEventType::AllNotesOff;
  send_synth_event(event);
}

static void send_waveform_event(Waveform waveform) {
  SynthEvent event = {};
  event.type = SynthEventType::SetWaveform;
  event.waveform = waveform;
  send_synth_event(event);
}

static void send_volume_absolute(float volume) {
  SynthEvent event = {};
  event.type = SynthEventType::SetVolume;
  event.value = clamp_float(volume, 0.0f, 1.0f);
  send_synth_event(event);
}

static void send_volume_delta(float delta) {
  SynthEvent event = {};
  event.type = SynthEventType::AdjustVolume;
  event.value = delta;
  send_synth_event(event);
}

static uint8_t active_slot_count(const SynthAudioState& state) {
  uint8_t count = 0;
  for (const auto& note : state.notes) {
    if (note.active) ++count;
  }
  return count;
}

static int pitch_class_for_midi(uint8_t midi) {
  return midi % 12;
}

static void detect_chord(const SynthAudioState& state, char* out, size_t outSize) {
  static constexpr const char* NOTE_NAMES[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  struct ChordPattern {
    uint16_t mask;
    const char* suffix;
  };
  static constexpr ChordPattern PATTERNS[] = {
      {static_cast<uint16_t>((1 << 0) | (1 << 4) | (1 << 8) | (1 << 11)), "Maj7#5"},
      {static_cast<uint16_t>((1 << 0) | (1 << 4) | (1 << 7) | (1 << 11)), "Maj7"},
      {static_cast<uint16_t>((1 << 0) | (1 << 3) | (1 << 7) | (1 << 10)), "m7"},
      {static_cast<uint16_t>((1 << 0) | (1 << 4) | (1 << 7) | (1 << 10)), "7"},
      {static_cast<uint16_t>((1 << 0) | (1 << 3) | (1 << 6)), "dim"},
      {static_cast<uint16_t>((1 << 0) | (1 << 4) | (1 << 8)), "aug"},
      {static_cast<uint16_t>((1 << 0) | (1 << 2) | (1 << 7)), "sus2"},
      {static_cast<uint16_t>((1 << 0) | (1 << 5) | (1 << 7)), "sus4"},
      {static_cast<uint16_t>((1 << 0) | (1 << 3) | (1 << 7)), "m"},
      {static_cast<uint16_t>((1 << 0) | (1 << 4) | (1 << 7)), ""},
  };

  if (out == nullptr || outSize == 0) return;
  snprintf(out, outSize, "--");
  if (state.activeCount < 3) return;

  uint16_t pitchMask = 0;
  uint8_t lowestMidi = 127;
  for (const auto& note : state.notes) {
    if (!note.active) continue;
    pitchMask |= static_cast<uint16_t>(1 << pitch_class_for_midi(note.midi));
    if (note.midi < lowestMidi) lowestMidi = note.midi;
  }

  const int bassPc = pitch_class_for_midi(lowestMidi);
  for (int root = 0; root < 12; ++root) {
    uint16_t relativeMask = 0;
    for (int pc = 0; pc < 12; ++pc) {
      if (pitchMask & (1 << pc)) {
        const int interval = (pc - root + 12) % 12;
        relativeMask |= static_cast<uint16_t>(1 << interval);
      }
    }

    for (const auto& pattern : PATTERNS) {
      if (relativeMask != pattern.mask) continue;
      if (bassPc == root) {
        snprintf(out, outSize, "%s%s", NOTE_NAMES[root], pattern.suffix);
      } else {
        snprintf(out, outSize, "%s%s/%s", NOTE_NAMES[root], pattern.suffix, NOTE_NAMES[bassPc]);
      }
      return;
    }
  }
}

static void note_on(SynthAudioState* state, uint8_t noteIndex, uint8_t midi) {
  const uint32_t bit = 1UL << noteIndex;
  if ((state->pressedMask & bit) != 0 || state->activeCount >= MAX_POLYPHONY) return;

  for (auto& note : state->notes) {
    if (note.active) continue;
    note.active = true;
    note.noteIndex = noteIndex;
    note.midi = midi;
    note.frequency = midi_frequency(midi);
    note.phase = 0.0f;
    note.phaseIncrement = note.frequency / static_cast<float>(SAMPLE_RATE);
    state->pressedMask |= bit;
    state->activeCount = active_slot_count(*state);
    return;
  }
}

static void note_off(SynthAudioState* state, uint8_t noteIndex) {
  const uint32_t bit = 1UL << noteIndex;
  state->pressedMask &= ~bit;
  for (auto& note : state->notes) {
    if (note.active && note.noteIndex == noteIndex) {
      note = {};
    }
  }
  state->activeCount = active_slot_count(*state);
}

static void all_notes_off(SynthAudioState* state) {
  state->pressedMask = 0;
  state->activeCount = 0;
  for (auto& note : state->notes) note = {};
}

static void render_audio_buffer(int32_t* buffer, size_t frames) {
  SynthAudioState local = {};
  copy_audio_state(&local);

  const float normalize = local.activeCount <= MAX_POLYPHONY ? INV_SQRT[local.activeCount] : INV_SQRT[MAX_POLYPHONY];
  int16_t firstInPair = 0;
  for (size_t frame = 0; frame < frames; ++frame) {
    float mixed = 0.0f;
    if (local.activeCount > 0) {
      for (auto& note : local.notes) {
        if (!note.active) continue;
        mixed += oscillator_sample(note.phase, local.waveform) * PER_NOTE_GAIN;
        note.phase += note.phaseIncrement;
        if (note.phase >= 1.0f) note.phase -= 1.0f;
      }
      mixed *= normalize;
      mixed *= local.masterVolume;
    }

    const int16_t pcm = float_to_i16(mixed);
    if ((frame & 1U) == 0) {
      firstInPair = pcm;
    } else {
      buffer[frame >> 1] = pack_i2s_mono_pair(firstInPair, pcm);
    }
  }

  if ((frames & 1U) != 0) buffer[frames >> 1] = pack_i2s_mono_pair(firstInPair, firstInPair);

  store_rendered_phases(local);
}

static esp_err_t ensure_i2c_bus() {
  i2c_config_t busConfig = {};
  busConfig.mode = I2C_MODE_MASTER;
  busConfig.sda_io_num = PIN_I2C_SDA;
  busConfig.scl_io_num = PIN_I2C_SCL;
  busConfig.sda_pullup_en = GPIO_PULLUP_ENABLE;
  busConfig.scl_pullup_en = GPIO_PULLUP_ENABLE;
  busConfig.master.clk_speed = 100000;
  busConfig.clk_flags = 0;

  esp_err_t err = i2c_param_config(I2C_NUM_0, &busConfig);
  if (err != ESP_OK) return err;

  err = i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
  if (err == ESP_ERR_INVALID_STATE) return ESP_OK;
  return err;
}

#if defined(CONFIG_IDF_TARGET_ESP32S3)
static void calc_i2s_clock_div(uint32_t* divA, uint32_t* divB, uint32_t* divN, uint32_t baseClock,
                               uint32_t targetFrequency) {
  if (baseClock <= (targetFrequency << 1)) {
    *divN = 2;
    *divA = 1;
    *divB = 0;
    return;
  }

  uint32_t saveN = 255;
  uint32_t saveA = 63;
  uint32_t saveB = 62;

  if (targetFrequency > 0) {
    float div = static_cast<float>(baseClock) / static_cast<float>(targetFrequency);
    const uint32_t n = static_cast<uint32_t>(div);
    if (n < 256) {
      div -= static_cast<float>(n);

      float checkBase = static_cast<float>(baseClock);
      while (static_cast<int32_t>(targetFrequency) >= 0) {
        targetFrequency <<= 1;
        checkBase *= 2.0f;
      }
      const float checkTarget = static_cast<float>(targetFrequency);

      uint32_t saveDiff = UINT32_MAX;
      if (n < 255) {
        saveA = 1;
        saveB = 0;
        saveN = n + 1;
        saveDiff = static_cast<uint32_t>(fabsf(checkTarget - (checkBase / static_cast<float>(saveN))));
      }

      for (uint32_t a = 1; a < 64; ++a) {
        const uint32_t b = static_cast<uint32_t>(roundf(static_cast<float>(a) * div));
        if (a <= b) continue;

        const uint32_t diff =
            static_cast<uint32_t>(fabsf(checkTarget - ((checkBase * static_cast<float>(a)) /
                                                       static_cast<float>((n * a) + b))));
        if (saveDiff <= diff) continue;

        saveDiff = diff;
        saveA = a;
        saveB = b;
        saveN = n;
        if (diff == 0) break;
      }
    }
  }

  *divN = saveN;
  *divA = saveA;
  *divB = saveB;
}

static void apply_cardputer_i2s_clock() {
  static constexpr uint32_t PLL_D2_CLK = 120 * 1000 * 1000;
  static constexpr uint32_t SAMPLE_BITS = 16;
  static constexpr uint32_t BCLK_DIV = 32 / SAMPLE_BITS;

  uint32_t divA = 0;
  uint32_t divB = 0;
  uint32_t divN = 0;
  calc_i2s_clock_div(&divA, &divB, &divN, PLL_D2_CLK, BCLK_DIV * SAMPLE_BITS * SAMPLE_RATE);

  i2s_dev_t* dev = &I2S1;
  i2s_ll_tx_clk_set_src(dev, I2S_CLK_SRC_PLL_240M);
  dev->tx_clkm_conf.clk_en = 1;
  dev->tx_clkm_conf.tx_clk_active = 1;

  dev->tx_conf.tx_mono = 1;
  dev->tx_conf.tx_chan_equal = 1;
  dev->tx_conf1.tx_bck_div_num = BCLK_DIV - 1;

  const uint32_t yn1 = divB > (divA >> 1);
  if (yn1) divB = divA - divB;

  uint32_t divY = 1;
  uint32_t divX = 0;
  if (divB != 0) {
    divX = (divA / divB) - 1;
    divY = divA % divB;
    if (divY == 0) {
      divY = 1;
      divB = 511;
    }
  }

  i2s_ll_tx_set_raw_clk_div(dev, divN, divX, divY, divB, yn1);
  dev->tx_conf.tx_update = 1;
  dev->tx_conf.tx_update = 0;
}
#else
static void apply_cardputer_i2s_clock() {}
#endif

static esp_err_t init_i2s() {
  i2s_chan_config_t channelConfig = {};
  channelConfig.id = I2S_NUM_1;
  channelConfig.role = I2S_ROLE_MASTER;
  channelConfig.dma_desc_num = 6;
  channelConfig.dma_frame_num = AUDIO_BUFFER_FRAMES;
  channelConfig.auto_clear = true;
  channelConfig.auto_clear_after_cb = true;
  channelConfig.auto_clear_before_cb = false;
  channelConfig.allow_pd = false;
  channelConfig.intr_priority = 0;

  esp_err_t err = i2s_new_channel(&channelConfig, &gI2sTx, nullptr);
  if (err != ESP_OK) return err;

  i2s_std_config_t stdConfig = {};
  stdConfig.clk_cfg.sample_rate_hz = SAMPLE_RATE;
  stdConfig.clk_cfg.clk_src = I2S_CLK_SRC_PLL_240M;
#if SOC_I2S_HW_VERSION_2
  stdConfig.clk_cfg.ext_clk_freq_hz = 0;
#endif
  stdConfig.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_128;
  stdConfig.clk_cfg.bclk_div = 8;

  stdConfig.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_16BIT;
  stdConfig.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_16BIT;
  stdConfig.slot_cfg.slot_mode = I2S_SLOT_MODE_MONO;
  stdConfig.slot_cfg.slot_mask = I2S_STD_SLOT_BOTH;
  stdConfig.slot_cfg.ws_width = I2S_DATA_BIT_WIDTH_16BIT;
  stdConfig.slot_cfg.ws_pol = false;
  stdConfig.slot_cfg.bit_shift = true;
#if SOC_I2S_HW_VERSION_1
  stdConfig.slot_cfg.msb_right = true;
#else
  stdConfig.slot_cfg.left_align = true;
  stdConfig.slot_cfg.big_endian = false;
  stdConfig.slot_cfg.bit_order_lsb = false;
#endif

  stdConfig.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  stdConfig.gpio_cfg.bclk = PIN_I2S_BCLK;
  stdConfig.gpio_cfg.ws = PIN_I2S_LRCK;
  stdConfig.gpio_cfg.dout = PIN_I2S_DOUT;
  stdConfig.gpio_cfg.din = PIN_I2S_DIN;
  stdConfig.gpio_cfg.invert_flags.mclk_inv = false;
  stdConfig.gpio_cfg.invert_flags.bclk_inv = false;
  stdConfig.gpio_cfg.invert_flags.ws_inv = false;

  err = i2s_channel_init_std_mode(gI2sTx, &stdConfig);
  if (err != ESP_OK) return err;
  apply_cardputer_i2s_clock();
  return i2s_channel_enable(gI2sTx);
}

static bool probe_i2c_device(uint8_t address, uint8_t reg) {
  uint8_t value = 0;
  return i2c_master_write_read_device(I2C_NUM_0, address, &reg, 1, &value, 1, pdMS_TO_TICKS(50)) == ESP_OK;
}

static esp_err_t write_codec_reg(uint8_t reg, uint8_t value) {
  const uint8_t data[2] = {reg, value};
  return i2c_master_write_to_device(I2C_NUM_0, ES8311_ADDRESS_0, data, sizeof(data), pdMS_TO_TICKS(100));
}

static esp_err_t init_codec() {
  for (int attempt = 0; attempt < 12; ++attempt) {
    if (probe_i2c_device(ES8311_ADDRESS_0, ES8311_CHD1_REGFD)) break;
    vTaskDelay(pdMS_TO_TICKS(50));
  }

  static constexpr uint8_t CARDPUTER_ADV_DAC_INIT[][2] = {
      {ES8311_RESET_REG00, 0x80},
      {ES8311_CLK_MANAGER_REG01, 0xB5},
      {ES8311_CLK_MANAGER_REG02, 0x18},
      {ES8311_SYSTEM_REG0D, 0x01},
      {ES8311_SYSTEM_REG12, 0x00},
      {ES8311_SYSTEM_REG13, 0x10},
      {ES8311_DAC_REG32, 0xBF},
      {ES8311_DAC_REG37, 0x08},
  };

  for (const auto& regValue : CARDPUTER_ADV_DAC_INIT) {
    ESP_RETURN_ON_ERROR(write_codec_reg(regValue[0], regValue[1]), TAG, "ES8311 register init failed");
  }

  return ESP_OK;
}

static void control_task(void*) {
  SynthAudioState controlState = {};
  controlState.waveform = Waveform::Sine;
  controlState.masterVolume = INITIAL_MASTER_VOLUME;
  char chord[16] = "--";
  publish_ui_from_audio_state(controlState, chord);

  for (;;) {
    SynthEvent event = {};
    if (xQueueReceive(gSynthEventQueue, &event, portMAX_DELAY) != pdTRUE) continue;

    switch (event.type) {
      case SynthEventType::NoteOn:
        note_on(&controlState, event.noteIndex, event.midi);
        break;
      case SynthEventType::NoteOff:
        note_off(&controlState, event.noteIndex);
        break;
      case SynthEventType::SetWaveform:
        controlState.waveform = event.waveform;
        break;
      case SynthEventType::SetVolume:
        controlState.masterVolume = clamp_float(event.value, 0.0f, 1.0f);
        break;
      case SynthEventType::AdjustVolume:
        controlState.masterVolume = clamp_float(controlState.masterVolume + event.value, 0.0f, 1.0f);
        break;
      case SynthEventType::AllNotesOff:
        all_notes_off(&controlState);
        break;
    }

    detect_chord(controlState, chord, sizeof(chord));
    portENTER_CRITICAL(&gAudioStateMux);
    gAudioState = controlState;
    portEXIT_CRITICAL(&gAudioStateMux);
    publish_ui_from_audio_state(controlState, chord);
  }
}

static void audio_task(void*) {
  static int32_t audioBuffer[(AUDIO_BUFFER_FRAMES + 1) / 2];

  for (;;) {
    render_audio_buffer(audioBuffer, AUDIO_BUFFER_FRAMES);
    size_t bytesWritten = 0;
    if (gI2sTx != nullptr) {
      i2s_channel_write(gI2sTx,
                        audioBuffer,
                        AUDIO_BUFFER_FRAMES * sizeof(int16_t),
                        &bytesWritten,
                        portMAX_DELAY);
    } else {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
}

static void input_task(void*) {
  CardputerKeyboard keyboard;
  const bool keyboardReady = keyboard.begin();
  publish_keyboard_status(keyboardReady, keyboardReady ? "kbd: ready" : keyboard.diagnostic());

  if (!keyboardReady) {
    ESP_LOGW(TAG, "Keyboard init failed: %s", keyboard.diagnostic());
  }

  for (;;) {
    if (keyboardReady) {
      CardputerKeyEvent keyEvent = {};
      while (keyboard.readEvent(&keyEvent)) {
        if (keyEvent.key == CardputerKey::Character) {
          const KeyNote* note = find_note_by_key(keyEvent.character);
          if (note != nullptr) send_note_event(*note, keyEvent.pressed);
          continue;
        }

        if (!keyEvent.pressed) continue;
        switch (keyEvent.key) {
          case CardputerKey::F1:
            send_waveform_event(Waveform::Sine);
            break;
          case CardputerKey::F2:
            send_waveform_event(Waveform::Square);
            break;
          case CardputerKey::F3:
            send_waveform_event(Waveform::Rectangle);
            break;
          case CardputerKey::F4:
            send_waveform_event(Waveform::Saw);
            break;
          case CardputerKey::Up:
            send_volume_delta(VOLUME_STEP);
            break;
          case CardputerKey::Down:
            send_volume_delta(-VOLUME_STEP);
            break;
          default:
            break;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(INPUT_POLL_MS));
  }
}

static uint16_t color_bg() { return CardputerDisplay::rgb565(11, 16, 24); }
static uint16_t color_panel() { return CardputerDisplay::rgb565(8, 12, 18); }
static uint16_t color_text() { return CardputerDisplay::rgb565(199, 215, 239); }
static uint16_t color_muted() { return CardputerDisplay::rgb565(143, 160, 187); }
static uint16_t color_green() { return CardputerDisplay::rgb565(155, 255, 183); }
static uint16_t color_blue() { return CardputerDisplay::rgb565(124, 199, 255); }
static uint16_t color_stroke() { return CardputerDisplay::rgb565(52, 68, 93); }

static void draw_wave_icon(CardputerDisplay& display, Waveform waveform, int x, int y, uint16_t color) {
  switch (waveform) {
    case Waveform::Sine:
      display.drawLine(x, y + 4, x + 2, y + 2, color);
      display.drawLine(x + 2, y + 2, x + 4, y + 1, color);
      display.drawLine(x + 4, y + 1, x + 7, y + 5, color);
      display.drawLine(x + 7, y + 5, x + 9, y + 6, color);
      display.drawLine(x + 9, y + 6, x + 12, y + 2, color);
      break;
    case Waveform::Square:
      display.drawLine(x, y + 2, x + 4, y + 2, color);
      display.drawLine(x + 4, y + 2, x + 4, y + 6, color);
      display.drawLine(x + 4, y + 6, x + 8, y + 6, color);
      display.drawLine(x + 8, y + 6, x + 8, y + 2, color);
      display.drawLine(x + 8, y + 2, x + 12, y + 2, color);
      break;
    case Waveform::Rectangle:
      display.drawLine(x, y + 2, x + 3, y + 2, color);
      display.drawLine(x + 3, y + 2, x + 3, y + 6, color);
      display.drawLine(x + 3, y + 6, x + 6, y + 6, color);
      display.drawLine(x + 6, y + 6, x + 6, y + 2, color);
      display.drawLine(x + 6, y + 2, x + 12, y + 2, color);
      break;
    case Waveform::Saw:
      display.drawLine(x, y + 6, x + 11, y + 1, color);
      display.drawLine(x + 11, y + 1, x + 11, y + 6, color);
      display.drawLine(x + 11, y + 6, x + 13, y + 5, color);
      break;
  }
}

static void draw_wave_selector(CardputerDisplay& display, Waveform option, Waveform selected, int x, const char* label) {
  const bool active = option == selected;
  const uint16_t fill = active ? CardputerDisplay::rgb565(25, 51, 34) : CardputerDisplay::rgb565(16, 24, 35);
  const uint16_t stroke = active ? color_green() : color_stroke();
  const uint16_t text = active ? CardputerDisplay::rgb565(215, 255, 225) : CardputerDisplay::rgb565(184, 200, 223);
  display.fillRoundRect(x, 19, 27, 13, 2, fill);
  display.drawRoundRect(x, 19, 27, 13, 2, stroke);
  display.drawText(label, x + 3, 22, text, 1);
  draw_wave_icon(display, option, x + 14, 22, text);
}

static void draw_spark_axes(CardputerDisplay& display, int x, int y, int w, int h) {
  display.fillRect(x, y, w, h, color_panel());
  display.drawRect(x, y, w, h, color_stroke());
  display.drawLine(x, y + h / 2, x + w - 1, y + h / 2, CardputerDisplay::rgb565(45, 57, 76));
}

static void draw_wave_preview(CardputerDisplay& display, const UiState& state) {
  const int x = 10;
  const int y = 41;
  const int w = 87;
  const int h = 37;
  draw_spark_axes(display, x, y, w, h);
  display.drawText("WAVE", x + 3, y + 4, color_muted(), 1);
  display.drawText(waveform_short_name(state.waveform), x + 55, y + 4, color_green(), 1);

  int prevX = x;
  int prevY = y + h / 2;
  for (int i = 0; i < 40; ++i) {
    const float phase = static_cast<float>(i) / 39.0f;
    const float sample = oscillator_sample(phase, state.waveform);
    const int px = x + 2 + (i * (w - 5)) / 39;
    const int py = y + h / 2 - static_cast<int>(sample * 13.0f);
    if (i > 0) display.drawLine(prevX, prevY, px, py, color_green());
    prevX = px;
    prevY = py;
  }
}

static void draw_output_preview(CardputerDisplay& display) {
  const int x = 115;
  const int y = 40;
  const int w = 85;
  const int h = 39;
  draw_spark_axes(display, x, y, w, h);
  display.drawText("OUT", x + 3, y + 4, color_muted(), 1);

  SynthAudioState preview = {};
  copy_audio_state(&preview);
  if (preview.activeCount == 0) return;

  ActiveNote notes[MAX_POLYPHONY] = {};
  for (int i = 0; i < MAX_POLYPHONY; ++i) notes[i] = preview.notes[i];

  int prevX = x;
  int prevY = y + h / 2;
  const float normalize = preview.activeCount <= MAX_POLYPHONY ? INV_SQRT[preview.activeCount] : INV_SQRT[MAX_POLYPHONY];
  for (int i = 0; i < 42; ++i) {
    float mixed = 0.0f;
    for (auto& note : notes) {
      if (!note.active) continue;
      mixed += oscillator_sample(note.phase, preview.waveform) * PER_NOTE_GAIN;
      note.phase += note.phaseIncrement * 4.0f;
      if (note.phase >= 1.0f) note.phase -= floorf(note.phase);
    }
    mixed = clamp_float(mixed * normalize * preview.masterVolume, -1.0f, 1.0f);
    const int px = x + 2 + (i * (w - 5)) / 41;
    const int py = y + h / 2 - static_cast<int>(mixed * 15.0f);
    if (i > 0) display.drawLine(prevX, prevY, px, py, color_blue());
    prevX = px;
    prevY = py;
  }
}

static void draw_volume(CardputerDisplay& display, float volume) {
  const int x = 220;
  const int y = 85;
  const int w = 10;
  const int h = 34;
  display.drawRoundRect(x, y, w, h, 3, color_stroke());
  const int fillH = static_cast<int>((h - 4) * clamp_float(volume, 0.0f, 1.0f));
  display.fillRoundRect(x + 2, y + h - 2 - fillH, w - 4, fillH, 1, color_blue());
  display.drawText("VOL", 218, 121, color_muted(), 1);
}

static void draw_white_key(CardputerDisplay& display, int x, char label, uint8_t noteIndex, uint32_t pressedMask) {
  const bool active = (pressedMask & (1UL << noteIndex)) != 0;
  const uint16_t fill = active ? CardputerDisplay::rgb565(216, 236, 255) : CardputerDisplay::rgb565(248, 251, 255);
  const uint16_t stroke = active ? color_blue() : CardputerDisplay::rgb565(15, 22, 31);
  display.fillRoundRect(x, 84, 10, 35, 2, fill);
  display.drawRoundRect(x, 84, 10, 35, 2, stroke);
  char text[2] = {label, '\0'};
  display.drawTextCentered(text, x + 5, 111, CardputerDisplay::rgb565(0, 0, 0), 1);
}

static void draw_black_key(CardputerDisplay& display, int x, char label, uint8_t noteIndex, uint32_t pressedMask) {
  const bool active = (pressedMask & (1UL << noteIndex)) != 0;
  const uint16_t fill = active ? CardputerDisplay::rgb565(37, 65, 95) : CardputerDisplay::rgb565(0, 0, 0);
  const uint16_t stroke = active ? color_blue() : CardputerDisplay::rgb565(92, 92, 92);
  display.fillRoundRect(x, 84, 10, 21, 2, fill);
  display.drawRoundRect(x, 84, 10, 21, 2, stroke);
  char text[2] = {label, '\0'};
  display.drawTextCentered(text, x + 5, 93, CardputerDisplay::rgb565(248, 251, 255), 1);
}

static void draw_piano(CardputerDisplay& display, uint32_t pressedMask) {
  struct WhiteKey {
    int x;
    char label;
    uint8_t noteIndex;
  };
  static constexpr WhiteKey WHITE_KEYS[] = {
      {45, 'z', 0},  {55, 'x', 2},  {65, 'c', 4},  {75, 'v', 5},  {85, 'b', 7},
      {95, 'n', 9},  {105, 'm', 11}, {115, 'q', 12}, {125, 'w', 14}, {135, 'e', 16},
      {145, 'r', 17}, {155, 't', 19}, {165, 'y', 21}, {175, 'u', 23}, {185, 'i', 24},
  };
  struct BlackKey {
    int x;
    char label;
    uint8_t noteIndex;
  };
  static constexpr BlackKey BLACK_KEYS[] = {
      {50, 's', 1},   {60, 'd', 3},   {80, 'g', 6},   {90, 'h', 8},   {100, 'j', 10},
      {120, '2', 13}, {130, '3', 15}, {150, '5', 18}, {160, '6', 20}, {170, '7', 22},
  };

  for (const auto& key : WHITE_KEYS) draw_white_key(display, key.x, key.label, key.noteIndex, pressedMask);
  for (const auto& key : BLACK_KEYS) draw_black_key(display, key.x, key.label, key.noteIndex, pressedMask);
  display.drawTextCentered("C4", 52, 125, CardputerDisplay::rgb565(159, 178, 207), 1);
  display.drawTextCentered("C5", 122, 125, CardputerDisplay::rgb565(159, 178, 207), 1);
  display.drawTextCentered("C6", 192, 125, CardputerDisplay::rgb565(159, 178, 207), 1);
}

static void draw_status_led(CardputerDisplay& display, int x, bool ok, const char* label) {
  const uint16_t fill = ok ? CardputerDisplay::rgb565(38, 93, 58) : CardputerDisplay::rgb565(76, 39, 39);
  const uint16_t stroke = ok ? color_green() : CardputerDisplay::rgb565(255, 150, 150);
  display.fillRoundRect(x, 122, 19, 8, 2, fill);
  display.drawRoundRect(x, 122, 19, 8, 2, stroke);
  display.drawText(label, x + 3, 123, ok ? color_green() : CardputerDisplay::rgb565(255, 180, 180), 1);
}

static void draw_ui(CardputerDisplay& display, const UiState& state) {
  display.clear(color_bg());
  display.drawText("pocketsynth", 9, 6, color_text(), 1);

  char polyText[8] = {};
  snprintf(polyText, sizeof(polyText), "%u/8", state.activeCount);
  display.drawText(polyText, 84, 6, CardputerDisplay::rgb565(159, 178, 207), 1);
  display.drawText("CHORD", 115, 6, color_muted(), 1);
  display.drawText(state.chord, 149, 6, color_green(), 1);

  display.drawRoundRect(216, 6, 19, 7, 3, color_stroke());
  display.fillRoundRect(218, 8, 9, 3, 1, CardputerDisplay::rgb565(118, 187, 64));

  draw_wave_selector(display, Waveform::Sine, state.waveform, 8, "F1");
  draw_wave_selector(display, Waveform::Square, state.waveform, 38, "F2");
  draw_wave_selector(display, Waveform::Rectangle, state.waveform, 68, "F3");
  draw_wave_selector(display, Waveform::Saw, state.waveform, 98, "F4");
  draw_wave_preview(display, state);
  draw_output_preview(display);
  draw_volume(display, state.masterVolume);
  draw_piano(display, state.pressedMask);

  draw_status_led(display, 8, state.audioReady, "AU");
  draw_status_led(display, 30, state.codecReady, "CD");
  draw_status_led(display, 202, state.keyboardReady, "KB");
}

static void ui_task(void*) {
  CardputerDisplay display;
  if (!display.begin()) {
    ESP_LOGE(TAG, "Display init failed");
    vTaskDelete(nullptr);
    return;
  }

  UiState state = {};
  uint32_t lastVersion = UINT32_MAX;
  for (;;) {
    copy_ui_state(&state);
    if (state.version != lastVersion) {
      draw_ui(display, state);
      display.flush();
      lastVersion = state.version;
    }
    vTaskDelay(pdMS_TO_TICKS(UI_FRAME_MS));
  }
}

static void print_status() {
  UiState ui = {};
  copy_ui_state(&ui);
  printf("[pocketsynth] wave=%s vol=%d poly=%u/8 chord=%s audio=%s codec=%s keyboard=%s %s\n",
         waveform_short_name(ui.waveform),
         static_cast<int>(ui.masterVolume * 100.0f + 0.5f),
         ui.activeCount,
         ui.chord,
         ui.audioReady ? "ok" : "fail",
         ui.codecReady ? "ok" : "fail",
         ui.keyboardReady ? "ok" : "fail",
         ui.keyboardDiag);
}

static void run_smoke_sequence() {
  puts("[pocketsynth] smoke: z x c, C major chord, wave/volume changes");
  const char singleNotes[] = {'z', 'x', 'c'};
  for (char key : singleNotes) {
    const KeyNote* note = find_note_by_key(key);
    if (!note) continue;
    send_note_event(*note, true);
    vTaskDelay(pdMS_TO_TICKS(180));
    send_note_event(*note, false);
    vTaskDelay(pdMS_TO_TICKS(80));
  }

  const char chordNotes[] = {'z', 'c', 'b'};
  for (char key : chordNotes) {
    const KeyNote* note = find_note_by_key(key);
    if (note) send_note_event(*note, true);
  }
  vTaskDelay(pdMS_TO_TICKS(250));
  send_waveform_event(Waveform::Square);
  vTaskDelay(pdMS_TO_TICKS(250));
  send_waveform_event(Waveform::Rectangle);
  vTaskDelay(pdMS_TO_TICKS(250));
  send_waveform_event(Waveform::Saw);
  vTaskDelay(pdMS_TO_TICKS(250));
  send_waveform_event(Waveform::Sine);
  send_volume_delta(-0.20f);
  vTaskDelay(pdMS_TO_TICKS(250));
  send_volume_delta(0.20f);
  vTaskDelay(pdMS_TO_TICKS(250));
  send_all_notes_off();
  vTaskDelay(pdMS_TO_TICKS(50));
  print_status();
}

static bool parse_waveform_number(char number, Waveform* waveform) {
  if (waveform == nullptr) return false;
  switch (number) {
    case '1':
      *waveform = Waveform::Sine;
      return true;
    case '2':
      *waveform = Waveform::Square;
      return true;
    case '3':
      *waveform = Waveform::Rectangle;
      return true;
    case '4':
      *waveform = Waveform::Saw;
      return true;
    default:
      return false;
  }
}

static void handle_serial_command(const char* command) {
  if (command == nullptr || command[0] == '\0') return;

  if (strcmp(command, "status") == 0) {
    print_status();
    return;
  }
  if (strcmp(command, "alloff") == 0) {
    send_all_notes_off();
    puts("[pocketsynth] all notes off");
    return;
  }
  if (strcmp(command, "smoke") == 0) {
    run_smoke_sequence();
    return;
  }
  if (strncmp(command, "on ", 3) == 0 && command[3] != '\0') {
    const KeyNote* note = find_note_by_key(command[3]);
    if (note != nullptr) {
      send_note_event(*note, true);
      printf("[pocketsynth] note on %c\n", note->key);
    }
    return;
  }
  if (strncmp(command, "off ", 4) == 0 && command[4] != '\0') {
    const KeyNote* note = find_note_by_key(command[4]);
    if (note != nullptr) {
      send_note_event(*note, false);
      printf("[pocketsynth] note off %c\n", note->key);
    }
    return;
  }
  if (strncmp(command, "wave ", 5) == 0 && command[5] != '\0') {
    Waveform waveform = Waveform::Sine;
    if (parse_waveform_number(command[5], &waveform)) {
      send_waveform_event(waveform);
      printf("[pocketsynth] wave %s\n", waveform_short_name(waveform));
    }
    return;
  }
  if (strncmp(command, "vol ", 4) == 0 && command[4] != '\0') {
    const float percent = strtof(command + 4, nullptr);
    send_volume_absolute(clamp_float(percent / 100.0f, 0.0f, 1.0f));
    printf("[pocketsynth] volume %d\n", static_cast<int>(clamp_float(percent, 0.0f, 100.0f)));
    return;
  }

  puts("[pocketsynth] commands: status, smoke, on <key>, off <key>, wave 1..4, vol 0..100, alloff");
}

static void serial_task(void*) {
  usb_serial_jtag_driver_config_t serialConfig = {};
  serialConfig.tx_buffer_size = 512;
  serialConfig.rx_buffer_size = 256;
  const esp_err_t serialErr = usb_serial_jtag_driver_install(&serialConfig);
  if (serialErr != ESP_OK && serialErr != ESP_ERR_INVALID_STATE) {
    ESP_LOGW(TAG, "USB Serial/JTAG RX setup failed: %s", esp_err_to_name(serialErr));
  }

  puts("[pocketsynth] serial ready: status, smoke, on/off, wave, vol, alloff");
  char command[48] = {};
  size_t length = 0;

  for (;;) {
    uint8_t byte = 0;
    const int read = usb_serial_jtag_read_bytes(&byte, 1, pdMS_TO_TICKS(100));
    if (read <= 0) continue;
    const char ch = static_cast<char>(byte);

    if (ch == '\r' || ch == '\n') {
      command[length] = '\0';
      handle_serial_command(command);
      length = 0;
      command[0] = '\0';
      continue;
    }

    if (length < sizeof(command) - 1) {
      command[length++] = ch;
    }
  }
}

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "Starting pocketsynth iteration 1");

  gSynthEventQueue = xQueueCreate(32, sizeof(SynthEvent));
  if (gSynthEventQueue == nullptr) {
    ESP_LOGE(TAG, "Synth event queue allocation failed");
    return;
  }

  portENTER_CRITICAL(&gAudioStateMux);
  gAudioState.waveform = Waveform::Sine;
  gAudioState.masterVolume = INITIAL_MASTER_VOLUME;
  portEXIT_CRITICAL(&gAudioStateMux);

  esp_err_t err = ensure_i2c_bus();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "I2C init failed: %s", esp_err_to_name(err));
  }

  bool audioReady = false;
  bool codecReady = false;
  err = init_i2s();
  if (err == ESP_OK) {
    audioReady = true;
  } else {
    ESP_LOGE(TAG, "I2S init failed: %s", esp_err_to_name(err));
  }

  if (audioReady) {
    err = init_codec();
    if (err == ESP_OK) {
      codecReady = true;
    } else {
      ESP_LOGE(TAG, "ES8311 init failed: %s", esp_err_to_name(err));
    }
  }
  publish_hardware_status(audioReady, codecReady);

  xTaskCreatePinnedToCore(control_task, "SynthControlTask", CONTROL_TASK_STACK, nullptr, CONTROL_TASK_PRIORITY, nullptr, 0);
  xTaskCreatePinnedToCore(input_task, "InputTask", INPUT_TASK_STACK, nullptr, INPUT_TASK_PRIORITY, nullptr, 0);
  xTaskCreatePinnedToCore(serial_task, "SerialDebugTask", SERIAL_TASK_STACK, nullptr, SERIAL_TASK_PRIORITY, nullptr, 0);
  xTaskCreatePinnedToCore(ui_task, "UiTask", UI_TASK_STACK, nullptr, UI_TASK_PRIORITY, nullptr, 1);
  xTaskCreatePinnedToCore(audio_task, "AudioTask", AUDIO_TASK_STACK, nullptr, AUDIO_TASK_PRIORITY, nullptr, 1);
}
