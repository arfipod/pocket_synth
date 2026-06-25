# Code Agent Guide

This repository implements `pocketsynth`, a real-time synthesizer for the
M5Stack Cardputer ADV.

The project is now beyond the original iteration-1 firmware only scope. It
contains the base synthesizer, WiFi Dev Mode infrastructure, OTA update support,
USB Host diagnostics, and an experimental USB MIDI Host path for the Native
Instruments Komplete M32.

Before changing code, read these files:

- `README.md`
- `docs/iteration-1.md`
- `docs/architecture.md`
- `docs/ui.md`
- `docs/dev-mode-ota.md`
- `docs/midi-komplete-m32.md`
- `docs/test-checklist.md`
- `docs/midi-komplete-m32-observations.md`

## Project Priorities

Priority order is strict:

1. Stable audio.
2. Reliable boot and recovery.
3. Safe development infrastructure.
4. Reliable keyboard and MIDI input.
5. Compact UI that reflects real state.
6. Basic musical detection.
7. Expressive MIDI features such as velocity and sustain.
8. Advanced synthesis features only after the foundations are stable.

Do not add ADSR, LFO, filters, presets, sequencers, arpeggiators, menus, or deep
sound-design features unless the current task explicitly asks for them.

## Hard Real-Time Audio Rules

The audio render path must remain deterministic.

The function or task that renders audio must not do any of the following:

- `printf`, `ESP_LOG*`, or diagnostic logging.
- `malloc`, `free`, `new`, `delete`, dynamic `std::vector`, or `String`.
- WiFi work.
- USB Host work.
- I2C register transactions.
- Display drawing.
- SD card reads.
- HTTP server work.
- Long mutex waits.
- Blocking queue waits.
- Heavy musical analysis.

Audio must be able to render buffers predictably and write them to I2S.

If a change touches audio rendering, verify that:

- `AudioTask` priority remains the highest runtime task priority.
- Audio state copies are small and bounded.
- Any shared state is protected only by short critical sections.
- No diagnostic code was added to the render loop.
- Audio still works without WiFi, USB Host, or MIDI connected.

## Expected Runtime Architecture

Current intended architecture:

```text
Cardputer keyboard
  -> InputTask
  -> SynthEvent queue
  -> SynthControlTask
  -> SynthAudioState
  -> AudioTask
  -> I2S / codec / speaker

Komplete M32 / USB MIDI
  -> USB Host runtime
  -> UsbMidiHost task
  -> MIDI parser
  -> SynthEvent queue
  -> SynthControlTask
  -> SynthAudioState
  -> AudioTask

SynthControlTask
  -> UiState
  -> UiTask
  -> Cardputer display

WiFi Dev Mode
  -> HTTP /status
  -> HTTP /logs
  -> HTTP /ota
  -> bounded diagnostic ring buffer
```

Audio is not allowed to depend on WiFi, USB, HTTP, or display progress.

## Build Environments

Main serial development build:

```powershell
pio run -e cardputer_adv
```

OTA-capable build using the 8 MB OTA partition layout:

```powershell
pio run -e cardputer_adv_ota
```

Forced WiFi Dev Mode build:

```powershell
pio run -e cardputer_adv_wifi_dev
```

UI test bench:

```powershell
pio run -d lib/firmware/cardputer_adv_ui_test -e cardputer_adv
```

## Hardware Setups

Use these setup names in final reports and validation notes.

### SETUP A - Serial Basic

```text
PC <-> USB-C <-> Cardputer ADV
```

Use for:

- Initial flashing.
- Serial monitor.
- Recovery.
- Basic boot validation.

### SETUP B - Audio Capture

```text
PC <-> USB-C <-> Cardputer ADV
Cardputer audio out / minijack -> Focusrite input -> PC
```

Use for:

- Audio sanity checks.
- Clipping checks.
- Recording sine/square/saw tests.
- Comparing soft vs hard MIDI velocity.

### SETUP C - WiFi Dev

```text
PC and Cardputer ADV on the same WiFi network
or
PC connected to the Cardputer AP `pocketsynth-dev`
```

Use for:

- `/status`
- `/logs`
- `/ota`
- OTA upload scripts.
- WiFi diagnostics.

### SETUP D - MIDI Host Real

```text
Cardputer ADV USB-C -> powered USB-C hub with PD
Hub PD input -> charger or power bank
Hub USB port -> Komplete M32
PC and Cardputer ADV -> same WiFi or Cardputer AP
Optional: Cardputer audio out -> Focusrite -> PC
```

Use for:

- USB Host enumeration.
- Komplete M32 tests.
- USB MIDI packet capture.
- Note On/Off.
- Velocity.
- Sustain.

### SETUP E - Recovery

```text
PC <-> USB-C <-> Cardputer ADV
No hub
No Komplete M32
```

Use when:

- OTA image cannot boot.
- Device is unreachable over WiFi.
- Partition table changed.
- USB Host experiments broke the dev image.

## Flash and Partition Rules

The Cardputer ADV has 8 MB flash. The repository must not revert to the old 2 MB
flash configuration.

OTA must use application slots only.

Do not implement or enable OTA updates for:

