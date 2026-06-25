# pocketsynth

`pocketsynth` is a real-time synthesizer for the M5Stack Cardputer ADV. The
project started by validating the critical instrument chain: physical keyboard,
note on/off events, mathematical oscillators, up to 8 mixed notes, master
volume, audio buffer rendering, and I2S/speaker output.

The current firmware also includes WiFi Dev Mode, OTA update support, USB Host
diagnostics, USB MIDI input for a Native Instruments Komplete M32, velocity,
sustain, pitch bend, and optional M32 OLED feedback. Stable audio still leads:
development infrastructure and external devices must remain isolated from the
audio render path.

## Current State

- Main PlatformIO/ESP-IDF project at the repo root.
- Refactored firmware split across focused modules in `include/` and `src/`.
- Optional WiFi Dev Mode for `/status`, `/logs`, `/ota`, and `/dev-note`.
- Experimental USB MIDI Host path for the Komplete M32.
- Cardputer ADV UI test bench in `lib/firmware/cardputer_adv_ui_test`.
- Original design references stored in `docs/references/`.

## Documentation

- `docs/iteration-1.md`: full scope of the first iteration.
- `docs/architecture.md`: FreeRTOS architecture, state flow, and real-time rules.
- `docs/ui.md`: compact UI, controls, colors, and JSON reference.
- `docs/implementation-plan.md`: phases 1A-1G with acceptance criteria.
- `docs/decisions.md`: initial technical decisions.
- `docs/dev-mode-ota.md`: local WiFi Dev Mode OTA upload and recovery checklist.
- `docs/midi-komplete-m32.md`: USB MIDI Host behavior and M32 validation flow.
- `AGENTS.md`: guide for code agents working in this repo.

## Build

Main firmware:

```powershell
pio run -e cardputer_adv
```

OTA-capable development build, using two 3 MB app slots on the 8 MB flash:

```powershell
pio run -e cardputer_adv_ota
```

WiFi Dev Mode build, forced active for local OTA testing:

```powershell
pio run -e cardputer_adv_wifi_dev
```

UI test bench:

```powershell
pio run -d lib/firmware/cardputer_adv_ui_test -e cardputer_adv
```

## Target Hardware

- M5Stack Cardputer ADV.
- 240 x 135 px ST7789V2 display in landscape orientation.
- I2S/speaker audio output.
- Integrated physical keyboard.

## Serial Recovery

Keep serial flashing as the recovery path for bad OTA/dev images or partition
changes. Connect over USB, hold the Cardputer ADV boot button while resetting or
plugging in if needed, then reflash from PlatformIO:

```powershell
pio run -e cardputer_adv -t upload
```

For the OTA partition layout, use:

```powershell
pio run -e cardputer_adv_ota -t upload
```

OTA updates must write only application slots. If a partition table changes or
an OTA image cannot boot, recover with serial upload rather than OTA.

## WiFi Dev Mode

WiFi Dev Mode is disabled in the normal `cardputer_adv` build. The firmware only
initializes WiFi when both compile-time flags are present:

```text
POCKETSYNTH_ENABLE_WIFI_DEV_MODE=1
POCKETSYNTH_FORCE_DEV_MODE=1
```

When forced active, it starts a small WPA2 access point. If a non-tracked
`include/wifi_credentials.h` file is present, it also joins that 2.4 GHz station
network; if the file is absent, AP-only Dev Mode still works. It serves:

```text
http://192.168.4.1/status
POST http://192.168.4.1/ota
http://192.168.4.1/logs
POST http://192.168.4.1/dev-note
```

Manual test:

1. Flash `pio run -e cardputer_adv_wifi_dev -t upload`.
2. Connect to WiFi SSID `pocketsynth-dev` with password `pocketsynth`, or keep
   the PC on the same router network as the station credentials.
3. Open `http://192.168.4.1/status` or run:

```powershell
curl http://192.168.4.1/status
```

Expected response:

```json
{"app":"pocketsynth","dev_mode":true,"ota":true}
```

Local OTA upload:

```powershell
python tools/ota_upload.py --host 192.168.4.1 --bin .pio/build/cardputer_adv_ota/firmware.bin
```

WiFi diagnostics:

```powershell
python tools/pocketsynth_status.py --host 192.168.4.1
python tools/pocketsynth_logs.py --host 192.168.4.1
```

The OTA endpoint is only available in Dev Mode. It writes the inactive OTA app
partition, requires the placeholder token `X-PocketSynth-Token:
pocketsynth-dev`, and reboots only after a successful image validation and boot
partition switch. See `docs/dev-mode-ota.md` for the manual checklist and serial
recovery flow.
