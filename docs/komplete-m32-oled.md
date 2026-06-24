# Komplete M32 OLED Integration

This document describes the experimental `pocketsynth` library that writes to
the Native Instruments Komplete Kontrol M32 OLED from the ESP32-S3 USB host.

The OLED path is integrated with the existing USB MIDI host. MIDI notes and CCs
are still received through the class-compliant MIDI streaming interface; OLED
updates are sent to the M32 HID OUT endpoint.

## Scope

Implemented:

- Native Instruments VID/PID filtering: `0x17cc:0x1860`.
- M32 HID OUT endpoint discovery.
- 128 x 32 monochrome framebuffer.
- Pixel drawing, lines, rectangles, filled rectangles, bars, waveform icons,
  and crisp 5 x 7 text.
- Frame queue with latest-frame-wins behavior.
- Full OLED frame updates using two 128 x 16 HID OUT reports.
- Splash frame on M32 connection.
- OLED feedback for MIDI Note On/Off, Control Change, and Pitch Bend.
- Forced repaint window after input feedback to keep the PocketSynth UI visible
  over the M32's built-in `Template 1` MIDI-mode screen.
- OLED ownership heartbeat that re-sends the latest PocketSynth frame roughly
  every 80 ms while the M32 is connected.
- Raw HID OUT reports on the OLED endpoint, currently used for the observed
  `0x80` LED/state report.
- JSON status in `GET /status`.
- Bounded logs in `GET /logs`.
- Optional build flag: `POCKETSYNTH_ENABLE_M32_OLED`.

Not implemented yet:

- Proprietary HID input report decoding for non-MIDI M32 buttons.
- A known proprietary command that disables the M32's built-in `Template 1`
  MIDI-mode display owner. The capture did not show one; the current firmware
  wins ownership by repainting the PocketSynth bitmap shortly after each input
  event and periodically while connected.
- Full suppression of the built-in `Template 1` / `CC ##` screen. It can still
  appear briefly when the M32 firmware redraws its own MIDI-template UI, but the
  PocketSynth heartbeat should repaint over it quickly. A future iteration should
  find a real M32 host/takeover command or another transport strategy that
  prevents the built-in screen from appearing at all.
- Komplete Kontrol, NKS, DAW integration, or M32 display protocol beyond bitmap
  OLED writes.
- Proportional fonts, Unicode, or partial dirty-region tracking.

## Hardware Setup

Use SETUP D:

```text
Cardputer ADV USB-C -> powered USB-C hub with PD
Hub PD input -> charger or power bank
Hub USB port -> Komplete M32
PC and Cardputer ADV -> same WiFi or Cardputer AP
```

The M32 is bus-powered, so a powered hub is strongly recommended.

## Build Flag

The library is enabled in the WiFi development environment:

```ini
-DPOCKETSYNTH_ENABLE_M32_OLED=1
```

Build:

```powershell
pio run -e cardputer_adv_wifi_dev
```

OTA upload:

```powershell
python tools/ota_upload.py --host 192.168.31.147 --bin .pio/build/cardputer_adv_wifi_dev/firmware.bin
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
void notifyM32OledMidiNote(uint8_t midi, bool pressed, uint8_t velocity);
void notifyM32OledControlChange(uint8_t control, uint8_t value);
void notifyM32OledPitchBend(int16_t pitchBend);
M32OledStatus getM32OledStatus();
size_t writeM32OledStatusJson(char* out, size_t outSize);
```

## OLED Protocol

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

The WebHID reference in `tools/m32-oled-webhid-v2.zip` sends this as
`sendReport(0xE0, report.slice(1))`. The browser prepends report ID `0xE0`, so
USBPcap/Wireshark shows the same 265-byte interrupt OUT payload that firmware
sends directly.

Default output is inverted because the M32 OLED expects the observed visible
pixel polarity:

```cpp
setM32OledOutputInverted(true);
```

## Transport Architecture

```text
USB MIDI packets
  -> parseUsbMidiPacket()
  -> notifyM32Oled...
  -> M32 OLED feedback task
  -> framebuffer queue
  -> usb_midi_host composite M32 transport
  -> HID OUT endpoint 0x01
```

The M32 HID interface exposes:

```text
Interface 2, alternate 0
  ep 0x81 interrupt IN,  MPS 64, interval 1
  ep 0x01 interrupt OUT, MPS 64, interval 10
```

