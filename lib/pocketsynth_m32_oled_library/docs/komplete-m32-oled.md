# Komplete Kontrol M32 OLED Integration

This document describes the experimental `pocketsynth` library that writes directly to
the Native Instruments Komplete Kontrol M32 OLED from the ESP32-S3 USB host.

The integration is independent from class-compliant USB MIDI. MIDI notes and CCs are
received through the MIDI streaming interface; OLED updates are sent through the M32
HID interface.

## Scope

Implemented:

- USB Host client for the M32 HID interface.
- Native Instruments VID/PID filtering: `0x17cc:0x1860`.
- HID OUT endpoint discovery.
- 128 × 32 monochrome framebuffer.
- Pixel drawing.
- Lines, rectangles, filled rectangles, bars and waveform icons.
- Pixel-perfect 5 × 7 text rendering without antialiasing.
- Frame queue with latest-frame-wins behavior.
- Full OLED frame updates using two HID OUT reports.
- JSON status writer for diagnostics.
- Optional build flag: `POCKETSYNTH_ENABLE_M32_OLED`.

Not implemented yet:

- Reading proprietary M32 knobs/buttons from HID input reports.
- Komplete Kontrol/NKS protocol compatibility.
- Text rendering with proportional fonts or Unicode.
- Partial dirty-region tracking. The current implementation always sends the full
  128 × 32 display as two 128 × 16 regions.

## Hardware setup

Recommended setup:

```text
Cardputer ADV USB-C -> powered USB-C hub with PD
Hub PD input -> charger or power bank
Hub USB port -> Komplete Kontrol M32
```

The M32 is bus-powered, so a powered hub is strongly recommended.

## Build flag

The library is enabled in the WiFi development environment:

```ini
-DPOCKETSYNTH_ENABLE_M32_OLED=1
```

Build:

```powershell
pio run -e cardputer_adv_wifi_dev
```

Upload initially by serial:

```powershell
pio run -e cardputer_adv_wifi_dev -t upload
```

Then OTA can be used as usual:

```powershell
python tools/ota_upload.py --host 192.168.4.1 --bin .pio/build/cardputer_adv_wifi_dev/firmware.bin
```

## Public API

Header:

```cpp
#include "m32_oled.h"
```

Main functions:

```cpp
bool isM32OledBuildEnabled();
esp_err_t initializeM32Oled();
esp_err_t sendM32OledFrame(const M32OledFramebuffer& frame, TickType_t timeoutTicks = 0);
esp_err_t sendM32OledText(const char* line1,
                          const char* line2 = nullptr,
                          const char* line3 = nullptr,
                          TickType_t timeoutTicks = 0);
esp_err_t sendM32OledParameter(const char* name,
                               const char* value,
                               float normalizedValue,
                               TickType_t timeoutTicks = 0);
M32OledStatus getM32OledStatus();
size_t writeM32OledStatusJson(char* out, size_t outSize);
```

Framebuffer helpers:

```cpp
M32OledFramebuffer frame;
frame.clear(false);
frame.setPixel(0, 0, true);
frame.drawText(0, 0, "POCKETSYNTH", 1);
frame.drawBar(0, 24, 64, 8, 0.75f);
frame.drawWaveIcon(86, 5, 38, 20);
sendM32OledFrame(frame, pdMS_TO_TICKS(10));
```

Simple text:

```cpp
sendM32OledText("POCKETSYNTH", "M32 OLED", "USB HID OK");
```

Parameter feedback:

```cpp
sendM32OledParameter("CUTOFF", "1.24 KHZ", 0.62f);
```

## OLED protocol

The screen is treated as:

```text
128 px wide
32 px high
1 bit per pixel
4 pages of 8 vertical pixels
```

A full refresh is sent as two reports:

```text
Top half:    E0 00 00 00 00 80 00 02 00 [256 bytes]
Bottom half: E0 00 00 02 00 80 00 02 00 [256 bytes]
```

Decoded header:

```text
byte 0      report id / command: 0xE0
bytes 1-2   x offset, little-endian
bytes 3-4   page offset, little-endian
bytes 5-6   width, little-endian
bytes 7-8   height in 8-pixel pages, little-endian
bytes 9..   vertical-page bitmap payload
```

The payload is generated from the logical framebuffer. The default output is
inverted because the real M32 OLED test showed that this matches visible white
pixels on the device:

```cpp
setM32OledOutputInverted(true);
```

## Architecture

```text
Application/UI/Synth code
  -> M32OledFramebuffer drawing helpers
  -> sendM32OledFrame()
  -> FreeRTOS frame queue
  -> M32OledHost task
  -> ESP-IDF USB Host client
  -> M32 HID OUT endpoint
  -> OLED report 0xE0
```

The USB host runtime is shared with the existing MIDI host and USB diagnostics.
`initializeUsbHostRuntime()` is now enabled when any of these flags is active:

```text
POCKETSYNTH_ENABLE_USB_HOST_DIAG
POCKETSYNTH_ENABLE_USB_MIDI_HOST
POCKETSYNTH_ENABLE_M32_OLED
```

## Integration idea for PocketSynth parameters

Initial mapping proposal:

| M32 control | PocketSynth parameter | OLED feedback |
| --- | --- | --- |
| Knob 1 | Attack | `ATTACK 34 MS` + bar |
| Knob 2 | Decay | `DECAY 120 MS` + bar |
| Knob 3 | Sustain | `SUSTAIN 70%` + bar |
| Knob 4 | Release | `RELEASE 480 MS` + bar |
| Knob 5 | Cutoff | `CUTOFF 1.24 KHZ` + bar |
| Knob 6 | Resonance | `RESONANCE 42%` + bar |
| Knob 7 | LFO rate | `LFO RATE 4.2 HZ` + bar |
| Knob 8 | LFO depth | `LFO DEPTH 31%` + bar |

The current library only writes the OLED. HID input decoding for proprietary knob
reports should be added in a separate iteration after short per-control captures.

## Diagnostics

Expected boot log when enabled:

```text
pocketsynth: Komplete M32 OLED build=yes
m32_oled: M32 OLED host client ready
m32_oled: Komplete M32 OLED HID OUT addr=... intf=... ep=0x01 mps=...
```

If the M32 does not update:

1. Check power: use a powered hub.
2. Check the build flag `POCKETSYNTH_ENABLE_M32_OLED=1`.
3. Confirm that no other host is holding the M32.
4. Check logs for `hid_not_found`, `claim_error`, `transfer_error` or `stall`.
5. Temporarily disable `POCKETSYNTH_ENABLE_USB_MIDI_HOST` if the ESP-IDF host stack
   refuses two clients on the same composite USB device. If that happens, the next
   refactor should merge MIDI and OLED into one composite M32 driver.

## Known risk

This is based on observed USB traffic and a successful WebHID proof of concept, not
on a public Native Instruments protocol specification. Treat the OLED transport as
experimental and keep it behind the build flag until it is validated on hardware.