- bootloader
- partition table
- secure boot configuration
- flash encryption configuration

If the partition table changes, recover by serial flashing.

## WiFi Dev Mode Rules

WiFi Dev Mode is development infrastructure, not a musical feature.

Rules:

- Normal `cardputer_adv` builds must not start WiFi.
- Dev Mode must be compile-time gated.
- OTA endpoints must be available only in Dev Mode.
- Secrets must not be committed.
- `include/wifi_credentials.h` must remain ignored by Git.
- The firmware must still compile if local WiFi credentials are absent.
- If no station credentials exist, AP-only Dev Mode must still work.
- Do not log the WiFi password.
- Do not log OTA tokens except as placeholder documentation.

Preferred credential behavior:

1. Try stored NVS credentials if implemented.
2. Else try local non-tracked `include/wifi_credentials.h` if present.
3. Else start AP-only Dev Mode.

## OTA Rules

OTA must:

- Write only the inactive app partition.
- Reject images larger than the target partition.
- Validate the image with ESP-IDF OTA APIs before switching boot partition.
- Reboot only after a successful upload and partition switch.
- Keep serial recovery documented.
- Never assume OTA can fix a broken bootloader or partition table.

Rollback support is enabled for OTA builds and should be tested periodically on
hardware.

The boot self-test should be conservative:

- App state and event queue initialization are required.
- Task creation viability is required.
- I2C/I2S/codec results should be logged.
- Peripheral warnings should not necessarily trigger rollback unless explicitly
  declared fatal.
- MIDI device presence must not be required for app validity.
- WiFi success must not be required for normal app validity.

## USB Host Rules

USB Host is experimental and must remain optional.

Rules:

- USB Host code must be isolated from the audio renderer.
- USB Host diagnostics must not be required for normal synth boot.
- USB Host tasks should run at lower priority than audio.
- Disconnect/reconnect must not crash the firmware.
- Hub support may be enabled for powered hub + Komplete M32 testing.
- If USB Host is active, do not assume USB Serial/JTAG is available at the same
  time on the same physical USB-C path.

## USB MIDI Rules

USB MIDI should initially target class-compliant MIDI only.

Do not implement Native Instruments proprietary / NKS / Komplete Kontrol display
or DAW integration unless explicitly requested.

Minimum supported MIDI messages:

- Note On
- Note Off
- Note On with velocity 0 as Note Off
- Control Change
- Pitch Bend

Current integration priority:

1. Note On/Off.
2. Velocity.
3. Sustain CC64.
4. Pitch bend.
5. Mod strip / CC mapping.
6. Other controls.

## MIDI Velocity Rules

Velocity behavior:

- Store raw MIDI velocity per note.
- Store computed `velocityGain` per note.
- Keep Cardputer keyboard notes using a fixed default velocity.
- Keep `masterVolume` separate from per-note velocity.
- Review `PER_NOTE_GAIN` to avoid clipping.
- Do not add ADSR in the same task unless explicitly requested.

Suggested first velocity curve:

```cpp
v = velocity / 127.0f;
gain = 0.10f + 0.90f * sqrtf(v);
```

## Sustain Rules

Sustain CC64 behavior:

- CC64 value >= 64 means sustain on.
- CC64 value < 64 means sustain off.
- While sustain is on, NoteOff should mark a note as released-but-held.
- When sustain turns off, stop notes that are no longer physically held.
- Avoid stuck notes.
- Add an all-notes-off recovery path if needed.
- Do not implement ADSR release as part of the first sustain task.

## Repository Layout

Expected important paths:

```text
src/ and include/
  Main firmware.

docs/
  Project documentation.

docs/references/
  Original design references and UI source.

tools/
  PC-side helper scripts.

scripts/
  PlatformIO auxiliary scripts.

lib/firmware/cardputer_adv_ui_test/
  UI test bench.
```

## Validation Requirements

For main firmware changes:

```powershell
pio run -e cardputer_adv
```

For OTA changes:

```powershell
pio run -e cardputer_adv_ota
pio run -e cardputer_adv_wifi_dev
```

For UI test bench changes:

```powershell
pio run -d lib/firmware/cardputer_adv_ui_test -e cardputer_adv
```

For OTA upload tests:

```powershell
python tools/ota_upload.py --host 192.168.4.1 --bin .pio/build/cardputer_adv_ota/firmware.bin
```

For WiFi diagnostics:

```powershell
python tools/pocketsynth_status.py --host 192.168.4.1
python tools/pocketsynth_logs.py --host 192.168.4.1
```

If hardware tests cannot be run, the final response must say so explicitly and
include a manual checklist for the user.

## Current Known Technical Debt

These items should not be forgotten:

1. WiFi credentials should eventually be stored in NVS or provisioned.
2. OTA rollback behavior should be revalidated after boot-flow changes.
3. Audio buffer / I2S PCM format should be simplified and validated.
4. `PER_NOTE_GAIN` and waveform trims should be confirmed with real recordings.
5. All-notes-off recovery should be added for stuck MIDI notes.
6. Advanced CC mapping remains future work.
7. `docs/test-checklist.md` must be kept current with real hardware results.