ESP-IDF's public `usb_host_interface_claim()` claims all endpoints in an
interface. With MIDI already claimed, claiming the full HID interface can fail
with `ESP_ERR_NOT_SUPPORTED` because the HID IN endpoint consumes extra host
resources that the OLED writer does not need.

The firmware therefore uses this order:

1. Claim the MIDI streaming interface normally for MIDI IN.
2. Try to claim the full M32 HID interface.
3. If that fails, allocate only HID OUT endpoint `0x01` through the USBH layer.

The direct OUT endpoint path is intentionally limited to the M32 OLED build and
is not used in normal firmware.

## LED / State Report

The capture also contains HID OUT report `0x80` on endpoint `0x01`:

```text
80 7c 7c 7c 00 00 00 00 00 00 00 00 00 7c 7c 00 7c 7c 7e 00 7c 7c
```

Observed changes suggest this 22-byte report controls lit/dim/off state for
M32 control buttons. Known correlated positions:

```text
HID IN byte 1 bit 0x02 -> OUT 0x80 byte 2
HID IN byte 1 bit 0x04 -> OUT 0x80 byte 3
HID IN byte 2 bit 0x10 -> OUT 0x80 byte 13
HID IN byte 2 bit 0x20 -> OUT 0x80 byte 14
HID IN byte 2 bit 0x40 -> OUT 0x80 byte 15
HID IN byte 2 bit 0x80 -> OUT 0x80 byte 16
HID IN byte 3 bit 0x10 -> OUT 0x80 byte 21
```

Values observed:

```text
0x00 off
0x7c normal/dim lit
0x7e highlighted/pressed
0x02 alternate low state on byte 18
```

The firmware sends the baseline `0x80` report on connection and exposes a raw
HID OUT queue so future UI code can drive these states deliberately.

## Diagnostics

Expected log sequence on SETUP D:

```text
pocketsynth: Komplete M32 OLED build=yes
usb_midi: MIDI intf=1 alt=0 eps=2
usb_midi: M32 HID intf=2 alt=0 eps=2
usb_midi: M32 OLED claim failed: ESP_ERR_NOT_SUPPORTED
usb_midi: M32 OLED using direct OUT endpoint intf=2 ep=0x01
usb_midi: M32 OLED direct addr=3 intf=2 ep=0x01
usb_midi: MIDI IN addr=3 vid=0x17cc pid=0x1860 intf=1 ep=0x82
```

Expected `GET /status` values after a successful splash and feedback:

```json
"m32_oled": {
  "device_connected": true,
  "endpoint_address": "0x01",
  "frame_count": 1,
  "transfer_count": 2
}
```

On 2026-06-24, SETUP D over WiFi at `192.168.31.147` validated:

```text
frame_count: 23
transfer_count: 46
feedback_count: 37
transfer_active: false
```

MIDI packets continued to arrive while OLED feedback frames were being sent.

With the ownership heartbeat enabled, `frame_count` should continue increasing
while the M32 is connected even when the user is not moving a control. On
2026-06-25, SETUP D showed `frame_count=94` and `transfer_count=190` about
9.4 seconds after boot.

`GET /status` now includes a `usb_midi` object. The M32 exposes HID IN endpoint
`0x81`, and the PC host in the capture uses that stream to detect control
button activity. On the Cardputer ADV test setup, attempting to reserve HID IN
directly alongside MIDI IN and OLED OUT returned `ESP_ERR_NOT_SUPPORTED`, so
`usb_midi.hid_in_endpoint_allocated` remains `false`. The heartbeat is the
current workaround for M32 controls that redraw `Template 1` without producing
MIDI.

## Troubleshooting

If the M32 OLED does not update:

1. Check power: use a powered hub.
2. Confirm `POCKETSYNTH_ENABLE_M32_OLED=1`.
3. Confirm `GET /status` shows `"m32_oled_enabled": true`.
4. Check `/logs` for `M32 OLED using direct OUT endpoint`.
5. If only `claim_error` appears and no direct endpoint log appears, inspect the
   HID descriptor and endpoint `0x01`.
6. If `transfer_status` or `stall` appears, compare generated reports against
   `tools/m32-oled-webhid-v2.zip` and `CaptureKomplete.pcapng`.

## Notes

This is based on USBPcap/Wireshark traffic and the working WebHID reference, not
on a public Native Instruments protocol specification. Keep the OLED path behind
the build flag until more hardware combinations are tested.
